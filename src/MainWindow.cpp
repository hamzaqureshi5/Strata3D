#include "MainWindow.h"
#include "dcm_interactor.h"
#include "meshData.h"

#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QProgressBar>
#include <QRegularExpression>
#include <QScreen>
#include <QSettings>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QtConcurrent/QtConcurrentRun>

#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkDICOMImageReader.h>
#include <vtkImageFlip.h>
#include <vtkImageMapToColors.h>
#include <vtkImageMapper3D.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyDataNormals.h>
#include <vtkProperty.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkVersion.h>

#include <cmath>
#include <limits>

namespace {

// vtkImageViewer2 creates a vtkRenderWindow in its constructor. On Linux VTK's
// default factory makes that an X11/GLX window and probes it immediately, which
// aborts the app with "BadAccess (X_GLXMakeCurrent)" on Wayland desktops (Qt is
// not using X there). Everything is rendered through QVTKOpenGLNativeWidget
// anyway, so hand out the Qt-compatible vtkGenericOpenGLRenderWindow for every
// vtkRenderWindow request.
class GenericRenderWindowFactory : public vtkObjectFactory {
public:
  static GenericRenderWindowFactory *New() {
    auto *f = new GenericRenderWindowFactory;
    f->InitializeObjectBase();
    return f;
  }
  vtkTypeMacro(GenericRenderWindowFactory, vtkObjectFactory);
  const char *GetVTKSourceVersion() override {
    return vtkVersion::GetVTKSourceVersion();
  }
  const char *GetDescription() override {
    return "vtkRenderWindow -> vtkGenericOpenGLRenderWindow";
  }

protected:
  GenericRenderWindowFactory() {
    this->RegisterOverride("vtkRenderWindow", "vtkGenericOpenGLRenderWindow",
                           "Qt-hosted render window", 1, []() -> vtkObject * {
                             return vtkGenericOpenGLRenderWindow::New();
                           });
  }
};

void useGenericRenderWindows() {
  static bool done = false;
  if (done)
    return;
  done = true;

  vtkNew<GenericRenderWindowFactory> factory;
  vtkObjectFactory::RegisterFactory(factory);
  // The OpenGL2 module registered its own vtkRenderWindow override first and
  // would win.
  vtkObjectFactory::SetAllEnableFlags(0, "vtkRenderWindow");
  factory->SetEnableFlag(1, "vtkRenderWindow", "vtkGenericOpenGLRenderWindow");
}

const double kOverlayOpacity = 0.45;
const double kNerveMinDiameter = 1.0, kNerveMaxDiameter = 6.0; // mm
const int kMaxRecent = 8;
const char *kNeedsAi = "Requires the AI segmentation (not integrated yet)";

// Runs on a worker thread. vtkDICOMImageReader applies RescaleSlope/Intercept
// (CT arrives in HU), but it flips the rows and reverses the slice order, which
// turns the anatomy upside down. Flip both back so i, j, k follow the DICOM
// column, row and slice-position axes (x -> patient left, y -> posterior,
// z -> superior for axial scans) and exported meshes are upright.
DicomLoadResult readDicomSeries(const QString &folder) {
  DicomLoadResult result;
  result.folder = folder;

  vtkNew<vtkDICOMImageReader> reader;
  reader->SetDirectoryName(folder.toStdString().c_str());
  reader->ReleaseDataFlagOn();
  vtkNew<vtkImageFlip> flipRows;
  flipRows->SetInputConnection(reader->GetOutputPort());
  flipRows->SetFilteredAxis(1);
  flipRows->ReleaseDataFlagOn();
  vtkNew<vtkImageFlip> flipSlices;
  flipSlices->SetInputConnection(flipRows->GetOutputPort());
  flipSlices->SetFilteredAxis(2);
  flipSlices->Update();

  vtkImageData *output = flipSlices->GetOutput();
  int *dims = output->GetDimensions();
  if (dims[0] < 2 || dims[1] < 2 || dims[2] < 2 ||
      !output->GetPointData()->GetScalars()) {
    result.error = "No readable DICOM series was found in this folder. It "
                   "must contain the slices of one uncompressed CT or CBCT "
                   "series.";
    return result;
  }
  result.volume = vtkSmartPointer<vtkImageData>::New();
  result.volume->ShallowCopy(output);
  result.patientName = QString(reader->GetPatientName()).trimmed();
  return result;
}

QIcon swatch(const double rgb[3]) {
  QPixmap pm(24, 24);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  // a darker outline keeps pale colors visible on light backgrounds
  QColor fill = QColor::fromRgbF(rgb[0], rgb[1], rgb[2]);
  p.setPen(QPen(fill.darker(160), 1.5));
  p.setBrush(fill);
  p.drawRoundedRect(QRectF(3, 3, 18, 18), 4, 4);
  return QIcon(pm);
}

void repolish(QWidget *w) {
  w->style()->unpolish(w);
  w->style()->polish(w);
}

} // namespace

// =================================================================== setup

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
      mRenderWindow(vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New()),
      mRenderer(vtkSmartPointer<vtkRenderer>::New()),
      mInteractor(vtkSmartPointer<QVTKInteractor>::New()),
      mInteractorStyle(
          vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New()) {
  useGenericRenderWindows();
  ui->setupUi(this);
  setAcceptDrops(true);

  qRegisterMetaType<vtkSmartPointer<vtkImageData>>();
  qRegisterMetaType<vtkSmartPointer<vtkPolyData>>();

  auto define = [this](int id, const char *name, const char *file,
                       std::array<double, 3> color,
                       std::array<double, 3> overlay) {
    Structure &s = m_structures[id];
    s.name = name;
    s.fileName = file;
    std::copy(color.begin(), color.end(), s.color);
    std::copy(overlay.begin(), overlay.end(), s.overlayColor);
  };
  define(BoneStructure, "Bone", "bone.stl", {0.89, 0.85, 0.76},
         {1.00, 0.55, 0.00});
  define(TeethStructure, "All teeth", "teeth.stl", {0.62, 0.80, 1.00},
         {0.00, 0.85, 1.00});
  define(ToothStructure, "Single tooth", "tooth.stl", {0.55, 0.95, 0.60},
         {0.10, 1.00, 0.30});
  define(NerveStructure, "Nerve", "nerve.stl", {1.00, 0.85, 0.10},
         {1.00, 0.90, 0.00});

  // Mesh building runs on a worker thread
  m_converter = new DicomConverter(this);
  m_converter->moveToThread(&m_conversionThread);
  connect(&m_conversionThread, &QThread::finished, m_converter,
          &QObject::deleteLater);
  connect(this, &MainWindow::doBuildStructureMesh, m_converter,
          &DicomConverter::buildStructureMesh);
  connect(this, &MainWindow::doBuildSeededMesh, m_converter,
          &DicomConverter::buildSeededMesh);
  connect(m_converter, &DicomConverter::structureMeshReady, this,
          &MainWindow::structureMeshReady);
  connect(m_converter, &DicomConverter::stageChanged, this,
          &MainWindow::setStatus);
  connect(this, &MainWindow::doUpdateProgress, this,
          &MainWindow::updateProgress);
  m_conversionThread.start();

  connect(&m_loadWatcher, &QFutureWatcher<DicomLoadResult>::finished, this,
          &MainWindow::loadFinished);

  // 3D view
  mRenderWindow->AddRenderer(mRenderer);
  mRenderWindow->SetInteractor(mInteractor);
  ui->openGLWidget->setRenderWindow(mRenderWindow);
  mInteractor->SetInteractorStyle(mInteractorStyle);
  mInteractor->Initialize();

  createActions();
  createStatusBar();
  setupSidebar();
  readSettings();

  ui->viewStack->setCurrentWidget(ui->emptyPage);
  configureSetting();
  setBusy(false);
  setHint("Open a DICOM series to begin.");
  setStatus("Ready");
}

MainWindow::~MainWindow() {
  m_loadWatcher.waitForFinished();
  m_conversionThread.quit();
  m_conversionThread.wait();
  delete ui;
}

void MainWindow::createActions() {
  QStyle *st = style();

  // File
  QMenu *file = menuBar()->addMenu("&File");
  m_openAction = file->addAction(st->standardIcon(QStyle::SP_DirOpenIcon),
                                 "&Open DICOM Folder…", this,
                                 &MainWindow::openDicomDialog);
  m_openAction->setShortcut(QKeySequence::Open);
  m_openAction->setStatusTip("Open a folder with one CT or CBCT series");
  m_recentMenu = file->addMenu("Open &Recent");
  m_exportAction =
      file->addAction(st->standardIcon(QStyle::SP_DialogSaveButton),
                      "&Export STL…", this, [this] { exportDialog(); });
  m_exportAction->setShortcut(QKeySequence("Ctrl+E"));
  m_exportAction->setStatusTip("Write one STL file per mesh");
  file->addSeparator();
  m_quitAction = file->addAction("&Quit", this, &QWidget::close);
  m_quitAction->setShortcut(QKeySequence::Quit);
  m_quitAction->setMenuRole(QAction::QuitRole);

  // Edit
  QMenu *edit = menuBar()->addMenu("&Edit");
  m_undoPointAction = edit->addAction("&Undo Nerve Point", this,
                                      &MainWindow::undoNervePoint);
  m_undoPointAction->setShortcut(QKeySequence::Undo);
  m_newCanalAction = edit->addAction("Start &New Nerve Canal", this,
                                     &MainWindow::newNerveCanal);
  m_newCanalAction->setShortcut(QKeySequence("Ctrl+N"));
  m_clearTraceAction = edit->addAction("&Clear Nerve Trace", this,
                                       &MainWindow::clearNerveTrace);

  // View
  QMenu *view = menuBar()->addMenu("&View");
  auto *viewGroup = new QActionGroup(this);
  m_sliceViewAction = view->addAction("&Slices", this,
                                      &MainWindow::showSliceView);
  m_sliceViewAction->setShortcut(QKeySequence(Qt::Key_F2));
  m_view3DAction = view->addAction("&3D", this, &MainWindow::show3DView);
  m_view3DAction->setShortcut(QKeySequence(Qt::Key_F3));
  for (QAction *a : {m_sliceViewAction, m_view3DAction}) {
    a->setCheckable(true);
    viewGroup->addAction(a);
  }
  m_resetViewAction = view->addAction("&Reset View", this,
                                      &MainWindow::resetView);
  m_resetViewAction->setShortcut(QKeySequence("Ctrl+0"));
  view->addSeparator();
  QMenu *themeMenu = view->addMenu("&Theme");
  m_themeGroup = new QActionGroup(this);
  for (auto [label, mode] :
       {std::pair{"&Dark", Theme::Mode::Dark},
        std::pair{"&Light", Theme::Mode::Light},
        std::pair{"Follow &System", Theme::Mode::System}}) {
    QAction *a = themeMenu->addAction(label);
    a->setCheckable(true);
    a->setData(Theme::modeToString(mode));
    m_themeGroup->addAction(a);
    connect(a, &QAction::triggered, this, [this, mode] { setTheme(mode); });
  }
  m_fullScreenAction = view->addAction("&Full Screen");
  m_fullScreenAction->setCheckable(true);
  m_fullScreenAction->setShortcut(QKeySequence::FullScreen);
  connect(m_fullScreenAction, &QAction::toggled, this, [this](bool on) {
    on ? showFullScreen() : showNormal();
  });

  // Segment
  QMenu *segment = menuBar()->addMenu("&Segment");
  const char *keys[StructureCount] = {"Ctrl+1", "Ctrl+2", "Ctrl+3", "Ctrl+4"};
  for (int id = 0; id < StructureCount; ++id) {
    QAction *a = segment->addAction(swatch(m_structures[id].color),
                                    m_structures[id].name, this,
                                    [this, id] { selectStructure(id); });
    a->setShortcut(QKeySequence(keys[id]));
    m_structureActions[id] = a;
  }
  segment->addSeparator();
  m_createMeshAction =
      segment->addAction("&Create Mesh", this, &MainWindow::createMesh);
  m_createMeshAction->setShortcut(QKeySequence("Ctrl+Return"));
  QAction *ai = segment->addAction("&AI Segmentation");
  ai->setEnabled(false);
  ai->setToolTip(kNeedsAi);

  // Help
  QMenu *help = menuBar()->addMenu("&Help");
  QAction *keysAction = help->addAction("&Keyboard Shortcuts", this,
                                        &MainWindow::showShortcuts);
  keysAction->setShortcut(QKeySequence::HelpContents);
  QAction *about = help->addAction("&About", this, &MainWindow::showAbout);
  about->setMenuRole(QAction::AboutRole);

  // Toolbar: the most frequent file and view commands
  QToolBar *bar = addToolBar("Main");
  bar->setObjectName("mainToolBar");
  bar->setMovable(false);
  bar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  bar->addAction(m_openAction);
  bar->addAction(m_exportAction);
  bar->addSeparator();
  bar->addAction(m_resetViewAction);
}

void MainWindow::createStatusBar() {
  m_cursorLabel = new QLabel(this);
  m_cursorLabel->setAccessibleName("Value under the mouse");
  m_progress = new QProgressBar(this);
  m_progress->setTextVisible(false);
  m_progress->setAccessibleName("Progress");
  m_progress->hide();
  statusBar()->addPermanentWidget(m_cursorLabel);
  statusBar()->addPermanentWidget(m_progress);
}

void MainWindow::setupSidebar() {
  // Structure picker
  m_structureButtons = new QButtonGroup(this);
  m_structureButtons->setExclusive(true);
  const std::array<QToolButton *, StructureCount> buttons = {
      ui->boneButton, ui->teethButton, ui->toothButton, ui->nerveButton};
  const char *tips[StructureCount] = {
      "Bone by threshold (Ctrl+1)", "All teeth by threshold (Ctrl+2)",
      "One tooth: set the threshold, then click the tooth (Ctrl+3)",
      "Nerve canal: click points along it on the slices (Ctrl+4)"};
  for (int id = 0; id < StructureCount; ++id) {
    buttons[id]->setIcon(swatch(m_structures[id].color));
    buttons[id]->setToolTip(tips[id]);
    buttons[id]->setAccessibleName(m_structures[id].name);
    m_structureButtons->addButton(buttons[id], id);
  }
  connect(m_structureButtons, &QButtonGroup::idClicked, this,
          &MainWindow::selectStructure);
  ui->aiButton->setToolTip(kNeedsAi);

  // Threshold / diameter: slider and spin box stay in sync
  ui->settingSlider->setAccessibleName("Threshold");
  ui->settingSpin->setAccessibleName("Threshold value");
  // the slider, wheel and arrow keys adjust it; arrow buttons only add clutter
  ui->settingSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
  connect(ui->settingSlider, &QSlider::valueChanged, this,
          &MainWindow::sliderChanged);
  connect(ui->settingSpin, &QDoubleSpinBox::valueChanged, this,
          &MainWindow::spinChanged);

  connect(ui->createMeshButton, &QPushButton::clicked, this,
          &MainWindow::createMesh);
  ui->createMeshButton->setToolTip("Build the mesh of the selected structure "
                                   "(Ctrl+Enter)");
  connect(ui->exportButton, &QPushButton::clicked, this,
          [this] { exportDialog(); });
  ui->exportButton->setToolTip("Write one STL file per mesh (Ctrl+E)");

  m_meshModel = new QStandardItemModel(this);
  ui->meshList->setModel(m_meshModel);
  ui->meshList->setAccessibleName("Meshes");
  connect(m_meshModel, &QStandardItemModel::itemChanged, this,
          &MainWindow::meshItemChanged);
  connect(ui->meshList, &QWidget::customContextMenuRequested, this,
          &MainWindow::meshContextMenu);
  auto *deleteAction = new QAction("Delete Mesh", ui->meshList);
  deleteAction->setShortcut(QKeySequence::Delete);
  deleteAction->setShortcutContext(Qt::WidgetShortcut);
  connect(deleteAction, &QAction::triggered, this, [this] {
    QModelIndex idx = ui->meshList->currentIndex();
    if (idx.isValid() && idx.data(Qt::UserRole).isValid())
      deleteMesh(idx.data(Qt::UserRole).toInt());
  });
  ui->meshList->addAction(deleteAction);
  refreshMeshList();

  // Viewer header and empty state
  connect(ui->sliceViewButton, &QToolButton::clicked, this,
          &MainWindow::showSliceView);
  connect(ui->view3DButton, &QToolButton::clicked, this,
          &MainWindow::show3DView);
  ui->sliceViewButton->setToolTip("Slice view (F2)");
  ui->view3DButton->setToolTip("3D view (F3)");
  connect(ui->emptyOpenButton, &QPushButton::clicked, this,
          &MainWindow::openDicomDialog);
  connect(ui->emptyRecent, &QLabel::linkActivated, this,
          [this](const QString &folder) { loadDicomFolder(folder); });
  ui->sliceSlider->setAccessibleName("Slice");
  ui->sliceSlider->setToolTip("Slice (mouse wheel or Up/Down in the view)");
  connect(ui->sliceSlider, &QSlider::valueChanged, this,
          &MainWindow::sliceSliderChanged);
  ui->openGLWidget->setToolTip(
      "Slices: mouse wheel or Up/Down to scroll, left drag for contrast, "
      "right drag to zoom, middle drag to pan.\n3D: left drag to rotate, "
      "right drag or wheel to zoom.");

  ui->mainSplitter->setHandleWidth(1);
  ui->mainSplitter->setStretchFactor(0, 0);
  ui->mainSplitter->setStretchFactor(1, 1);
  ui->mainSplitter->setSizes({300, 1100});
}

// ------------------------------------------------------------- settings

void MainWindow::readSettings() {
  QSettings s;
  m_lastDir = s.value("lastDir").toString();
  m_recent = s.value("recent").toStringList();
  setTheme(Theme::modeFromString(s.value("theme", "dark").toString()));

  if (!restoreGeometry(s.value("geometry").toByteArray())) {
    // first start: 80 % of the screen, centred
    QRect avail = screen()->availableGeometry();
    resize(std::max(minimumWidth(), avail.width() * 4 / 5),
           std::max(minimumHeight(), avail.height() * 4 / 5));
    move(avail.center() - rect().center());
  }
  restoreState(s.value("windowState").toByteArray());
  ui->mainSplitter->restoreState(s.value("splitter").toByteArray());
  updateRecentUi();
}

void MainWindow::writeSettings() const {
  QSettings s;
  s.setValue("geometry", saveGeometry());
  s.setValue("windowState", saveState());
  s.setValue("splitter", ui->mainSplitter->saveState());
  s.setValue("theme", Theme::modeToString(m_themeMode));
  s.setValue("lastDir", m_lastDir);
  s.setValue("recent", m_recent);
}

void MainWindow::setTheme(Theme::Mode mode) {
  m_themeMode = mode;
  const Theme::Tokens &t = Theme::apply(*qApp, mode);
  const QColor bg = t.viewport3D;
  mRenderer->SetBackground(bg.redF(), bg.greenF(), bg.blueF());
  for (QAction *a : m_themeGroup->actions())
    a->setChecked(a->data().toString() == Theme::modeToString(mode));
  if (m_showing3D)
    mRenderWindow->Render();
  QSettings().setValue("theme", Theme::modeToString(mode));
}

void MainWindow::addRecent(const QString &folder) {
  m_recent.removeAll(folder);
  m_recent.prepend(folder);
  while (m_recent.size() > kMaxRecent)
    m_recent.removeLast();
  QSettings().setValue("recent", m_recent);
  updateRecentUi();
}

void MainWindow::updateRecentUi() {
  m_recentMenu->clear();
  QStringList links;
  for (const QString &folder : m_recent) {
    QString name = QDir(folder).dirName();
    QAction *a = m_recentMenu->addAction(name, this,
                                         [this, folder] {
                                           loadDicomFolder(folder);
                                         });
    a->setToolTip(folder);
    a->setStatusTip(folder);
    if (links.size() < 4)
      links << QString("<a href=\"%1\">%2</a>")
                   .arg(folder.toHtmlEscaped(), name.toHtmlEscaped());
  }
  m_recentMenu->setEnabled(!m_recent.isEmpty());
  if (!m_recent.isEmpty()) {
    m_recentMenu->addSeparator();
    m_recentMenu->addAction("Clear Recent", this, [this] {
      m_recent.clear();
      QSettings().setValue("recent", m_recent);
      updateRecentUi();
    });
  }
  ui->emptyRecent->setText(links.isEmpty()
                               ? QString()
                               : "Recent: " + links.join(" &nbsp;·&nbsp; "));
}

// ------------------------------------------------------------ window events

void MainWindow::closeEvent(QCloseEvent *event) {
  if (m_busy || m_loading) {
    QMessageBox::information(this, "Please wait",
                             "A scan is being read or a mesh is being "
                             "built. Close the window when it has finished.");
    event->ignore();
    return;
  }
  if (!confirmDiscardMeshes("quit")) {
    event->ignore();
    return;
  }
  writeSettings();
  event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
  const QList<QUrl> urls = event->mimeData()->urls();
  if (!urls.isEmpty() && urls.first().isLocalFile() && !m_busy && !m_loading)
    event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event) {
  QFileInfo info(event->mimeData()->urls().first().toLocalFile());
  // a dropped DICOM file opens the series in its folder
  loadDicomFolder(info.isDir() ? info.absoluteFilePath() : info.absolutePath());
  event->acceptProposedAction();
}

// =================================================================== loading

void MainWindow::openDicomDialog() {
  QString start = m_lastDir.isEmpty() ? QDir::homePath() : m_lastDir;
  QString folder =
      QFileDialog::getExistingDirectory(this, "Open DICOM Folder", start);
  if (!folder.isEmpty())
    loadDicomFolder(folder);
}

bool MainWindow::loadDicomFolder(const QString &folder) {
  if (m_busy || m_loading)
    return false;
  if (!QFileInfo(folder).isDir()) {
    QMessageBox::warning(this, "Cannot open folder",
                         QString("%1 is not a folder.").arg(folder));
    return false;
  }
  if (!confirmDiscardMeshes("open another scan"))
    return false;

  m_loading = true;
  setBusy(true);
  m_progress->setRange(0, 0); // indeterminate
  m_progress->show();
  setStatus(QString("Reading %1…").arg(QDir(folder).dirName()));
  m_loadWatcher.setFuture(QtConcurrent::run(readDicomSeries, folder));
  return true;
}

void MainWindow::loadFinished() {
  DicomLoadResult result = m_loadWatcher.result();
  m_loading = false;
  m_progress->hide();
  m_progress->setRange(0, 100);
  if (!result.volume) {
    setBusy(false);
    setStatus("Could not open " + QDir(result.folder).dirName());
    QMessageBox::warning(this, "Cannot open DICOM series",
                         QString("%1\n\n%2").arg(result.error, result.folder));
    return;
  }

  m_dicom_path = result.folder;
  m_lastDir = QFileInfo(result.folder).absolutePath();
  addRecent(result.folder);
  m_volume = result.volume;
  m_patientName = result.patientName;

  // New scan: drop meshes and markers of the previous one
  for (Structure &s : m_structures) {
    if (s.actor)
      mRenderer->RemoveActor(s.actor);
    s.mesh = nullptr;
    s.actor = nullptr;
  }
  setUnexported(false);
  m_hasToothSeed = false;
  m_nervePaths.clear();

  ThresholdPresets presets = estimateThresholdPresets(m_volume);
  m_hounsfield = presets.hounsfield;
  m_structures[BoneStructure].lower = presets.bone;
  m_structures[TeethStructure].lower = presets.teeth;
  m_structures[ToothStructure].lower = presets.teeth;

  double range[2];
  m_volume->GetScalarRange(range);
  m_valueRange[0] = static_cast<int>(std::floor(range[0]));
  m_valueRange[1] = static_cast<int>(std::ceil(range[1]));

  // Study panel
  int *dims = m_volume->GetDimensions();
  double *sp = m_volume->GetSpacing();
  ui->patientLabel->setText(m_patientName.isEmpty()
                                ? QDir(result.folder).dirName()
                                : m_patientName);
  ui->studyDetailsLabel->setText(
      QString("%1 × %2 × %3 voxels\n%4 × %5 × %6 mm\n%7")
          .arg(dims[0])
          .arg(dims[1])
          .arg(dims[2])
          .arg(sp[0], 0, 'g', 3)
          .arg(sp[1], 0, 'g', 3)
          .arg(sp[2], 0, 'g', 3)
          .arg(m_hounsfield ? "CT, Hounsfield units"
                            : "Uncalibrated gray values (e.g. CBCT)"));
  ui->studyDetailsLabel->setToolTip(result.folder);
  setWindowTitle(QString("%1[*] — DICOM Segmentation")
                     .arg(ui->patientLabel->text()));

  ui->viewStack->setCurrentWidget(ui->viewerPage);
  if (!imageViewer)
    createSliceView();

  imageViewer->SetInputData(m_volume);
  m_overlayThreshold->SetInputData(m_volume);
  static_cast<myVtkInteractorStyleImage *>(m_sliceStyle.Get())
      ->SetImageViewer(imageViewer);
  if (m_hounsfield) {
    imageViewer->SetColorWindow(3000);
    imageViewer->SetColorLevel(700);
  } else {
    imageViewer->SetColorWindow(range[1] - range[0]);
    imageViewer->SetColorLevel((range[0] + range[1]) / 2.0);
  }
  int sliceMin = imageViewer->GetSliceMin();
  int sliceMax = imageViewer->GetSliceMax();
  {
    QSignalBlocker block(ui->sliceSlider);
    ui->sliceSlider->setRange(sliceMin, sliceMax);
  }
  imageViewer->SetSlice((sliceMin + sliceMax) / 2);

  m_structureButtons->setExclusive(false);
  for (QAbstractButton *b : m_structureButtons->buttons())
    b->setChecked(false);
  m_structureButtons->setExclusive(true);
  m_activeStructure = -1;
  m_resetMeshCamera = true;
  configureSetting();
  updateOverlay();
  updateAnnotations();
  refreshMeshList();
  setBusy(false);

  showSliceView();
  resetView();
  setHint(structureHint(-1));
  setStatus(QString("Opened %1").arg(QDir(result.folder).dirName()));
}

// ================================================================ slice view

void MainWindow::createSliceView() {
  imageViewer = vtkSmartPointer<vtkImageViewer2>::New();
  m_sliceRenderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
  m_sliceInteractor = vtkSmartPointer<QVTKInteractor>::New();
  m_sliceRenderWindow->SetInteractor(m_sliceInteractor);
  imageViewer->SetRenderWindow(m_sliceRenderWindow);
  imageViewer->SetupInteractor(m_sliceInteractor);

  vtkNew<myVtkInteractorStyleImage> style;
  style->SetInteractionModeToImageSlicing();
  m_sliceStyle = style;
  m_sliceInteractor->SetInteractorStyle(m_sliceStyle);

  // Threshold overlay: voxels inside the selected structure's range, tinted
  // with its color. Only the displayed slice is computed (streaming).
  m_overlayThreshold = vtkSmartPointer<vtkImageThreshold>::New();
  m_overlayThreshold->SetInValue(1);
  m_overlayThreshold->SetOutValue(0);
  m_overlayThreshold->ReplaceInOn();
  m_overlayThreshold->ReplaceOutOn();
  m_overlayThreshold->SetOutputScalarTypeToUnsignedChar();

  m_overlayLut = vtkSmartPointer<vtkLookupTable>::New();
  m_overlayLut->SetNumberOfTableValues(2);
  m_overlayLut->SetTableRange(0, 1);
  m_overlayLut->Build();
  m_overlayLut->SetTableValue(0, 0, 0, 0, 0);

  vtkNew<vtkImageMapToColors> toColors;
  toColors->SetLookupTable(m_overlayLut);
  toColors->SetOutputFormatToRGBA();
  toColors->SetInputConnection(m_overlayThreshold->GetOutputPort());

  m_overlayActor = vtkSmartPointer<vtkImageActor>::New();
  m_overlayActor->GetMapper()->SetInputConnection(toColors->GetOutputPort());
  m_overlayActor->GetMapper()->StreamingOn();
  m_overlayActor->InterpolateOff();
  // RGBA with transparent background; without this it is drawn opaque (black)
  m_overlayActor->ForceTranslucentOn();
  m_overlayActor->VisibilityOff();
  imageViewer->GetRenderer()->AddViewProp(m_overlayActor);

  // Tooth seed (point) and nerve trace (points + line), projected onto the
  // current slice so the whole trace stays visible while scrolling
  m_seedMarker = vtkSmartPointer<vtkPolyData>::New();
  vtkNew<vtkPolyDataMapper> seedMapper;
  seedMapper->SetInputData(m_seedMarker);
  m_seedActor = vtkSmartPointer<vtkActor>::New();
  m_seedActor->SetMapper(seedMapper);
  m_seedActor->GetProperty()->SetColor(1.0, 0.15, 0.15);
  m_seedActor->GetProperty()->SetPointSize(14);
  m_seedActor->GetProperty()->RenderPointsAsSpheresOn();
  m_seedActor->GetProperty()->LightingOff();
  imageViewer->GetRenderer()->AddViewProp(m_seedActor);

  m_nerveTrace = vtkSmartPointer<vtkPolyData>::New();
  vtkNew<vtkPolyDataMapper> traceMapper;
  traceMapper->SetInputData(m_nerveTrace);
  m_nerveTraceActor = vtkSmartPointer<vtkActor>::New();
  m_nerveTraceActor->SetMapper(traceMapper);
  m_nerveTraceActor->GetProperty()->SetColor(
      m_structures[NerveStructure].overlayColor);
  m_nerveTraceActor->GetProperty()->SetPointSize(9);
  m_nerveTraceActor->GetProperty()->SetLineWidth(2);
  m_nerveTraceActor->GetProperty()->RenderPointsAsSpheresOn();
  m_nerveTraceActor->GetProperty()->LightingOff();
  imageViewer->GetRenderer()->AddViewProp(m_nerveTraceActor);

  // Before every render: keep overlay, markers, slice label and slider in
  // sync with the viewer's slice, however it was changed.
  m_sliceRenderCB = vtkSmartPointer<vtkCallbackCommand>::New();
  m_sliceRenderCB->SetCallback(&MainWindow::onSliceRenderStart);
  m_sliceRenderCB->SetClientData(this);
  m_sliceRenderWindow->AddObserver(vtkCommand::StartEvent, m_sliceRenderCB);

  // Clicks and keys for Single tooth / Nerve run before the interactor style
  // (priority 1) and swallow the events they handle; mouse moves update the
  // value readout.
  m_sliceInputCB = vtkSmartPointer<vtkCallbackCommand>::New();
  m_sliceInputCB->SetCallback(&MainWindow::onSliceInput);
  m_sliceInputCB->SetClientData(this);
  for (unsigned long event :
       {vtkCommand::LeftButtonPressEvent, vtkCommand::KeyPressEvent,
        vtkCommand::MouseMoveEvent, vtkCommand::LeaveEvent})
    m_sliceInteractor->AddObserver(event, m_sliceInputCB, 1.0);
}

void MainWindow::onSliceRenderStart(vtkObject *, unsigned long,
                                    void *clientData, void *) {
  static_cast<MainWindow *>(clientData)->syncSliceUi();
}

void MainWindow::onSliceInput(vtkObject *, unsigned long eventId,
                              void *clientData, void *) {
  auto *self = static_cast<MainWindow *>(clientData);
  if (!self->m_volume)
    return;
  if (eventId == vtkCommand::MouseMoveEvent ||
      eventId == vtkCommand::LeaveEvent) {
    if (eventId == vtkCommand::LeaveEvent) {
      self->m_cursorIJ[0] = self->m_cursorIJ[1] = -1;
    } else {
      int *xy = self->m_sliceInteractor->GetEventPosition();
      vtkRenderer *ren = self->imageViewer->GetRenderer();
      ren->SetDisplayPoint(xy[0], xy[1], 0.0);
      ren->DisplayToWorld();
      double w[4];
      ren->GetWorldPoint(w);
      if (w[3] != 0.0)
        for (int a = 0; a < 3; ++a)
          w[a] /= w[3];
      double o[3], sp[3];
      self->m_volume->GetOrigin(o);
      self->m_volume->GetSpacing(sp);
      self->m_cursorIJ[0] = static_cast<int>(std::lround((w[0] - o[0]) / sp[0]));
      self->m_cursorIJ[1] = static_cast<int>(std::lround((w[1] - o[1]) / sp[1]));
    }
    self->updateCursorInfo();
    return; // never swallow: the style still pans, zooms and adjusts contrast
  }

  if (self->m_busy || (self->m_activeStructure != ToothStructure &&
                       self->m_activeStructure != NerveStructure))
    return;
  bool handled = false;
  if (eventId == vtkCommand::LeftButtonPressEvent) {
    self->onSliceClick();
    handled = true;
  } else if (eventId == vtkCommand::KeyPressEvent) {
    const char *key = self->m_sliceInteractor->GetKeySym();
    handled = key && self->onSliceKey(key);
  }
  if (handled)
    self->m_sliceInputCB->SetAbortFlag(1);
}

void MainWindow::updateCursorInfo() {
  int extent[6];
  m_volume->GetExtent(extent);
  int i = m_cursorIJ[0], j = m_cursorIJ[1], k = imageViewer->GetSlice();
  if (i < extent[0] || i > extent[1] || j < extent[2] || j > extent[3]) {
    m_cursorLabel->clear();
    return;
  }
  double value = m_volume->GetScalarComponentAsDouble(i, j, k, 0);
  m_cursorLabel->setText(QString("x %1  y %2  slice %3   %4")
                             .arg(i)
                             .arg(j)
                             .arg(k + 1)
                             .arg(thresholdText(std::round(value))));
}

void MainWindow::onSliceClick() {
  int *xy = m_sliceInteractor->GetEventPosition();
  vtkRenderer *renderer = imageViewer->GetRenderer();
  renderer->SetDisplayPoint(xy[0], xy[1], 0.0);
  renderer->DisplayToWorld();
  double world[4];
  renderer->GetWorldPoint(world);
  if (world[3] != 0.0)
    for (int a = 0; a < 3; ++a)
      world[a] /= world[3];

  // Parallel projection: x, y are exact; z is the displayed slice
  double origin[3], spacing[3];
  int extent[6];
  m_volume->GetOrigin(origin);
  m_volume->GetSpacing(spacing);
  m_volume->GetExtent(extent);
  int slice = imageViewer->GetSlice();
  world[2] = origin[2] + slice * spacing[2];
  int i = static_cast<int>(std::lround((world[0] - origin[0]) / spacing[0]));
  int j = static_cast<int>(std::lround((world[1] - origin[1]) / spacing[1]));
  if (i < extent[0] || i > extent[1] || j < extent[2] || j > extent[3]) {
    setHint("Click inside the image.", true);
    return;
  }

  if (m_activeStructure == ToothStructure)
    setToothSeed(i, j, slice);
  else
    addNervePoint({world[0], world[1], world[2]});
}

bool MainWindow::onSliceKey(const std::string &key) {
  if (m_activeStructure != NerveStructure)
    return false;
  if (key == "BackSpace" || key == "Delete")
    undoNervePoint();
  else if (key == "Return" || key == "KP_Enter")
    newNerveCanal();
  else if (key == "Escape")
    clearNerveTrace();
  else
    return false;
  return true;
}

void MainWindow::setToothSeed(int i, int j, int k) {
  m_toothSeed[0] = i;
  m_toothSeed[1] = j;
  m_toothSeed[2] = k;
  m_hasToothSeed = true;
  updateAnnotations();
  if (!m_showing3D)
    imageViewer->Render();
  setHint(structureHint(ToothStructure));
  updateActions();
}

void MainWindow::addNervePoint(const Point3 &position) {
  if (m_nervePaths.empty())
    m_nervePaths.emplace_back();
  m_nervePaths.back().push_back(position);
  updateAnnotations();
  if (!m_showing3D)
    imageViewer->Render();
  setHint(nerveHelp());
  updateActions();
}

void MainWindow::undoNervePoint() {
  if (m_nervePaths.empty())
    return;
  // last point, or step back into the previous canal
  if (m_nervePaths.back().empty() && m_nervePaths.size() > 1)
    m_nervePaths.pop_back();
  if (!m_nervePaths.back().empty())
    m_nervePaths.back().pop_back();
  updateAnnotations();
  if (imageViewer && !m_showing3D)
    imageViewer->Render();
  setHint(nerveHelp());
  updateActions();
}

void MainWindow::newNerveCanal() {
  if (!m_nervePaths.empty() && !m_nervePaths.back().empty())
    m_nervePaths.emplace_back();
  setHint(nerveHelp());
  updateActions();
}

void MainWindow::clearNerveTrace() {
  m_nervePaths.clear();
  updateAnnotations();
  if (imageViewer && !m_showing3D)
    imageViewer->Render();
  setHint(nerveHelp());
  updateActions();
}

QString MainWindow::nerveHelp() const {
  int points = 0, canals = 0;
  for (const NervePath &p : m_nervePaths) {
    points += static_cast<int>(p.size());
    canals += p.size() >= 2 ? 1 : 0;
  }
  return QString("%1 point(s), %2 canal(s) ready.\nClick along the canal on "
                 "the slices. Backspace: undo · Enter: start the other "
                 "canal · Esc: clear. Then Create mesh.")
      .arg(points)
      .arg(canals);
}

void MainWindow::updateAnnotations() {
  if (!m_seedMarker || !m_volume)
    return;
  int slice = imageViewer->GetSlice();
  double origin[3], spacing[3];
  m_volume->GetOrigin(origin);
  m_volume->GetSpacing(spacing);
  double *dop =
      imageViewer->GetRenderer()->GetActiveCamera()->GetDirectionOfProjection();
  // on the slice, slightly towards the camera so they are drawn on top
  double z = origin[2] + slice * spacing[2] - 0.5 * spacing[2] * dop[2];

  {
    vtkNew<vtkPoints> points;
    vtkNew<vtkCellArray> verts;
    if (m_hasToothSeed && m_activeStructure == ToothStructure) {
      points->InsertNextPoint(origin[0] + m_toothSeed[0] * spacing[0],
                              origin[1] + m_toothSeed[1] * spacing[1], z);
      vtkIdType id = 0;
      verts->InsertNextCell(1, &id);
    }
    m_seedMarker->Initialize();
    m_seedMarker->SetPoints(points);
    m_seedMarker->SetVerts(verts);
    m_seedMarker->Modified();
    // the seed only belongs to its own slice
    m_seedActor->SetVisibility(m_hasToothSeed &&
                               m_activeStructure == ToothStructure &&
                               slice == m_toothSeed[2]);
  }

  vtkNew<vtkPoints> points;
  vtkNew<vtkCellArray> verts, lines;
  if (m_activeStructure == NerveStructure) {
    for (const NervePath &path : m_nervePaths) {
      std::vector<vtkIdType> ids;
      for (const Point3 &p : path) {
        vtkIdType id = points->InsertNextPoint(p[0], p[1], z);
        verts->InsertNextCell(1, &id);
        ids.push_back(id);
      }
      if (ids.size() >= 2)
        lines->InsertNextCell(static_cast<vtkIdType>(ids.size()), ids.data());
    }
  }
  m_nerveTrace->Initialize();
  m_nerveTrace->SetPoints(points);
  m_nerveTrace->SetVerts(verts);
  m_nerveTrace->SetLines(lines);
  m_nerveTrace->Modified();
}

void MainWindow::syncSliceUi() {
  if (!m_volume)
    return;

  int *extent = imageViewer->GetImageActor()->GetDisplayExtent();
  m_overlayActor->SetDisplayExtent(extent);
  // Lift the overlay a little towards the camera so it is drawn over the slice
  double *spacing = m_volume->GetSpacing();
  double *dop =
      imageViewer->GetRenderer()->GetActiveCamera()->GetDirectionOfProjection();
  m_overlayActor->SetPosition(0, 0, -0.25 * spacing[2] * dop[2]);

  updateAnnotations();

  int slice = imageViewer->GetSlice();
  ui->sliceLabel->setText(QString("Slice %1 / %2")
                              .arg(slice + 1)
                              .arg(imageViewer->GetSliceMax() + 1));
  if (ui->sliceSlider->value() != slice) {
    QSignalBlocker block(ui->sliceSlider);
    ui->sliceSlider->setValue(slice);
  }
  if (m_cursorIJ[0] >= 0)
    updateCursorInfo();
}

void MainWindow::sliceSliderChanged(int value) {
  if (!imageViewer || !m_volume)
    return;
  imageViewer->SetSlice(value);
  imageViewer->Render();
}

void MainWindow::showSliceView() {
  if (!m_volume) {
    setStatus("Open a DICOM series first");
    updateActions();
    return;
  }
  ui->openGLWidget->setRenderWindow(m_sliceRenderWindow);
  m_showing3D = false;
  ui->sliceSlider->show();
  ui->sliceLabel->show();
  updateActions();
  imageViewer->Render();
}

void MainWindow::show3DView() {
  if (!m_volume) {
    setStatus("Open a DICOM series first");
    updateActions();
    return;
  }
  ui->openGLWidget->setRenderWindow(mRenderWindow);
  m_showing3D = true;
  ui->sliceSlider->hide();
  ui->sliceLabel->hide();
  m_cursorLabel->clear();
  if (!hasMeshes())
    setStatus("No meshes yet: choose a structure and Create mesh");
  updateActions();
  mRenderWindow->Render();
}

void MainWindow::resetView() {
  if (!m_volume)
    return;
  if (m_showing3D) {
    // Facing the patient (from anterior, -y), slightly from above
    vtkCamera *camera = mRenderer->GetActiveCamera();
    camera->SetFocalPoint(0, 0, 0);
    camera->SetPosition(0, -1, 0);
    camera->SetViewUp(0, 0, 1);
    camera->Elevation(30);
    camera->OrthogonalizeViewUp();
    mRenderer->ResetCamera();
    mRenderWindow->Render();
  } else {
    // Radiological view: looking from the feet, anterior up, patient's left
    // on the right of the screen
    vtkCamera *camera = imageViewer->GetRenderer()->GetActiveCamera();
    camera->SetFocalPoint(0, 0, 0);
    camera->SetPosition(0, 0, -1);
    camera->SetViewUp(0, -1, 0);
    imageViewer->GetRenderer()->ResetCamera();
    imageViewer->Render();
  }
}

// ============================================================ segmentation

void MainWindow::structureRange(int structureId, double &lower, bool &hasUpper,
                                double &upper) const {
  lower = m_structures[structureId].lower;
  hasUpper = false;
  upper = std::numeric_limits<double>::max();
  // Bone stops where teeth start, so the two meshes do not overlap
  if (structureId == BoneStructure &&
      m_structures[TeethStructure].lower > lower) {
    hasUpper = true;
    upper = m_structures[TeethStructure].lower;
  }
}

QString MainWindow::thresholdText(double value) const {
  return m_hounsfield ? QString("%1 HU").arg(value) : QString::number(value);
}

QString MainWindow::settingText(int structureId, double value) const {
  if (structureId == NerveStructure)
    return QString("⌀ %1 mm").arg(value);
  return thresholdText(value);
}

QString MainWindow::structureHint(int structureId) const {
  switch (structureId) {
  case BoneStructure:
    return "Adjust the threshold until the orange overlay covers the bone, "
           "then Create mesh. Bone stops where tooth density starts.";
  case TeethStructure:
    return "Adjust the threshold until the cyan overlay covers the teeth, "
           "then Create mesh.";
  case ToothStructure:
    if (m_hasToothSeed)
      return QString("Tooth selected on slice %1. Create mesh, or click "
                     "another tooth.")
          .arg(m_toothSeed[2] + 1);
    return "Adjust the threshold until the green overlay covers the teeth, "
           "then click one tooth on a slice.";
  case NerveStructure:
    return nerveHelp();
  default:
    return "Choose a structure to segment.";
  }
}

void MainWindow::configureSetting() {
  const bool nerve = m_activeStructure == NerveStructure;
  QSignalBlocker b1(ui->settingSlider), b2(ui->settingSpin);
  if (nerve) {
    ui->settingSection->setText("DIAMETER");
    ui->settingSlider->setRange(static_cast<int>(kNerveMinDiameter * 10),
                                static_cast<int>(kNerveMaxDiameter * 10));
    ui->settingSlider->setValue(
        static_cast<int>(std::lround(m_nerveDiameter * 10)));
    ui->settingSpin->setDecimals(1);
    ui->settingSpin->setSingleStep(0.1);
    ui->settingSpin->setRange(kNerveMinDiameter, kNerveMaxDiameter);
    ui->settingSpin->setSuffix(" mm");
    ui->settingSpin->setSpecialValueText("");
    ui->settingSpin->setValue(m_nerveDiameter);
    ui->settingSlider->setAccessibleName("Nerve diameter");
  } else if (m_activeStructure < 0) {
    // nothing selected yet: placeholder instead of a meaningless 0
    ui->settingSection->setText("THRESHOLD");
    ui->settingSlider->setRange(0, 0);
    ui->settingSpin->setRange(0, 0);
    ui->settingSpin->setSuffix("");
    ui->settingSpin->setSpecialValueText("—");
    ui->settingSlider->setAccessibleName("Threshold");
  } else {
    ui->settingSection->setText("THRESHOLD");
    ui->settingSlider->setRange(m_valueRange[0], m_valueRange[1]);
    ui->settingSpin->setDecimals(0);
    ui->settingSpin->setSingleStep(10);
    ui->settingSpin->setRange(m_valueRange[0], m_valueRange[1]);
    ui->settingSpin->setSuffix(m_hounsfield ? " HU" : "");
    ui->settingSpin->setSpecialValueText("");
    if (m_activeStructure >= 0) {
      double v = m_structures[m_activeStructure].lower;
      ui->settingSlider->setValue(static_cast<int>(v));
      ui->settingSpin->setValue(v);
    }
    ui->settingSlider->setAccessibleName("Threshold");
  }
  ui->settingSpin->setAccessibleName(ui->settingSlider->accessibleName() +
                                     " value");
}

void MainWindow::selectStructure(int structureId) {
  if (!m_volume || m_busy)
    return;
  m_activeStructure = structureId;
  m_structureButtons->button(structureId)->setChecked(true);
  configureSetting();
  const double *c = m_structures[structureId].overlayColor;
  m_overlayLut->SetTableValue(1, c[0], c[1], c[2], kOverlayOpacity);
  m_overlayLut->Modified();
  if (m_showing3D)
    showSliceView();
  updateOverlay();
  updateAnnotations();
  imageViewer->Render();
  setHint(structureHint(structureId));
  updateActions();
  // clicks and keys for Single tooth / Nerve go to the view
  if (structureId == ToothStructure || structureId == NerveStructure)
    ui->openGLWidget->setFocus();
}

void MainWindow::sliderChanged(int value) {
  if (m_activeStructure < 0)
    return;
  double v = m_activeStructure == NerveStructure ? value / 10.0 : value;
  {
    QSignalBlocker block(ui->settingSpin);
    ui->settingSpin->setValue(v);
  }
  spinChanged(v);
}

void MainWindow::spinChanged(double value) {
  if (m_activeStructure < 0)
    return;
  {
    QSignalBlocker block(ui->settingSlider);
    ui->settingSlider->setValue(static_cast<int>(std::lround(
        m_activeStructure == NerveStructure ? value * 10 : value)));
  }
  if (m_activeStructure == NerveStructure) {
    m_nerveDiameter = value;
    return;
  }
  m_structures[m_activeStructure].lower = value;
  updateOverlay();
}

void MainWindow::updateOverlay() {
  if (!m_overlayActor)
    return;
  if (m_activeStructure < 0 || m_activeStructure == NerveStructure ||
      !m_volume) {
    m_overlayActor->VisibilityOff();
  } else {
    double lower, upper;
    bool hasUpper;
    structureRange(m_activeStructure, lower, hasUpper, upper);
    if (hasUpper)
      m_overlayThreshold->ThresholdBetween(lower, upper - 1e-3);
    else
      m_overlayThreshold->ThresholdByUpper(lower);
    m_overlayActor->VisibilityOn();
  }
  if (!m_showing3D && imageViewer)
    imageViewer->Render();
}

void MainWindow::createMesh() {
  if (!m_volume || m_busy)
    return;
  if (m_activeStructure < 0) {
    setHint("Choose a structure first.", true);
    return;
  }

  if (m_activeStructure == NerveStructure) {
    // A few tube segments: fast enough to build right here
    m_requestedSetting[NerveStructure] = m_nerveDiameter;
    vtkSmartPointer<vtkPolyData> tube =
        buildNerveTube(m_nervePaths, m_nerveDiameter);
    structureMeshReady(NerveStructure, tube,
                       tube ? QString()
                            : QString("Click at least two points along the "
                                      "canal first."));
    return;
  }
  if (m_activeStructure == ToothStructure && !m_hasToothSeed) {
    setHint("Click the tooth on a slice first.", true);
    return;
  }

  double lower, upper;
  bool hasUpper;
  structureRange(m_activeStructure, lower, hasUpper, upper);
  m_requestedSetting[m_activeStructure] = lower;

  // The worker gets its own data object sharing the (read-only) voxels
  vtkSmartPointer<vtkImageData> volume = vtkSmartPointer<vtkImageData>::New();
  volume->ShallowCopy(m_volume);

  setBusy(true);
  m_progress->setRange(0, 100);
  m_progress->setValue(0);
  m_progress->show();
  setStatus(QString("Creating %1 mesh at %2")
                .arg(m_structures[m_activeStructure].name.toLower(),
                     thresholdText(lower)));
  if (m_activeStructure == ToothStructure)
    emit doBuildSeededMesh(ToothStructure, volume, lower, m_toothSeed[0],
                           m_toothSeed[1], m_toothSeed[2]);
  else
    emit doBuildStructureMesh(m_activeStructure, volume, lower, hasUpper,
                              hasUpper ? upper : 0.0);
}

void MainWindow::structureMeshReady(int structureId,
                                    vtkSmartPointer<vtkPolyData> mesh,
                                    const QString &error) {
  setBusy(false);
  m_progress->hide();
  Structure &s = m_structures[structureId];
  if (!mesh) {
    QString text = QString("%1: %2").arg(s.name, error);
    setHint(text, true);
    setStatus(QString("%1 mesh not created").arg(s.name));
    return;
  }

  if (s.actor)
    mRenderer->RemoveActor(s.actor);
  s.mesh = mesh;
  s.meshSetting = m_requestedSetting[structureId];

  // Display with smooth normals; the exported mesh is left as built
  vtkNew<vtkPolyDataNormals> normals;
  normals->SetInputData(mesh);
  normals->SplittingOff();
  vtkNew<vtkPolyDataMapper> mapper;
  mapper->SetInputConnection(normals->GetOutputPort());
  mapper->ScalarVisibilityOff();
  s.actor = vtkSmartPointer<vtkActor>::New();
  s.actor->SetMapper(mapper);
  s.actor->GetProperty()->SetColor(s.color);
  mRenderer->AddActor(s.actor);
  setUnexported(true);

  refreshMeshList();
  show3DView();
  if (m_resetMeshCamera) {
    resetView();
    m_resetMeshCamera = false;
  } else {
    mRenderer->ResetCameraClippingRange();
    mRenderWindow->Render();
  }
  setHint(QString("%1 mesh created. Choose another structure, or Export STL.")
              .arg(s.name));
  setStatus(QString("%1 mesh: %2 triangles")
                .arg(s.name)
                .arg(mesh->GetNumberOfCells()));
}

// ================================================================== meshes

bool MainWindow::hasMeshes() const {
  for (const Structure &s : m_structures)
    if (s.mesh)
      return true;
  return false;
}

void MainWindow::refreshMeshList() {
  // Not QSignalBlocker: the view must see the rows change, only our
  // itemChanged handler should ignore the rebuild.
  m_refreshingList = true;
  m_meshModel->clear();
  for (int id = 0; id < StructureCount; ++id) {
    const Structure &s = m_structures[id];
    if (!s.mesh)
      continue;
    auto *item = new QStandardItem(swatch(s.color),
                                   QString("%1  ·  %2")
                                       .arg(s.name,
                                            settingText(id, s.meshSetting)));
    item->setData(id, Qt::UserRole);
    item->setCheckable(true);
    item->setCheckState(s.actor && s.actor->GetVisibility() ? Qt::Checked
                                                            : Qt::Unchecked);
    item->setToolTip(QString("%1 mesh: %2, %3 triangles\nCheckbox: show in 3D "
                             "· Right-click or Delete: remove")
                         .arg(s.name, settingText(id, s.meshSetting))
                         .arg(s.mesh->GetNumberOfCells()));
    m_meshModel->appendRow(item);
  }
  if (m_meshModel->rowCount() == 0) {
    auto *empty = new QStandardItem(
        m_volume ? "No meshes yet" : "Open a scan to create meshes");
    empty->setEnabled(false);
    empty->setSelectable(false);
    m_meshModel->appendRow(empty);
  }
  m_refreshingList = false;
  updateActions();
}

void MainWindow::meshItemChanged(QStandardItem *item) {
  if (m_refreshingList)
    return;
  QVariant id = item->data(Qt::UserRole);
  if (!id.isValid())
    return;
  Structure &s = m_structures[id.toInt()];
  if (!s.actor)
    return;
  s.actor->SetVisibility(item->checkState() == Qt::Checked);
  if (m_showing3D)
    mRenderWindow->Render();
}

void MainWindow::meshContextMenu(const QPoint &pos) {
  QModelIndex idx = ui->meshList->indexAt(pos);
  if (!idx.isValid() || !idx.data(Qt::UserRole).isValid())
    return;
  int id = idx.data(Qt::UserRole).toInt();
  QMenu menu(this);
  QStandardItem *item = m_meshModel->itemFromIndex(idx);
  bool visible = item->checkState() == Qt::Checked;
  menu.addAction(visible ? "Hide" : "Show", this, [item, visible] {
    item->setCheckState(visible ? Qt::Unchecked : Qt::Checked);
  });
  menu.addAction("Show Only This", this, [this, id] {
    for (int r = 0; r < m_meshModel->rowCount(); ++r) {
      QStandardItem *it = m_meshModel->item(r);
      if (it->data(Qt::UserRole).isValid())
        it->setCheckState(it->data(Qt::UserRole).toInt() == id ? Qt::Checked
                                                               : Qt::Unchecked);
    }
    show3DView();
  });
  menu.addSeparator();
  menu.addAction("Delete Mesh", this, [this, id] { deleteMesh(id); });
  menu.exec(ui->meshList->viewport()->mapToGlobal(pos));
}

void MainWindow::deleteMesh(int structureId) {
  Structure &s = m_structures[structureId];
  if (!s.mesh)
    return;
  if (QMessageBox::question(this, "Delete mesh",
                            QString("Delete the %1 mesh? You can create it "
                                    "again from the scan.")
                                .arg(s.name.toLower()),
                            QMessageBox::Yes | QMessageBox::Cancel,
                            QMessageBox::Cancel) != QMessageBox::Yes)
    return;
  if (s.actor)
    mRenderer->RemoveActor(s.actor);
  s.mesh = nullptr;
  s.actor = nullptr;
  if (!hasMeshes())
    setUnexported(false);
  refreshMeshList();
  if (m_showing3D)
    mRenderWindow->Render();
  setStatus(QString("%1 mesh deleted").arg(s.name));
}

void MainWindow::setUnexported(bool unexported) {
  m_unexported = unexported;
  setWindowModified(unexported);
}

bool MainWindow::confirmDiscardMeshes(const QString &action) {
  if (!m_unexported)
    return true;
  QMessageBox box(QMessageBox::Warning, "Unexported meshes",
                  QString("The meshes have not been exported. Export them "
                          "before you %1?")
                      .arg(action),
                  QMessageBox::NoButton, this);
  QPushButton *exportButton =
      box.addButton("Export…", QMessageBox::AcceptRole);
  QPushButton *discard = box.addButton("Discard", QMessageBox::DestructiveRole);
  box.addButton(QMessageBox::Cancel);
  box.setDefaultButton(exportButton);
  box.exec();
  if (box.clickedButton() == exportButton)
    return exportDialog();
  return box.clickedButton() == discard;
}

// ================================================================== export

bool MainWindow::exportDialog() {
  if (!hasMeshes()) {
    setHint("Nothing to export yet: create a mesh first.", true);
    return false;
  }
  QString start = m_lastDir.isEmpty() ? QDir::homePath() : m_lastDir;
  QString dir = QFileDialog::getExistingDirectory(
      this, "Choose a Folder for the STL Files", start);
  if (dir.isEmpty())
    return false;
  return !exportStructures(dir).isEmpty();
}

QString MainWindow::exportStructures(const QString &dir) {
  QString base = m_patientName;
  base.replace(QRegularExpression("[^A-Za-z0-9_-]+"), "_");
  base.remove(QRegularExpression("^_+|_+$"));
  if (base.isEmpty())
    base = "scan";
  QString folder = QDir(dir).filePath(
      base + "_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
  if (!QDir().mkpath(folder)) {
    QMessageBox::warning(this, "Export failed",
                         QString("Cannot create the folder\n%1").arg(folder));
    return QString();
  }

  QApplication::setOverrideCursor(Qt::WaitCursor);
  VTKMeshData writer;
  QStringList written, failed;
  for (const Structure &s : m_structures) {
    if (!s.mesh)
      continue;
    QString path = QDir(folder).filePath(s.fileName);
    writer.exportAsStlFile(s.mesh, path.toStdString(), true);
    if (QFileInfo(path).size() > 0)
      written << s.fileName;
    else
      failed << s.fileName;
  }
  QApplication::restoreOverrideCursor();

  if (!failed.isEmpty()) {
    QMessageBox::warning(this, "Export failed",
                         QString("Could not write %1 to\n%2")
                             .arg(failed.join(", "), folder));
    return QString();
  }
  setUnexported(false);
  setStatus(QString("Exported %1 to %2").arg(written.join(", "), folder));
  return folder;
}

// ==================================================================== misc

void MainWindow::updateActions() {
  const bool loaded = static_cast<bool>(m_volume);
  const bool idle = !m_busy && !m_loading;
  const bool nerve = m_activeStructure == NerveStructure;
  bool nervePoints = false;
  for (const NervePath &p : m_nervePaths)
    nervePoints = nervePoints || !p.empty();

  m_openAction->setEnabled(idle);
  m_recentMenu->setEnabled(idle && !m_recent.isEmpty());
  m_exportAction->setEnabled(idle && hasMeshes());
  m_undoPointAction->setEnabled(idle && nerve && nervePoints);
  m_newCanalAction->setEnabled(idle && nerve && nervePoints);
  m_clearTraceAction->setEnabled(idle && nerve && nervePoints);
  m_sliceViewAction->setEnabled(loaded);
  m_view3DAction->setEnabled(loaded);
  m_resetViewAction->setEnabled(loaded);
  m_createMeshAction->setEnabled(idle && loaded && m_activeStructure >= 0);
  for (QAction *a : m_structureActions)
    a->setEnabled(idle && loaded);

  m_sliceViewAction->setChecked(loaded && !m_showing3D);
  m_view3DAction->setChecked(loaded && m_showing3D);
  ui->sliceViewButton->setEnabled(loaded);
  ui->view3DButton->setEnabled(loaded);
  ui->sliceViewButton->setChecked(loaded && !m_showing3D);
  ui->view3DButton->setChecked(loaded && m_showing3D);

  for (QAbstractButton *b : m_structureButtons->buttons())
    b->setEnabled(idle && loaded);
  ui->settingSlider->setEnabled(idle && loaded && m_activeStructure >= 0);
  ui->settingSpin->setEnabled(idle && loaded && m_activeStructure >= 0);
  ui->createMeshButton->setEnabled(idle && loaded && m_activeStructure >= 0);
  ui->exportButton->setEnabled(idle && hasMeshes());
  ui->emptyOpenButton->setEnabled(idle);
}

void MainWindow::setBusy(bool busy) {
  m_busy = busy;
  if (busy)
    QApplication::setOverrideCursor(Qt::BusyCursor);
  else
    while (QApplication::overrideCursor())
      QApplication::restoreOverrideCursor();
  updateActions();
}

void MainWindow::setStatus(const QString &text) {
  statusBar()->showMessage(text);
  qDebug() << "status:" << text;
}

void MainWindow::setHint(const QString &text, bool error) {
  ui->hintLabel->setText(text);
  ui->hintLabel->setProperty("state", error ? "error" : "");
  repolish(ui->hintLabel);
  ui->hintLabel->setAccessibleDescription(text);
}

void MainWindow::converterProgress(double progress) {
  emit doUpdateProgress(progress);
}

void MainWindow::updateProgress(double progress) {
  if (m_progress->maximum() > 0)
    m_progress->setValue(static_cast<int>(progress));
}

void MainWindow::showShortcuts() {
  QMessageBox::information(
      this, "Keyboard Shortcuts",
      "<table cellspacing='6'>"
      "<tr><td><b>Ctrl+O</b></td><td>Open DICOM folder</td></tr>"
      "<tr><td><b>Ctrl+E</b></td><td>Export STL</td></tr>"
      "<tr><td><b>Ctrl+1 … 4</b></td><td>Bone · All teeth · Single tooth · "
      "Nerve</td></tr>"
      "<tr><td><b>Ctrl+Enter</b></td><td>Create mesh</td></tr>"
      "<tr><td><b>F2 / F3</b></td><td>Slice view / 3D view</td></tr>"
      "<tr><td><b>Ctrl+0</b></td><td>Reset view</td></tr>"
      "<tr><td><b>Up / Down, wheel</b></td><td>Previous / next slice</td></tr>"
      "<tr><td><b>Ctrl+Z, Backspace</b></td><td>Undo nerve point</td></tr>"
      "<tr><td><b>Enter / Ctrl+N</b></td><td>Start the other nerve "
      "canal</td></tr>"
      "<tr><td><b>Esc</b></td><td>Clear nerve trace (in the view)</td></tr>"
      "<tr><td><b>Delete</b></td><td>Delete selected mesh</td></tr>"
      "<tr><td><b>F11</b></td><td>Full screen</td></tr>"
      "</table>");
}

void MainWindow::showAbout() {
  QMessageBox::about(
      this, "About DICOM Segmentation",
      "<b>DICOM Segmentation</b><br>Segment bone, teeth, a single tooth and "
      "the nerve canal from CT/CBCT and export them as STL.<br><br>Built "
      "with Qt, VTK and DCMTK. Mesh pipeline based on dicom2mesh (MIT). "
      "<br><br>Not cleared for clinical use.");
}

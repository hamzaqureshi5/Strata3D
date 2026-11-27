#include "ui/main_window.h"

#include "ui/log.h"
#include "ui/mesh_view.h"
#include "ui/mesh_worker.h"
#include "ui_main_window.h"

#include "core/stl_export.h"

#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QDateTime>
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
#include <QScreen>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QtConcurrent/QtConcurrentRun>

#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkObjectFactory.h>
#include <vtkVersion.h>

#include <cmath>

using namespace strata3d;

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

const char *kNeedsAi = "Requires the AI segmentation (not integrated yet)";
// The diameter slider works in tenths of a millimetre
constexpr int kDiameterSliderScale = 10;

QIcon swatch(const Rgb &rgb) {
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

QString structureName(int id) { return structureInfo(id).name; }

void repolish(QWidget *w) {
  w->style()->unpolish(w);
  w->style()->polish(w);
}

} // namespace

// =================================================================== setup

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  useGenericRenderWindows();
  ui->setupUi(this);
  setAcceptDrops(true);

  m_sliceView = new SliceView(this);
  m_meshView = new MeshView(this);
  connect(m_sliceView, &SliceView::sliceChanged, this,
          &MainWindow::sliceChanged);
  connect(m_sliceView, &SliceView::voxelClicked, this,
          &MainWindow::voxelClicked);
  connect(m_sliceView, &SliceView::clickedOutsideImage, this,
          [this] { setHint("Click inside the image.", true); });
  connect(m_sliceView, &SliceView::traceKeyPressed, this,
          &MainWindow::traceKeyPressed);
  connect(m_sliceView, &SliceView::cursorMoved, this,
          &MainWindow::cursorMoved);
  connect(m_sliceView, &SliceView::cursorLeft, this,
          [this] { m_cursorLabel->clear(); });
  ui->openGLWidget->setRenderWindow(m_meshView->renderWindow());

  // Meshes are built on a worker thread
  MeshWorker::registerMetaTypes();
  m_meshWorker = new MeshWorker;
  m_meshWorker->moveToThread(&m_meshThread);
  connect(&m_meshThread, &QThread::finished, m_meshWorker,
          &QObject::deleteLater);
  connect(this, &MainWindow::buildMeshRequested, m_meshWorker,
          &MeshWorker::build);
  connect(m_meshWorker, &MeshWorker::finished, this, &MainWindow::meshReady);
  connect(m_meshWorker, &MeshWorker::stageChanged, this,
          &MainWindow::setStatus);
  connect(m_meshWorker, &MeshWorker::progressChanged, this,
          [this](double percent) {
            if (m_progress->maximum() > 0)
              m_progress->setValue(static_cast<int>(percent));
          });
  m_meshThread.start();

  connect(&m_loadWatcher, &QFutureWatcher<DicomSeries>::finished, this,
          &MainWindow::loadFinished);

  createActions();
  createStatusBar();
  setupSidebar();
  restoreWindow();

  ui->viewStack->setCurrentWidget(ui->emptyPage);
  configureSetting();
  setBusy(false);
  setHint("Open a DICOM series to begin.");
  setStatus("Ready");
}

MainWindow::~MainWindow() {
  m_loadWatcher.waitForFinished();
  m_meshThread.quit();
  m_meshThread.wait();
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
  QAction *quit = file->addAction("&Quit", this, &QWidget::close);
  quit->setShortcut(QKeySequence::Quit);
  quit->setMenuRole(QAction::QuitRole);

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
  m_sliceViewAction =
      view->addAction("&Slices", this, &MainWindow::showSliceView);
  m_sliceViewAction->setShortcut(QKeySequence(Qt::Key_F2));
  m_view3DAction = view->addAction("&3D", this, &MainWindow::show3DView);
  m_view3DAction->setShortcut(QKeySequence(Qt::Key_F3));
  for (QAction *a : {m_sliceViewAction, m_view3DAction}) {
    a->setCheckable(true);
    viewGroup->addAction(a);
  }
  m_resetViewAction =
      view->addAction("&Reset View", this, &MainWindow::resetView);
  m_resetViewAction->setShortcut(QKeySequence("Ctrl+0"));
  view->addSeparator();
  QMenu *themeMenu = view->addMenu("&Theme");
  m_themeGroup = new QActionGroup(this);
  for (auto [label, mode] : {std::pair{"&Dark", Theme::Mode::Dark},
                             std::pair{"&Light", Theme::Mode::Light},
                             std::pair{"Follow &System", Theme::Mode::System}}) {
    QAction *a = themeMenu->addAction(label);
    a->setCheckable(true);
    a->setData(Theme::modeToString(mode));
    m_themeGroup->addAction(a);
    connect(a, &QAction::triggered, this, [this, mode] { setTheme(mode); });
  }
  QAction *fullScreen = view->addAction("&Full Screen");
  fullScreen->setCheckable(true);
  fullScreen->setShortcut(QKeySequence::FullScreen);
  connect(fullScreen, &QAction::toggled, this,
          [this](bool on) { on ? showFullScreen() : showNormal(); });

  // Segment
  QMenu *segment = menuBar()->addMenu("&Segment");
  for (int id = 0; id < StructureCount; ++id) {
    QAction *a = segment->addAction(swatch(structureInfo(id).meshColor),
                                    structureName(id), this,
                                    [this, id] { selectStructure(id); });
    a->setShortcut(QKeySequence(QString("Ctrl+%1").arg(id + 1)));
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
  QAction *keys = help->addAction("&Keyboard Shortcuts", this,
                                  &MainWindow::showShortcuts);
  keys->setShortcut(QKeySequence::HelpContents);
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
    buttons[id]->setIcon(swatch(structureInfo(id).meshColor));
    buttons[id]->setToolTip(tips[id]);
    buttons[id]->setAccessibleName(structureName(id));
    m_structureButtons->addButton(buttons[id], id);
  }
  connect(m_structureButtons, &QButtonGroup::idClicked, this,
          &MainWindow::selectStructure);
  ui->aiButton->setToolTip(kNeedsAi);

  // Threshold / diameter: slider and spin box stay in sync
  ui->settingSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
  connect(ui->settingSlider, &QSlider::valueChanged, this,
          &MainWindow::sliderChanged);
  connect(ui->settingSpin, &QDoubleSpinBox::valueChanged, this,
          &MainWindow::spinChanged);

  connect(ui->createMeshButton, &QPushButton::clicked, this,
          &MainWindow::createMesh);
  ui->createMeshButton->setToolTip(
      "Build the mesh of the selected structure (Ctrl+Enter)");
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
    QVariant id = ui->meshList->currentIndex().data(Qt::UserRole);
    if (id.isValid())
      deleteMesh(id.toInt());
  });
  ui->meshList->addAction(deleteAction);
  refreshMeshList();

  // Viewer header and start screen
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
  connect(ui->sliceSlider, &QSlider::valueChanged, this, [this](int slice) {
    m_sliceView->setSlice(slice);
    m_sliceView->render();
  });
  ui->openGLWidget->setToolTip(
      "Slices: mouse wheel or Up/Down to scroll, left drag for contrast, "
      "right drag to zoom, middle drag to pan.\n3D: left drag to rotate, "
      "right drag or wheel to zoom.");

  ui->mainSplitter->setHandleWidth(1);
  ui->mainSplitter->setStretchFactor(0, 0);
  ui->mainSplitter->setStretchFactor(1, 1);
  ui->mainSplitter->setSizes({300, 1100});
}

// ---------------------------------------------------------- persistence

void MainWindow::restoreWindow() {
  setTheme(m_settings.theme());
  if (!restoreGeometry(m_settings.windowGeometry())) {
    // first start: 80 % of the screen, centred
    QRect avail = screen()->availableGeometry();
    resize(std::max(minimumWidth(), avail.width() * 4 / 5),
           std::max(minimumHeight(), avail.height() * 4 / 5));
    move(avail.center() - rect().center());
  }
  restoreState(m_settings.windowState());
  ui->mainSplitter->restoreState(m_settings.splitterState());
  updateRecentUi(m_settings.recentScans());
}

void MainWindow::saveWindow() const {
  AppSettings settings;
  settings.saveWindow(saveGeometry(), saveState(),
                      ui->mainSplitter->saveState());
}

void MainWindow::setTheme(Theme::Mode mode) {
  m_themeMode = mode;
  const Theme::Tokens &tokens = Theme::apply(*qApp, mode);
  m_meshView->setBackground(tokens.viewport3D);
  for (QAction *a : m_themeGroup->actions())
    a->setChecked(a->data().toString() == Theme::modeToString(mode));
  if (m_showing3D)
    m_meshView->render();
  m_settings.setTheme(mode);
}

void MainWindow::updateRecentUi(const QStringList &recent) {
  m_recentMenu->clear();
  QStringList links;
  for (const QString &folder : recent) {
    const QString name = QDir(folder).dirName();
    QAction *a = m_recentMenu->addAction(
        name, this, [this, folder] { loadDicomFolder(folder); });
    a->setToolTip(folder);
    a->setStatusTip(folder);
    if (links.size() < 4)
      links << QString("<a href=\"%1\">%2</a>")
                   .arg(folder.toHtmlEscaped(), name.toHtmlEscaped());
  }
  m_recentMenu->setEnabled(!recent.isEmpty());
  if (!recent.isEmpty()) {
    m_recentMenu->addSeparator();
    m_recentMenu->addAction("Clear Recent", this, [this] {
      m_settings.clearRecentScans();
      updateRecentUi({});
    });
  }
  ui->emptyRecent->setText(links.isEmpty()
                               ? QString()
                               : "Recent: " + links.join(" &nbsp;·&nbsp; "));
}

// ----------------------------------------------------------- window events

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
  saveWindow();
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

// ================================================================= loading

void MainWindow::openDicomDialog() {
  QString start = m_settings.lastFolder();
  if (start.isEmpty())
    start = QDir::homePath();
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

  qCInfo(lcLoad) << "reading" << folder;
  m_loading = true;
  setBusy(true);
  m_progress->setRange(0, 0); // indeterminate
  m_progress->show();
  setStatus(QString("Reading %1…").arg(QDir(folder).dirName()));
  const std::string path = folder.toStdString();
  m_loadWatcher.setFuture(
      QtConcurrent::run([path] { return readDicomSeries(path); }));
  return true;
}

void MainWindow::loadFinished() {
  const DicomSeries series = m_loadWatcher.result();
  const QString folder = QString::fromStdString(series.folder);
  m_loading = false;
  m_progress->hide();
  m_progress->setRange(0, 100);
  if (!series.volume) {
    qCWarning(lcLoad) << "failed:" << folder << series.error.c_str();
    setBusy(false);
    setStatus("Could not open " + QDir(folder).dirName());
    QMessageBox::warning(
        this, "Cannot open DICOM series",
        QString("%1\n\n%2").arg(QString::fromStdString(series.error), folder));
    return;
  }
  int *dims = series.volume->GetDimensions();
  qCInfo(lcLoad) << "loaded" << folder << dims[0] << "x" << dims[1] << "x"
                 << dims[2];

  m_dicomFolder = folder;
  m_settings.setLastFolder(QFileInfo(folder).absolutePath());
  updateRecentUi(m_settings.addRecentScan(folder));
  m_volume = series.volume;
  m_patientName = QString::fromStdString(series.patientName);

  const ThresholdPresets presets = estimateThresholdPresets(m_volume);
  m_hounsfield = presets.hounsfield;
  m_session.reset(presets);
  setUnexported(false);
  m_meshView->clear();
  m_activeStructure = -1;
  m_resetMeshCamera = true;
  m_structureButtons->setExclusive(false);
  for (QAbstractButton *b : m_structureButtons->buttons())
    b->setChecked(false);
  m_structureButtons->setExclusive(true);

  showStudy(series);
  ui->viewStack->setCurrentWidget(ui->viewerPage);
  m_sliceView->setVolume(m_volume,
                         defaultDisplayWindow(m_volume, m_hounsfield));
  {
    QSignalBlocker block(ui->sliceSlider);
    ui->sliceSlider->setRange(0, m_sliceView->sliceCount() - 1);
  }
  configureSetting();
  updateOverlay();
  updateMarkers();
  refreshMeshList();
  setBusy(false);

  showSliceView();
  resetView();
  setHint(structureHint(-1));
  setStatus(QString("Opened %1").arg(QDir(folder).dirName()));
}

void MainWindow::showStudy(const DicomSeries &series) {
  const QString folder = QString::fromStdString(series.folder);
  int *dims = m_volume->GetDimensions();
  double *sp = m_volume->GetSpacing();
  ui->patientLabel->setText(m_patientName.isEmpty() ? QDir(folder).dirName()
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
  ui->studyDetailsLabel->setToolTip(folder);
  setWindowTitle(
      QString("%1[*] — Strata3D").arg(ui->patientLabel->text()));
}

// ================================================================== views

void MainWindow::sliceChanged(int slice, int sliceCount) {
  ui->sliceLabel->setText(QString("Slice %1 / %2").arg(slice + 1).arg(sliceCount));
  if (ui->sliceSlider->value() != slice) {
    QSignalBlocker block(ui->sliceSlider);
    ui->sliceSlider->setValue(slice);
  }
}

void MainWindow::cursorMoved(int i, int j, int k, double value) {
  m_cursorLabel->setText(QString("x %1  y %2  slice %3   %4")
                             .arg(i)
                             .arg(j)
                             .arg(k + 1)
                             .arg(thresholdText(std::round(value))));
}

void MainWindow::showSliceView() {
  if (!m_volume) {
    setStatus("Open a DICOM series first");
    updateActions();
    return;
  }
  ui->openGLWidget->setRenderWindow(m_sliceView->renderWindow());
  m_showing3D = false;
  ui->sliceSlider->show();
  ui->sliceLabel->show();
  updateActions();
  m_sliceView->render();
}

void MainWindow::show3DView() {
  if (!m_volume) {
    setStatus("Open a DICOM series first");
    updateActions();
    return;
  }
  ui->openGLWidget->setRenderWindow(m_meshView->renderWindow());
  m_showing3D = true;
  ui->sliceSlider->hide();
  ui->sliceLabel->hide();
  m_cursorLabel->clear();
  if (!m_session.hasMeshes())
    setStatus("No meshes yet: choose a structure and Create mesh");
  updateActions();
  m_meshView->render();
}

void MainWindow::resetView() {
  if (!m_volume)
    return;
  if (m_showing3D) {
    m_meshView->resetCamera();
    m_meshView->render();
  } else {
    m_sliceView->resetCamera();
    m_sliceView->render();
  }
}

// =========================================================== segmentation

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
    if (m_session.toothSeed())
      return QString("Tooth selected on slice %1. Create mesh, or click "
                     "another tooth.")
          .arg((*m_session.toothSeed())[2] + 1);
    return "Adjust the threshold until the green overlay covers the teeth, "
           "then click one tooth on a slice.";
  case NerveStructure:
    return nerveHelp();
  default:
    return "Choose a structure to segment.";
  }
}

QString MainWindow::nerveHelp() const {
  return QString("%1 point(s), %2 canal(s) ready.\nClick along the canal on "
                 "the slices. Backspace: undo · Enter: start the other "
                 "canal · Esc: clear. Then Create mesh.")
      .arg(m_session.nerve().pointCount())
      .arg(m_session.nerve().canalCount());
}

void MainWindow::configureSetting() {
  QSignalBlocker b1(ui->settingSlider), b2(ui->settingSpin);
  int lo, hi;
  if (m_volume) {
    double range[2];
    m_volume->GetScalarRange(range);
    lo = static_cast<int>(std::floor(range[0]));
    hi = static_cast<int>(std::ceil(range[1]));
  } else {
    lo = hi = 0;
  }

  if (m_activeStructure == NerveStructure) {
    ui->settingSection->setText("DIAMETER");
    ui->settingSlider->setRange(
        static_cast<int>(kNerveMinDiameter * kDiameterSliderScale),
        static_cast<int>(kNerveMaxDiameter * kDiameterSliderScale));
    ui->settingSlider->setValue(static_cast<int>(
        std::lround(m_session.nerveDiameter() * kDiameterSliderScale)));
    ui->settingSpin->setDecimals(1);
    ui->settingSpin->setSingleStep(0.1);
    ui->settingSpin->setRange(kNerveMinDiameter, kNerveMaxDiameter);
    ui->settingSpin->setSuffix(" mm");
    ui->settingSpin->setSpecialValueText("");
    ui->settingSpin->setValue(m_session.nerveDiameter());
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
    ui->settingSlider->setRange(lo, hi);
    ui->settingSpin->setDecimals(0);
    ui->settingSpin->setSingleStep(10);
    ui->settingSpin->setRange(lo, hi);
    ui->settingSpin->setSuffix(m_hounsfield ? " HU" : "");
    ui->settingSpin->setSpecialValueText("");
    const double v = m_session.lower(m_activeStructure);
    ui->settingSlider->setValue(static_cast<int>(v));
    ui->settingSpin->setValue(v);
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
  if (m_showing3D)
    showSliceView();
  updateOverlay();
  updateMarkers();
  m_sliceView->render();
  setHint(structureHint(structureId));
  updateActions();
  // clicks and keys for Single tooth / Nerve go to the view
  if (structureId == ToothStructure || structureId == NerveStructure)
    ui->openGLWidget->setFocus();
}

void MainWindow::sliderChanged(int value) {
  if (m_activeStructure < 0)
    return;
  const double v = m_activeStructure == NerveStructure
                       ? double(value) / kDiameterSliderScale
                       : value;
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
    ui->settingSlider->setValue(static_cast<int>(
        std::lround(m_activeStructure == NerveStructure
                        ? value * kDiameterSliderScale
                        : value)));
  }
  if (m_activeStructure == NerveStructure) {
    m_session.setNerveDiameter(value);
    return;
  }
  m_session.setLower(m_activeStructure, value);
  updateOverlay();
  if (!m_showing3D)
    m_sliceView->render();
}

void MainWindow::updateOverlay() {
  if (m_activeStructure < 0 || m_activeStructure == NerveStructure)
    m_sliceView->hideOverlay();
  else
    m_sliceView->showOverlay(m_session.range(m_activeStructure),
                             structureInfo(m_activeStructure).overlayColor);
}

// Markers belong to the active tool: the tooth seed while Single tooth is
// selected, the trace while Nerve is.
void MainWindow::updateMarkers() {
  m_sliceView->setToothMarker(m_activeStructure == ToothStructure
                                  ? m_session.toothSeed()
                                  : std::nullopt);
  m_sliceView->setNerveTrace(m_activeStructure == NerveStructure
                                 ? m_session.nerve().paths()
                                 : std::vector<NervePath>{});
  m_sliceView->setPickingEnabled(!m_busy &&
                                 (m_activeStructure == ToothStructure ||
                                  m_activeStructure == NerveStructure));
}

void MainWindow::voxelClicked(int i, int j, int k, const Point3 &position) {
  if (m_activeStructure == ToothStructure) {
    m_session.setToothSeed({i, j, k});
    updateMarkers();
    m_sliceView->render();
    setHint(structureHint(ToothStructure));
    updateActions();
  } else if (m_activeStructure == NerveStructure) {
    m_session.nerve().addPoint(position);
    nerveTraceChanged();
  }
}

void MainWindow::traceKeyPressed(SliceView::TraceKey key) {
  switch (key) {
  case SliceView::TraceKey::Undo:
    undoNervePoint();
    break;
  case SliceView::TraceKey::NewPath:
    newNerveCanal();
    break;
  case SliceView::TraceKey::Clear:
    clearNerveTrace();
    break;
  }
}

void MainWindow::undoNervePoint() {
  m_session.nerve().undo();
  nerveTraceChanged();
}

void MainWindow::newNerveCanal() {
  m_session.nerve().startNewPath();
  nerveTraceChanged();
}

void MainWindow::clearNerveTrace() {
  m_session.nerve().clear();
  nerveTraceChanged();
}

void MainWindow::nerveTraceChanged() {
  updateMarkers();
  if (!m_showing3D)
    m_sliceView->render();
  setHint(nerveHelp());
  updateActions();
}

void MainWindow::createMesh() {
  if (!m_volume || m_busy)
    return;
  if (m_activeStructure < 0) {
    setHint("Choose a structure first.", true);
    return;
  }

  std::string reason;
  if (m_activeStructure == NerveStructure) {
    // A few tube segments: fast enough to build right here
    m_requestedSetting[NerveStructure] = m_session.nerveDiameter();
    vtkSmartPointer<vtkPolyData> tube = m_session.buildNerveMesh(reason);
    meshReady(NerveStructure, tube, QString::fromStdString(reason));
    return;
  }

  MeshRequest request;
  if (!m_session.meshRequest(m_activeStructure, request, reason)) {
    setHint(QString::fromStdString(reason), true);
    return;
  }
  m_requestedSetting[m_activeStructure] = request.range.lower;

  // The worker gets its own data object sharing the (read-only) voxels
  vtkSmartPointer<vtkImageData> volume = vtkSmartPointer<vtkImageData>::New();
  volume->ShallowCopy(m_volume);

  setBusy(true);
  m_progress->setRange(0, 100);
  m_progress->setValue(0);
  m_progress->show();
  setStatus(QString("Creating %1 mesh at %2")
                .arg(structureName(m_activeStructure).toLower(),
                     thresholdText(request.range.lower)));
  emit buildMeshRequested(volume, request);
}

void MainWindow::meshReady(int structureId, vtkSmartPointer<vtkPolyData> mesh,
                           const QString &error) {
  setBusy(false);
  m_progress->hide();
  const QString name = structureName(structureId);
  if (!mesh) {
    setHint(QString("%1: %2").arg(name, error), true);
    setStatus(QString("%1 mesh not created").arg(name));
    return;
  }

  m_session.setMesh(structureId, mesh, m_requestedSetting[structureId]);
  m_meshView->setMesh(structureId, mesh, structureInfo(structureId).meshColor);
  setUnexported(true);
  refreshMeshList();

  show3DView();
  if (m_resetMeshCamera) {
    resetView();
    m_resetMeshCamera = false;
  }
  setHint(QString("%1 mesh created. Choose another structure, or Export STL.")
              .arg(name));
  setStatus(QString("%1 mesh: %2 triangles")
                .arg(name)
                .arg(mesh->GetNumberOfCells()));
}

// ================================================================= meshes

void MainWindow::refreshMeshList() {
  // Not QSignalBlocker: the view must see the rows change, only our
  // itemChanged handler should ignore the rebuild.
  m_refreshingList = true;
  m_meshModel->clear();
  for (int id = 0; id < StructureCount; ++id) {
    vtkPolyData *mesh = m_session.mesh(id);
    if (!mesh)
      continue;
    const QString setting = settingText(id, m_session.meshSetting(id));
    auto *item = new QStandardItem(swatch(structureInfo(id).meshColor),
                                   QString("%1  ·  %2")
                                       .arg(structureName(id), setting));
    item->setData(id, Qt::UserRole);
    item->setCheckable(true);
    item->setCheckState(m_meshView->isMeshVisible(id) ? Qt::Checked
                                                      : Qt::Unchecked);
    item->setToolTip(QString("%1 mesh: %2, %3 triangles\nCheckbox: show in "
                             "3D · Right-click or Delete: remove")
                         .arg(structureName(id), setting)
                         .arg(mesh->GetNumberOfCells()));
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
  m_meshView->setMeshVisible(id.toInt(), item->checkState() == Qt::Checked);
  if (m_showing3D)
    m_meshView->render();
}

void MainWindow::meshContextMenu(const QPoint &pos) {
  QModelIndex index = ui->meshList->indexAt(pos);
  if (!index.isValid() || !index.data(Qt::UserRole).isValid())
    return;
  const int id = index.data(Qt::UserRole).toInt();
  QStandardItem *item = m_meshModel->itemFromIndex(index);
  const bool visible = item->checkState() == Qt::Checked;
  QMenu menu(this);
  menu.addAction(visible ? "Hide" : "Show", this, [item, visible] {
    item->setCheckState(visible ? Qt::Unchecked : Qt::Checked);
  });
  menu.addAction("Show Only This", this, [this, id] {
    for (int row = 0; row < m_meshModel->rowCount(); ++row) {
      QStandardItem *it = m_meshModel->item(row);
      if (it->data(Qt::UserRole).isValid())
        it->setCheckState(it->data(Qt::UserRole).toInt() == id
                              ? Qt::Checked
                              : Qt::Unchecked);
    }
    show3DView();
  });
  menu.addSeparator();
  menu.addAction("Delete Mesh", this, [this, id] { deleteMesh(id); });
  menu.exec(ui->meshList->viewport()->mapToGlobal(pos));
}

void MainWindow::deleteMesh(int structureId) {
  if (!m_session.mesh(structureId))
    return;
  const QString name = structureName(structureId);
  if (QMessageBox::question(this, "Delete mesh",
                            QString("Delete the %1 mesh? You can create it "
                                    "again from the scan.")
                                .arg(name.toLower()),
                            QMessageBox::Yes | QMessageBox::Cancel,
                            QMessageBox::Cancel) != QMessageBox::Yes)
    return;
  m_session.removeMesh(structureId);
  m_meshView->removeMesh(structureId);
  setUnexported(m_session.hasUnexportedMeshes());
  refreshMeshList();
  if (m_showing3D)
    m_meshView->render();
  setStatus(QString("%1 mesh deleted").arg(name));
}

void MainWindow::setUnexported(bool unexported) {
  if (!unexported)
    m_session.discardUnexported();
  setWindowModified(unexported);
}

bool MainWindow::confirmDiscardMeshes(const QString &action) {
  if (!m_session.hasUnexportedMeshes())
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

// ================================================================= export

bool MainWindow::exportDialog() {
  if (!m_session.hasMeshes()) {
    setHint("Nothing to export yet: create a mesh first.", true);
    return false;
  }
  QString start = m_settings.lastFolder();
  if (start.isEmpty())
    start = QDir::homePath();
  QString dir = QFileDialog::getExistingDirectory(
      this, "Choose a Folder for the STL Files", start);
  if (dir.isEmpty())
    return false;
  return !exportStructures(dir).isEmpty();
}

QString MainWindow::exportStructures(const QString &dir) {
  const std::string name = exportFolderName(
      m_patientName.toStdString(),
      QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss").toStdString());
  const QString folder = QDir(dir).filePath(QString::fromStdString(name));

  std::vector<ExportItem> items;
  for (int id = 0; id < StructureCount; ++id)
    if (vtkPolyData *mesh = m_session.mesh(id))
      items.push_back({structureInfo(id).fileName, mesh});

  QApplication::setOverrideCursor(Qt::WaitCursor);
  const ExportResult result = exportStl(folder.toStdString(), items);
  QApplication::restoreOverrideCursor();

  auto join = [](const std::vector<std::string> &names) {
    QStringList list;
    for (const std::string &n : names)
      list << QString::fromStdString(n);
    return list.join(", ");
  };
  if (!result.ok()) {
    const QString detail =
        result.error.empty()
            ? QString("Could not write %1 to\n%2").arg(join(result.failed), folder)
            : QString::fromStdString(result.error);
    qCWarning(lcMesh) << "export failed:" << detail;
    QMessageBox::warning(this, "Export failed", detail);
    return QString();
  }
  qCInfo(lcMesh) << "exported" << join(result.written) << "to" << folder;
  m_session.markExported();
  setUnexported(false);
  setStatus(QString("Exported %1 to %2").arg(join(result.written), folder));
  return folder;
}

// =================================================================== misc

void MainWindow::updateActions() {
  const bool loaded = m_volume != nullptr;
  const bool idle = !m_busy && !m_loading;
  const bool nerve = m_activeStructure == NerveStructure;
  const bool points = !m_session.nerve().empty();
  const bool canCreate = idle && loaded && m_activeStructure >= 0;

  m_openAction->setEnabled(idle);
  m_recentMenu->setEnabled(idle && !m_recentMenu->isEmpty());
  m_exportAction->setEnabled(idle && m_session.hasMeshes());
  m_undoPointAction->setEnabled(idle && nerve && points);
  m_newCanalAction->setEnabled(idle && nerve && points);
  m_clearTraceAction->setEnabled(idle && nerve && points);
  m_sliceViewAction->setEnabled(loaded);
  m_view3DAction->setEnabled(loaded);
  m_resetViewAction->setEnabled(loaded);
  m_createMeshAction->setEnabled(canCreate);
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
  ui->settingSlider->setEnabled(canCreate);
  ui->settingSpin->setEnabled(canCreate);
  ui->createMeshButton->setEnabled(canCreate);
  ui->exportButton->setEnabled(idle && m_session.hasMeshes());
  ui->emptyOpenButton->setEnabled(idle);
}

void MainWindow::setBusy(bool busy) {
  m_busy = busy;
  if (busy)
    QApplication::setOverrideCursor(Qt::BusyCursor);
  else
    while (QApplication::overrideCursor())
      QApplication::restoreOverrideCursor();
  updateMarkers();
  updateActions();
}

void MainWindow::setStatus(const QString &text) {
  statusBar()->showMessage(text);
  qCInfo(lcUi).noquote() << "status:" << text;
}

void MainWindow::setHint(const QString &text, bool error) {
  ui->hintLabel->setText(text);
  ui->hintLabel->setProperty("state", error ? "error" : "");
  repolish(ui->hintLabel);
  ui->hintLabel->setAccessibleDescription(text);
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
      this, "About Strata3D",
      "<b>Strata3D</b><br>Segment bone, teeth, a single tooth and the nerve "
      "canal from CT/CBCT and export them as STL.<br><br>Built with Qt and "
      "VTK. Mesh pipeline based on dicom2mesh (MIT).<br><br>Not cleared for "
      "clinical use.");
}

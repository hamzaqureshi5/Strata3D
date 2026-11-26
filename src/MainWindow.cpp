#include "MainWindow.h"
#include "dcm_interactor.h"
#include "meshData.h"

#include <QButtonGroup>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardItemModel>

#include <vtkActor2D.h>
#include <vtkCellArray.h>
#include <vtkCamera.h>
#include <vtkCoordinate.h>
#include <vtkDICOMImageReader.h>
#include <vtkImageFlip.h>
#include <vtkImageMapToColors.h>
#include <vtkImageMapper3D.h>
#include <vtkObjectFactory.h>
#include <vtkPoints.h>
#include <vtkPointData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyDataNormals.h>
#include <vtkProperty.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkTextProperty.h>
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
const char *kNeedsAi = "Requires the AI segmentation (not integrated yet)";
const double kNerveMinDiameter = 1.0, kNerveMaxDiameter = 6.0; // mm

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
      mRenderWindow(vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New()),
      mRenderer(vtkSmartPointer<vtkRenderer>::New()),
      mInteractor(vtkSmartPointer<QVTKInteractor>::New()),
      mInteractorStyle(
          vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New()) {
  useGenericRenderWindows();
  ui->setupUi(this);
  showMaximized();

  qRegisterMetaType<vtkSmartPointer<vtkImageData>>();
  qRegisterMetaType<vtkSmartPointer<vtkPolyData>>();

  // Structures segmented by threshold
  m_structures[BoneStructure].name = "Bone";
  m_structures[BoneStructure].fileName = "bone.stl";
  m_structures[BoneStructure].color[0] = 0.89;
  m_structures[BoneStructure].color[1] = 0.85;
  m_structures[BoneStructure].color[2] = 0.76;
  m_structures[BoneStructure].overlayColor[0] = 1.00;
  m_structures[BoneStructure].overlayColor[1] = 0.55;
  m_structures[BoneStructure].overlayColor[2] = 0.00;
  m_structures[TeethStructure].name = "Teeth";
  m_structures[TeethStructure].fileName = "teeth.stl";
  m_structures[TeethStructure].color[0] = 0.62;
  m_structures[TeethStructure].color[1] = 0.80;
  m_structures[TeethStructure].color[2] = 1.00;
  m_structures[TeethStructure].overlayColor[0] = 0.00;
  m_structures[TeethStructure].overlayColor[1] = 0.85;
  m_structures[TeethStructure].overlayColor[2] = 1.00;
  m_structures[ToothStructure].name = "Tooth";
  m_structures[ToothStructure].fileName = "tooth.stl";
  m_structures[ToothStructure].color[0] = 0.55;
  m_structures[ToothStructure].color[1] = 0.95;
  m_structures[ToothStructure].color[2] = 0.60;
  m_structures[ToothStructure].overlayColor[0] = 0.10;
  m_structures[ToothStructure].overlayColor[1] = 1.00;
  m_structures[ToothStructure].overlayColor[2] = 0.30;
  m_structures[NerveStructure].name = "Nerve";
  m_structures[NerveStructure].fileName = "nerve.stl";
  m_structures[NerveStructure].color[0] = 1.00;
  m_structures[NerveStructure].color[1] = 0.85;
  m_structures[NerveStructure].color[2] = 0.10;
  m_structures[NerveStructure].overlayColor[0] = 1.00;
  m_structures[NerveStructure].overlayColor[1] = 0.90;
  m_structures[NerveStructure].overlayColor[2] = 0.00;

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

  // 3D view
  mRenderWindow->AddRenderer(mRenderer);
  mRenderWindow->SetInteractor(mInteractor);
  ui->openGLWidget->setRenderWindow(mRenderWindow);
  mInteractor->SetInteractorStyle(mInteractorStyle);
  mInteractor->Initialize();
  mRenderer->SetBackground(0, 0, 0);

  // Structure selection
  const QString checkedStyle =
      "QPushButton:checked { background-color: #0a84ff; color: white; }";
  m_structureButtons = new QButtonGroup(this);
  m_structureButtons->setExclusive(true);
  for (auto [button, id] :
       {std::pair<QPushButton *, int>{ui->bone, BoneStructure},
        {ui->teeth, TeethStructure},
        {ui->tooth, ToothStructure},
        {ui->nerve, NerveStructure}}) {
    button->setCheckable(true);
    button->setStyleSheet(button->styleSheet() + checkedStyle);
    m_structureButtons->addButton(button, id);
  }
  connect(m_structureButtons, &QButtonGroup::idClicked, this,
          &MainWindow::selectStructure);

  ui->bone->setToolTip("Bone by threshold");
  ui->teeth->setToolTip("All teeth by threshold (one mesh)");
  ui->tooth->setToolTip("One tooth: set the threshold, then click the tooth");
  ui->nerve->setToolTip("Nerve canal: click points along it on the slices");
  ui->AIsegement->setEnabled(false);
  ui->AIsegement->setToolTip(kNeedsAi);

  m_structureModel = new QStandardItemModel(this);
  ui->listView->setModel(m_structureModel);
  ui->listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
  // the window style sheet makes text white; the list background is white
  ui->listView->setStyleSheet(
      "QListView { background: white; color: #202020; }");
  ui->listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  ui->listView->setTextElideMode(Qt::ElideRight);
  connect(m_structureModel, &QStandardItemModel::itemChanged, this,
          &MainWindow::structureItemChanged);
  refreshStructureList();

  ui->horizontalSlider->setToolTip("Threshold of the selected structure");
  ui->verticalSlider->setToolTip("Slice");
  connect(ui->horizontalSlider, &QSlider::valueChanged, this,
          &MainWindow::thresholdChanged);
  connect(ui->verticalSlider, &QSlider::valueChanged, this,
          &MainWindow::sliceSliderChanged);

  connect(ui->importDICOM, &QPushButton::clicked, this,
          &MainWindow::importDicom);
  connect(ui->createSTL, &QPushButton::clicked, this, &MainWindow::createStl);
  connect(ui->exportSTL, &QPushButton::clicked, this, &MainWindow::exportStl);
  connect(ui->DICOM, &QPushButton::clicked, this, &MainWindow::showSliceView);
  connect(ui->STL, &QPushButton::clicked, this, &MainWindow::show3DView);

  setBusy(false);
  setStatus("Import a DICOM folder to start");
}

MainWindow::~MainWindow() {
  m_conversionThread.quit();
  m_conversionThread.wait();
  delete ui;
}

// ---------------------------------------------------------------- loading

void MainWindow::importDicom() {
  QString folder = QFileDialog::getExistingDirectory(
      this, "Select DICOM Directory", m_dicom_path);
  if (!folder.isEmpty())
    loadDicomFolder(folder);
}

bool MainWindow::loadDicomFolder(const QString &folder) {
  setStatus("Reading DICOM images");
  QApplication::setOverrideCursor(Qt::WaitCursor);

  // vtkDICOMImageReader applies RescaleSlope/Intercept (CT arrives in HU), but
  // it flips the rows and reverses the slice order, which turns the anatomy
  // upside down. Flip both back so i, j, k follow the DICOM column, row and
  // slice-position axes (x -> patient left, y -> posterior, z -> superior for
  // axial scans) and exported meshes are upright.
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

  QApplication::restoreOverrideCursor();

  vtkImageData *output = flipSlices->GetOutput();
  int *dims = output->GetDimensions();
  if (dims[0] < 2 || dims[1] < 2 || dims[2] < 2 ||
      !output->GetPointData()->GetScalars()) {
    setStatus("No DICOM series found in " + folder);
    return false;
  }

  m_dicom_path = folder;
  m_volume = vtkSmartPointer<vtkImageData>::New();
  m_volume->ShallowCopy(output);
  m_patientName = QString(reader->GetPatientName()).trimmed();
  ui->patientname->setText(m_patientName);

  // New scan: drop meshes of the previous one
  for (Structure &s : m_structures) {
    if (s.actor)
      mRenderer->RemoveActor(s.actor);
    s.mesh = nullptr;
    s.actor = nullptr;
  }

  ThresholdPresets presets = estimateThresholdPresets(m_volume);
  m_hounsfield = presets.hounsfield;
  m_structures[BoneStructure].lower = presets.bone;
  m_structures[TeethStructure].lower = presets.teeth;
  m_structures[ToothStructure].lower = presets.teeth;
  m_hasToothSeed = false;
  m_nervePaths.clear();

  double range[2];
  m_volume->GetScalarRange(range);
  m_valueRange[0] = static_cast<int>(std::floor(range[0]));
  m_valueRange[1] = static_cast<int>(std::ceil(range[1]));

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
    QSignalBlocker block(ui->verticalSlider);
    ui->verticalSlider->setRange(sliceMin, sliceMax);
  }
  imageViewer->SetSlice((sliceMin + sliceMax) / 2);

  m_structureButtons->setExclusive(false);
  for (QAbstractButton *b : m_structureButtons->buttons())
    b->setChecked(false);
  m_structureButtons->setExclusive(true);
  m_activeStructure = -1;
  configureSlider();
  updateOverlay();
  updateAnnotations();

  refreshStructureList();
  setBusy(false);
  showSliceView();
  // Radiological view: looking from the feet, anterior up, patient's left on
  // the right of the screen
  vtkCamera *sliceCamera = imageViewer->GetRenderer()->GetActiveCamera();
  sliceCamera->SetFocalPoint(0, 0, 0);
  sliceCamera->SetPosition(0, 0, -1);
  sliceCamera->SetViewUp(0, -1, 0);
  imageViewer->GetRenderer()->ResetCamera();
  imageViewer->Render();

  m_resetMeshCamera = true;

  setStatus(QString("Loaded %1 x %2 x %3 voxels (%4). Select BONE, All "
                    "TEETH, TOOTH or NERVE")
                .arg(dims[0])
                .arg(dims[1])
                .arg(dims[2])
                .arg(m_hounsfield ? "Hounsfield units"
                                  : "uncalibrated gray values"));
  return true;
}

// ------------------------------------------------------------- slice view

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

  // Slice number (bottom left) and usage hint (top left)
  vtkNew<vtkTextProperty> sliceTextProp;
  sliceTextProp->SetFontFamilyToCourier();
  sliceTextProp->SetFontSize(20);
  sliceTextProp->SetVerticalJustificationToBottom();
  sliceTextProp->SetJustificationToLeft();
  m_sliceTextMapper = vtkSmartPointer<vtkTextMapper>::New();
  m_sliceTextMapper->SetTextProperty(sliceTextProp);
  vtkNew<vtkActor2D> sliceTextActor;
  sliceTextActor->SetMapper(m_sliceTextMapper);
  sliceTextActor->SetPosition(15, 10);

  vtkNew<vtkTextProperty> usageTextProp;
  usageTextProp->SetFontFamilyToCourier();
  usageTextProp->SetFontSize(14);
  usageTextProp->SetVerticalJustificationToTop();
  usageTextProp->SetJustificationToLeft();
  vtkNew<vtkTextMapper> usageTextMapper;
  usageTextMapper->SetInput("- Slice with mouse wheel, Up/Down key or the "
                            "right slider\n- Zoom: right mouse button drag\n"
                            "- Contrast: left mouse button drag");
  usageTextMapper->SetTextProperty(usageTextProp);
  vtkNew<vtkActor2D> usageTextActor;
  usageTextActor->SetMapper(usageTextMapper);
  usageTextActor->GetPositionCoordinate()
      ->SetCoordinateSystemToNormalizedDisplay();
  usageTextActor->GetPositionCoordinate()->SetValue(0.05, 0.95);

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

  imageViewer->GetRenderer()->AddViewProp(sliceTextActor);
  imageViewer->GetRenderer()->AddViewProp(usageTextActor);

  // Before every render: keep overlay, slice text and slice slider in sync
  // with the viewer's slice, however it was changed.
  m_sliceRenderCB = vtkSmartPointer<vtkCallbackCommand>::New();
  m_sliceRenderCB->SetCallback(&MainWindow::onSliceRenderStart);
  m_sliceRenderCB->SetClientData(this);
  m_sliceRenderWindow->AddObserver(vtkCommand::StartEvent, m_sliceRenderCB);

  // Clicks and keys for TOOTH / NERVE; runs before the interactor style
  // (priority 1) and swallows the events it handles
  m_sliceInputCB = vtkSmartPointer<vtkCallbackCommand>::New();
  m_sliceInputCB->SetCallback(&MainWindow::onSliceInput);
  m_sliceInputCB->SetClientData(this);
  m_sliceInteractor->AddObserver(vtkCommand::LeftButtonPressEvent,
                                 m_sliceInputCB, 1.0);
  m_sliceInteractor->AddObserver(vtkCommand::KeyPressEvent, m_sliceInputCB,
                                 1.0);
}

void MainWindow::onSliceInput(vtkObject *, unsigned long eventId,
                              void *clientData, void *) {
  auto *self = static_cast<MainWindow *>(clientData);
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
    setStatus("Click inside the image");
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
  if (m_nervePaths.empty())
    m_nervePaths.emplace_back();
  if (key == "BackSpace" || key == "Delete") {
    // undo: last point, or step back into the previous path
    if (m_nervePaths.back().empty() && m_nervePaths.size() > 1)
      m_nervePaths.pop_back();
    if (!m_nervePaths.back().empty())
      m_nervePaths.back().pop_back();
  } else if (key == "Return" || key == "KP_Enter") {
    if (!m_nervePaths.back().empty())
      m_nervePaths.emplace_back();
  } else if (key == "Escape") {
    m_nervePaths.back().clear();
  } else {
    return false;
  }
  updateAnnotations();
  setStatus(nerveHelp());
  imageViewer->Render();
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
  setStatus(QString("Tooth selected at slice %1. CREATE STL to build it "
                    "(click again to choose another tooth)")
                .arg(k + 1));
}

void MainWindow::addNervePoint(const Point3 &position) {
  if (m_nervePaths.empty())
    m_nervePaths.emplace_back();
  m_nervePaths.back().push_back(position);
  updateAnnotations();
  if (!m_showing3D)
    imageViewer->Render();
  setStatus(nerveHelp());
}

QString MainWindow::nerveHelp() const {
  int points = 0, paths = 0;
  for (const NervePath &p : m_nervePaths) {
    points += static_cast<int>(p.size());
    paths += p.size() >= 2 ? 1 : 0;
  }
  return QString("Nerve: %1 point(s), %2 canal(s) ready, diameter %3 mm. "
                 "Click along the canal; Backspace undo, Enter new canal, "
                 "Esc clear, then CREATE STL")
      .arg(points)
      .arg(paths)
      .arg(m_nerveDiameter);
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

void MainWindow::onSliceRenderStart(vtkObject *, unsigned long,
                                    void *clientData, void *) {
  static_cast<MainWindow *>(clientData)->syncSliceUi();
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
  m_sliceTextMapper->SetInput(
      StatusMessage::Format(slice, imageViewer->GetSliceMax()).c_str());
  if (ui->verticalSlider->value() != slice) {
    QSignalBlocker block(ui->verticalSlider);
    ui->verticalSlider->setValue(slice);
  }
}

void MainWindow::sliceSliderChanged(int value) {
  if (!imageViewer || !m_volume)
    return;
  imageViewer->SetSlice(value);
  imageViewer->Render();
}

void MainWindow::showSliceView() {
  if (!m_volume) {
    setStatus("No DICOM data loaded");
    return;
  }
  ui->openGLWidget->setRenderWindow(m_sliceRenderWindow);
  m_showing3D = false;
  imageViewer->Render();
}

// ---------------------------------------------------------- segmentation

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
    return QString("%1 mm").arg(value);
  return thresholdText(value);
}

void MainWindow::configureSlider() {
  QSignalBlocker block(ui->horizontalSlider);
  if (m_activeStructure == NerveStructure) {
    ui->horizontalSlider->setRange(static_cast<int>(kNerveMinDiameter * 10),
                                   static_cast<int>(kNerveMaxDiameter * 10));
    ui->horizontalSlider->setValue(
        static_cast<int>(std::lround(m_nerveDiameter * 10)));
    ui->horizontalSlider->setToolTip("Nerve diameter");
  } else {
    ui->horizontalSlider->setRange(m_valueRange[0], m_valueRange[1]);
    if (m_activeStructure >= 0)
      ui->horizontalSlider->setValue(
          static_cast<int>(m_structures[m_activeStructure].lower));
    ui->horizontalSlider->setToolTip("Threshold of the selected structure");
  }
}

void MainWindow::selectStructure(int structureId) {
  if (!m_volume) {
    setStatus("Import a DICOM folder first");
    m_structureButtons->setExclusive(false);
    m_structureButtons->button(structureId)->setChecked(false);
    m_structureButtons->setExclusive(true);
    return;
  }
  m_activeStructure = structureId;
  configureSlider();
  const double *c = m_structures[structureId].overlayColor;
  m_overlayLut->SetTableValue(1, c[0], c[1], c[2], kOverlayOpacity);
  m_overlayLut->Modified();
  if (m_showing3D)
    showSliceView();
  updateOverlay();
  updateAnnotations();
  imageViewer->Render();
  const Structure &s = m_structures[structureId];
  if (structureId == NerveStructure)
    setStatus(nerveHelp());
  else if (structureId == ToothStructure)
    setStatus(QString("Tooth threshold: %1. Adjust until the green overlay "
                      "covers the tooth, click the tooth, then CREATE STL")
                  .arg(thresholdText(s.lower)));
  else
    setStatus(QString("%1 threshold: %2. Adjust the slider until the overlay "
                      "covers the %3, then CREATE STL")
                  .arg(s.name, thresholdText(s.lower), s.name.toLower()));
}

void MainWindow::thresholdChanged(int value) {
  if (m_activeStructure < 0)
    return;
  if (m_activeStructure == NerveStructure) {
    m_nerveDiameter = value / 10.0;
    setStatus(nerveHelp());
    return;
  }
  m_structures[m_activeStructure].lower = value;
  updateOverlay();
  setStatus(QString("%1 threshold: %2")
                .arg(m_structures[m_activeStructure].name)
                .arg(thresholdText(value)));
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

void MainWindow::createStl() {
  if (!m_volume) {
    setStatus("Import a DICOM folder first");
    return;
  }
  if (m_activeStructure < 0) {
    setStatus("Select BONE, All TEETH, TOOTH or NERVE first");
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
                                      "canal first"));
    return;
  }
  if (m_activeStructure == ToothStructure && !m_hasToothSeed) {
    setStatus("Click the tooth on a slice first");
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
  updateProgress(0);
  setStatus(QString("Creating %1 mesh at %2")
                .arg(m_structures[m_activeStructure].name.toLower())
                .arg(thresholdText(lower)));
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
  Structure &s = m_structures[structureId];
  if (!mesh) {
    setStatus(QString("%1: %2 (%3)")
                  .arg(s.name, error,
                       settingText(structureId,
                                   m_requestedSetting[structureId])));
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

  refreshStructureList();
  updateProgress(100);
  if (m_resetMeshCamera) {
    // Facing the patient (from anterior, -y), slightly from above
    vtkCamera *camera = mRenderer->GetActiveCamera();
    camera->SetFocalPoint(0, 0, 0);
    camera->SetPosition(0, -1, 0);
    camera->SetViewUp(0, 0, 1);
    camera->Elevation(30);
    camera->OrthogonalizeViewUp();
    m_resetMeshCamera = false;
  }
  mRenderer->ResetCamera();
  show3DView();
  setStatus(QString("%1 mesh created (%2 triangles). Select another "
                    "structure or EXPORT STL")
                .arg(s.name)
                .arg(mesh->GetNumberOfCells()));
}

void MainWindow::show3DView() {
  bool any = false;
  for (const Structure &s : m_structures)
    any = any || s.mesh;
  if (!any)
    setStatus("No mesh yet: select a structure and CREATE STL");
  ui->openGLWidget->setRenderWindow(mRenderWindow);
  m_showing3D = true;
  mRenderWindow->Render();
}

// ------------------------------------------------------- structure list

void MainWindow::refreshStructureList() {
  // Not QSignalBlocker: the view must see the rows change, only our
  // itemChanged handler should ignore the rebuild.
  m_refreshingList = true;
  m_structureModel->clear();
  for (int id = 0; id < StructureCount; ++id) {
    const Structure &s = m_structures[id];
    auto *item = new QStandardItem;
    item->setData(id, Qt::UserRole);
    if (s.mesh) {
      item->setText(
          QString("%1  %2").arg(s.name, settingText(id, s.meshSetting)));
      item->setCheckable(true);
      item->setCheckState(s.actor && s.actor->GetVisibility() ? Qt::Checked
                                                              : Qt::Unchecked);
      item->setToolTip(QString("%1 mesh: %2, %3 triangles. Check to show in "
                               "the 3D view")
                           .arg(s.name, settingText(id, s.meshSetting))
                           .arg(s.mesh->GetNumberOfCells()));
    } else {
      item->setText(s.name + "  (not created)");
    }
    m_structureModel->appendRow(item);
  }
  m_refreshingList = false;
}

void MainWindow::structureItemChanged(QStandardItem *item) {
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

// ---------------------------------------------------------------- export

void MainWindow::exportStl() {
  bool any = false;
  for (const Structure &s : m_structures)
    any = any || s.mesh;
  if (!any) {
    setStatus("Nothing to export: CREATE STL first");
    return;
  }
  QString dir = QFileDialog::getExistingDirectory(
      this, "Select Directory to Save STL Files");
  if (!dir.isEmpty())
    exportStructures(dir);
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
    setStatus("Export failed: cannot create " + folder);
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
    setStatus(QString("Export failed for %1 in %2")
                  .arg(failed.join(", "), folder));
    return QString();
  }
  setStatus(QString("Exported %1 to %2").arg(written.join(", "), folder));
  return folder;
}

// ----------------------------------------------------------------- misc

void MainWindow::setBusy(bool busy) {
  m_busy = busy;
  bool loaded = static_cast<bool>(m_volume);
  ui->importDICOM->setEnabled(!busy);
  ui->bone->setEnabled(!busy && loaded);
  ui->teeth->setEnabled(!busy && loaded);
  ui->tooth->setEnabled(!busy && loaded);
  ui->nerve->setEnabled(!busy && loaded);
  ui->horizontalSlider->setEnabled(!busy && loaded);
  ui->createSTL->setEnabled(!busy && loaded);
  ui->exportSTL->setEnabled(!busy);
  if (busy)
    QApplication::setOverrideCursor(Qt::BusyCursor);
  else
    while (QApplication::overrideCursor())
      QApplication::restoreOverrideCursor();
}

void MainWindow::setStatus(const QString &text) {
  ui->infoLable->setText(text);
  qDebug() << "status:" << text;
}

void MainWindow::converterProgress(double progress) {
  emit doUpdateProgress(progress);
}

void MainWindow::updateProgress(double progress) {
  ui->progressBar->setValue(static_cast<int>(progress));
}

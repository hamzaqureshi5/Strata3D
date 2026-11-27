#include "ui/slice_view.h"

#include "ui/slice_interactor_style.h"

#include <QVTKInteractor.h>

#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkCommand.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkImageActor.h>
#include <vtkImageData.h>
#include <vtkImageMapToColors.h>
#include <vtkImageMapper3D.h>
#include <vtkImageThreshold.h>
#include <vtkImageViewer2.h>
#include <vtkLookupTable.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>

#include <cmath>
#include <string>

using strata3d::Point3;

namespace {

constexpr double kOverlayOpacity = 0.45;
// Overlay and markers are lifted towards the camera by this many slice
// thicknesses so they are drawn over the image of the same slice.
constexpr double kOverlayLift = 0.25;
constexpr double kMarkerLift = 0.5;

vtkSmartPointer<vtkActor> markerActor(vtkPolyData *data, double pointSize) {
  vtkNew<vtkPolyDataMapper> mapper;
  mapper->SetInputData(data);
  auto actor = vtkSmartPointer<vtkActor>::New();
  actor->SetMapper(mapper);
  actor->GetProperty()->SetPointSize(pointSize);
  actor->GetProperty()->RenderPointsAsSpheresOn();
  actor->GetProperty()->LightingOff();
  return actor;
}

} // namespace

SliceView::SliceView(QObject *parent) : QObject(parent) {
  m_viewer = vtkSmartPointer<vtkImageViewer2>::New();
  m_window = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
  m_interactor = vtkSmartPointer<QVTKInteractor>::New();
  m_window->SetInteractor(m_interactor);
  m_viewer->SetRenderWindow(m_window);
  m_viewer->SetupInteractor(m_interactor);
  m_style = vtkSmartPointer<SliceInteractorStyle>::New();
  m_style->SetInteractionModeToImageSlicing();
  m_style->SetImageViewer(m_viewer);
  m_interactor->SetInteractorStyle(m_style);
  vtkRenderer *renderer = m_viewer->GetRenderer();

  // Threshold overlay: voxels inside the selected range, tinted. Only the
  // displayed slice is computed (streaming).
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
  renderer->AddViewProp(m_overlayActor);

  m_seedMarker = vtkSmartPointer<vtkPolyData>::New();
  m_seedActor = markerActor(m_seedMarker, 14);
  m_seedActor->GetProperty()->SetColor(1.0, 0.15, 0.15);
  renderer->AddViewProp(m_seedActor);
  m_traceMarker = vtkSmartPointer<vtkPolyData>::New();
  m_traceActor = markerActor(m_traceMarker, 9);
  m_traceActor->GetProperty()->SetLineWidth(2);
  const strata3d::Rgb &traceColor =
      strata3d::structureInfo(strata3d::NerveStructure).overlayColor;
  m_traceActor->GetProperty()->SetColor(traceColor[0], traceColor[1],
                                        traceColor[2]);
  renderer->AddViewProp(m_traceActor);

  // Before every render: overlay, markers and listeners follow the slice,
  // however it was changed (wheel, keys, slider).
  m_renderCallback = vtkSmartPointer<vtkCallbackCommand>::New();
  m_renderCallback->SetCallback(&SliceView::onRenderStart);
  m_renderCallback->SetClientData(this);
  m_window->AddObserver(vtkCommand::StartEvent, m_renderCallback);

  // Input runs before the interactor style (priority 1); handled clicks and
  // keys are aborted so the style does not also react.
  m_inputCallback = vtkSmartPointer<vtkCallbackCommand>::New();
  m_inputCallback->SetCallback(&SliceView::onInput);
  m_inputCallback->SetClientData(this);
  for (unsigned long event :
       {vtkCommand::LeftButtonPressEvent, vtkCommand::KeyPressEvent,
        vtkCommand::MouseMoveEvent, vtkCommand::LeaveEvent})
    m_interactor->AddObserver(event, m_inputCallback, 1.0);
}

SliceView::~SliceView() {
  m_window->RemoveObserver(m_renderCallback);
  m_interactor->RemoveObserver(m_inputCallback);
}

vtkGenericOpenGLRenderWindow *SliceView::renderWindow() const {
  return m_window;
}

void SliceView::setVolume(vtkImageData *volume,
                          const strata3d::DisplayWindow &displayWindow) {
  m_volume = volume;
  m_toothSeed.reset();
  m_trace.clear();
  m_viewer->SetInputData(volume);
  m_overlayThreshold->SetInputData(volume);
  m_overlayActor->VisibilityOff();
  m_viewer->SetColorWindow(displayWindow.window);
  m_viewer->SetColorLevel(displayWindow.level);
  m_viewer->SetSlice((m_viewer->GetSliceMin() + m_viewer->GetSliceMax()) / 2);
  updateMarkers();
}

int SliceView::slice() const { return m_viewer->GetSlice(); }

int SliceView::sliceCount() const {
  return m_volume ? m_viewer->GetSliceMax() - m_viewer->GetSliceMin() + 1 : 0;
}

void SliceView::setSlice(int slice) {
  if (m_volume)
    m_viewer->SetSlice(slice);
}

void SliceView::resetCamera() {
  vtkCamera *camera = m_viewer->GetRenderer()->GetActiveCamera();
  camera->SetFocalPoint(0, 0, 0);
  camera->SetPosition(0, 0, -1);
  camera->SetViewUp(0, -1, 0);
  m_viewer->GetRenderer()->ResetCamera();
}

void SliceView::render() {
  if (m_volume)
    m_viewer->Render();
}

void SliceView::showOverlay(const strata3d::ThresholdRange &range,
                            const strata3d::Rgb &color) {
  if (range.hasUpper)
    m_overlayThreshold->ThresholdBetween(range.lower, range.upper - 1e-3);
  else
    m_overlayThreshold->ThresholdByUpper(range.lower);
  m_overlayLut->SetTableValue(1, color[0], color[1], color[2],
                              kOverlayOpacity);
  m_overlayLut->Modified();
  m_overlayActor->SetVisibility(m_volume != nullptr);
}

void SliceView::hideOverlay() { m_overlayActor->VisibilityOff(); }

void SliceView::setToothMarker(
    const std::optional<std::array<int, 3>> &voxel) {
  m_toothSeed = voxel;
  updateMarkers();
}

void SliceView::setNerveTrace(const std::vector<strata3d::NervePath> &paths) {
  m_trace = paths;
  updateMarkers();
}

void SliceView::onRenderStart(vtkObject *, unsigned long, void *clientData,
                              void *) {
  static_cast<SliceView *>(clientData)->syncWithSlice();
}

void SliceView::syncWithSlice() {
  if (!m_volume)
    return;
  m_overlayActor->SetDisplayExtent(m_viewer->GetImageActor()->GetDisplayExtent());
  const double thickness = m_volume->GetSpacing()[2];
  const double *dop =
      m_viewer->GetRenderer()->GetActiveCamera()->GetDirectionOfProjection();
  m_overlayActor->SetPosition(0, 0, -kOverlayLift * thickness * dop[2]);
  updateMarkers();
  emit sliceChanged(slice(), sliceCount());
}

void SliceView::updateMarkers() {
  m_seedMarker->Initialize();
  m_traceMarker->Initialize();
  m_seedActor->VisibilityOff();
  if (!m_volume)
    return;

  double origin[3], spacing[3];
  m_volume->GetOrigin(origin);
  m_volume->GetSpacing(spacing);
  const int k = slice();
  const double *dop =
      m_viewer->GetRenderer()->GetActiveCamera()->GetDirectionOfProjection();
  // on the slice, slightly towards the camera so they are drawn on top
  const double z =
      origin[2] + k * spacing[2] - kMarkerLift * spacing[2] * dop[2];

  if (m_toothSeed && (*m_toothSeed)[2] == k) {
    vtkNew<vtkPoints> points;
    vtkNew<vtkCellArray> verts;
    vtkIdType id = points->InsertNextPoint(
        origin[0] + (*m_toothSeed)[0] * spacing[0],
        origin[1] + (*m_toothSeed)[1] * spacing[1], z);
    verts->InsertNextCell(1, &id);
    m_seedMarker->SetPoints(points);
    m_seedMarker->SetVerts(verts);
    m_seedActor->VisibilityOn();
  }
  m_seedMarker->Modified();

  vtkNew<vtkPoints> points;
  vtkNew<vtkCellArray> verts, lines;
  for (const strata3d::NervePath &path : m_trace) {
    std::vector<vtkIdType> ids;
    for (const Point3 &p : path) {
      vtkIdType id = points->InsertNextPoint(p[0], p[1], z);
      verts->InsertNextCell(1, &id);
      ids.push_back(id);
    }
    if (ids.size() >= 2)
      lines->InsertNextCell(static_cast<vtkIdType>(ids.size()), ids.data());
  }
  m_traceMarker->SetPoints(points);
  m_traceMarker->SetVerts(verts);
  m_traceMarker->SetLines(lines);
  m_traceMarker->Modified();
}

void SliceView::onInput(vtkObject *, unsigned long eventId, void *clientData,
                        void *) {
  auto *self = static_cast<SliceView *>(clientData);
  if (self->m_volume && self->handleInput(eventId))
    self->m_inputCallback->SetAbortFlag(1);
}

bool SliceView::handleInput(unsigned long eventId) {
  if (eventId == vtkCommand::LeaveEvent) {
    emit cursorLeft();
    return false;
  }
  if (eventId == vtkCommand::MouseMoveEvent) {
    // never swallowed: the style still pans, zooms and adjusts contrast
    int i, j;
    if (toVoxel(eventPosition(), i, j))
      emit cursorMoved(i, j, slice(),
                       m_volume->GetScalarComponentAsDouble(i, j, slice(), 0));
    else
      emit cursorLeft();
    return false;
  }
  if (!m_picking)
    return false;

  if (eventId == vtkCommand::LeftButtonPressEvent) {
    const Point3 position = eventPosition();
    int i, j;
    if (toVoxel(position, i, j))
      emit voxelClicked(i, j, slice(), position);
    else
      emit clickedOutsideImage();
    return true;
  }
  if (eventId == vtkCommand::KeyPressEvent) {
    const char *sym = m_interactor->GetKeySym();
    const std::string key = sym ? sym : "";
    if (key == "BackSpace" || key == "Delete")
      emit traceKeyPressed(TraceKey::Undo);
    else if (key == "Return" || key == "KP_Enter")
      emit traceKeyPressed(TraceKey::NewPath);
    else if (key == "Escape")
      emit traceKeyPressed(TraceKey::Clear);
    else
      return false;
    return true;
  }
  return false;
}

Point3 SliceView::eventPosition() const {
  int *xy = m_interactor->GetEventPosition();
  vtkRenderer *renderer = m_viewer->GetRenderer();
  renderer->SetDisplayPoint(xy[0], xy[1], 0.0);
  renderer->DisplayToWorld();
  double world[4];
  renderer->GetWorldPoint(world);
  if (world[3] != 0.0)
    for (int a = 0; a < 3; ++a)
      world[a] /= world[3];
  // Parallel projection: x, y are exact; z is the displayed slice
  double origin[3], spacing[3];
  m_volume->GetOrigin(origin);
  m_volume->GetSpacing(spacing);
  return {world[0], world[1], origin[2] + slice() * spacing[2]};
}

bool SliceView::toVoxel(const Point3 &position, int &i, int &j) const {
  double origin[3], spacing[3];
  int extent[6];
  m_volume->GetOrigin(origin);
  m_volume->GetSpacing(spacing);
  m_volume->GetExtent(extent);
  i = static_cast<int>(std::lround((position[0] - origin[0]) / spacing[0]));
  j = static_cast<int>(std::lround((position[1] - origin[1]) / spacing[1]));
  return i >= extent[0] && i <= extent[1] && j >= extent[2] && j <= extent[3];
}

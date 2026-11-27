#include "ui/mesh_view.h"

#include <QVTKInteractor.h>

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkNew.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyDataNormals.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

namespace {
constexpr double kCameraElevation = 30.0; // degrees above the front view
}

MeshView::MeshView(QObject *parent) : QObject(parent) {
  m_renderer = vtkSmartPointer<vtkRenderer>::New();
  m_window = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
  m_interactor = vtkSmartPointer<QVTKInteractor>::New();
  m_style = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
  m_window->AddRenderer(m_renderer);
  m_window->SetInteractor(m_interactor);
  m_interactor->SetInteractorStyle(m_style);
  m_interactor->Initialize();
}

MeshView::~MeshView() = default;

vtkGenericOpenGLRenderWindow *MeshView::renderWindow() const {
  return m_window;
}

void MeshView::setMesh(int structureId, vtkPolyData *mesh,
                       const strata3d::Rgb &color) {
  removeMesh(structureId);
  vtkNew<vtkPolyDataNormals> normals;
  normals->SetInputData(mesh);
  normals->SplittingOff();
  vtkNew<vtkPolyDataMapper> mapper;
  mapper->SetInputConnection(normals->GetOutputPort());
  mapper->ScalarVisibilityOff();
  auto actor = vtkSmartPointer<vtkActor>::New();
  actor->SetMapper(mapper);
  actor->GetProperty()->SetColor(color[0], color[1], color[2]);
  m_renderer->AddActor(actor);
  m_actors[structureId] = actor;
  m_renderer->ResetCameraClippingRange();
}

void MeshView::removeMesh(int structureId) {
  if (m_actors[structureId])
    m_renderer->RemoveActor(m_actors[structureId]);
  m_actors[structureId] = nullptr;
}

void MeshView::clear() {
  for (int id = 0; id < strata3d::StructureCount; ++id)
    removeMesh(id);
}

void MeshView::setMeshVisible(int structureId, bool visible) {
  if (m_actors[structureId])
    m_actors[structureId]->SetVisibility(visible);
}

bool MeshView::isMeshVisible(int structureId) const {
  return m_actors[structureId] && m_actors[structureId]->GetVisibility();
}

void MeshView::setBackground(const QColor &color) {
  m_renderer->SetBackground(color.redF(), color.greenF(), color.blueF());
}

void MeshView::resetCamera() {
  vtkCamera *camera = m_renderer->GetActiveCamera();
  camera->SetFocalPoint(0, 0, 0);
  camera->SetPosition(0, -1, 0);
  camera->SetViewUp(0, 0, 1);
  camera->Elevation(kCameraElevation);
  camera->OrthogonalizeViewUp();
  m_renderer->ResetCamera();
}

void MeshView::render() { m_window->Render(); }

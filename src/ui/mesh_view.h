#ifndef STRATA3D_UI_MESH_VIEW_H
#define STRATA3D_UI_MESH_VIEW_H

#include "core/structures.h"

#include <QColor>
#include <QObject>

#include <vtkSmartPointer.h>

#include <array>

class QVTKInteractor;
class vtkActor;
class vtkGenericOpenGLRenderWindow;
class vtkInteractorStyleTrackballCamera;
class vtkPolyData;
class vtkRenderer;

// 3D view with one actor per structure (shown in a QVTKOpenGLNativeWidget).
// Rendering is explicit: call render() after changes.
class MeshView : public QObject {
  Q_OBJECT
  friend struct Strata3DUiTest; // frames screenshots

public:
  explicit MeshView(QObject *parent = nullptr);
  ~MeshView() override;

  vtkGenericOpenGLRenderWindow *renderWindow() const;

  // Shows `mesh` for the structure, replacing its previous mesh (displayed
  // with smooth normals; the mesh itself is not modified).
  void setMesh(int structureId, vtkPolyData *mesh, const strata3d::Rgb &color);
  void removeMesh(int structureId);
  void clear();
  void setMeshVisible(int structureId, bool visible);
  bool isMeshVisible(int structureId) const;

  void setBackground(const QColor &color);
  // Facing the patient (from anterior), slightly from above, all meshes in view
  void resetCamera();
  void render();

private:
  vtkSmartPointer<vtkRenderer> m_renderer;
  vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_window;
  vtkSmartPointer<QVTKInteractor> m_interactor;
  vtkSmartPointer<vtkInteractorStyleTrackballCamera> m_style;
  std::array<vtkSmartPointer<vtkActor>, strata3d::StructureCount> m_actors;
};

#endif // STRATA3D_UI_MESH_VIEW_H

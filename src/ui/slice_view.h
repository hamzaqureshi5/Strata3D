#ifndef STRATA3D_UI_SLICE_VIEW_H
#define STRATA3D_UI_SLICE_VIEW_H

#include "core/nerve_trace.h"
#include "core/presets.h"
#include "core/structures.h"

#include <QObject>

#include <vtkSmartPointer.h>

#include <array>
#include <optional>
#include <vector>

class QVTKInteractor;
class SliceInteractorStyle;
class vtkActor;
class vtkCallbackCommand;
class vtkGenericOpenGLRenderWindow;
class vtkImageActor;
class vtkImageData;
class vtkImageThreshold;
class vtkImageViewer2;
class vtkLookupTable;
class vtkObject;
class vtkPolyData;

// Axial slice view of a volume (one render window, shown in a
// QVTKOpenGLNativeWidget). Draws the threshold overlay of the selected
// structure, the tooth marker and the nerve trace, and turns mouse/key input
// into signals. Rendering is explicit: call render() after changes.
class SliceView : public QObject {
  Q_OBJECT
  friend struct Strata3DUiTest; // simulates mouse and key input

public:
  enum class TraceKey { Undo, NewPath, Clear };

  explicit SliceView(QObject *parent = nullptr);
  ~SliceView() override;

  vtkGenericOpenGLRenderWindow *renderWindow() const;

  // Shows `volume` (not copied; must stay unchanged) on its middle slice.
  void setVolume(vtkImageData *volume,
                 const strata3d::DisplayWindow &displayWindow);
  int slice() const;
  int sliceCount() const;
  void setSlice(int slice);

  // Radiological view: from the feet, front up, patient's left on the right
  void resetCamera();
  void render();

  void showOverlay(const strata3d::ThresholdRange &range,
                   const strata3d::Rgb &color);
  void hideOverlay();
  // Marker on the clicked tooth voxel (shown on its own slice only)
  void setToothMarker(const std::optional<std::array<int, 3>> &voxel);
  // Trace drawn projected onto the current slice; empty hides it
  void setNerveTrace(const std::vector<strata3d::NervePath> &paths);

  // When enabled, left clicks become voxelClicked() and Backspace/Delete,
  // Enter and Esc become traceKeyPressed() instead of image interaction.
  void setPickingEnabled(bool enabled) { m_picking = enabled; }

signals:
  void sliceChanged(int slice, int sliceCount);
  void voxelClicked(int i, int j, int k, const strata3d::Point3 &position);
  void clickedOutsideImage();
  void traceKeyPressed(SliceView::TraceKey key);
  void cursorMoved(int i, int j, int k, double value);
  void cursorLeft();

private:
  static void onRenderStart(vtkObject *, unsigned long, void *clientData,
                            void *);
  static void onInput(vtkObject *, unsigned long eventId, void *clientData,
                      void *);
  void syncWithSlice();
  bool handleInput(unsigned long eventId);
  // Patient position under the mouse on the current slice
  strata3d::Point3 eventPosition() const;
  bool toVoxel(const strata3d::Point3 &position, int &i, int &j) const;
  void updateMarkers();

  vtkSmartPointer<vtkImageViewer2> m_viewer;
  vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_window;
  vtkSmartPointer<QVTKInteractor> m_interactor;
  vtkSmartPointer<SliceInteractorStyle> m_style;
  vtkSmartPointer<vtkImageThreshold> m_overlayThreshold;
  vtkSmartPointer<vtkLookupTable> m_overlayLut;
  vtkSmartPointer<vtkImageActor> m_overlayActor;
  vtkSmartPointer<vtkPolyData> m_seedMarker;
  vtkSmartPointer<vtkActor> m_seedActor;
  vtkSmartPointer<vtkPolyData> m_traceMarker;
  vtkSmartPointer<vtkActor> m_traceActor;
  vtkSmartPointer<vtkCallbackCommand> m_renderCallback;
  vtkSmartPointer<vtkCallbackCommand> m_inputCallback;

  vtkImageData *m_volume = nullptr;
  bool m_picking = false;
  std::optional<std::array<int, 3>> m_toothSeed;
  std::vector<strata3d::NervePath> m_trace;
};

#endif // STRATA3D_UI_SLICE_VIEW_H

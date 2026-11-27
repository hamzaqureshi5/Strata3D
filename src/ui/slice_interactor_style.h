#ifndef STRATA3D_UI_SLICE_INTERACTOR_STYLE_H
#define STRATA3D_UI_SLICE_INTERACTOR_STYLE_H

#include <vtkInteractorStyleImage.h>

class vtkImageViewer2;

// Image interaction for the slice view: mouse wheel and Up/Down change the
// slice (instead of zooming); the rest (contrast, zoom, pan) is
// vtkInteractorStyleImage's.
class SliceInteractorStyle : public vtkInteractorStyleImage {
public:
  static SliceInteractorStyle *New();
  vtkTypeMacro(SliceInteractorStyle, vtkInteractorStyleImage);

  void SetImageViewer(vtkImageViewer2 *viewer) { m_viewer = viewer; }

  void OnKeyDown() override;
  void OnMouseWheelForward() override { moveSlice(+1); }
  void OnMouseWheelBackward() override { moveSlice(-1); }

protected:
  SliceInteractorStyle() = default;

private:
  // The slice can also change from outside (slider), so always start from
  // the viewer's current slice.
  void moveSlice(int delta);

  vtkImageViewer2 *m_viewer = nullptr;
};

#endif // STRATA3D_UI_SLICE_INTERACTOR_STYLE_H

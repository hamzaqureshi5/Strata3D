#include "ui/slice_interactor_style.h"

#include <vtkImageViewer2.h>
#include <vtkObjectFactory.h>
#include <vtkRenderWindowInteractor.h>

#include <string>

vtkStandardNewMacro(SliceInteractorStyle);

void SliceInteractorStyle::OnKeyDown() {
  const char *sym = this->GetInteractor()->GetKeySym();
  const std::string key = sym ? sym : "";
  if (key == "Up")
    moveSlice(+1);
  else if (key == "Down")
    moveSlice(-1);
  vtkInteractorStyleImage::OnKeyDown();
}

void SliceInteractorStyle::moveSlice(int delta) {
  if (!m_viewer)
    return;
  const int slice = m_viewer->GetSlice() + delta;
  if (slice < m_viewer->GetSliceMin() || slice > m_viewer->GetSliceMax())
    return;
  m_viewer->SetSlice(slice);
  m_viewer->Render();
}

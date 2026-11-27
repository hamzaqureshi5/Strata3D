#ifndef STRATA3D_TEST_HELPERS_H
#define STRATA3D_TEST_HELPERS_H

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

#include <functional>

// Short volume of `dims` voxels (spacing 1 mm, origin 0) filled by value(i,j,k)
inline vtkSmartPointer<vtkImageData>
makeVolume(int nx, int ny, int nz,
           const std::function<double(int, int, int)> &value) {
  auto volume = vtkSmartPointer<vtkImageData>::New();
  volume->SetDimensions(nx, ny, nz);
  volume->SetSpacing(1, 1, 1);
  volume->SetOrigin(0, 0, 0);
  volume->AllocateScalars(VTK_SHORT, 1);
  for (int k = 0; k < nz; ++k)
    for (int j = 0; j < ny; ++j)
      for (int i = 0; i < nx; ++i)
        *static_cast<short *>(volume->GetScalarPointer(i, j, k)) =
            static_cast<short>(value(i, j, k));
  return volume;
}

inline double sq(double v) { return v * v; }

#endif // STRATA3D_TEST_HELPERS_H

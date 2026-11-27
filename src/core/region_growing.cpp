#include "core/region_growing.h"

#include <vtkImageConnectivityFilter.h>
#include <vtkImageContinuousDilate3D.h>
#include <vtkImageContinuousErode3D.h>
#include <vtkImageLogic.h>
#include <vtkImageMask.h>
#include <vtkImageThreshold.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>

#include <algorithm>
#include <cmath>

namespace strata3d {

namespace {

// Nearest voxel (in mm) to `seed` with a nonzero mask value, searched within
// radiusMm. Returns false when there is none.
bool nearestInside(vtkImageData *mask, const std::array<int, 3> &seed,
                   double radiusMm, std::array<int, 3> &found) {
  int ext[6];
  double sp[3];
  mask->GetExtent(ext);
  mask->GetSpacing(sp);
  int r[3];
  for (int a = 0; a < 3; ++a)
    r[a] = static_cast<int>(std::ceil(radiusMm / sp[a]));

  double best = radiusMm * radiusMm;
  bool any = false;
  for (int k = std::max(ext[4], seed[2] - r[2]);
       k <= std::min(ext[5], seed[2] + r[2]); ++k)
    for (int j = std::max(ext[2], seed[1] - r[1]);
         j <= std::min(ext[3], seed[1] + r[1]); ++j)
      for (int i = std::max(ext[0], seed[0] - r[0]);
           i <= std::min(ext[1], seed[0] + r[0]); ++i) {
        if (*static_cast<unsigned char *>(mask->GetScalarPointer(i, j, k)) ==
            0)
          continue;
        double dx = (i - seed[0]) * sp[0], dy = (j - seed[1]) * sp[1],
               dz = (k - seed[2]) * sp[2];
        double d = dx * dx + dy * dy + dz * dz;
        if (d <= best) {
          best = d;
          found = {i, j, k};
          any = true;
        }
      }
  return any;
}

} // namespace

vtkSmartPointer<vtkImageData>
isolateRegion(vtkImageData *volume, double lower,
              const std::array<int, 3> &seed, double separationMm,
              std::string *error) {
  vtkNew<vtkImageThreshold> inRange;
  inRange->SetInputData(volume);
  inRange->ThresholdByUpper(lower);
  inRange->SetInValue(1);
  inRange->SetOutValue(0);
  inRange->ReplaceInOn();
  inRange->ReplaceOutOn();
  inRange->SetOutputScalarTypeToUnsignedChar();
  inRange->Update();

  double sp[3];
  volume->GetSpacing(sp);
  int kernel[3];
  for (int a = 0; a < 3; ++a)
    kernel[a] =
        2 * std::max(1, static_cast<int>(std::lround(separationMm / sp[a]))) +
        1;

  vtkNew<vtkImageContinuousErode3D> erode;
  erode->SetInputConnection(inRange->GetOutputPort());
  erode->SetKernelSize(kernel[0], kernel[1], kernel[2]);
  erode->Update();

  std::array<int, 3> start;
  if (!nearestInside(erode->GetOutput(), seed, kSeedSearchRadiusMm, start)) {
    if (error)
      *error = "No tooth at the clicked point: click inside a tooth, or lower "
               "the threshold";
    return nullptr;
  }
  double origin[3];
  volume->GetOrigin(origin);
  vtkNew<vtkPoints> seedPoint;
  seedPoint->InsertNextPoint(origin[0] + start[0] * sp[0],
                             origin[1] + start[1] * sp[1],
                             origin[2] + start[2] * sp[2]);
  vtkNew<vtkPolyData> seeds;
  seeds->SetPoints(seedPoint);

  vtkNew<vtkImageConnectivityFilter> connected;
  connected->SetInputConnection(erode->GetOutputPort());
  connected->SetSeedData(seeds);
  // The mask is 0/1; a fractional range such as 0.5..1.5 makes this VTK build
  // treat every voxel of an integer image as inside (one region = whole volume)
  connected->SetScalarRange(1, 1);
  connected->SetExtractionModeToSeededRegions();
  connected->SetLabelModeToConstantValue();
  connected->SetLabelConstantValue(1);
  connected->SetLabelScalarTypeToUnsignedChar();

  vtkNew<vtkImageContinuousDilate3D> dilate;
  dilate->SetInputConnection(connected->GetOutputPort());
  dilate->SetKernelSize(kernel[0], kernel[1], kernel[2]);

  // Dilating restores the eroded border; AND keeps it inside the threshold
  vtkNew<vtkImageLogic> region;
  region->SetOperationToAnd();
  region->SetOutputTrueValue(1);
  region->SetInputConnection(0, inRange->GetOutputPort());
  region->SetInputConnection(1, dilate->GetOutputPort());

  double range[2];
  volume->GetScalarRange(range);
  vtkNew<vtkImageMask> masked;
  masked->SetImageInputData(volume);
  masked->SetInputConnection(1, region->GetOutputPort());
  masked->SetMaskedOutputValue(std::max(range[0], lower - 1.0));
  masked->Update();

  return masked->GetOutput();
}

} // namespace strata3d

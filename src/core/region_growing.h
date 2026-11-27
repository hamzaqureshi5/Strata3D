#ifndef STRATA3D_CORE_REGION_GROWING_H
#define STRATA3D_CORE_REGION_GROWING_H

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

#include <array>
#include <string>

namespace strata3d {

// Erosion depth that separates touching teeth before region growing.
constexpr double kToothSeparationMm = 0.6;
// How far from the clicked voxel the region may start (the click can land on
// the border, which the erosion removes).
constexpr double kSeedSearchRadiusMm = 3.0;

// Single tooth: among voxels >= lower, keeps the region connected to `seed`
// (voxel index i, j, k). Thin contacts (neighbouring teeth, roots touching
// bone) are cut by eroding `separationMm` before growing and dilating back
// afterwards. Returns a copy of the volume in which every voxel outside the
// region is set below `lower`, ready for an iso-surface at `lower`; null with
// `error` set when there is no region near the seed.
vtkSmartPointer<vtkImageData>
isolateRegion(vtkImageData *volume, double lower,
              const std::array<int, 3> &seed,
              double separationMm, std::string *error);

} // namespace strata3d

#endif // STRATA3D_CORE_REGION_GROWING_H

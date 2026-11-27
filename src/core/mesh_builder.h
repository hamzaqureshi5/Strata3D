#ifndef STRATA3D_CORE_MESH_BUILDER_H
#define STRATA3D_CORE_MESH_BUILDER_H

#include "core/region_growing.h"
#include "core/structures.h"

#include <vtkImageData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <array>
#include <functional>
#include <string>

namespace strata3d {

// Cleanup applied to every threshold-based mesh
struct MeshSettings {
  double smallObjectRatio = 0.02; // drop parts below 2 % of the largest part
  double reductionRate = 0.5;     // remove half of the triangles
  unsigned int smoothingIterations = 20;
  double toothSeparationMm = kToothSeparationMm;
};

// What to mesh: a threshold range, optionally limited to the region connected
// to a seed voxel (single tooth).
struct MeshRequest {
  int structureId = BoneStructure;
  ThresholdRange range;
  bool seeded = false;
  std::array<int, 3> seed{};
};

struct MeshResult {
  vtkSmartPointer<vtkPolyData> mesh; // null on failure
  std::string error;
};

struct MeshCallbacks {
  std::function<void(const std::string &)> stage; // e.g. "Smoothing mesh"
  std::function<void(double)> progress;           // 0..100 per stage
};

// Iso-surface of the request, then removal of small fragments, reduction and
// smoothing. The mesh stays in the volume's coordinates (no centering), so
// meshes of different structures line up. `volume` is only read; it may be
// shared with other threads as long as nobody modifies it.
MeshResult buildMesh(vtkImageData *volume, const MeshRequest &request,
                     const MeshSettings &settings = {},
                     const MeshCallbacks &callbacks = {});

} // namespace strata3d

#endif // STRATA3D_CORE_MESH_BUILDER_H

#ifndef STRATA3D_CORE_STRUCTURES_H
#define STRATA3D_CORE_STRUCTURES_H

#include <array>

namespace strata3d {

// Anatomical structures that can be segmented. The order is used for the
// structure picker, the Segment menu (Ctrl+1..4) and the mesh list.
enum StructureId {
  BoneStructure = 0,
  TeethStructure,
  ToothStructure,
  NerveStructure,
  StructureCount
};

using Rgb = std::array<double, 3>;

struct StructureInfo {
  const char *name;     // shown in the UI
  const char *fileName; // written by Export STL
  Rgb meshColor;        // 3D view
  Rgb overlayColor;     // slice overlay: saturated to stand out on gray
};

const StructureInfo &structureInfo(int structureId);

// Voxel value range [lower, upper) that belongs to a structure; open-ended
// unless hasUpper.
struct ThresholdRange {
  double lower = 0.0;
  bool hasUpper = false;
  double upper = 0.0;
};

// Bone stops where tooth density starts, so the bone and teeth meshes do not
// overlap. All other structures are open-ended above their threshold.
ThresholdRange thresholdRange(int structureId, double lower, double teethLower);

} // namespace strata3d

#endif // STRATA3D_CORE_STRUCTURES_H

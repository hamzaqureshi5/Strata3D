#include "core/structures.h"

#include <cassert>

namespace strata3d {

namespace {

const StructureInfo kStructures[StructureCount] = {
    {"Bone", "bone.stl", {0.89, 0.85, 0.76}, {1.00, 0.55, 0.00}},
    {"All teeth", "teeth.stl", {0.62, 0.80, 1.00}, {0.00, 0.85, 1.00}},
    {"Single tooth", "tooth.stl", {0.55, 0.95, 0.60}, {0.10, 1.00, 0.30}},
    {"Nerve", "nerve.stl", {1.00, 0.85, 0.10}, {1.00, 0.90, 0.00}},
};

} // namespace

const StructureInfo &structureInfo(int structureId) {
  assert(structureId >= 0 && structureId < StructureCount);
  return kStructures[structureId];
}

ThresholdRange thresholdRange(int structureId, double lower,
                              double teethLower) {
  ThresholdRange range;
  range.lower = lower;
  if (structureId == BoneStructure && teethLower > lower) {
    range.hasUpper = true;
    range.upper = teethLower;
  }
  return range;
}

} // namespace strata3d

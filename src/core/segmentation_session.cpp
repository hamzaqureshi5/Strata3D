#include "core/segmentation_session.h"

#include <algorithm>

namespace strata3d {

void SegmentationSession::reset(const ThresholdPresets &presets) {
  m_lower.fill(0.0);
  m_lower[BoneStructure] = presets.bone;
  m_lower[TeethStructure] = presets.teeth;
  m_lower[ToothStructure] = presets.teeth;
  m_toothSeed.reset();
  m_nerve.clear();
  for (BuiltMesh &m : m_meshes)
    m = BuiltMesh{};
  m_unexported = false;
}

ThresholdRange SegmentationSession::range(int structureId) const {
  return thresholdRange(structureId, m_lower[structureId],
                        m_lower[TeethStructure]);
}

void SegmentationSession::setNerveDiameter(double mm) {
  m_nerveDiameter = std::clamp(mm, kNerveMinDiameter, kNerveMaxDiameter);
}

bool SegmentationSession::meshRequest(int structureId, MeshRequest &request,
                                      std::string &reason) const {
  if (structureId == NerveStructure) {
    reason = "The nerve is built from its trace, not from a threshold.";
    return false;
  }
  if (structureId == ToothStructure && !m_toothSeed) {
    reason = "Click the tooth on a slice first.";
    return false;
  }
  request = MeshRequest{};
  request.structureId = structureId;
  request.range = range(structureId);
  if (structureId == ToothStructure) {
    request.seeded = true;
    request.seed = *m_toothSeed;
  }
  return true;
}

vtkSmartPointer<vtkPolyData>
SegmentationSession::buildNerveMesh(std::string &reason) const {
  vtkSmartPointer<vtkPolyData> tube =
      buildNerveTube(m_nerve.paths(), m_nerveDiameter);
  if (!tube)
    reason = "Click at least two points along the canal first.";
  return tube;
}

void SegmentationSession::setMesh(int structureId,
                                  vtkSmartPointer<vtkPolyData> mesh,
                                  double setting) {
  m_meshes[structureId] = {mesh, setting};
  m_unexported = true;
}

void SegmentationSession::removeMesh(int structureId) {
  m_meshes[structureId] = BuiltMesh{};
  if (!hasMeshes())
    m_unexported = false;
}

bool SegmentationSession::hasMeshes() const {
  return std::any_of(m_meshes.begin(), m_meshes.end(),
                     [](const BuiltMesh &m) { return m.mesh != nullptr; });
}

} // namespace strata3d

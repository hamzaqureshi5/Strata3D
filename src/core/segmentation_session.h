#ifndef STRATA3D_CORE_SEGMENTATION_SESSION_H
#define STRATA3D_CORE_SEGMENTATION_SESSION_H

#include "core/mesh_builder.h"
#include "core/nerve_trace.h"
#include "core/presets.h"
#include "core/structures.h"

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <array>
#include <optional>
#include <string>

namespace strata3d {

// State of segmenting one scan: thresholds per structure, the selected tooth,
// the nerve trace and the meshes built so far. Holds the rules for what a
// mesh request needs; no UI, no threads.
class SegmentationSession {
public:
  // New scan: thresholds from the presets; seed, trace and meshes cleared.
  void reset(const ThresholdPresets &presets);

  double lower(int structureId) const { return m_lower[structureId]; }
  void setLower(int structureId, double value) { m_lower[structureId] = value; }
  // Range that the overlay shows and the mesh contains (bone stops at teeth)
  ThresholdRange range(int structureId) const;

  void setToothSeed(const std::array<int, 3> &voxel) { m_toothSeed = voxel; }
  const std::optional<std::array<int, 3>> &toothSeed() const {
    return m_toothSeed;
  }

  NerveTrace &nerve() { return m_nerve; }
  const NerveTrace &nerve() const { return m_nerve; }
  double nerveDiameter() const { return m_nerveDiameter; }
  void setNerveDiameter(double mm); // clamped to the allowed range

  // Request for a threshold-based structure (not the nerve). Returns false
  // with a reason the user can act on when an input is missing.
  bool meshRequest(int structureId, MeshRequest &request,
                   std::string &reason) const;
  // Tube along the nerve trace; null with a reason when it has no canal.
  vtkSmartPointer<vtkPolyData> buildNerveMesh(std::string &reason) const;

  // Meshes; `setting` is the threshold (nerve: diameter) used to build it.
  void setMesh(int structureId, vtkSmartPointer<vtkPolyData> mesh,
               double setting);
  void removeMesh(int structureId);
  vtkPolyData *mesh(int structureId) const {
    return m_meshes[structureId].mesh;
  }
  double meshSetting(int structureId) const {
    return m_meshes[structureId].setting;
  }
  bool hasMeshes() const;
  // Meshes created or changed since the last export
  bool hasUnexportedMeshes() const { return m_unexported; }
  void markExported() { m_unexported = false; }
  void discardUnexported() { m_unexported = false; }

private:
  struct BuiltMesh {
    vtkSmartPointer<vtkPolyData> mesh;
    double setting = 0.0;
  };

  std::array<double, StructureCount> m_lower{};
  std::optional<std::array<int, 3>> m_toothSeed;
  NerveTrace m_nerve;
  double m_nerveDiameter = kNerveDefaultDiameter;
  std::array<BuiltMesh, StructureCount> m_meshes;
  bool m_unexported = false;
};

} // namespace strata3d

#endif // STRATA3D_CORE_SEGMENTATION_SESSION_H

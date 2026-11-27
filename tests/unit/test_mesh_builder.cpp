#include "core/mesh_builder.h"
#include "test_helpers.h"

#include <gtest/gtest.h>

#include <vtkPolyData.h>

#include <vector>

using namespace strata3d;

namespace {

// Spheres of radius 6 at x = 10 and x = 30; the left one has a denser core
// (radius 3) like a tooth inside bone.
vtkSmartPointer<vtkImageData> twoSpheres() {
  return makeVolume(40, 20, 20, [](int i, int j, int k) {
    double dl = sq(i - 10) + sq(j - 10) + sq(k - 10);
    double dr = sq(i - 30) + sq(j - 10) + sq(k - 10);
    if (dl <= 9)
      return 3000;
    return (dl <= 36 || dr <= 36) ? 1000 : 0;
  });
}

} // namespace

TEST(MeshBuilder, ThresholdMeshCoversAllObjects) {
  MeshRequest request;
  request.range = {500, false, 0};
  MeshResult result = buildMesh(twoSpheres(), request);
  ASSERT_NE(result.mesh, nullptr) << result.error;
  double b[6];
  result.mesh->GetBounds(b);
  EXPECT_LT(b[0], 6);  // left sphere
  EXPECT_GT(b[1], 34); // right sphere
}

TEST(MeshBuilder, SeededMeshIsOnlyTheClickedObject) {
  MeshRequest request;
  request.range = {500, false, 0};
  request.seeded = true;
  request.seed = {30, 10, 10};
  MeshResult result = buildMesh(twoSpheres(), request);
  ASSERT_NE(result.mesh, nullptr) << result.error;
  double b[6];
  result.mesh->GetBounds(b);
  EXPECT_GT(b[0], 20); // left sphere excluded
}

TEST(MeshBuilder, NoSurfaceAboveTheMaximum) {
  MeshRequest request;
  request.range = {5000, false, 0};
  MeshResult result = buildMesh(twoSpheres(), request);
  EXPECT_EQ(result.mesh, nullptr);
  EXPECT_FALSE(result.error.empty());
}

TEST(MeshBuilder, UpperBoundLeavesTheVolumeUnchanged) {
  auto volume = twoSpheres();
  MeshRequest request;
  request.range = {500, true, 2000};
  ASSERT_NE(buildMesh(volume, request).mesh, nullptr);
  EXPECT_EQ(volume->GetScalarComponentAsDouble(10, 10, 10, 0), 3000);
}

TEST(MeshBuilder, ReportsStages) {
  std::vector<std::string> stages;
  MeshCallbacks callbacks;
  callbacks.stage = [&](const std::string &s) { stages.push_back(s); };
  MeshRequest request;
  request.range = {500, false, 0};
  buildMesh(twoSpheres(), request, {}, callbacks);
  ASSERT_FALSE(stages.empty());
  EXPECT_EQ(stages.front(), "Extracting surface");
  EXPECT_EQ(stages.back(), "Smoothing mesh");
}

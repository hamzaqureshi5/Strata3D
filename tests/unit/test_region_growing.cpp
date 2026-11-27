#include "core/region_growing.h"
#include "test_helpers.h"

#include <gtest/gtest.h>

using namespace strata3d;

namespace {

// Two dense cubes (x 4..13 and 16..25) joined by a one-voxel bridge at y=z=10,
// like two teeth touching at a contact point.
vtkSmartPointer<vtkImageData> twoTouchingCubes() {
  return makeVolume(30, 20, 20, [](int i, int j, int k) {
    bool inY = j >= 5 && j <= 14, inZ = k >= 5 && k <= 14;
    bool cubeA = i >= 4 && i <= 13 && inY && inZ;
    bool cubeB = i >= 16 && i <= 25 && inY && inZ;
    bool bridge = (i == 14 || i == 15) && j == 10 && k == 10;
    return (cubeA || cubeB || bridge) ? 2000 : 0;
  });
}

int countAtLeast(vtkImageData *v, double lower, int iMin, int iMax) {
  int dims[3], n = 0;
  v->GetDimensions(dims);
  for (int k = 0; k < dims[2]; ++k)
    for (int j = 0; j < dims[1]; ++j)
      for (int i = iMin; i <= iMax; ++i)
        n += v->GetScalarComponentAsDouble(i, j, k, 0) >= lower;
  return n;
}

} // namespace

TEST(RegionGrowing, KeepsOnlyTheClickedObject) {
  auto volume = twoTouchingCubes();
  std::string error;
  auto region = isolateRegion(volume, 1500, {8, 10, 10}, 0.6, &error);
  ASSERT_NE(region, nullptr) << error;
  // cube A; the ellipsoidal erode/dilate kernel rounds its 8 corners
  EXPECT_GE(countAtLeast(region, 1500, 4, 13), 990);
  EXPECT_EQ(countAtLeast(region, 1500, 16, 29), 0); // none of cube B
}

TEST(RegionGrowing, ClickOnTheBorderStillWorks) {
  auto volume = twoTouchingCubes();
  std::string error;
  // corner voxel of cube B: removed by the erosion, found by the search
  auto region = isolateRegion(volume, 1500, {25, 14, 14}, 0.6, &error);
  ASSERT_NE(region, nullptr) << error;
  EXPECT_GE(countAtLeast(region, 1500, 16, 25), 990);
  EXPECT_EQ(countAtLeast(region, 1500, 0, 13), 0);
}

TEST(RegionGrowing, ClickAwayFromAnyObjectFails) {
  auto volume = twoTouchingCubes();
  std::string error;
  EXPECT_EQ(isolateRegion(volume, 1500, {0, 0, 0}, 0.6, &error), nullptr);
  EXPECT_FALSE(error.empty());
}

TEST(RegionGrowing, InputVolumeIsNotModified) {
  auto volume = twoTouchingCubes();
  std::string error;
  isolateRegion(volume, 1500, {8, 10, 10}, 0.6, &error);
  EXPECT_EQ(countAtLeast(volume, 1500, 16, 25), 1000);
}

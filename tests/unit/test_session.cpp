#include "core/segmentation_session.h"
#include "core/structures.h"

#include <gtest/gtest.h>

#include <vtkPolyData.h>

using namespace strata3d;

TEST(Structures, BoneStopsWhereTeethStart) {
  ThresholdRange bone = thresholdRange(BoneStructure, 400, 1500);
  EXPECT_EQ(bone.lower, 400);
  EXPECT_TRUE(bone.hasUpper);
  EXPECT_EQ(bone.upper, 1500);
}

TEST(Structures, BoneIsOpenWhenTeethThresholdIsNotAbove) {
  EXPECT_FALSE(thresholdRange(BoneStructure, 1500, 1500).hasUpper);
  EXPECT_FALSE(thresholdRange(BoneStructure, 1600, 1500).hasUpper);
}

TEST(Structures, OtherStructuresAreOpenEnded) {
  for (int id : {TeethStructure, ToothStructure, NerveStructure})
    EXPECT_FALSE(thresholdRange(id, 400, 1500).hasUpper) << id;
}

TEST(Structures, EveryStructureHasAnStlFile) {
  for (int id = 0; id < StructureCount; ++id)
    EXPECT_NE(std::string(structureInfo(id).fileName).find(".stl"),
              std::string::npos);
}

class SessionTest : public ::testing::Test {
protected:
  void SetUp() override { session.reset({400, 1500, true}); }
  SegmentationSession session;
};

TEST_F(SessionTest, ResetTakesThresholdsFromPresets) {
  EXPECT_EQ(session.lower(BoneStructure), 400);
  EXPECT_EQ(session.lower(TeethStructure), 1500);
  EXPECT_EQ(session.lower(ToothStructure), 1500);
  EXPECT_FALSE(session.toothSeed());
  EXPECT_TRUE(session.nerve().empty());
  EXPECT_FALSE(session.hasMeshes());
}

TEST_F(SessionTest, BoneRangeFollowsTheTeethThreshold) {
  session.setLower(TeethStructure, 1800);
  EXPECT_EQ(session.range(BoneStructure).upper, 1800);
}

TEST_F(SessionTest, SingleToothNeedsAClick) {
  MeshRequest request;
  std::string reason;
  EXPECT_FALSE(session.meshRequest(ToothStructure, request, reason));
  EXPECT_NE(reason.find("Click the tooth"), std::string::npos);

  session.setToothSeed({3, 4, 5});
  ASSERT_TRUE(session.meshRequest(ToothStructure, request, reason));
  EXPECT_TRUE(request.seeded);
  EXPECT_EQ(request.seed, (std::array<int, 3>{3, 4, 5}));
  EXPECT_EQ(request.range.lower, 1500);
}

TEST_F(SessionTest, BoneRequestIsUnseededWithUpperBound) {
  MeshRequest request;
  std::string reason;
  ASSERT_TRUE(session.meshRequest(BoneStructure, request, reason));
  EXPECT_FALSE(request.seeded);
  EXPECT_TRUE(request.range.hasUpper);
}

TEST_F(SessionTest, NerveIsNotAThresholdRequest) {
  MeshRequest request;
  std::string reason;
  EXPECT_FALSE(session.meshRequest(NerveStructure, request, reason));
}

TEST_F(SessionTest, NerveMeshNeedsTwoPoints) {
  std::string reason;
  session.nerve().addPoint({0, 0, 0});
  EXPECT_EQ(session.buildNerveMesh(reason), nullptr);
  EXPECT_FALSE(reason.empty());
  session.nerve().addPoint({10, 0, 0});
  EXPECT_NE(session.buildNerveMesh(reason), nullptr);
}

TEST_F(SessionTest, NerveDiameterIsClamped) {
  session.setNerveDiameter(100);
  EXPECT_EQ(session.nerveDiameter(), kNerveMaxDiameter);
  session.setNerveDiameter(0.1);
  EXPECT_EQ(session.nerveDiameter(), kNerveMinDiameter);
}

TEST_F(SessionTest, UnexportedMeshesAreTracked) {
  auto mesh = vtkSmartPointer<vtkPolyData>::New();
  session.setMesh(BoneStructure, mesh, 400);
  EXPECT_TRUE(session.hasMeshes());
  EXPECT_TRUE(session.hasUnexportedMeshes());
  EXPECT_EQ(session.meshSetting(BoneStructure), 400);

  session.markExported();
  EXPECT_FALSE(session.hasUnexportedMeshes());

  session.setMesh(TeethStructure, mesh, 1500);
  EXPECT_TRUE(session.hasUnexportedMeshes());
  session.removeMesh(TeethStructure);
  session.removeMesh(BoneStructure);
  EXPECT_FALSE(session.hasMeshes());
  EXPECT_FALSE(session.hasUnexportedMeshes());
}

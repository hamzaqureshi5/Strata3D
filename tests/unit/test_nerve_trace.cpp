#include "core/nerve_trace.h"

#include <gtest/gtest.h>

using namespace strata3d;

TEST(NerveTrace, AddUndoAndNewPath) {
  NerveTrace trace;
  trace.addPoint({0, 0, 0});
  trace.addPoint({1, 0, 0});
  EXPECT_EQ(trace.pointCount(), 2);
  EXPECT_EQ(trace.canalCount(), 1);

  trace.startNewPath();
  trace.addPoint({5, 0, 0});
  ASSERT_EQ(trace.paths().size(), 2u);
  EXPECT_EQ(trace.canalCount(), 1); // second canal has one point

  trace.undo(); // removes (5,0,0)
  trace.undo(); // empty second path: steps back and removes (1,0,0)
  ASSERT_EQ(trace.paths().size(), 1u);
  EXPECT_EQ(trace.pointCount(), 1);
}

TEST(NerveTrace, NewPathOnlyAfterPoints) {
  NerveTrace trace;
  trace.startNewPath();
  EXPECT_TRUE(trace.paths().empty());
  trace.addPoint({0, 0, 0});
  trace.startNewPath();
  trace.startNewPath();
  EXPECT_EQ(trace.paths().size(), 2u);
}

TEST(NerveTrace, UndoAndClearOnEmptyTraceAreSafe) {
  NerveTrace trace;
  trace.undo();
  trace.clear();
  EXPECT_TRUE(trace.empty());
}

TEST(NerveTube, FollowsThePathWithTheDiameter) {
  vtkSmartPointer<vtkPolyData> tube =
      buildNerveTube({{{0, 0, 0}, {20, 0, 0}}}, 2.0);
  ASSERT_NE(tube, nullptr);
  double b[6];
  tube->GetBounds(b);
  EXPECT_NEAR(b[0], 0, 1e-6);
  EXPECT_NEAR(b[1], 20, 1e-6);
  EXPECT_NEAR(b[2], -1, 0.01); // radius 1 mm
  EXPECT_NEAR(b[3], 1, 0.01);
  EXPECT_NEAR(b[4], -1, 0.01);
  EXPECT_NEAR(b[5], 1, 0.01);
}

TEST(NerveTube, SkipsPathsWithOnePoint) {
  EXPECT_EQ(buildNerveTube({{{0, 0, 0}}}, 2.0), nullptr);
  EXPECT_NE(buildNerveTube({{{0, 0, 0}}, {{0, 0, 0}, {5, 5, 0}}}, 2.0),
            nullptr);
}

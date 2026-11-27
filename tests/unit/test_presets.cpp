#include "core/dicom_series.h"
#include "core/presets.h"
#include "test_helpers.h"

#include <gtest/gtest.h>

using namespace strata3d;

TEST(Presets, CalibratedCtUsesHounsfieldDefaults) {
  auto volume = makeVolume(10, 10, 10, [](int i, int, int) {
    return i < 5 ? -1000 : 900;
  });
  ThresholdPresets presets = estimateThresholdPresets(volume);
  EXPECT_TRUE(presets.hounsfield);
  EXPECT_EQ(presets.bone, kHuBoneThreshold);
  EXPECT_EQ(presets.teeth, kHuTeethThreshold);
}

// example/dental_cbct (run from the repository root): stored values are
// HU + 1000 with no rescale, i.e. air 0, soft tissue 1040, bone 1900,
// dentin 2700, enamel 3500. The presets must fall between soft tissue and
// bone, and between bone and teeth.
//
// The nested Otsu splits assume air is the largest class, as in real scans.
// When soft tissue and air are similar in size, the first split can group
// them, and the bone preset ends up too high; the user then adjusts it.
TEST(Presets, UncalibratedScanSplitsTheHistogram) {
  DicomSeries series = readDicomSeries("example/dental_cbct");
  ASSERT_NE(series.volume, nullptr) << series.error;
  ThresholdPresets presets = estimateThresholdPresets(series.volume);
  EXPECT_FALSE(presets.hounsfield);
  EXPECT_GT(presets.bone, 1040);
  EXPECT_LT(presets.bone, 1900);
  EXPECT_GT(presets.teeth, 1900);
  EXPECT_LT(presets.teeth, 2700);
}

TEST(Presets, DisplayWindow) {
  auto volume = makeVolume(4, 4, 4, [](int i, int, int) { return i * 100; });
  DisplayWindow raw = defaultDisplayWindow(volume, false);
  EXPECT_EQ(raw.window, 300);
  EXPECT_EQ(raw.level, 150);
  DisplayWindow hu = defaultDisplayWindow(volume, true);
  EXPECT_GT(hu.window, 0);
}

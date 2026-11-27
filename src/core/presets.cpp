#include "core/presets.h"

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkPointData.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace strata3d {

namespace {

constexpr int kHistogramBins = 1024;
constexpr vtkIdType kMaxHistogramSamples = 4000000; // subsample big volumes
constexpr double kHuWindow = 3000.0, kHuLevel = 700.0;

// Otsu split of histogram bins [lo, hi): returns the first bin of the upper
// class. When several splits are equally good (sparse histograms), the middle
// one is used.
int otsuSplit(const std::vector<double> &hist, int lo, int hi) {
  double total = 0.0, sum = 0.0;
  for (int b = lo; b < hi; ++b) {
    total += hist[b];
    sum += b * hist[b];
  }

  double w0 = 0.0, sum0 = 0.0, best = -1.0;
  int bestFirst = lo + 1, bestLast = lo + 1;
  for (int s = lo + 1; s < hi; ++s) {
    w0 += hist[s - 1];
    sum0 += (s - 1) * hist[s - 1];
    double w1 = total - w0;
    if (w0 <= 0.0 || w1 <= 0.0)
      continue;
    double d = sum0 / w0 - (sum - sum0) / w1;
    double between = w0 * w1 * d * d;
    if (between > best * (1.0 + 1e-12)) {
      best = between;
      bestFirst = bestLast = s;
    } else if (between >= best * (1.0 - 1e-12)) {
      bestLast = s;
    }
  }
  return (bestFirst + bestLast) / 2;
}

} // namespace

ThresholdPresets estimateThresholdPresets(vtkImageData *volume) {
  double range[2];
  volume->GetScalarRange(range);

  if (range[0] <= kHounsfieldAirLimit)
    return {kHuBoneThreshold, kHuTeethThreshold, true};

  const double width = (range[1] - range[0]) / kHistogramBins;
  if (width <= 0.0)
    return {range[0], range[1], false};

  std::vector<double> hist(kHistogramBins, 0.0);
  vtkDataArray *scalars = volume->GetPointData()->GetScalars();
  const vtkIdType n = scalars->GetNumberOfTuples();
  const vtkIdType step = std::max<vtkIdType>(1, n / kMaxHistogramSamples);
  for (vtkIdType i = 0; i < n; i += step) {
    int b = static_cast<int>((scalars->GetComponent(i, 0) - range[0]) / width);
    hist[std::clamp(b, 0, kHistogramBins - 1)] += 1.0;
  }

  int airTissue = otsuSplit(hist, 0, kHistogramBins);
  int softHard = otsuSplit(hist, airTissue, kHistogramBins);
  int boneTeeth = otsuSplit(hist, softHard, kHistogramBins);

  return {std::round(range[0] + softHard * width),
          std::round(range[0] + boneTeeth * width), false};
}

DisplayWindow defaultDisplayWindow(vtkImageData *volume, bool hounsfield) {
  if (hounsfield)
    return {kHuWindow, kHuLevel};
  double range[2];
  volume->GetScalarRange(range);
  return {range[1] - range[0], (range[0] + range[1]) / 2.0};
}

} // namespace strata3d

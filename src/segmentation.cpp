#include "segmentation.h"

#include <vtkAppendPolyData.h>
#include <vtkCleanPolyData.h>
#include <vtkDataArray.h>
#include <vtkImageConnectivityFilter.h>
#include <vtkImageContinuousDilate3D.h>
#include <vtkImageContinuousErode3D.h>
#include <vtkImageData.h>
#include <vtkImageLogic.h>
#include <vtkImageMask.h>
#include <vtkImageThreshold.h>
#include <vtkNew.h>
#include <vtkParametricFunctionSource.h>
#include <vtkParametricSpline.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkTriangleFilter.h>
#include <vtkTubeFilter.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

const double kHuBone = 400.0;
const double kHuTeeth = 1500.0;

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

// Nearest voxel (in mm) to `seed` with a nonzero mask value, searched within
// radiusMm. Returns false when there is none.
bool nearestInside(vtkImageData *mask, const int seed[3], double radiusMm,
                   int found[3]) {
  int ext[6];
  double sp[3];
  mask->GetExtent(ext);
  mask->GetSpacing(sp);
  int r[3];
  for (int a = 0; a < 3; ++a)
    r[a] = static_cast<int>(std::ceil(radiusMm / sp[a]));

  double best = radiusMm * radiusMm;
  bool any = false;
  for (int k = std::max(ext[4], seed[2] - r[2]);
       k <= std::min(ext[5], seed[2] + r[2]); ++k)
    for (int j = std::max(ext[2], seed[1] - r[1]);
         j <= std::min(ext[3], seed[1] + r[1]); ++j)
      for (int i = std::max(ext[0], seed[0] - r[0]);
           i <= std::min(ext[1], seed[0] + r[0]); ++i) {
        if (*static_cast<unsigned char *>(mask->GetScalarPointer(i, j, k)) ==
            0)
          continue;
        double dx = (i - seed[0]) * sp[0], dy = (j - seed[1]) * sp[1],
               dz = (k - seed[2]) * sp[2];
        double d = dx * dx + dy * dy + dz * dz;
        if (d <= best) {
          best = d;
          found[0] = i;
          found[1] = j;
          found[2] = k;
          any = true;
        }
      }
  return any;
}

} // namespace

ThresholdPresets estimateThresholdPresets(vtkImageData *volume) {
  double range[2];
  volume->GetScalarRange(range);

  // vtkDICOMImageReader applies RescaleSlope/Intercept, so CT data arrives in
  // HU with air near -1000.
  if (range[0] <= -900.0)
    return {kHuBone, kHuTeeth, true};

  const int bins = 1024;
  const double width = (range[1] - range[0]) / bins;
  if (width <= 0.0)
    return {range[0], range[1], false};

  std::vector<double> hist(bins, 0.0);
  vtkDataArray *scalars = volume->GetPointData()->GetScalars();
  const vtkIdType n = scalars->GetNumberOfTuples();
  const vtkIdType step =
      std::max<vtkIdType>(1, n / 4000000); // subsample big volumes
  for (vtkIdType i = 0; i < n; i += step) {
    int b = static_cast<int>((scalars->GetComponent(i, 0) - range[0]) / width);
    hist[std::clamp(b, 0, bins - 1)] += 1.0;
  }

  int airTissue = otsuSplit(hist, 0, bins);
  int softHard = otsuSplit(hist, airTissue, bins);
  int boneTeeth = otsuSplit(hist, softHard, bins);

  return {std::round(range[0] + softHard * width),
          std::round(range[0] + boneTeeth * width), false};
}

vtkSmartPointer<vtkImageData> isolateRegion(vtkImageData *volume, double lower,
                                            const int seed[3],
                                            double separationMm,
                                            QString *error) {
  vtkNew<vtkImageThreshold> inRange;
  inRange->SetInputData(volume);
  inRange->ThresholdByUpper(lower);
  inRange->SetInValue(1);
  inRange->SetOutValue(0);
  inRange->ReplaceInOn();
  inRange->ReplaceOutOn();
  inRange->SetOutputScalarTypeToUnsignedChar();
  inRange->Update();

  double sp[3];
  volume->GetSpacing(sp);
  int kernel[3];
  for (int a = 0; a < 3; ++a)
    kernel[a] = 2 * std::max(1, static_cast<int>(std::lround(separationMm / sp[a]))) + 1;

  vtkNew<vtkImageContinuousErode3D> erode;
  erode->SetInputConnection(inRange->GetOutputPort());
  erode->SetKernelSize(kernel[0], kernel[1], kernel[2]);
  erode->Update();

  // The click may land near the edge, which the erosion removes
  int start[3];
  if (!nearestInside(erode->GetOutput(), seed, 3.0, start)) {
    if (error)
      *error = "No tooth at the clicked point: click inside a tooth, or lower "
               "the threshold";
    return nullptr;
  }
  double origin[3];
  volume->GetOrigin(origin);
  vtkNew<vtkPoints> seedPoint;
  seedPoint->InsertNextPoint(origin[0] + start[0] * sp[0],
                             origin[1] + start[1] * sp[1],
                             origin[2] + start[2] * sp[2]);
  vtkNew<vtkPolyData> seeds;
  seeds->SetPoints(seedPoint);

  vtkNew<vtkImageConnectivityFilter> connected;
  connected->SetInputConnection(erode->GetOutputPort());
  connected->SetSeedData(seeds);
  // The mask is 0/1; a fractional range such as 0.5..1.5 makes this VTK build
  // treat every voxel of an integer image as inside (one region = whole volume)
  connected->SetScalarRange(1, 1);
  connected->SetExtractionModeToSeededRegions();
  connected->SetLabelModeToConstantValue();
  connected->SetLabelConstantValue(1);
  connected->SetLabelScalarTypeToUnsignedChar();

  vtkNew<vtkImageContinuousDilate3D> dilate;
  dilate->SetInputConnection(connected->GetOutputPort());
  dilate->SetKernelSize(kernel[0], kernel[1], kernel[2]);

  // Dilating restores the eroded border; AND keeps it inside the threshold
  vtkNew<vtkImageLogic> region;
  region->SetOperationToAnd();
  region->SetOutputTrueValue(1);
  region->SetInputConnection(0, inRange->GetOutputPort());
  region->SetInputConnection(1, dilate->GetOutputPort());

  double range[2];
  volume->GetScalarRange(range);
  vtkNew<vtkImageMask> masked;
  masked->SetImageInputData(volume);
  masked->SetInputConnection(1, region->GetOutputPort());
  masked->SetMaskedOutputValue(std::max(range[0], lower - 1.0));
  masked->Update();

  return masked->GetOutput();
}

vtkSmartPointer<vtkPolyData> buildNerveTube(const std::vector<NervePath> &paths,
                                            double diameter) {
  vtkNew<vtkAppendPolyData> all;
  int used = 0;
  for (const NervePath &path : paths) {
    if (path.size() < 2)
      continue;
    vtkNew<vtkPoints> points;
    for (const Point3 &p : path)
      points->InsertNextPoint(p.data());
    vtkNew<vtkParametricSpline> spline;
    spline->SetPoints(points);
    vtkNew<vtkParametricFunctionSource> curve;
    curve->SetParametricFunction(spline);
    curve->SetUResolution(std::max<int>(50, 20 * static_cast<int>(path.size())));

    vtkNew<vtkTubeFilter> tube;
    tube->SetInputConnection(curve->GetOutputPort());
    tube->SetRadius(diameter / 2.0);
    tube->SetNumberOfSides(24);
    tube->CappingOn();
    vtkNew<vtkTriangleFilter> triangles;
    triangles->SetInputConnection(tube->GetOutputPort());
    all->AddInputConnection(triangles->GetOutputPort());
    ++used;
  }
  if (used == 0)
    return nullptr;

  vtkNew<vtkCleanPolyData> clean;
  clean->SetInputConnection(all->GetOutputPort());
  clean->Update();
  vtkSmartPointer<vtkPolyData> mesh = vtkSmartPointer<vtkPolyData>::New();
  mesh->DeepCopy(clean->GetOutput());
  return mesh;
}

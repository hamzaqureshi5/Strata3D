#ifndef STRATA3D_CORE_PRESETS_H
#define STRATA3D_CORE_PRESETS_H

class vtkImageData;

namespace strata3d {

// Below this the minimum value means air in Hounsfield units, i.e. a
// calibrated CT (vtkDICOMImageReader applies RescaleSlope/Intercept).
constexpr double kHounsfieldAirLimit = -900.0;
constexpr double kHuBoneThreshold = 400.0;
constexpr double kHuTeethThreshold = 1500.0;

struct ThresholdPresets {
  double bone = 0.0;
  double teeth = 0.0;
  bool hounsfield = false; // CT in HU; otherwise raw gray values (e.g. CBCT)
};

// Starting thresholds for bone and teeth. Fixed HU values for calibrated CT;
// for uncalibrated scans three nested Otsu splits of the histogram
// (air | soft tissue | bone | teeth), so they are a starting point to adjust.
ThresholdPresets estimateThresholdPresets(vtkImageData *volume);

struct DisplayWindow {
  double window = 0.0;
  double level = 0.0;
};

// Initial slice contrast: a wide bone/teeth window for HU, the full value
// range otherwise.
DisplayWindow defaultDisplayWindow(vtkImageData *volume, bool hounsfield);

} // namespace strata3d

#endif // STRATA3D_CORE_PRESETS_H

#ifndef SEGMENTATION_H
#define SEGMENTATION_H

#include <QString>

#include <vtkActor.h>
#include <vtkImageData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <array>
#include <vector>

// Structures segmented without AI: bone and teeth by threshold, a single tooth
// by threshold + click-seeded region growing, the nerve by manual tracing.
enum StructureId
{
	BoneStructure = 0,
	TeethStructure = 1,
	ToothStructure = 2,
	NerveStructure = 3,
	StructureCount
};

struct Structure
{
	QString name;      // label in the UI
	QString fileName;  // file written by EXPORT STL
	double color[3];
	double overlayColor[3]; // slice overlay (saturated, to stand out on gray)
	double lower = 0.0;  // voxels >= lower belong to the structure (bone: and < teeth lower)

	vtkSmartPointer<vtkPolyData> mesh;  // last mesh built with CREATE STL
	vtkSmartPointer<vtkActor> actor;    // its actor in the 3D view
	double meshSetting = 0.0;           // threshold (nerve: diameter) the mesh was built with
};

struct ThresholdPresets
{
	double bone;
	double teeth;
	bool hounsfield;  // volume is calibrated in HU (CT); otherwise raw gray values (e.g. CBCT)
};

using Point3 = std::array<double, 3>;
using NervePath = std::vector<Point3>;

// Starting thresholds for bone and teeth. Fixed HU values for calibrated CT; for uncalibrated
// scans they are estimated from the histogram (air | soft tissue | bone | teeth Otsu splits),
// so they are a starting point to adjust with the slider.
ThresholdPresets estimateThresholdPresets(vtkImageData* volume);

// Region growing for a single tooth: among voxels >= lower, keeps the region connected to
// `seed` (voxel index). Thin contacts (neighbouring teeth, roots touching bone) are cut by
// eroding `separationMm` before growing and dilating back afterwards. Returns a copy of the
// volume in which every voxel outside the region is set below `lower`, ready for an
// iso-surface at `lower`; null with `error` set when there is no region near the seed.
vtkSmartPointer<vtkImageData> isolateRegion(vtkImageData* volume, double lower, const int seed[3],
                                            double separationMm, QString* error);

// Closed tube of the given diameter (mm) along each path (smoothed by a spline); paths with
// fewer than two points are skipped. Null when no path is usable.
vtkSmartPointer<vtkPolyData> buildNerveTube(const std::vector<NervePath>& paths, double diameter);

#endif // SEGMENTATION_H

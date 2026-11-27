#ifndef STRATA3D_CORE_DICOM_SERIES_H
#define STRATA3D_CORE_DICOM_SERIES_H

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

#include <string>

namespace strata3d {

struct DicomSeries {
  std::string folder;
  std::string patientName;
  std::string error; // set when no volume could be read
  vtkSmartPointer<vtkImageData> volume;
};

// Reads the DICOM series in `folder` (one series, uncompressed). Safe to call
// on a worker thread.
//
// vtkDICOMImageReader applies RescaleSlope/Intercept (CT arrives in HU) but
// flips the rows and reverses the slice order, which turns the anatomy upside
// down. Both are flipped back, so the voxel axes follow DICOM for axial scans:
// x -> patient left, y -> posterior, z -> superior.
DicomSeries readDicomSeries(const std::string &folder);

} // namespace strata3d

#endif // STRATA3D_CORE_DICOM_SERIES_H

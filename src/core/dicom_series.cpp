#include "core/dicom_series.h"

#include <vtkDICOMImageReader.h>
#include <vtkImageFlip.h>
#include <vtkNew.h>
#include <vtkPointData.h>

#include <exception>

namespace strata3d {

DicomSeries readDicomSeries(const std::string &folder) {
  DicomSeries result;
  result.folder = folder;
  try {
    vtkNew<vtkDICOMImageReader> reader;
    reader->SetDirectoryName(folder.c_str());
    reader->ReleaseDataFlagOn();
    vtkNew<vtkImageFlip> flipRows;
    flipRows->SetInputConnection(reader->GetOutputPort());
    flipRows->SetFilteredAxis(1);
    flipRows->ReleaseDataFlagOn();
    vtkNew<vtkImageFlip> flipSlices;
    flipSlices->SetInputConnection(flipRows->GetOutputPort());
    flipSlices->SetFilteredAxis(2);
    flipSlices->Update();

    vtkImageData *output = flipSlices->GetOutput();
    int *dims = output->GetDimensions();
    if (dims[0] < 2 || dims[1] < 2 || dims[2] < 2 ||
        !output->GetPointData()->GetScalars()) {
      result.error = "No readable DICOM series was found in this folder. It "
                     "must contain the slices of one uncompressed CT or CBCT "
                     "series.";
      return result;
    }
    result.volume = vtkSmartPointer<vtkImageData>::New();
    result.volume->ShallowCopy(output);
    const char *name = reader->GetPatientName();
    result.patientName = name ? name : "";
    // DICOM pads values to even length with spaces
    result.patientName.erase(result.patientName.find_last_not_of(' ') + 1);
  } catch (const std::exception &e) {
    // e.g. std::bad_alloc for a scan larger than the available memory
    result.volume = nullptr;
    result.error = std::string("Reading the series failed: ") + e.what();
  }
  return result;
}

} // namespace strata3d

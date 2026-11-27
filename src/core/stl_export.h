#ifndef STRATA3D_CORE_STL_EXPORT_H
#define STRATA3D_CORE_STL_EXPORT_H

#include <string>
#include <vector>

class vtkPolyData;

namespace strata3d {

// "<patient>_<timestamp>" with everything except letters, digits, '_' and '-'
// replaced by '_'; "scan" when the patient name is empty.
std::string exportFolderName(const std::string &patientName,
                             const std::string &timestamp);

struct ExportItem {
  std::string fileName; // e.g. "bone.stl"
  vtkPolyData *mesh;
};

struct ExportResult {
  std::vector<std::string> written;
  std::vector<std::string> failed;
  std::string error; // folder could not be created
  bool ok() const { return error.empty() && failed.empty(); }
};

// Creates `folder` (and parents) and writes each mesh as a binary STL.
ExportResult exportStl(const std::string &folder,
                       const std::vector<ExportItem> &items);

} // namespace strata3d

#endif // STRATA3D_CORE_STL_EXPORT_H

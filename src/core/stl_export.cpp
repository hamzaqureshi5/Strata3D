#include "core/stl_export.h"

#include "meshData.h"

#include <vtkPolyData.h>

#include <cctype>
#include <filesystem>
#include <system_error>

namespace strata3d {

std::string exportFolderName(const std::string &patientName,
                             const std::string &timestamp) {
  std::string base;
  for (char c : patientName) {
    bool keep = std::isalnum(static_cast<unsigned char>(c)) || c == '_' ||
                c == '-';
    char out = keep ? c : '_';
    if (out == '_' && !base.empty() && base.back() == '_')
      continue; // collapse runs
    base += out;
  }
  while (!base.empty() && base.front() == '_')
    base.erase(base.begin());
  while (!base.empty() && base.back() == '_')
    base.pop_back();
  if (base.empty())
    base = "scan";
  return base + "_" + timestamp;
}

ExportResult exportStl(const std::string &folder,
                       const std::vector<ExportItem> &items) {
  namespace fs = std::filesystem;
  ExportResult result;
  std::error_code ec;
  fs::create_directories(folder, ec);
  if (ec || !fs::is_directory(folder)) {
    result.error = "Cannot create the folder " + folder;
    return result;
  }

  VTKMeshData writer;
  for (const ExportItem &item : items) {
    const fs::path path = fs::path(folder) / item.fileName;
    vtkSmartPointer<vtkPolyData> mesh = item.mesh;
    writer.exportAsStlFile(mesh, path.string(), true);
    if (fs::exists(path, ec) && fs::file_size(path, ec) > 0)
      result.written.push_back(item.fileName);
    else
      result.failed.push_back(item.fileName);
  }
  return result;
}

} // namespace strata3d

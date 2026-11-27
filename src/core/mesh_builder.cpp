#include "core/mesh_builder.h"

#include "dicomFactory.h"
#include "dicomRoutines.h"
#include "meshRoutines.h"

#include <vtkAlgorithm.h>
#include <vtkCallbackCommand.h>
#include <vtkNew.h>

#include <exception>

namespace strata3d {

namespace {

// Forwards dicom2meshlib's VTK progress events to MeshCallbacks::progress
void forwardProgress(vtkObject *caller, unsigned long, void *clientData,
                     void *) {
  auto *callbacks = static_cast<const MeshCallbacks *>(clientData);
  if (callbacks->progress)
    callbacks->progress(static_cast<vtkAlgorithm *>(caller)->GetProgress() *
                        100.0);
}

void stage(const MeshCallbacks &callbacks, const char *text) {
  if (callbacks.stage)
    callbacks.stage(text);
}

} // namespace

MeshResult buildMesh(vtkImageData *volume, const MeshRequest &request,
                     const MeshSettings &settings,
                     const MeshCallbacks &callbacks) {
  MeshResult result;
  try {
    vtkNew<vtkCallbackCommand> progress;
    progress->SetCallback(forwardProgress);
    progress->SetClientData(const_cast<MeshCallbacks *>(&callbacks));
    std::shared_ptr<VTKDicomRoutines> dicomRoutines =
        VTKDicomFactory::getDicomRoutines();
    dicomRoutines->SetProgressCallback(progress.Get());
    VTKMeshRoutines meshRoutines;
    meshRoutines.SetProgressCallback(progress.Get());

    // dicom2meshlib's dicomToMesh replaces the data of the volume it is given
    // when an upper bound is used, so it always gets its own data object
    // (sharing the read-only voxels).
    vtkSmartPointer<vtkImageData> input = vtkSmartPointer<vtkImageData>::New();
    input->ShallowCopy(volume);
    if (request.seeded) {
      stage(callbacks, "Growing region from the clicked point");
      input = isolateRegion(volume, request.range.lower, request.seed,
                            settings.toothSeparationMm, &result.error);
      if (!input)
        return result;
    }

    stage(callbacks, "Extracting surface");
    vtkSmartPointer<vtkPolyData> mesh = dicomRoutines->dicomToMesh(
        input, static_cast<int>(request.range.lower), request.range.hasUpper,
        static_cast<int>(request.range.upper));
    if (!mesh || mesh->GetNumberOfCells() == 0) {
      result.error = "No surface found at this threshold";
      return result;
    }

    stage(callbacks, "Removing small fragments");
    meshRoutines.removeSmallObjects(mesh, settings.smallObjectRatio);
    stage(callbacks, "Reducing mesh");
    meshRoutines.meshReduction(mesh, settings.reductionRate);
    stage(callbacks, "Smoothing mesh");
    meshRoutines.smoothMesh(mesh, settings.smoothingIterations);
    result.mesh = mesh;
  } catch (const std::exception &e) {
    // e.g. std::bad_alloc on a very large volume
    result.mesh = nullptr;
    result.error = std::string("Building the mesh failed: ") + e.what();
  }
  return result;
}

} // namespace strata3d

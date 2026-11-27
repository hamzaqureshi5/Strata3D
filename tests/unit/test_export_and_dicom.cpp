#include "core/dicom_series.h"
#include "core/stl_export.h"

#include <gtest/gtest.h>

#include <vtkNew.h>
#include <vtkPolyData.h>
#include <vtkSTLReader.h>
#include <vtkSphereSource.h>

#include <filesystem>

using namespace strata3d;
namespace fs = std::filesystem;

TEST(ExportFolderName, ReplacesUnsafeCharacters) {
  EXPECT_EQ(exportFolderName("Phantom^DentalCT", "20260101_120000"),
            "Phantom_DentalCT_20260101_120000");
  EXPECT_EQ(exportFolderName("  Doe, John / ok-1 ", "T"), "Doe_John_ok-1_T");
}

TEST(ExportFolderName, EmptyNameFallsBackToScan) {
  EXPECT_EQ(exportFolderName("", "T"), "scan_T");
  EXPECT_EQ(exportFolderName("^^^", "T"), "scan_T");
}

TEST(ExportStl, WritesOneReadableFilePerMesh) {
  const fs::path folder =
      fs::temp_directory_path() / "strata3d_unit_export" / "nested";
  fs::remove_all(folder.parent_path());
  vtkNew<vtkSphereSource> sphere;
  sphere->Update();

  ExportResult result = exportStl(
      folder.string(),
      {{"bone.stl", sphere->GetOutput()}, {"teeth.stl", sphere->GetOutput()}});
  ASSERT_TRUE(result.ok()) << result.error;
  EXPECT_EQ(result.written.size(), 2u);

  vtkNew<vtkSTLReader> reader;
  reader->SetFileName((folder / "teeth.stl").string().c_str());
  reader->Update();
  EXPECT_EQ(reader->GetOutput()->GetNumberOfCells(),
            sphere->GetOutput()->GetNumberOfCells());
  fs::remove_all(folder.parent_path());
}

TEST(ExportStl, ReportsAFolderThatCannotBeCreated) {
  ExportResult result = exportStl("/proc/strata3d_cannot_create", {});
  EXPECT_FALSE(result.ok());
  EXPECT_FALSE(result.error.empty());
}

// Reads example/dental_ct (run from the repository root). The phantom's
// enamel caps are at z = 32..36 mm, i.e. slice index 64..72 when the volume is
// upright (DICOM slice order), and at small y (front of the arch).
TEST(DicomSeries, PhantomIsReadUpright) {
  DicomSeries series = readDicomSeries("example/dental_ct");
  ASSERT_NE(series.volume, nullptr) << series.error;
  int dims[3];
  series.volume->GetDimensions(dims);
  EXPECT_EQ(dims[0], 160);
  EXPECT_EQ(dims[1], 160);
  EXPECT_EQ(dims[2], 100);
  EXPECT_EQ(series.patientName, "Phantom^DentalCT");
  double range[2];
  series.volume->GetScalarRange(range);
  EXPECT_EQ(range[0], -1000); // RescaleIntercept applied: HU
  // front tooth (arch angle 82.5°): centre x = 43.5 mm, y = 23.2 mm
  EXPECT_EQ(series.volume->GetScalarComponentAsDouble(87, 46, 70, 0), 2500);
  EXPECT_EQ(series.volume->GetScalarComponentAsDouble(87, 46, 30, 0), 1700);
}

TEST(DicomSeries, MissingFolderReportsAnError) {
  DicomSeries series = readDicomSeries("example/does_not_exist");
  EXPECT_EQ(series.volume, nullptr);
  EXPECT_FALSE(series.error.empty());
}

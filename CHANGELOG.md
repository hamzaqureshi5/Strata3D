# Changelog

All notable changes to this project are listed here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/). There are no tagged releases
yet.

## [Unreleased]

### Added
- Project documentation: README, user guide, architecture and development guides,
  contributing guide, security policy, issue and pull request templates, CI workflow.
- End-to-end UI test (`tests/ui/ui_test.cpp`) that also captures the README
  screenshots.
- Application logo.

### Changed
- Renamed the application to **Strata3D** (executable `build/Strata3D`).
- Redesigned the main window: layout-based UI with a resizable sidebar, dark and light
  themes from one design system, menus, toolbar, status bar with value readout,
  keyboard shortcuts, start screen with recent scans and drag and drop, background
  loading, remembered window layout and preferences, prompts before unexported meshes
  are lost.
- Layered architecture: Qt-free `strata3d_core` (structures and rules, presets, DICOM
  reading, region growing, nerve trace, mesh building, export, session state),
  `strata3d_ui` (main window, separate slice and 3D views, mesh worker, typed settings)
  and the `Strata3D` executable. See `docs/architecture.md` and `docs/adr/`.
- DicomToMesh (dicom2meshlib) is now a git submodule in `3rdparty/DicomToMesh` instead of
  copied `lib/` and `dicom2mesh/` folders; CMake ≥ 3.24 required.
- Logging through Qt categories (`strata3d.*`), with VTK's messages routed into them.
- vtk-dicom is no longer needed; DCMTK is optional (test-data generator only).
- Unit tests of the core (`strata3d_unit_tests`, 35 tests); one CMake option
  `STRATA3D_BUILD_TESTS` for unit and UI tests.
- README screenshots use the real CT only.

### Removed
- Dead code: the commented-out `d2mWidget`, the unbuilt upstream `gui/` copy, an unused
  icon and helper class.

## 2026-11-26 – Linux, segmentation workflow and examples

### Added
- Interactive segmentation without AI: bone and all teeth by threshold with a live
  overlay, single tooth by click-seeded region growing, nerve canal by manual tracing.
- One STL per structure, exported upright in the scan's patient axes.
- `tools/dental_phantom` synthetic dental DICOM generator; example scans in `example/`
  (real head and neck CT from TCIA, dental phantoms).

### Changed
- Ported to Qt 6 and VTK 9 on Linux (Ubuntu 26.04).

### Fixed
- DICOM loading always failed because of an inverted "no data" check.
- `dicomToMesh` overwrote the input volume when an upper threshold was used.
- Crash on Wayland (`BadAccess` on `X_GLXMakeCurrent`) and blank rendering on NVIDIA.
- Upside-down volumes from `vtkDICOMImageReader`.

## 2024-07 – 2025-01 – Initial development

- dicom2mesh pipeline integrated; first Qt user interface (Windows, Qt 5).

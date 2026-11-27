# 0001: Layered architecture with a Qt-free core

**Status:** accepted (2026-11)

## Context

All behaviour lived in one 1,600-line `MainWindow`: menus and settings, DICOM loading,
the VTK slice and 3D views, the segmentation rules (bone stops at tooth density, what a
single tooth or nerve needs), nerve-trace editing and STL export. Rules could only be
tested through the window, and every change touched the same file.

## Decision

Split the application into three CMake targets with one-way dependencies:

- `strata3d_core` (`src/core`): the segmentation domain on VTK types, with no Qt. Rules
  are plain functions and small classes (`SegmentationSession`, `NerveTrace`,
  `buildMesh`, `exportStl`); errors are returned as values.
- `strata3d_ui` (`src/ui`): `MainWindow` (presentation and wiring), `SliceView` and
  `MeshView` (VTK views that draw what they are given and report input as signals),
  `MeshWorker` (thread wrapper), `AppSettings`, theme.
- `Strata3D` (`src/app`): `main()` and logging setup.

## Consequences

- The core is unit-tested without a display (`tests/unit`, also in CI).
- New structures or tools add rules in the core and presentation in the UI, instead of
  growing `MainWindow`.
- `MainWindow` is still the largest file (presentation of every control); further
  splits (for example a sidebar controller) are possible when it grows.

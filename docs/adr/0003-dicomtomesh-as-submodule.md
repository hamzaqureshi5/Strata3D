# 0003: DicomToMesh as a git submodule in `3rdparty/`

**Status:** accepted (2026-11)

## Context

`lib/` (dicom2meshlib) and `dicom2mesh/` (its command-line tool) were copies of
[AOT-AG/DicomToMesh](https://github.com/AOT-AG/DicomToMesh) (MIT) with local edits.
Copies drift from upstream, hide where code comes from and cannot be reused by other
projects. Upstream had meanwhile fixed the bugs found in the copy, except one:
`dicomToMesh` replaces the data of the volume it is given when an upper threshold is
used.

## Decision

- Include DicomToMesh as a git submodule at `3rdparty/DicomToMesh`, pinned to a specific
  commit, and build only its `lib/` by default (`dicom2mesh` CLI with
  `STRATA3D_BUILD_DICOM2MESH_CLI`).
- Do not edit submodule code. Work around upstream behaviour in `src/core`:
  `buildMesh` always passes a shallow copy of the volume.
- Update the submodule deliberately (`git -C 3rdparty/DicomToMesh checkout <commit>`),
  then run the unit and UI tests.

## Consequences

- Clones need `--recurse-submodules` (or `git submodule update --init`); CMake stops
  with a clear message when the submodule is missing.
- CMake ≥ 3.24 is required (the submodule's minimum).
- Fixes that belong upstream can be offered as pull requests to DicomToMesh.

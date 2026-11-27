# Development guide

## Requirements

| Dependency | Version | Notes |
|---|---|---|
| C++ compiler | C++17 | GCC 15 is used for testing |
| CMake | ≥ 3.24 | Required by the DicomToMesh submodule |
| Qt | 6 | Widgets, OpenGLWidgets, OpenGL, Concurrent; Test for the UI test |
| VTK | 9 | Built with Qt 6 support (`GUISupportQt`) |
| DCMTK | ≥ 3.6 | Optional: only `tools/dental_phantom` needs it |
| git | | The DicomToMesh code is a submodule |

Only **Ubuntu 26.04** (Wayland, NVIDIA driver) is tested. Ubuntu's VTK Qt module is
built against Qt 6, which is why the project uses Qt 6; Qt 5 and Qt 6 cannot be mixed in
one program.

```bash
sudo apt install build-essential cmake git qt6-base-dev libvtk9-dev libvtk9-qt-dev \
                 libgl1-mesa-dev libdcmtk-dev
git clone --recurse-submodules <repository-url>
# or, in an existing clone:
git submodule update --init
```

On other systems, install the same libraries and point CMake at them with
`-DCMAKE_PREFIX_PATH=…` or the environment variables `QTDIR` and `VTK_DIR`.

## Build

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

| Target | Output |
|---|---|
| `Strata3D` | `build/Strata3D`, the application |
| `strata3d_core`, `strata3d_ui` | static libraries (`src/core`, `src/ui`) |
| `dicom2meshlib` | static library from `3rdparty/DicomToMesh/lib` |
| `dental_phantom` | `build/dental_phantom <dir> [--cbct]` (only when DCMTK is found) |

| CMake option | Default | Effect |
|---|---|---|
| `STRATA3D_BUILD_TESTS` | OFF | Builds `strata3d_unit_tests` and `strata3d_ui_test` (GoogleTest is downloaded at configure time) |
| `STRATA3D_BUILD_DICOM2MESH_CLI` | OFF | Builds the upstream `dicom2mesh` command-line converter |

Options of the submodule (for example `TESTDICOM2MESHLIB`) are passed through as usual.

Sources are collected with `file(GLOB … CONFIGURE_DEPENDS)`, so new files in
`src/core`, `src/ui` or `tests/unit` are picked up by the next build.

## Layout and dependency rules

```text
src/app   ->  src/ui  ->  src/core  ->  3rdparty/DicomToMesh/lib (dicom2meshlib)
                 |            |
               Qt 6         VTK 9
```

- `src/core` must not include Qt or anything from `src/ui`. Keep rules (what a
  structure contains, what a request needs, validation messages) here, as plain
  functions or small classes, and return errors as values.
- `src/ui` presents the core: widgets, VTK views, threads, settings. Views draw what they
  are given and report input through signals; they do not know the rules.
- Work that can take more than a moment runs off the UI thread (`MeshWorker`, the
  QtConcurrent loader). Pass volumes to other threads as a `ShallowCopy`.
- Tuning values are named constants next to the code that uses them (`MeshSettings`,
  `presets.h`, `region_growing.h`, `nerve_trace.h`).
- Third-party code stays in `3rdparty/` as submodules and is not edited; work around
  upstream behaviour in `src/core` (see [ADR 0003](adr/0003-dicomtomesh-as-submodule.md)).

## Tests

Run all tests **from the repository root**; they read `example/`.

```bash
cmake -S . -B build -DSTRATA3D_BUILD_TESTS=ON
cmake --build build -j"$(nproc)"
./build/strata3d_unit_tests
./build/strata3d_ui_test
```

| Test | What it covers |
|---|---|
| `strata3d_unit_tests` (35 tests) | The core on synthetic volumes with known answers: bone/teeth rule, session validation and unexported tracking, nerve trace editing and tube geometry, presets (CT and the CBCT phantom), region growing across a contact bridge, mesh building (threshold, seeded, upper bound, stages), export naming and writing, DICOM orientation on the phantom |
| `strata3d_ui_test` (44 checks) | The real main window driven through buttons, menu actions, shortcuts and mouse/key events on the slice view. On the real CT: loading, presets, threshold controls, all four tools, mesh list, prompts, export, theme, settings. On the dental phantom: drag-and-drop loading and exact geometry (single tooth, two nerve canals, 12 teeth). |

Run a single unit test with a filter, for example
`./build/strata3d_unit_tests --gtest_filter='RegionGrowing.*'`.

The UI test opens a real window, so it needs a desktop session (X11 or Wayland). It
stores its settings under a separate name and does not touch your Strata3D preferences.
If another window has keyboard focus, shortcut checks trigger the bound action instead
and print a note.

The DicomToMesh submodule has its own tests (`-DTESTDICOM2MESHLIB=ON`, run from
`3rdparty/DicomToMesh`).

### Screenshots

The README images are captured from the running application, always on real scans:
the head-only CT (`example/downloads/cptac_aml_head_ct`) for bone, the head and neck CT
for teeth, single tooth and nerve:

```bash
./build/strata3d_ui_test --screenshots docs/assets/screenshots
```

### Test data

- `example/downloads/tcga_thca_headneck_ct/`: real head and neck CT with teeth (TCIA,
  CC BY 3.0).
- `example/downloads/cptac_aml_head_ct/`: real head-only CT without table, no teeth (TCIA,
  CC BY 4.0).
- `example/dental_ct/`, `example/dental_cbct/`: synthetic dental phantom
  (`tools/dental_phantom.cpp`; geometry documented in `example/README.md`). Used by
  tests with known geometry, not for screenshots.

## Logging

All diagnostics go through Qt logging categories `strata3d.load`, `strata3d.mesh`,
`strata3d.ui` and `strata3d.vtk` (VTK's own messages are routed there). Enable debug
output with `QT_LOGGING_RULES="strata3d.*.debug=true" ./build/Strata3D`.

## Code conventions

- Match the surrounding code: LLVM-style formatting with 2-space indents, `m_` member
  prefix, `k` prefix for constants, `strata3d` namespace in `src/core`.
- Styling goes through `src/ui/theme.cpp`. Use a `role` property for widget variants;
  do not add per-widget style sheets.
- Never use a standalone `vtkRenderWindowInteractor` or call `Start()` in the GUI; use
  the widget's interactor (see [platform pitfalls](#platform-pitfalls)).
- No formatter or linter is enforced in the build; build with no warnings.

## Platform pitfalls

- **Wayland and NVIDIA:** `src/app/main.cpp` must call
  `QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat())` before
  `QApplication`, or NVIDIA rejects VTK's shaders. `main_window.cpp` registers a VTK
  factory override so every `vtkRenderWindow` is a `vtkGenericOpenGLRenderWindow`;
  VTK's own X11 window crashes on Wayland with `BadAccess (X_GLXMakeCurrent)`.
- Every target that renders with VTK 9 needs `vtk_module_autoinit` in CMake.
- `vtkImageConnectivityFilter` with a fractional `ScalarRange` (such as 0.5..1.5) on an
  integer image treats every voxel as inside; use an integer range such as (1, 1).
- Queued signals carrying `vtkSmartPointer` or `MeshRequest` need `ui/meta_types.h`
  included before the signal is declared.
- File names are case-sensitive on Linux (`#include` lines, headers named in `.ui`
  files).

## Packaging and releases

There is no packaging yet: no installer, AppImage or release workflow, and no tagged
releases. Distribution today means building from source. Candidates are CPack (DEB) for
Ubuntu and an AppImage for other Linux distributions; Windows and macOS builds first need
to be tested.

## Continuous integration

`.github/workflows/ci.yml` checks out the submodules, builds everything with
`STRATA3D_BUILD_TESTS=ON` and runs the unit tests in an Ubuntu 26.04 container on pushes
and pull requests. The UI test is compiled but not run (it needs a desktop session).

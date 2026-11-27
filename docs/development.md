# Development guide

## Requirements

| Dependency | Version | Notes |
|---|---|---|
| C++ compiler | C++17 | GCC 15 is used for testing |
| CMake | ≥ 3.17 | |
| Qt | 6 | Widgets, OpenGLWidgets, OpenGL, Xml, Concurrent; Test for the UI test |
| VTK | 9 | Built with Qt 6 support (`GUISupportQt`) |
| vtk-dicom | | Headers and library (`libvtk-dicom-dev`) |
| DCMTK | ≥ 3.6 | |

Only **Ubuntu 26.04** (Wayland, NVIDIA driver) is tested. Ubuntu's VTK Qt module is
built against Qt 6, which is why the project uses Qt 6; Qt 5 and Qt 6 cannot be mixed in
one program.

```bash
sudo apt install build-essential cmake qt6-base-dev libvtk9-dev libvtk9-qt-dev \
                 libdcmtk-dev libvtk-dicom-dev libgl1-mesa-dev
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
| `dental_phantom` | `build/dental_phantom <dir> [--cbct]`, writes a synthetic dental series |
| `dicom2mesh` | `build/dicom2mesh/dicom2mesh`, command-line converter |
| `dicom2meshlib` | static library used by all of the above |

CMake options:

| Option | Default | Effect |
|---|---|---|
| `TESTDICOM2MESHLIB` | OFF | Builds `runLibTests` (GoogleTest is downloaded at configure time) |
| `TESTDICOM2MESH` | OFF | Builds `runD2MTests` |
| `STRATA3D_BUILD_UI_TESTS` | OFF | Builds `strata3d_ui_test` |
| `USE_VTK_DICOM` | OFF | Uses vtk-dicom's reader in dicom2meshlib (CLI only; the GUI reads with `vtkDICOMImageReader`) |

Sources are collected with `file(GLOB …)`: after adding or removing files, run the
configure step again.

## Tests

Run all tests **from the repository root**; they read `lib/test/data/` and `example/`.

```bash
cmake -S . -B build -DTESTDICOM2MESHLIB=ON -DTESTDICOM2MESH=ON -DSTRATA3D_BUILD_UI_TESTS=ON
cmake --build build -j"$(nproc)"
./build/lib/runLibTests
./build/dicom2mesh/runD2MTests
./build/strata3d_ui_test
```

| Test | What it covers |
|---|---|
| `runLibTests` (10 tests) | DICOM/PNG loading, iso-surface extraction, mesh reduce/center/import/export |
| `runD2MTests` (23 tests) | CLI argument parsing and end-to-end meshing |
| `strata3d_ui_test` (44 checks) | The real main window driven through buttons, menu actions, shortcuts and mouse/key events on the slice view. On the real CT: loading, presets, threshold controls, all four tools, mesh list, prompts, export, theme, settings. On the dental phantom: drag-and-drop loading and exact geometry (single tooth is the clicked tooth, two nerve canals at canal height and diameter, 12 teeth). |

Run a single GoogleTest test with a filter, for example
`./build/dicom2mesh/runD2MTests --gtest_filter='ArgumentParser.*'`.

The GoogleTest suites write output files (`*.info`, `*Export.obj`, `*.stl`) into the
current directory; delete them after a run.

The UI test opens a real window, so it needs a desktop session (X11 or Wayland). It
stores its settings under a separate name and does not touch your Strata3D preferences.
Keyboard-shortcut checks need the test window to have focus; do not use the desktop
while it runs.

### Screenshots

The README images are captured from the running application:

```bash
./build/strata3d_ui_test --screenshots docs/assets/screenshots
```

### Test data

- `example/downloads/tcga_thca_headneck_ct/`: real head and neck CT (TCIA, CC BY 3.0).
- `example/dental_ct/`, `example/dental_cbct/`: synthetic dental phantom
  (`tools/dental_phantom.cpp`; geometry documented in `example/README.md`).
- `lib/test/data/`: PNG slices and `torus.obj` from upstream dicom2mesh.

## Code conventions

- Match the surrounding code. Newer files (`src/MainWindow.cpp`, `src/theme.cpp`,
  `src/segmentation.cpp`, `tests/`) use LLVM-style formatting with 2-space indents;
  upstream files in `lib/` and `dicom2mesh/` keep their original style.
- Keep segmentation logic out of the UI: VTK-only functions go in
  `src/segmentation.cpp` or `lib/`, heavy work runs on the converter thread.
- Styling goes through `src/theme.cpp`. Use a `role` property for widget variants;
  do not add per-widget style sheets.
- Never use a standalone `vtkRenderWindowInteractor` or call `Start()` in the GUI; use
  the widget's interactor (`CLAUDE.md` explains the Linux/Wayland details).
- No formatter or linter is enforced in the build; build with no warnings.

## Packaging and releases

There is no packaging yet: no installer, AppImage or release workflow, and no tagged
releases. Distribution today means building from source. Candidates are CPack (DEB) for
Ubuntu and an AppImage for other Linux distributions; Windows and macOS builds first need
to be tested.

## Continuous integration

`.github/workflows/ci.yml` builds the project and runs the GoogleTest suites in an
Ubuntu 26.04 container on pushes and pull requests. The UI test is not run in CI (it
needs a desktop session).

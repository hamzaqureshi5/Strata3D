# Contributing to Strata3D

Thanks for helping. This page covers the workflow; [docs/development.md](docs/development.md)
has the build options, tests and code conventions.

## Set up

```bash
sudo apt install build-essential cmake git qt6-base-dev libvtk9-dev libvtk9-qt-dev \
                 libgl1-mesa-dev libdcmtk-dev
git clone --recurse-submodules <repository-url>
cmake -S . -B build -DSTRATA3D_BUILD_TESTS=ON
cmake --build build -j"$(nproc)"
```

## Branches and commits

- Branch from `main`. Name branches by purpose: `feature/<topic>`, `fix/<topic>`,
  `docs/<topic>`.
- Keep commits focused. Write the subject in the imperative ("Add crop box"), and
  explain *why* in the body when it is not obvious.

## Before opening a pull request

1. Build with no new warnings.
2. From the repository root, run `./build/strata3d_unit_tests`. Add unit tests in
   `tests/unit` for new rules in `src/core`.
3. For application or UI changes, run `./build/strata3d_ui_test` and try the change in
   the running app. Add a check to `tests/ui/ui_test.cpp` for new behaviour.
4. Update the documentation (`README.md`, `docs/`) when behaviour changes; regenerate
   screenshots with `./build/strata3d_ui_test --screenshots docs/assets/screenshots`
   if the UI looks different.
5. Fill in the pull request template.

## Coding guidelines

- Match the style of the file you are editing.
- Respect the layers (`src/core` has no Qt, see
  [docs/development.md](docs/development.md#layout-and-dependency-rules)); keep rules out
  of `MainWindow`; heavy work runs off the UI thread.
- Do not edit `3rdparty/`; it holds pinned git submodules.
- Style widgets through `src/ui/theme.cpp` (`role` properties), not per-widget style sheets.
- See the [platform pitfalls](docs/development.md#platform-pitfalls) (VTK on Wayland,
  OpenGL context, file-name case).

## Patient data

Never commit, attach or link real patient scans or anything derived from them. Use the
data in `example/` or the synthetic phantom (`./build/dental_phantom <dir>`) to reproduce
problems.

## Reporting bugs and ideas

Use the issue templates: [bug report](.github/ISSUE_TEMPLATE/bug_report.md) or
[feature request](.github/ISSUE_TEMPLATE/feature_request.md). For security problems see
[SECURITY.md](SECURITY.md).

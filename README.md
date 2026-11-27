# Strata3D

Strata3D turns CT and CBCT scans (DICOM series) into 3D surface models. Open a scan,
segment bone, teeth, a single tooth or the nerve canal on the slices, and export each
structure as its own STL file for 3D printing or planning.

Built with Qt 6, VTK 9 and DCMTK (C++17). Not cleared for clinical use.

## Features

- **Open** a folder with one CT or CBCT series: File ▸ Open, recent scans, or drag the
  folder onto the window. CT is read in Hounsfield units; uncalibrated CBCT is detected.
- **Bone / All teeth**: threshold with a live colored overlay on the slices. Starting
  values are 400 / 1500 HU for CT, or estimated from the histogram for CBCT.
- **Single tooth**: set the threshold, click the tooth; the region connected to the click
  is kept, with touching neighbours cut off.
- **Nerve**: click points along the canal on the slices; a tube of adjustable diameter is
  built through them (Backspace undo, Enter for the other side's canal).
- **3D view** with one colored mesh per structure, visibility per mesh.
- **Export STL**: `bone.stl`, `teeth.stl`, `tooth.stl`, `nerve.stl` in a timestamped
  folder, upright in the scan's patient axes.
- Dark and light themes, keyboard shortcuts (Help ▸ Keyboard Shortcuts), remembered
  window layout and recent scans.
- AI segmentation is planned (the button is in place).

## Build (Ubuntu 26.04)

```bash
sudo apt install build-essential cmake qt6-base-dev libvtk9-dev libvtk9-qt-dev \
                 libdcmtk-dev libvtk-dicom-dev libgl1-mesa-dev
cmake -S . -B build
cmake --build build -j$(nproc)
```

Other platforms need the same libraries: Qt 6 (Widgets, OpenGLWidgets, Xml, Concurrent),
VTK 9 built with Qt 6 support and vtk-dicom, and DCMTK ≥ 3.6. Set `QTDIR` / `VTK_DIR` if
CMake cannot find them.

## Run

```bash
./build/Strata3D                                         # empty window
./build/Strata3D example/downloads/tcga_thca_headneck_ct # open a scan directly
```

Example scans (a real head & neck CT and synthetic dental phantoms) are in `example/`;
see `example/README.md` for their license and citation. `./build/dental_phantom <dir>`
writes a new synthetic dental series.

## Command-line converter

`./build/dicom2mesh/dicom2mesh -i <dicom_folder> -o out.stl -t 400` converts a series
without the GUI (run it without arguments for all options).

## Credits

The mesh pipeline in `lib/` and `dicom2mesh/` is based on
[dicom2mesh](https://github.com/AOT-AG/DicomToMesh) by Adrian Schneider, AOT AG (MIT
License). DICOM is a registered trademark of NEMA.

# User guide

This guide walks through Strata3D from opening a scan to exporting STL files.

## 1. Open a scan

A scan is a folder that contains the slices of **one** DICOM series (uncompressed CT or
CBCT). Open it in any of these ways:

- File ▸ Open DICOM Folder… (Ctrl+O), or the button on the start screen
- File ▸ Open Recent, or a recent scan listed on the start screen
- Drag the folder (or one of its files) onto the window
- Command line: `./build/Strata3D <folder>`

The scan is read in the background. When it has loaded, the **Study** panel shows the
patient name, the volume size, the voxel spacing and whether the values are Hounsfield
units (CT) or uncalibrated gray values (typical for CBCT). The slice view opens on the
middle slice.

If no readable series is found, an error dialog says so. JPEG-compressed DICOM and
folders that mix several series are not supported yet.

## 2. Look around

**Slice view** (View ▸ Slices, F2):

| Action | Mouse / key |
|---|---|
| Next / previous slice | Mouse wheel, Up/Down keys, or the slider right of the view |
| Contrast (window/level) | Left-drag (when no clicking tool is active) |
| Zoom | Right-drag |
| Pan | Middle-drag |
| Reset view | View ▸ Reset View (Ctrl+0) |

The slices are shown in the radiological convention: viewed from the feet, front of the
patient at the top, patient's left on the right. The status bar shows the voxel under
the mouse and its value, which helps to choose thresholds.

**3D view** (View ▸ 3D, F3): left-drag rotates, right-drag or the wheel zooms,
middle-drag pans.

## 3. Segment a structure

Pick a structure in the sidebar (or Segment menu, Ctrl+1 … Ctrl+4). The hint box under
the slider always says what to do next and turns red when something is missing.

### Bone (Ctrl+1)

Move the **threshold** slider (or type a value) until the orange overlay covers the bone
on the slices, then **Create mesh** (Ctrl+Enter). On CT the starting value is 400 HU; on
CBCT it is estimated from the histogram.

Bone stops where the All teeth threshold starts, so bone and teeth meshes do not
overlap. Anything else dense enough (for example the scanner table or head rest) is
included too; a crop box is planned.

### All teeth (Ctrl+2)

Same as bone with the cyan overlay. The starting value is 1500 HU on CT. All teeth are
exported together in `teeth.stl`.

### Single tooth (Ctrl+3)

1. Set the threshold so the green overlay covers the teeth.
2. Click inside the tooth you want, on any slice where it is visible. A red marker shows
   the click.
3. Create mesh.

Only the region connected to the click is kept. Before growing the region, thin
contacts are cut (about 0.6 mm), so a neighbouring tooth that touches at a contact point
is usually not included. A root fused with bone at the same density can still be.
Clicking again replaces the selected tooth.

### Nerve (Ctrl+4)

The nerve canal has no distinct density, so it is traced by hand:

1. Scroll to a slice where the canal is visible and click on it. Repeat along the canal,
   on the slices where it runs. A yellow line shows the trace.
2. Set the **diameter** with the slider (1–6 mm, default 2.5 mm).
3. Create mesh. A smooth tube is built through the points.

| Key (with the view focused) / menu | Effect |
|---|---|
| Backspace, Edit ▸ Undo Nerve Point (Ctrl+Z) | Remove the last point |
| Enter, Edit ▸ Start New Nerve Canal (Ctrl+N) | Start the second canal (other side) |
| Esc, Edit ▸ Clear Nerve Trace | Remove all points |

## 4. Review the meshes

After **Create mesh** the 3D view opens. The **Meshes** list shows each mesh with its
color and the threshold or diameter it was built with.

- Checkbox: show or hide the mesh in 3D.
- Right-click: Hide/Show, Show Only This, Delete Mesh.
- Delete key: delete the selected mesh (asks first).

Creating a structure again replaces its mesh.

## 5. Export

File ▸ Export STL… (Ctrl+E) asks for a folder and writes a new subfolder
`<patient>_<yyyyMMdd_HHmmss>` with one binary STL per mesh:

| File | Structure |
|---|---|
| `bone.stl` | Bone |
| `teeth.stl` | All teeth |
| `tooth.stl` | Single tooth |
| `nerve.stl` | Nerve |

All files share one coordinate frame in millimetres, upright in the scan's patient
axes (x towards the patient's left, y towards the back, z up). They line up when
imported together. The origin is the corner of the scan volume, not the scanner's
patient position.

Until you export, the window title shows `*`. Opening another scan or quitting then
asks whether to export, discard or cancel.

## 6. Preferences

- View ▸ Theme: Dark (default), Light, or Follow System.
- View ▸ Full Screen (F11).
- Window size, panel width, theme, last folder and recent scans are remembered.

## Keyboard shortcuts

| Shortcut | Action |
|---|---|
| Ctrl+O | Open DICOM folder |
| Ctrl+E | Export STL |
| Ctrl+1 … Ctrl+4 | Bone · All teeth · Single tooth · Nerve |
| Ctrl+Enter | Create mesh |
| F2 / F3 | Slice view / 3D view |
| Ctrl+0 | Reset view |
| Ctrl+Z, Backspace | Undo nerve point |
| Ctrl+N, Enter | Start the other nerve canal |
| Esc | Clear nerve trace (in the view) |
| Delete | Delete selected mesh |
| F11 | Full screen |
| F1 | Keyboard shortcuts |
| Ctrl+Q | Quit |

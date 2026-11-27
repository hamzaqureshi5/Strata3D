# Security policy

## Reporting a vulnerability

Please do not report security problems in public issues. Report them privately to the
repository owner through GitHub (a private security advisory, if enabled for this
repository, or a direct message to the owner). Include the affected commit, steps to
reproduce and the impact you expect.

## Scope

Strata3D is a desktop application that reads local DICOM files. Relevant reports
include crashes or memory corruption triggered by crafted DICOM or STL files, and files
written outside the folder chosen for export.

## Patient data

Strata3D does not send data anywhere. Do not include real patient data in reports; use
the files in `example/` or a synthetic series from `tools/dental_phantom`.

## Supported versions

There are no releases yet; fixes go into `main`.

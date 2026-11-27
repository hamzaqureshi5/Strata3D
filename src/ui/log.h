#ifndef STRATA3D_UI_LOG_H
#define STRATA3D_UI_LOG_H

#include <QLoggingCategory>

// Logging categories; enable debug output per area with QT_LOGGING_RULES, e.g.
//   QT_LOGGING_RULES="strata3d.*.debug=true" ./build/Strata3D
Q_DECLARE_LOGGING_CATEGORY(lcLoad) // reading DICOM series
Q_DECLARE_LOGGING_CATEGORY(lcMesh) // building and exporting meshes
Q_DECLARE_LOGGING_CATEGORY(lcUi)   // user actions and status messages
Q_DECLARE_LOGGING_CATEGORY(lcVtk)  // messages from VTK itself

#endif // STRATA3D_UI_LOG_H

#ifndef STRATA3D_APP_LOGGING_H
#define STRATA3D_APP_LOGGING_H

// Sets a timestamped message format for all Qt logging and routes VTK's
// warnings and errors (normally printed straight to the terminal or a VTK
// window) into the strata3d.vtk category, so all diagnostics go through one
// channel. Call once at startup.
void installLogging();

#endif // STRATA3D_APP_LOGGING_H

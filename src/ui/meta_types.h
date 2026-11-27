#ifndef STRATA3D_UI_META_TYPES_H
#define STRATA3D_UI_META_TYPES_H

// Types carried by queued signals between the UI and worker threads. Include
// this before declaring a signal that uses them.
#include "core/mesh_builder.h"

#include <QMetaType>

#include <vtkImageData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

Q_DECLARE_METATYPE(vtkSmartPointer<vtkImageData>)
Q_DECLARE_METATYPE(vtkSmartPointer<vtkPolyData>)
Q_DECLARE_METATYPE(strata3d::MeshRequest)

#endif // STRATA3D_UI_META_TYPES_H

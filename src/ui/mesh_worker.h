/****************************************************************************
** Copyright (c) 2017 Adrian Schneider, AOT AG
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and associated documentation files (the "Software"),
** to deal in the Software without restriction, including without limitation
** the rights to use, copy, modify, merge, publish, distribute, sublicense,
** and/or sell copies of the Software, and to permit persons to whom the
** Software is furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
**
*****************************************************************************/

#ifndef STRATA3D_UI_MESH_WORKER_H
#define STRATA3D_UI_MESH_WORKER_H

#include "ui/meta_types.h"

#include <QObject>
#include <QString>

// Builds meshes on its own thread (move it with moveToThread). Wraps
// strata3d::buildMesh and reports through queued signals.
class MeshWorker : public QObject {
  Q_OBJECT

public:
  // Call once before connecting signals that carry these types
  static void registerMetaTypes();

public slots:
  // `volume` must not be modified while this runs; pass a shallow copy.
  void build(vtkSmartPointer<vtkImageData> volume,
             strata3d::MeshRequest request);

signals:
  void stageChanged(const QString &text);
  void progressChanged(double percent);
  // mesh is null and error set when nothing could be built
  void finished(int structureId, vtkSmartPointer<vtkPolyData> mesh,
                const QString &error);
};

#endif // STRATA3D_UI_MESH_WORKER_H

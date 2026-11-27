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

#include "ui/mesh_worker.h"

#include "ui/log.h"

void MeshWorker::registerMetaTypes() {
  qRegisterMetaType<vtkSmartPointer<vtkImageData>>();
  qRegisterMetaType<vtkSmartPointer<vtkPolyData>>();
  qRegisterMetaType<strata3d::MeshRequest>();
}

void MeshWorker::build(vtkSmartPointer<vtkImageData> volume,
                       strata3d::MeshRequest request) {
  qCInfo(lcMesh) << "building" << request.structureId << "from"
                 << request.range.lower
                 << (request.seeded ? "(seeded)" : "");
  strata3d::MeshCallbacks callbacks;
  callbacks.stage = [this](const std::string &text) {
    emit stageChanged(QString::fromStdString(text));
  };
  callbacks.progress = [this](double percent) { emit progressChanged(percent); };

  strata3d::MeshResult result = strata3d::buildMesh(volume, request, {}, callbacks);
  if (result.mesh)
    qCInfo(lcMesh) << "built" << request.structureId << "with"
                   << result.mesh->GetNumberOfCells() << "triangles";
  else
    qCWarning(lcMesh) << "no mesh for" << request.structureId << ":"
                      << result.error.c_str();
  emit finished(request.structureId, result.mesh,
                QString::fromStdString(result.error));
}

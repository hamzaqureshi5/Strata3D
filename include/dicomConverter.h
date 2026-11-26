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


#ifndef QDICOM_CONVERTER_H
#define QDICOM_CONVERTER_H

#include "dicomRoutines.h"
#include "meshRoutines.h"

#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkPolyData.h>
#include <vtkCallbackCommand.h>

#include <QObject>
#include <QMetaType>
#include <QString>

#include <memory>

Q_DECLARE_METATYPE(vtkSmartPointer<vtkImageData>)
Q_DECLARE_METATYPE(vtkSmartPointer<vtkPolyData>)

class DicomConverter_Listener
{
public:
    virtual void converterProgress(double progress) = 0;
};


// Lives on a worker thread; builds structure meshes from the loaded volume.
class DicomConverter : public QObject
{
Q_OBJECT

public:
    DicomConverter(DicomConverter_Listener* host);

    static void progressCallback(vtkObject* caller, long unsigned int eventId, void* clientData, void* callData);

public slots:
    // Iso-surface at `lower` (voxels >= upper are excluded when useUpper), then remove small
    // fragments, reduce and smooth. The mesh stays in scanner coordinates so structures line up.
    // `volume` must not be modified while this runs; pass a shallow copy.
    void buildStructureMesh(int structureId, vtkSmartPointer<vtkImageData> volume,
                            double lower, bool useUpper, double upper);
    // Single tooth: region connected to the seed voxel (see isolateRegion), then
    // the same surface and cleanup steps.
    void buildSeededMesh(int structureId, vtkSmartPointer<vtkImageData> volume,
                         double lower, int seedI, int seedJ, int seedK);

signals:
    void stageChanged(const QString& text);
    // mesh is null and error set when nothing could be built
    void structureMeshReady(int structureId, vtkSmartPointer<vtkPolyData> mesh, const QString& error);

private:
    void finishMesh(int structureId, vtkSmartPointer<vtkPolyData> mesh);

    DicomConverter_Listener* m_host;
    vtkSmartPointer<vtkCallbackCommand> m_progressCB;
    std::shared_ptr<VTKDicomRoutines> m_vdr;
    std::shared_ptr<VTKMeshRoutines> m_vmr;
};

#endif // QDICOM_CONVERTER_H

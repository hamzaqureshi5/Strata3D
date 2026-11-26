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

#include "dicomConverter.h"

#include "dicomFactory.h"
#include "segmentation.h"

#include <QDebug>
#include <vtkAlgorithm.h>

namespace {
    // Fragments smaller than this fraction of the largest connected part are dropped.
    const double kSmallObjectRatio = 0.02;
    const double kReductionRate = 0.5;
    const unsigned int kSmoothingIterations = 20;
    // Erosion depth that separates touching teeth before region growing.
    const double kSeparationMm = 0.6;
}

DicomConverter::DicomConverter(DicomConverter_Listener* host)
{
    m_host = host;
    m_progressCB = vtkSmartPointer<vtkCallbackCommand>::New();
    m_progressCB->SetCallback(progressCallback);
    m_progressCB->SetClientData(static_cast<void*>(m_host));

    m_vdr = VTKDicomFactory::getDicomRoutines();
    m_vdr->SetProgressCallback( m_progressCB );

    m_vmr = std::shared_ptr<VTKMeshRoutines>( new VTKMeshRoutines() );
    m_vmr->SetProgressCallback( m_progressCB );
}

void DicomConverter::progressCallback(vtkObject* caller, long unsigned int /*eventId*/, void* clientData, void* /*callData*/)
{
    vtkAlgorithm* filter = static_cast<vtkAlgorithm*>(caller);
    DicomConverter_Listener* host = static_cast<DicomConverter_Listener*>(clientData);
    host->converterProgress( filter->GetProgress() * 100);
}

void DicomConverter::buildStructureMesh(int structureId, vtkSmartPointer<vtkImageData> volume,
                                        double lower, bool useUpper, double upper)
{
    qDebug() << "buildStructureMesh" << structureId << "lower" << lower << "upper" << (useUpper ? upper : -1);

    emit stageChanged("Extracting surface");
    vtkSmartPointer<vtkPolyData> mesh = m_vdr->dicomToMesh( volume, static_cast<int>(lower), useUpper, static_cast<int>(upper) );
    if( mesh == nullptr || mesh->GetNumberOfCells() == 0 )
    {
        emit structureMeshReady(structureId, nullptr, "No surface found at this threshold");
        return;
    }

    finishMesh(structureId, mesh);
}

void DicomConverter::buildSeededMesh(int structureId, vtkSmartPointer<vtkImageData> volume,
                                     double lower, int seedI, int seedJ, int seedK)
{
    emit stageChanged("Growing region from the clicked point");
    const int seed[3] = { seedI, seedJ, seedK };
    QString error;
    vtkSmartPointer<vtkImageData> region = isolateRegion( volume, lower, seed, kSeparationMm, &error );
    if( region == nullptr )
    {
        emit structureMeshReady(structureId, nullptr, error);
        return;
    }

    emit stageChanged("Extracting surface");
    vtkSmartPointer<vtkPolyData> mesh = m_vdr->dicomToMesh( region, static_cast<int>(lower), false, 0 );
    if( mesh == nullptr || mesh->GetNumberOfCells() == 0 )
    {
        emit structureMeshReady(structureId, nullptr, "No surface found at this threshold");
        return;
    }
    finishMesh(structureId, mesh);
}

void DicomConverter::finishMesh(int structureId, vtkSmartPointer<vtkPolyData> mesh)
{
    emit stageChanged("Removing small fragments");
    m_vmr->removeSmallObjects( mesh, kSmallObjectRatio );

    emit stageChanged("Reducing mesh");
    m_vmr->meshReduction( mesh, kReductionRate );

    emit stageChanged("Smoothing mesh");
    m_vmr->smoothMesh( mesh, kSmoothingIterations );

    emit structureMeshReady(structureId, mesh, QString());
}

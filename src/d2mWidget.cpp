///****************************************************************************
//** Copyright (c) 2017 Adrian Schneider, AOT AG
//**
//** Permission is hereby granted, free of charge, to any person obtaining a
//** copy of this software and associated documentation files (the "Software"),
//** to deal in the Software without restriction, including without limitation
//** the rights to use, copy, modify, merge, publish, distribute, sublicense,
//** and/or sell copies of the Software, and to permit persons to whom the
//** Software is furnished to do so, subject to the following conditions:
//**
//** The above copyright notice and this permission notice shall be included in
//** all copies or substantial portions of the Software.
//**
//** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//** DEALINGS IN THE SOFTWARE.
//**
//*****************************************************************************/
//
//
//#include "MainWindow.h"
//#include "ui_MainWindow.h"
//#include "meshVisualizer.h"
//
//#include <QFileDialog>
//
//MainWindow::MainWindow(QWidget *parent) : QWidget(parent), ui(new Ui::MainWindow)
//{
//    ui->setupUi(this);
//
//    connect(ui->openBtn, SIGNAL(clicked()), this, SLOT(openDicomBtn()));
//    connect(ui->saveBtn, SIGNAL(clicked()), this, SLOT(saveBtn()));
//    connect(ui->runBtn, SIGNAL(clicked()), this, SLOT(runBtn()));
//    connect(this, &MainWindow::doUpdateProgress, this, &MainWindow::updateProgress);
//
//    // conversion thread
//    m_converter = new DicomConverter(this);
//    m_converter->moveToThread(&m_conversionThread);
//    connect(&m_conversionThread, &QThread::finished, m_converter, &QObject::deleteLater);
//
//    connect(this, &MainWindow::doLoad, m_converter, &DicomConverter::loadDicomImage);
//    connect(m_converter, &DicomConverter::loadDicomImage_Done, this, &MainWindow::load_done);
//
//
//    connect(this, &MainWindow::doCenter, m_converter, &DicomConverter::centerMesh);
//    connect(m_converter, &DicomConverter::centerMesh_Done, this, &MainWindow::center_done);
//
//    connect(this, &MainWindow::doReduction, m_converter, &DicomConverter::reduction);
//    connect(m_converter, &DicomConverter::reduction_Done, this, &MainWindow::reduction_done);
//
//    connect(this, &MainWindow::doFilter, m_converter, &DicomConverter::filtering);
//    connect(m_converter, &DicomConverter::filtering_Done, this, &MainWindow::filter_done);
//
//    connect(this, &MainWindow::doSmoothing, m_converter, &DicomConverter::smoothing);
//    connect(m_converter, &DicomConverter::smoothing_Done, this, &MainWindow::smoothing_done);
//
//    connect(this, &MainWindow::doExport, m_converter, &DicomConverter::exportMesh);
//    connect(m_converter, &DicomConverter::export_Done, this, &MainWindow::export_done);
//
//    m_conversionThread.start();
//
//    m_mesh_path = "";
//    m_dicom_path = "";
//}
//
//MainWindow::~MainWindow()
//{
//    delete ui;
//
//    m_conversionThread.quit();
//    m_conversionThread.wait();
//}
//
//void MainWindow::handleStartConversion()
//{
//    ui->runBtn->setEnabled(false);
//}
//
//void MainWindow::handleEndConversion()
//{
//    ui->runBtn->setEnabled(true);
//}
//
//// choose path to dicom
//void MainWindow::openDicomBtn()
//{
//    QString dicomPath = QFileDialog::getExistingDirectory(this, "Choose DICOM directory", ".", QFileDialog::ShowDirsOnly);
//    if(!dicomPath.isEmpty())
//    {
//        QString newInLablePath = "Input: ";
//        newInLablePath.append(dicomPath);
//        ui->inLable->setText(newInLablePath);
//
//        m_dicom_path = dicomPath;
// 
//    }
//}
//
//// choose mesh path
//void MainWindow::saveBtn()
//{
//    QString savePath = QFileDialog::getSaveFileName(this, "Save mesh as", ".");
//    if(!savePath.isEmpty())
//    {
//        QString newOutLable = "Output: ";
//        newOutLable.append(savePath);
//        ui->outLable->setText(newOutLable);
//
//        m_mesh_path = savePath;
//    }
//}
//
//// start conversion -> a cascade of functions will be asynchronous called
//void MainWindow::runBtn()
//{
//    handleStartConversion();
//
//    // Read Dicom images
//    ui->infoLable->setText("Read DICOM images");
//    emit doLoad( m_dicom_path, ui->thresholdSpinBox->value());
//
//    // after dicom images loaded, slot load_done is called
//}
//
//void MainWindow::load_done(bool ok)
//{
//    if( ok )
//    {
//        if( ui->centerCB->isChecked() )
//        {
//            ui->infoLable->setText("Center mesh");
//            emit doCenter();
//
//            // when done, center_done is called
//        }
//        else
//        {
//            // jump directly to center_done
//            center_done(true);
//        }
//    }
//    else
//    {
//        ui->infoLable->setText("Read DICOM failed");
//        handleEndConversion();
//    }
//}
//
//void MainWindow::center_done(bool ok)
//{
//    if( ok )
//    {
//        if( ui->reduceCB->isChecked() )
//        {
//            ui->infoLable->setText("Mesh reduction");
//            emit doReduction(ui->reduceSP->value());
//
//            // after reducing, reduction_done is called
//        }
//        else
//        {
//            // jump directly to center_done
//            reduction_done(true);
//        }
//    }
//    else
//    {
//        ui->infoLable->setText("Center mesh failed");
//        handleEndConversion();
//    }
//}
//
//void MainWindow::reduction_done(bool ok)
//{
//    if( ok )
//    {
//        if(ui->filterCB->isChecked())
//        {
//            ui->infoLable->setText("Filter small objects");
//            emit doFilter(ui->filterSP->value());
//
//            // after filtering, filter_done is called
//        }
//        else
//        {
//            // jump directly to center_done
//            filter_done(true);
//        }
//    }
//    else
//    {
//        ui->infoLable->setText("Reduction failed");
//        handleEndConversion();
//    }
//}
//
//void MainWindow::filter_done(bool ok)
//{
//    if( ok )
//    {
//        if( ui->smoothCB->isChecked() )
//        {
//            ui->infoLable->setText("Smoothing mesh");
//            emit doSmoothing();
//
//            // after smoothing, smoothing_done is called
//        }
//        else
//        {
//            // jump directly to center_done
//            smoothing_done(true);
//        }
//    }
//    else
//    {
//        ui->infoLable->setText("Filtering failed");
//        handleEndConversion();
//    }
//}
//
//void MainWindow::smoothing_done(bool ok)
//{
//    if( ok )
//    {
//        if( m_mesh_path != "" )
//        {
//            ui->infoLable->setText("Export mesh file");
//            emit doExport(m_mesh_path);
//
//            // after exporting, export_done cis called
//        }
//        else
//        {
//            export_done(true);
//        }
//    }
//    else
//    {
//        ui->infoLable->setText("Smoothing failed");
//        handleEndConversion();
//    }
//}
//
//void MainWindow::export_done(bool ok)
//{
//    if( ok )
//    {
//        ui->infoLable->setText("Visualize");
//        VTKMeshVisualizer::displayMesh( m_converter->getMesh() );
//        handleEndConversion();
//    }
//    else
//    {
//        ui->infoLable->setText("Export failed");
//        handleEndConversion();
//    }
//}
//
//void MainWindow::converterProgress(double progress)
//{
//    emit doUpdateProgress(progress);
//}
//
//void MainWindow::updateProgress(double progress)
//{
//    ui->progressBar->setValue(static_cast<int>(progress));
//}
//

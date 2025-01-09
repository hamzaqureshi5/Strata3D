#pragma once


#include "MainWindow.h"
#include <QtCore/QDir>
#include <QtCore/QDebug>
#include <vtkSmartPointer.h>
#include <vtkImageViewer2.h>
#include <vtkDICOMImageReader.h>
#include <vtkNamedColors.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkTextProperty.h>
#include <vtkTextMapper.h>
#include <vtkActor2D.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkImageAppend.h>
#include <vtkImageData.h>
#include <vtkPointData.h>


MainWindow::MainWindow(QWidget* parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow),
	mRenderWindow(vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New()),
	mRenderer(vtkSmartPointer<vtkRenderer>::New()),
	mInteractor(vtkSmartPointer<QVTKInteractor>::New()),
	mInteractorStyle(vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New()) // Use vtkInteractorStyleTrackballCamera for interaction
{
	ui->setupUi(this);
	showMaximized();

	/*QPixmap p(":/icons/pic/icons8-paint-brush-50.png");
	QSize q = ui->icon->size();
	QPixmap scaled = p.scaled(q, Qt::KeepAspectRatio);
	ui->icon->setIcon(QIcon(scaled));
	ui->icon->setIconSize(q);*/


	//connect(ui->openBtn, SIGNAL(clicked()), this, SLOT(openDicomBtn()));
	//connect(ui->saveBtn, SIGNAL(clicked()), this, SLOT(saveBtn()));
	// Signal-Slot connections for UI buttons
	connect(ui->createSTL, &QPushButton::clicked, this, &MainWindow::startConvProcess);
	connect(ui->exportSTL, &QPushButton::clicked, this, &MainWindow::startExportProcess);


	connect(this, &MainWindow::doUpdateProgress, this, &MainWindow::updateProgress);



	m_converter = new DicomConverter(this);
	m_converter->moveToThread(&m_conversionThread);
	connect(&m_conversionThread, &QThread::finished, m_converter, &QObject::deleteLater);

	connect(this, &MainWindow::doLoad, m_converter, &DicomConverter::loadDicomImage);
	connect(m_converter, &DicomConverter::loadDicomImage_Done, this, &MainWindow::load_done);

	connect(this, &MainWindow::doCenter, m_converter, &DicomConverter::centerMesh);
	connect(m_converter, &DicomConverter::centerMesh_Done, this, &MainWindow::center_done);

	connect(this, &MainWindow::doReduction, m_converter, &DicomConverter::reduction);
	connect(m_converter, &DicomConverter::reduction_Done, this, &MainWindow::reduction_done);

	connect(this, &MainWindow::doFilter, m_converter, &DicomConverter::filtering);
	connect(m_converter, &DicomConverter::filtering_Done, this, &MainWindow::filter_done);

	//    connect(this, &MainWindow::doSmoothing, m_converter, &DicomConverter::smoothing);
	//    connect(m_converter, &DicomConverter::smoothing_Done, this, &MainWindow::smoothing_done);


	//	  connect(this, &MainWindow::doExport, m_converter, &DicomConverter::exportMesh);
	//    connect(m_converter, &DicomConverter::export_Done, this, &MainWindow::export_done);


	connect(this, &MainWindow::doSmoothing, m_converter, &DicomConverter::smoothing);
	connect(m_converter, &DicomConverter::smoothing_Done, this, &MainWindow::smoothing_done);



	connect(this, &MainWindow::doExport, m_converter, &DicomConverter::exportMesh);
	connect(m_converter, &DicomConverter::export_Done, this, &MainWindow::export_done);

	m_conversionThread.start();

	m_mesh_path = "";
	m_dicom_path = "";



	mRenderWindow->AddRenderer(mRenderer);
	mRenderWindow->SetInteractor(mInteractor);
	ui->openGLWidget->setRenderWindow(mRenderWindow);
	mInteractor->SetInteractorStyle(mInteractorStyle); // Set the interactor style
	mInteractor->Initialize();
	// Set the background color 

	mRenderer->SetBackground(0, 0, 0);


	QObject::connect(ui->importDICOM, &QPushButton::clicked, this, &MainWindow::on_importDICOMButton_clicked);
	QObject::connect(ui->STL, &QPushButton::clicked, this, &MainWindow::show_loaded_stl);
	QObject::connect(ui->DICOM, &QPushButton::clicked, this, &MainWindow::show_loaded_dicom);


//	ui->progressBar->setHidden(true);

}



MainWindow::~MainWindow()
{
	delete ui;
	m_conversionThread.quit();
	m_conversionThread.wait();

}


void MainWindow::handleStartConversion()
{
//	ui->progressBar->setHidden(false);
	//    ui->runBtn->setEnabled(false);
}

void MainWindow::handleEndConversion()
{
//	ui->progressBar->setHidden(true);
	//    ui->runBtn->setEnabled(true);
}

// choose path to dicom
void MainWindow::openDicomBtn()
{
	QString dicomPath = QFileDialog::getExistingDirectory(this, "Choose DICOM directory", ".", QFileDialog::ShowDirsOnly);
	if (!dicomPath.isEmpty())
	{
		QString newInLablePath = "Input: ";
		newInLablePath.append(dicomPath);
		//        ui->inLable->setText(newInLablePath);

		m_dicom_path = dicomPath;
	}
}

// choose mesh path
void MainWindow::saveBtn()
{
	QString savePath = QFileDialog::getSaveFileName(this, "Save mesh as", ".");
	if (!savePath.isEmpty())
	{
		QString newOutLable = "Output: ";
		newOutLable.append(savePath);
		//        ui->outLable->setText(newOutLable);

		m_mesh_path = savePath;
	}
}

//// start conversion -> a cascade of functions will be asynchronous called
void MainWindow::startConvProcess()
{
	handleStartConversion();
	//	m_dicom_path = "C:/Users/DELL/Downloads/dicomsegmentation_v7/example/dicom_example";
	//	m_dicom_path = "C:/Users/DELL/Downloads/dicomsegmentation_v7/example/MULTIFRAME";
	//	m_mesh_path = "C:/Users/DELL/Downloads/dicomsegmentation_v7/example/output.stl";

		// Read Dicom images
	ui->infoLable->setText("Read DICOM images");
	//    emit doLoad( m_dicom_path, ui->thresholdSpinBox->value());
	emit doLoad(m_dicom_path, 400);

	// after dicom images loaded, slot load_done is called

	qDebug() << "-----------------------------------> RUN BTN is Called()";

	handleEndConversion();

}

void MainWindow::load_dicom()
{
	handleStartConversion();

	// Read Dicom images
	ui->infoLable->setText("Read DICOM images");
	//    emit doLoad( m_dicom_path, ui->thresholdSpinBox->value());
	emit doLoad(m_dicom_path, 400);


	handleEndConversion();

}

void MainWindow::load_done(bool ok)
{

	qDebug() << "-----------------------------------> load_done is Called() P: " << ok;

	if (ok)
	{
		handleStartConversion();

		//        if( ui->centerCB->isChecked() )
		if (true)
		{
			ui->infoLable->setText("Center mesh");
			emit doCenter();

			// when done, center_done is called
		}
		else
		{
			// jump directly to center_done
			center_done(true);
		}
	}
	else
	{
		//        ui->infoLable->setText("Read DICOM failed");
		handleEndConversion();
	}
}

void MainWindow::center_done(bool ok)
{

	qDebug() << "-----------------------------------> center_done is Called()";

	if (ok)
	{
		handleStartConversion();

		if (true)

			//        if( ui->reduceCB->isChecked() )
		{
			ui->infoLable->setText("Mesh reduction");
			emit doReduction(0.50);

			// after reducing, reduction_done is called
		}
		else
		{
			// jump directly to center_done
			reduction_done(true);
		}
	}
	else
	{
        ui->infoLable->setText("Center mesh failed");
		handleEndConversion();
	}
}

void MainWindow::reduction_done(bool ok)
{
	qDebug() << "-----------------------------------> reduction_done is Called()";

	if (ok)
	{
		handleStartConversion();

		//        if(ui->filterCB->isChecked())
		if (false)

		{
			ui->infoLable->setText("Filter small objects");
			//            emit doFilter(ui->filterSP->value());
			emit doFilter(0.0050);

			// after filtering, filter_done is called
		}
		else
		{
			// jump directly to center_done
			filter_done(true);
		}
	}
	else
	{
		ui->infoLable->setText("Reduction failed");
		handleEndConversion();
	}
}

void MainWindow::filter_done(bool ok)
{
	qDebug() << "-----------------------------------> filter_done is Called()";

	if (ok)
	{
		handleStartConversion();

		//		if (ui->smoothCB->isChecked())
		if (true)
		{
			ui->infoLable->setText("Smoothing mesh");
			emit doSmoothing();

			// after smoothing, smoothing_done is called
		}
		else
		{
			// jump directly to center_done
			smoothing_done(true);
		}
	}
	else
	{
		ui->infoLable->setText("Filtering failed");
		handleEndConversion();
	}
}


void MainWindow::smoothing_done(bool ok)
{
	qDebug() << "-----------------------------------> smoothing_done is Called() P:" << ok;

	if (ok)

	{
		handleStartConversion();

		ui->infoLable->setText("Visualize");

		//VTKMeshVisualizer::displayMesh( m_converter->getMesh() );

		globalPolyData = m_converter->getMesh();  // Save the loaded STL data to the global variable


		vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
		//mapper->SetInputConnection(reader->GetOutputPort());
		mapper->SetInputData(globalPolyData);  // Use SetInputData to directly set the polydata

		vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
		actor->SetMapper(mapper);

		vtkSmartPointer<vtkNamedColors> colors = vtkSmartPointer<vtkNamedColors>::New();
		actor->GetProperty()->SetColor(colors->GetColor3d("LightSteelBlue").GetData());

		ui->openGLWidget->setRenderWindow(mRenderWindow);

		mRenderer->AddActor(actor);
		mRenderer->SetBackground(colors->GetColor3d("Black").GetData());
		mRenderer->ResetCamera();

		mRenderWindow->Render();


		//		m_mesh_path = "C:/Users/DELL/Downloads/dicomsegmentation_v6/example/output1.stl";

		   //     if( m_mesh_path != "" )
		   //     {
		   //        ui->infoLable->setText("Export mesh file");

					//qDebug() << "-----------------------------------> doExport is emitted";

		   //         emit doExport(m_mesh_path);

		   //         // after exporting, export_done cis called
		   //     }
		   //     else
		   //     {
		   //         export_done(true);
		   //     }
	}
	else
	{
		ui->infoLable->setText("Smoothing failed");
		handleEndConversion();
	}
}

void MainWindow::startExportProcess()
{
	//		QString exportDirPath = QFileDialog::getSaveFileName(this, "Save mesh as", ".stl");
	//		if (!exportDirPath.isEmpty()) {
	QString exportDirPath = QFileDialog::getExistingDirectory(this, "Select Directory to Save STL File");
	if (!exportDirPath.isEmpty())
	{

		qDebug() << "-----------------------------------> smoothing_done is Called()";

		//	if (m_mesh_path != "")
		if (1)
		{
			ui->infoLable->setText("Exporting 3D Model");

			{
				// Get the current date
				QDateTime currentDate = QDateTime::currentDateTime();
				QString dateString = currentDate.toString("yyyyMMdd");

				exportDirPath += "/ExportedSTL_" + dateString;
				QDir exportDir;
				if (!exportDir.exists(exportDirPath))
				{
					exportDir.mkpath(exportDirPath);
				}

				// File path for the exported STL
				QString exportFilePath = exportDirPath + "/exported_model.stl";

				m_mesh_path = exportFilePath;

			}

			qDebug() << "-----------------------------------> doExport is emitted";

			if (m_mesh_path != "")
			{
				ui->infoLable->setText("Export mesh file");

				qDebug() << "-----------------------------------> doExport is emitted";

				emit doExport(m_mesh_path);

				// after exporting, export_done cis called
			}
			else
			{
				export_done(true);
			}
			//emit doExport(m_mesh_path);

			// after exporting, export_done cis called
		}
		else
		{
			export_done(true);
		}
	}
}

void MainWindow::export_done(bool ok)
{
	qDebug() << "-----------------------------------> export_done is Called()";

	if (ok)
	{
		handleStartConversion();

		ui->infoLable->setText("Exported 3D Model");

		//ui->infoLable->setText("Visualize");

		////VTKMeshVisualizer::displayMesh( m_converter->getMesh() );

		//globalPolyData = m_converter->getMesh();  // Save the loaded STL data to the global variable


		//vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
		////mapper->SetInputConnection(reader->GetOutputPort());
		//mapper->SetInputData(globalPolyData);  // Use SetInputData to directly set the polydata

		//vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
		//actor->SetMapper(mapper);

		//vtkSmartPointer<vtkNamedColors> colors = vtkSmartPointer<vtkNamedColors>::New();
		//actor->GetProperty()->SetColor(colors->GetColor3d("LightSteelBlue").GetData());

		//ui->openGLWidget->setRenderWindow(mRenderWindow);

		//mRenderer->AddActor(actor);
		//mRenderer->SetBackground(colors->GetColor3d("Black").GetData());
		//mRenderer->ResetCamera();

		//mRenderWindow->Render();


		handleEndConversion();
	}
	else
	{
		ui->infoLable->setText("Export failed");
		handleEndConversion();
	}
}

void MainWindow::converterProgress(double progress)
{
	emit doUpdateProgress(progress);
}


void MainWindow::updateProgress(double progress)
{
	ui->progressBar->setValue(static_cast<int>(progress));
}

void MainWindow::on_importDICOMButton_clicked()
{
	// Select the DICOM folder
	QString folderName = QFileDialog::getExistingDirectory(this, "Select DICOM Directory", "");
	if (folderName.isEmpty())
		return;

	m_dicom_path = folderName;
	//m_mesh_path = "stl.stl";

	loadDicom(m_dicom_path);
	showDicom();
	//	show_loaded_dicom();
}

//void MainWindow::loadDicom(const QString& folderName)
//{
//	// Read the DICOM files from the specified directory
//	vtkSmartPointer<vtkDICOMImageReader> reader = vtkSmartPointer<vtkDICOMImageReader>::New();
//	reader->SetDirectoryName(folderName.toStdString().c_str());
//	reader->Update();
//
//	qDebug() << "-----------------------------------> Patient Name: " << QString(reader->GetPatientName());
//
//	// Create an image viewer to display the DICOM images
//	imageViewer = vtkSmartPointer<vtkImageViewer2>::New();
//	imageViewer->SetInputConnection(reader->GetOutputPort());
//}



void MainWindow::loadDicom(const QString& folderName)
{
	// Read the DICOM files from the specified directory
	vtkSmartPointer<vtkDICOMImageReader> reader = vtkSmartPointer<vtkDICOMImageReader>::New();
	reader->SetDirectoryName(folderName.toStdString().c_str());
	reader->Update();

	qDebug() << "-----------------------------------> Patient Name: " << QString(reader->GetPatientName());
	ui->patientname->setText(reader->GetPatientName());
	// Create an image viewer to display the DICOM images
	imageViewer = vtkSmartPointer<vtkImageViewer2>::New();
	imageViewer->SetInputConnection(reader->GetOutputPort());
}


void MainWindow::showDicom()
{
	// Check if DICOM is loaded
	if (!imageViewer)
	{
		qDebug() << "No DICOM data loaded.";
		ui->infoLable->setText("No DICOM data loaded.");
		return;
	}

	vtkSmartPointer<vtkNamedColors> colors = vtkSmartPointer<vtkNamedColors>::New();

	// Create and configure a text property for displaying slice information
	vtkNew<vtkTextProperty> sliceTextProp;
	sliceTextProp->SetFontFamilyToCourier();
	sliceTextProp->SetFontSize(20);
	sliceTextProp->SetVerticalJustificationToBottom();
	sliceTextProp->SetJustificationToLeft();

	// Create a text mapper to manage the slice information text
	vtkNew<vtkTextMapper> sliceTextMapper;
	std::string msg = StatusMessage::Format(imageViewer->GetSliceMin(), imageViewer->GetSliceMax());
	sliceTextMapper->SetInput(msg.c_str());
	sliceTextMapper->SetTextProperty(sliceTextProp);

	// Create an actor to display the slice information text
	vtkNew<vtkActor2D> sliceTextActor;
	sliceTextActor->SetMapper(sliceTextMapper);
	sliceTextActor->SetPosition(15, 10);

	// Create and configure a text property for usage instructions
	vtkNew<vtkTextProperty> usageTextProp;
	usageTextProp->SetFontFamilyToCourier();
	usageTextProp->SetFontSize(14);
	usageTextProp->SetVerticalJustificationToTop();
	usageTextProp->SetJustificationToLeft();

	// Create a text mapper to manage the usage instructions
	vtkNew<vtkTextMapper> usageTextMapper;
	usageTextMapper->SetInput(
		"- Slice with mouse wheel or Up/Down-Key\n- Zoom with pressed right mouse button while dragging");
	usageTextMapper->SetTextProperty(usageTextProp);

	// Create an actor to display the usage instructions
	vtkNew<vtkActor2D> usageTextActor;
	usageTextActor->SetMapper(usageTextMapper);
	usageTextActor->GetPositionCoordinate()->SetCoordinateSystemToNormalizedDisplay();
	usageTextActor->GetPositionCoordinate()->SetValue(0.05, 0.95);

	vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
	vtkNew<myVtkInteractorStyleImage> style;

	// Set up the render window and renderer
	vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
	vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
	renderWindow->AddRenderer(renderer);
	ui->openGLWidget->setRenderWindow(renderWindow);
	imageViewer->SetRenderWindow(renderWindow);

	ui->infoLable->setText("DICOM loaded successfully");

	style->SetInteractionModeToImageSlicing();
	style->SetImageViewer(imageViewer);
	style->SetStatusMapper(sliceTextMapper);

	imageViewer->SetupInteractor(renderWindowInteractor);
	renderWindowInteractor->SetInteractorStyle(style);

	imageViewer->GetRenderer()->AddActor2D(sliceTextActor);
	imageViewer->GetRenderer()->AddActor2D(usageTextActor);

	// Render and start interaction
	imageViewer->Render();
	imageViewer->GetRenderer()->ResetCamera();
	imageViewer->Render();
	renderWindowInteractor->Start();
}



void MainWindow::on_importSTLButton_clicked()
{


	QString fileName = QFileDialog::getOpenFileName(this, "Open STL File", "", "STL Files (*.stl)");
	if (fileName.isEmpty())
	{
		return;
	}

	//	QString fileName = "C:/Users/DELL/Downloads/Cube_3d_printing_sample.stl";

			//
	vtkSmartPointer<vtkSTLReader> reader = vtkSmartPointer<vtkSTLReader>::New();
	reader->SetFileName(fileName.toStdString().c_str());
	reader->Update();

	globalPolyData = reader->GetOutput();  // Save the loaded STL data to the global variable


	vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
	mapper->SetInputConnection(reader->GetOutputPort());

	vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
	actor->SetMapper(mapper);

	vtkSmartPointer<vtkNamedColors> colors = vtkSmartPointer<vtkNamedColors>::New();
	actor->GetProperty()->SetColor(colors->GetColor3d("LightSteelBlue").GetData());

	ui->openGLWidget->setRenderWindow(mRenderWindow);

	mRenderer->AddActor(actor);
	mRenderer->SetBackground(colors->GetColor3d("Black").GetData());
	mRenderer->ResetCamera();

	mRenderWindow->Render();


}

void MainWindow::show_loaded_dicom()
{

	// Check if DICOM is loaded
	if (!imageViewer)
	{
		qDebug() << "No DICOM data loaded.";
		ui->infoLable->setText("No DICOM data loaded.");
		return;
	}

	//	QString folderName = "C:/Users/DELL/Desktop/ReadDimcom/build/Release/digest_article";

	vtkSmartPointer<vtkNamedColors> colors = vtkSmartPointer<vtkNamedColors>::New();

	// Read the DICOM files from the specified directory
	//vtkSmartPointer<vtkDICOMImageReader> reader = vtkSmartPointer<vtkDICOMImageReader>::New();
	//reader->SetDirectoryName(folderName.toStdString().c_str());
	//reader->Update();

	//qDebug() << "----------------------------------->" << QString(reader->GetPatientName());

	// Create an image viewer to display the DICOM images
	//vtkSmartPointer<vtkImageViewer2> imageViewer = vtkSmartPointer<vtkImageViewer2>::New();
//	imageViewer = vtkSmartPointer<vtkImageViewer2>::New();
//	imageViewer->SetInputConnection(reader->GetOutputPort());


	// Create and configure a text property for displaying slice information
	vtkNew<vtkTextProperty> sliceTextProp;
	sliceTextProp->SetFontFamilyToCourier();
	sliceTextProp->SetFontSize(20);
	sliceTextProp->SetVerticalJustificationToBottom();
	sliceTextProp->SetJustificationToLeft();

	// Create a text mapper to manage the slice information text
	vtkNew<vtkTextMapper> sliceTextMapper;
	std::string msg = StatusMessage::Format(imageViewer->GetSliceMin(), imageViewer->GetSliceMax());
	sliceTextMapper->SetInput(msg.c_str());
	sliceTextMapper->SetTextProperty(sliceTextProp);

	// Create an actor to display the slice information text
	vtkNew<vtkActor2D> sliceTextActor;
	sliceTextActor->SetMapper(sliceTextMapper);
	sliceTextActor->SetPosition(15, 10);

	// Create and configure a text property for usage instructions
	vtkNew<vtkTextProperty> usageTextProp;
	usageTextProp->SetFontFamilyToCourier();
	usageTextProp->SetFontSize(14);
	usageTextProp->SetVerticalJustificationToTop();
	usageTextProp->SetJustificationToLeft();

	// Create a text mapper to manage the usage instructions
	// 
	// 
	vtkNew<vtkTextMapper> usageTextMapper;
	usageTextMapper->SetInput(
		"- Slice with mouse wheel or Up/Down-Key\n- Zoom with pressed right mouse button while dragging");
	usageTextMapper->SetTextProperty(usageTextProp);

	// Create an actor to display the usage instructions

	vtkNew<vtkActor2D> usageTextActor;

	usageTextActor->SetMapper(usageTextMapper);
	usageTextActor->GetPositionCoordinate()->SetCoordinateSystemToNormalizedDisplay();
	usageTextActor->GetPositionCoordinate()->SetValue(0.05, 0.95);



	vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
	vtkNew<myVtkInteractorStyleImage> style /*= vtkSmartPointer<myVtkInteractorStyleImage>::New()*/;


	// Set up the render window and renderer
	vtkSmartPointer<vtkGenericOpenGLRenderWindow> renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
	vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
	renderWindow->AddRenderer(renderer);
	ui->openGLWidget->setRenderWindow(renderWindow);
	imageViewer->SetRenderWindow(renderWindow);


	style->SetInteractionModeToImageSlicing();
	style->SetImageViewer(imageViewer);
	style->SetStatusMapper(sliceTextMapper);

	//	renderWindowInteractor->SetInteractorStyle(style);
	imageViewer->SetupInteractor(renderWindowInteractor);


	renderWindowInteractor->SetInteractorStyle(style);

	imageViewer->GetRenderer()->AddActor2D(sliceTextActor);
	imageViewer->GetRenderer()->AddActor2D(usageTextActor);
	// Create the render window interactor and set the custom interaction style



	// Render and start interaction
	imageViewer->Render();
	imageViewer->GetRenderer()->ResetCamera();
	imageViewer->Render();
	renderWindowInteractor->Start();

}


void MainWindow::show_loaded_stl()
{
	if (!globalPolyData)
	{
		ui->infoLable->setText("No STL data loaded.");
		return;
	}


	vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
	mapper->SetInputData(globalPolyData);  // Use SetInputData to directly set the polydata

	vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
	actor->SetMapper(mapper);

	vtkSmartPointer<vtkNamedColors> colors = vtkSmartPointer<vtkNamedColors>::New();
	actor->GetProperty()->SetColor(colors->GetColor3d("LightSteelBlue").GetData());

	ui->openGLWidget->setRenderWindow(mRenderWindow);

	mRenderer->AddActor(actor);
	mRenderer->SetBackground(colors->GetColor3d("Black").GetData());
	mRenderer->ResetCamera();

	mRenderWindow->Render();
}



void MainWindow::exportSTLFile()
{
	if (!globalPolyData)
	{
		qDebug() << "No STL data loaded.";
		return;
	}

	// Get the directory where the user wants to save the STL file
	QString exportDirPath = QFileDialog::getExistingDirectory(this, "Select Directory to Save STL File");
	if (exportDirPath.isEmpty())
	{
		return;
	}

	// Get the file name with .stl extension
	QString fileName = QFileDialog::getSaveFileName(this, "Save STL File", exportDirPath + "/output.stl", "STL Files (*.stl)");
	if (fileName.isEmpty())
	{
		return;
	}

	// Ensure the file name ends with .stl
	if (!fileName.endsWith(".stl", Qt::CaseInsensitive))
	{
		fileName += ".stl";
	}

	// Proceed with exporting the STL file
	ui->infoLable->setText("Exporting STL file: " + fileName);
	emit doExport(fileName);

	qDebug() << "Exporting STL file to: " << fileName;
}

//void MainWindow::exportSTLFile()
//{
//	if (!globalPolyData)
//	{
//		qDebug() << "No STL data loaded.";
//		return;
//	}
//
//	// Get the current date
//	QDateTime currentDate = QDateTime::currentDateTime();
//	QString dateString = currentDate.toString("yyyyMMdd");
//
//	// Create a directory with the current date
//	QString exportDirPath = QFileDialog::getExistingDirectory(this, "Select Directory to Save STL File");
//	if (exportDirPath.isEmpty())
//	{
//		return;
//	}
//
//	exportDirPath += "/ExportedSTL_" + dateString;
//	QDir exportDir;
//	if (!exportDir.exists(exportDirPath))
//	{
//		exportDir.mkpath(exportDirPath);
//	}
//
//	// File path for the exported STL
//	QString exportFilePath = exportDirPath + "/exported_model.stl";
//
//	// Write the current polydata to an STL file
//	vtkSmartPointer<vtkSTLWriter> writer = vtkSmartPointer<vtkSTLWriter>::New();
//	writer->SetFileName(exportFilePath.toStdString().c_str());
//	writer->SetInputData(globalPolyData);
//	writer->Write();
//
//	qDebug() << "STL file successfully exported to:" << exportFilePath;
//}
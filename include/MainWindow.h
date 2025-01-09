#pragma once
#ifndef MAINWINDOW_H
#define MAINWINDOW_H


#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>
#include <QVTKInteractor.h>
#include <vtkInteractorStyle.h>
#include <vtkNamedColors.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkActor2D.h>
#include <vtkDICOMReader.h>
#include <vtkDICOMImageReader.h>
#include <vtkImageViewer2.h>
#include <vtkInteractorStyleImage.h>
#include <vtkTextMapper.h>
#include <vtkTextProperty.h>
#include <vtkCallbackCommand.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkImageMapper3D.h>
#include <vtkImageActor.h>
#include <vtkImageViewer2.h>
#include <vtkProperty.h>
#include <vtkSTLReader.h>
#include <vtkSTLWriter.h>
#include <QVTKOpenGLNativeWidget.h>
//#include "meshVisualizer.h"

//#include <itkImage.h>


#include "ui_MainWindow.h"
#include "dicomConverter.h"
#include "dcm_interactor.h"

#include <QMainWindow>
#include <QWidget>
#include <QThread>
#include <QString>
#include <QDebug>
#include <QPixmap>
#include <QFile>
#include <QGraphicsView>
#include <QImage>
#include <QByteArray>
#include <QFileDialog>
#include <QGraphicsScene>
#include <QDateTime>

#include <sstream>

//#include "dcmtk/dcmjpeg/djcodece.h"


namespace Ui {
	class MainWindow;
}


class MainWindow : public QMainWindow , public DicomConverter_Listener
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget* parent = nullptr);
	~MainWindow();

private slots:
	void on_importDICOMButton_clicked();
	void on_importSTLButton_clicked();
	void exportSTLFile();
	void show_loaded_stl();
	void show_loaded_dicom();
	void loadDicom(const QString& folderName);
	void showDicom();



public:

	// from DicomConverter_Listener
	void converterProgress(double progress);

public slots:
	void openDicomBtn();
	void saveBtn();
	void startConvProcess();
	void updateProgress(double progress);
	void load_dicom();

	void load_done(bool ok);
	void center_done(bool ok);
	void reduction_done(bool ok);
	void filter_done(bool ok);
	void smoothing_done(bool ok);

	void startExportProcess( );
	void export_done(bool ok);

signals:
	void doLoad(const QString& pathToDicom, int threshold);
	void doCenter();
	void doReduction(double reductionRate);
	void doFilter(double filterRate);
	void doSmoothing();
	void doExport(const QString& path);

	void doUpdateProgress(double progress);


private:
	void handleStartConversion();
	void handleEndConversion();

private:
	Ui::MainWindow* ui;
	vtkSmartPointer<vtkInteractorStyleTrackballCamera> mInteractorStyle;
	vtkSmartPointer<vtkGenericOpenGLRenderWindow> mRenderWindow;
	vtkSmartPointer<vtkImageViewer2> imageViewer;
	vtkSmartPointer<QVTKInteractor> mInteractor;
	vtkSmartPointer<vtkRenderer> mRenderer;

	vtkSmartPointer<vtkActor> actor;
	vtkSmartPointer<vtkPolyData> globalPolyData /*= vtkSmartPointer<vtkPolyData>::New()*/;

	QString m_dicom_path;
	QString m_mesh_path;
	vtkSmartPointer<vtkCallbackCommand> m_progressCB /*= vtkSmartPointer<vtkCallbackCommand>::New()*/;
	QThread m_conversionThread;
	DicomConverter* m_converter;

};





//  namespace {
//
//	// Helper class to format slice status message.
//	class StatusMessage
//	{
//	public:
//		static std::string Format(int slice, int maxSlice)
//		{
//			std::stringstream tmp;
//			tmp << "Slice Number  " << slice + 1 << "/" << maxSlice + 1;
//			return tmp.str();
//		}
//	};
//
//	// Define own interaction style.
//	class myVtkInteractorStyleImage : public vtkInteractorStyleImage
//	{
//	public:
//		static myVtkInteractorStyleImage* New();
//		vtkTypeMacro(myVtkInteractorStyleImage, vtkInteractorStyleImage);
//
//	protected:
//		vtkImageViewer2* _ImageViewer;
//		vtkTextMapper* _StatusMapper;
//		int _Slice;
//		int _MinSlice;
//		int _MaxSlice;
//
//	public:
//		void SetImageViewer(vtkImageViewer2* imageViewer)
//		{
//			_ImageViewer = imageViewer;
//			_MinSlice = imageViewer->GetSliceMin();
//			_MaxSlice = imageViewer->GetSliceMax();
//			_Slice = _MinSlice;
//			cout << "Slicer: Min = " << _MinSlice << ", Max = " << _MaxSlice << std::endl;
//		}
//
//		void SetStatusMapper(vtkTextMapper* statusMapper)
//		{
//			_StatusMapper = statusMapper;
//		}
//
//	protected:
//		void MoveSliceForward()
//		{
//			if (_Slice < _MaxSlice)
//			{
//				_Slice += 1;
//				cout << "MoveSliceForward::Slice = " << _Slice << std::endl;
//				_ImageViewer->SetSlice(_Slice);
//				std::string msg = StatusMessage::Format(_Slice, _MaxSlice);
//				_StatusMapper->SetInput(msg.c_str());
//				_ImageViewer->Render();
//			}
//		}
//
//		void MoveSliceBackward()
//		{
//			if (_Slice > _MinSlice)
//			{
//				_Slice -= 1;
//				cout << "MoveSliceBackward::Slice = " << _Slice << std::endl;
//				_ImageViewer->SetSlice(_Slice);
//				std::string msg = StatusMessage::Format(_Slice, _MaxSlice);
//				_StatusMapper->SetInput(msg.c_str());
//				_ImageViewer->Render();
//
//			}
//		}
//
//		virtual void OnKeyDown()
//		{
//			std::string key = this->GetInteractor()->GetKeySym();
//			if (key.compare("Up") == 0)
//			{
//				// cout << "Up arrow key was pressed." << endl;
//				MoveSliceForward();
//			}
//			else if (key.compare("Down") == 0)
//			{
//				// cout << "Down arrow key was pressed." << endl;
//				MoveSliceBackward();
//			}
//			// forward event
//			vtkInteractorStyleImage::OnKeyDown();
//		}
//
//		virtual void OnMouseWheelForward()
//		{
//			// std::cout << "Scrolled mouse wheel forward." << std::endl;
//			MoveSliceForward();
//			// don't forward events, otherwise the image will be zoomed
//			// in case another interactorstyle is used (e.g. trackballstyle, ...)
//			// vtkInteractorStyleImage::OnMouseWheelForward();
//		}
//
//		virtual void OnMouseWheelBackward()
//		{
//			// std::cout << "Scrolled mouse wheel backward." << std::endl;
//			if (_Slice > _MinSlice)
//			{
//				MoveSliceBackward();
//			}
//			// don't forward events, otherwise the image will be zoomed
//			// in case another interactorstyle is used (e.g. trackballstyle, ...)
//			// vtkInteractorStyleImage::OnMouseWheelBackward();
//		}
//	};
//
//	vtkStandardNewMacro(myVtkInteractorStyleImage);
//
//} // namespace

#endif // MAINWINDOW_H

#pragma once
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QVTKInteractor.h>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkActor.h>
#include <vtkCallbackCommand.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkImageActor.h>
#include <vtkImageData.h>
#include <vtkImageThreshold.h>
#include <vtkImageViewer2.h>
#include <vtkInteractorStyleImage.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkLookupTable.h>
#include <vtkPolyData.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkTextMapper.h>

#include "dicomConverter.h"
#include "segmentation.h"
#include "ui_MainWindow.h"

#include <QMainWindow>
#include <QString>
#include <QThread>

#include <array>

class QButtonGroup;
class QStandardItem;
class QStandardItemModel;

namespace Ui {
class MainWindow;
}

// Non-AI workflow: Import DICOM -> pick a structure -> adjust it on the slices
// -> CREATE STL (one mesh per structure) -> EXPORT STL (one file each).
//   BONE / All TEETH: threshold with a live overlay
//   TOOTH: threshold + click the tooth (region grown from the click)
//   NERVE: click points along the canal; tube of the slider's diameter
class MainWindow : public QMainWindow, public DicomConverter_Listener {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

  // Loads a DICOM series folder and shows it in the slice view.
  bool loadDicomFolder(const QString &folder);
  // Writes one STL per created structure into a new timestamped folder inside
  // `dir`. Returns that folder, or an empty string on failure.
  QString exportStructures(const QString &dir);

  // What a left click on the slice view does in TOOTH / NERVE mode, by voxel
  // index (k is the slice) or patient position.
  void setToothSeed(int i, int j, int k);
  void addNervePoint(const Point3 &position);

  // from DicomConverter_Listener (called on the worker thread)
  void converterProgress(double progress) override;

signals:
  void doBuildStructureMesh(int structureId,
                            vtkSmartPointer<vtkImageData> volume, double lower,
                            bool useUpper, double upper);
  void doBuildSeededMesh(int structureId, vtkSmartPointer<vtkImageData> volume,
                         double lower, int seedI, int seedJ, int seedK);
  void doUpdateProgress(double progress);

private slots:
  void importDicom();
  void selectStructure(int structureId);
  void thresholdChanged(int value);
  void sliceSliderChanged(int value);
  void createStl();
  void exportStl();
  void showSliceView();
  void show3DView();
  void structureMeshReady(int structureId, vtkSmartPointer<vtkPolyData> mesh,
                          const QString &error);
  void structureItemChanged(QStandardItem *item);
  void updateProgress(double progress);

private:
  void createSliceView();
  void configureSlider();
  void updateAnnotations();
  void onSliceClick();
  bool onSliceKey(const std::string &key);
  QString nerveHelp() const;
  void updateOverlay();
  void syncSliceUi();
  void refreshStructureList();
  void setBusy(bool busy);
  void setStatus(const QString &text);
  QString thresholdText(double value) const;
  // threshold, or diameter for the nerve
  QString settingText(int structureId, double value) const;
  // Range [lower, upper) of a structure; upper is open-ended unless hasUpper.
  void structureRange(int structureId, double &lower, bool &hasUpper,
                      double &upper) const;
  static void onSliceRenderStart(vtkObject *caller, unsigned long eventId,
                                 void *clientData, void *callData);
  static void onSliceInput(vtkObject *caller, unsigned long eventId,
                           void *clientData, void *callData);

  Ui::MainWindow *ui;

  // 3D view (meshes)
  vtkSmartPointer<vtkGenericOpenGLRenderWindow> mRenderWindow;
  vtkSmartPointer<vtkRenderer> mRenderer;
  vtkSmartPointer<QVTKInteractor> mInteractor;
  vtkSmartPointer<vtkInteractorStyleTrackballCamera> mInteractorStyle;

  // Slice view (DICOM + threshold overlay)
  vtkSmartPointer<vtkImageData> m_volume;
  vtkSmartPointer<vtkImageViewer2> imageViewer;
  vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_sliceRenderWindow;
  vtkSmartPointer<QVTKInteractor> m_sliceInteractor;
  vtkSmartPointer<vtkInteractorStyleImage> m_sliceStyle;
  vtkSmartPointer<vtkTextMapper> m_sliceTextMapper;
  vtkSmartPointer<vtkImageThreshold> m_overlayThreshold;
  vtkSmartPointer<vtkLookupTable> m_overlayLut;
  vtkSmartPointer<vtkImageActor> m_overlayActor;
  vtkSmartPointer<vtkCallbackCommand> m_sliceRenderCB;
  vtkSmartPointer<vtkCallbackCommand> m_sliceInputCB;
  // Tooth seed and nerve trace, drawn on the current slice
  vtkSmartPointer<vtkPolyData> m_seedMarker;
  vtkSmartPointer<vtkActor> m_seedActor;
  vtkSmartPointer<vtkPolyData> m_nerveTrace;
  vtkSmartPointer<vtkActor> m_nerveTraceActor;

  bool m_hasToothSeed = false;
  int m_toothSeed[3] = {0, 0, 0};
  std::vector<NervePath> m_nervePaths;  // last one is being traced
  double m_nerveDiameter = 2.5;         // mm
  int m_valueRange[2] = {0, 0};         // threshold slider range of the scan

  std::array<Structure, StructureCount> m_structures;
  std::array<double, StructureCount> m_requestedSetting{};
  int m_activeStructure = -1;
  bool m_hounsfield = false;
  bool m_busy = false;
  bool m_showing3D = true;
  bool m_refreshingList = false;
  bool m_resetMeshCamera = true;  // set the 3D camera on the first mesh of a scan

  QButtonGroup *m_structureButtons;
  QStandardItemModel *m_structureModel;
  QString m_dicom_path;
  QString m_patientName;

  QThread m_conversionThread;
  DicomConverter *m_converter;
};

#endif // MAINWINDOW_H

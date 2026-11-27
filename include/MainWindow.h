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

#include "dicomConverter.h"
#include "segmentation.h"
#include "theme.h"
#include "ui_MainWindow.h"

#include <QFutureWatcher>
#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <QThread>

#include <array>

class QAction;
class QActionGroup;
class QButtonGroup;
class QLabel;
class QMenu;
class QProgressBar;
class QStandardItem;
class QStandardItemModel;

namespace Ui {
class MainWindow;
}

// Result of reading a DICOM folder on a worker thread
struct DicomLoadResult {
  QString folder;
  QString patientName;
  QString error;
  vtkSmartPointer<vtkImageData> volume;
};

// Main window. Non-AI workflow: open a DICOM series -> pick a structure ->
// adjust it on the slices -> Create mesh (one mesh per structure) -> Export
// STL (one file each).
//   Bone / All teeth: threshold with a live overlay
//   Single tooth:     threshold + click the tooth (region grown from the click)
//   Nerve:            click points along the canal; tube of the set diameter
class MainWindow : public QMainWindow, public DicomConverter_Listener {
  Q_OBJECT
  friend struct Strata3DUiTest; // tests/ui/ui_test.cpp drives the private UI

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

  // Starts reading a DICOM series folder in the background (asks first when
  // unexported meshes would be lost). Returns false if nothing was started.
  bool loadDicomFolder(const QString &folder);
  bool isLoading() const { return m_loading; }
  // Writes one STL per mesh into a new timestamped folder inside `dir`.
  // Returns that folder, or an empty string on failure.
  QString exportStructures(const QString &dir);

  // What a left click on the slice view does for Single tooth / Nerve, by
  // voxel index (k is the slice) or patient position.
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

protected:
  void closeEvent(QCloseEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;

private slots:
  void openDicomDialog();
  void loadFinished();
  void selectStructure(int structureId);
  void sliderChanged(int value);
  void spinChanged(double value);
  void sliceSliderChanged(int value);
  void createMesh();
  bool exportDialog();
  void showSliceView();
  void show3DView();
  void resetView();
  void structureMeshReady(int structureId, vtkSmartPointer<vtkPolyData> mesh,
                          const QString &error);
  void meshItemChanged(QStandardItem *item);
  void meshContextMenu(const QPoint &pos);
  void updateProgress(double progress);
  void undoNervePoint();
  void newNerveCanal();
  void clearNerveTrace();
  void showShortcuts();
  void showAbout();

private:
  // setup
  void createActions();
  void createStatusBar();
  void setupSidebar();
  void readSettings();
  void writeSettings() const;
  void setTheme(Theme::Mode mode);
  void addRecent(const QString &folder);
  void updateRecentUi();

  // scan / views
  void createSliceView();
  void updateOverlay();
  void syncSliceUi();
  void updateAnnotations();
  void updateCursorInfo();
  void onSliceClick();
  bool onSliceKey(const std::string &key);
  static void onSliceRenderStart(vtkObject *caller, unsigned long eventId,
                                 void *clientData, void *callData);
  static void onSliceInput(vtkObject *caller, unsigned long eventId,
                           void *clientData, void *callData);

  // state presentation
  void configureSetting();
  void refreshMeshList();
  void updateActions();
  void setBusy(bool busy);
  void setStatus(const QString &text);
  void setHint(const QString &text, bool error = false);
  QString structureHint(int structureId) const;
  QString nerveHelp() const;
  QString thresholdText(double value) const;
  QString settingText(int structureId, double value) const;
  // Range [lower, upper) of a structure; upper is open-ended unless hasUpper.
  void structureRange(int structureId, double &lower, bool &hasUpper,
                      double &upper) const;
  bool hasMeshes() const;
  void setUnexported(bool unexported);
  // true when it is fine to drop the current meshes (asks the user if needed)
  bool confirmDiscardMeshes(const QString &action);
  void deleteMesh(int structureId);

  Ui::MainWindow *ui;

  // 3D view (meshes)
  vtkSmartPointer<vtkGenericOpenGLRenderWindow> mRenderWindow;
  vtkSmartPointer<vtkRenderer> mRenderer;
  vtkSmartPointer<QVTKInteractor> mInteractor;
  vtkSmartPointer<vtkInteractorStyleTrackballCamera> mInteractorStyle;

  // Slice view (DICOM + threshold overlay + tooth/nerve markers)
  vtkSmartPointer<vtkImageData> m_volume;
  vtkSmartPointer<vtkImageViewer2> imageViewer;
  vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_sliceRenderWindow;
  vtkSmartPointer<QVTKInteractor> m_sliceInteractor;
  vtkSmartPointer<vtkInteractorStyleImage> m_sliceStyle;
  vtkSmartPointer<vtkImageThreshold> m_overlayThreshold;
  vtkSmartPointer<vtkLookupTable> m_overlayLut;
  vtkSmartPointer<vtkImageActor> m_overlayActor;
  vtkSmartPointer<vtkCallbackCommand> m_sliceRenderCB;
  vtkSmartPointer<vtkCallbackCommand> m_sliceInputCB;
  vtkSmartPointer<vtkPolyData> m_seedMarker;
  vtkSmartPointer<vtkActor> m_seedActor;
  vtkSmartPointer<vtkPolyData> m_nerveTrace;
  vtkSmartPointer<vtkActor> m_nerveTraceActor;

  std::array<Structure, StructureCount> m_structures;
  std::array<double, StructureCount> m_requestedSetting{};
  int m_activeStructure = -1;
  bool m_hounsfield = false;
  bool m_busy = false;
  bool m_loading = false;
  bool m_showing3D = false;
  bool m_unexported = false;
  bool m_refreshingList = false;
  bool m_resetMeshCamera = true; // set the 3D camera on the first mesh of a scan

  bool m_hasToothSeed = false;
  int m_toothSeed[3] = {0, 0, 0};
  std::vector<NervePath> m_nervePaths; // last one is being traced
  double m_nerveDiameter = 2.5;        // mm
  int m_valueRange[2] = {0, 0};        // threshold range of the scan
  int m_cursorIJ[2] = {-1, -1};        // voxel under the mouse, -1: none

  QString m_dicom_path;
  QString m_patientName;
  QString m_lastDir;
  QStringList m_recent;
  Theme::Mode m_themeMode = Theme::Mode::Dark;

  // widgets and actions created in code
  QButtonGroup *m_structureButtons = nullptr;
  QStandardItemModel *m_meshModel = nullptr;
  QLabel *m_cursorLabel = nullptr;
  QProgressBar *m_progress = nullptr;
  QMenu *m_recentMenu = nullptr;
  QAction *m_openAction = nullptr;
  QAction *m_exportAction = nullptr;
  QAction *m_quitAction = nullptr;
  QAction *m_undoPointAction = nullptr;
  QAction *m_newCanalAction = nullptr;
  QAction *m_clearTraceAction = nullptr;
  QAction *m_sliceViewAction = nullptr;
  QAction *m_view3DAction = nullptr;
  QAction *m_resetViewAction = nullptr;
  QAction *m_fullScreenAction = nullptr;
  QAction *m_createMeshAction = nullptr;
  std::array<QAction *, StructureCount> m_structureActions{};
  QActionGroup *m_themeGroup = nullptr;

  QFutureWatcher<DicomLoadResult> m_loadWatcher;
  QThread m_conversionThread;
  DicomConverter *m_converter;
};

#endif // MAINWINDOW_H

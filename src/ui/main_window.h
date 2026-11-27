#ifndef STRATA3D_UI_MAIN_WINDOW_H
#define STRATA3D_UI_MAIN_WINDOW_H

#include "core/dicom_series.h"
#include "core/segmentation_session.h"
#include "ui/app_settings.h"
#include "ui/meta_types.h"
#include "ui/slice_view.h"
#include "ui/theme.h"

#include <QFutureWatcher>
#include <QMainWindow>
#include <QString>
#include <QThread>

#include <vtkImageData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <array>

class MeshView;
class MeshWorker;
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

// Application window: menus, toolbar, sidebar and the view area. It turns user
// actions into calls on the segmentation session, the views and the mesh
// worker, and presents their state; the segmentation rules live in
// strata3d::SegmentationSession and the VTK views in SliceView / MeshView.
class MainWindow : public QMainWindow {
  Q_OBJECT
  friend struct Strata3DUiTest; // tests/ui/ui_test.cpp drives the private UI

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

  // Starts reading a DICOM series folder in the background (asks first when
  // unexported meshes would be lost). Returns false if nothing was started.
  bool loadDicomFolder(const QString &folder);
  bool isLoading() const { return m_loading; }
  // Writes one STL per mesh into a new timestamped folder inside `dir`.
  // Returns that folder, or an empty string on failure.
  QString exportStructures(const QString &dir);

signals:
  void buildMeshRequested(vtkSmartPointer<vtkImageData> volume,
                          strata3d::MeshRequest request);

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
  void createMesh();
  bool exportDialog();
  void showSliceView();
  void show3DView();
  void resetView();
  void meshReady(int structureId, vtkSmartPointer<vtkPolyData> mesh,
                 const QString &error);
  void meshItemChanged(QStandardItem *item);
  void meshContextMenu(const QPoint &pos);
  void voxelClicked(int i, int j, int k, const strata3d::Point3 &position);
  void traceKeyPressed(SliceView::TraceKey key);
  void sliceChanged(int slice, int sliceCount);
  void cursorMoved(int i, int j, int k, double value);
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
  void restoreWindow();
  void saveWindow() const;
  void setTheme(Theme::Mode mode);
  void updateRecentUi(const QStringList &recent);

  // presentation of the current state
  void showStudy(const strata3d::DicomSeries &series);
  void configureSetting();
  void updateOverlay();
  void updateMarkers();
  void refreshMeshList();
  void updateActions();
  void setBusy(bool busy);
  void setStatus(const QString &text);
  void setHint(const QString &text, bool error = false);
  QString structureHint(int structureId) const;
  QString nerveHelp() const;
  QString thresholdText(double value) const;
  QString settingText(int structureId, double value) const;
  void nerveTraceChanged();

  // workflow
  void deleteMesh(int structureId);
  void setUnexported(bool unexported);
  // true when it is fine to drop the current meshes (asks the user if needed)
  bool confirmDiscardMeshes(const QString &action);

  Ui::MainWindow *ui;
  SliceView *m_sliceView;
  MeshView *m_meshView;
  AppSettings m_settings;

  strata3d::SegmentationSession m_session;
  vtkSmartPointer<vtkImageData> m_volume;
  QString m_dicomFolder;
  QString m_patientName;
  bool m_hounsfield = false;
  int m_activeStructure = -1;
  std::array<double, strata3d::StructureCount> m_requestedSetting{};

  bool m_busy = false;
  bool m_loading = false;
  bool m_showing3D = false;
  bool m_refreshingList = false;
  bool m_resetMeshCamera = true; // set the 3D camera on the first mesh of a scan
  Theme::Mode m_themeMode = Theme::Mode::Dark;

  // widgets and actions created in code
  QButtonGroup *m_structureButtons = nullptr;
  QStandardItemModel *m_meshModel = nullptr;
  QLabel *m_cursorLabel = nullptr;
  QProgressBar *m_progress = nullptr;
  QMenu *m_recentMenu = nullptr;
  QAction *m_openAction = nullptr;
  QAction *m_exportAction = nullptr;
  QAction *m_undoPointAction = nullptr;
  QAction *m_newCanalAction = nullptr;
  QAction *m_clearTraceAction = nullptr;
  QAction *m_sliceViewAction = nullptr;
  QAction *m_view3DAction = nullptr;
  QAction *m_resetViewAction = nullptr;
  QAction *m_createMeshAction = nullptr;
  std::array<QAction *, strata3d::StructureCount> m_structureActions{};
  QActionGroup *m_themeGroup = nullptr;

  QFutureWatcher<strata3d::DicomSeries> m_loadWatcher;
  QThread m_meshThread;
  MeshWorker *m_meshWorker = nullptr;
};

#endif // STRATA3D_UI_MAIN_WINDOW_H

// End-to-end test of the Strata3D main window.
//
// Drives the real window through its buttons, menu actions, shortcuts and
// mouse/key events on the slice view, on the example scans:
//   - example/downloads/tcga_thca_headneck_ct  (real CT: workflow checks)
//   - example/dental_ct                        (phantom: known geometry)
//
// Needs a display (X11 or Wayland) and must run from the repository root:
//   ./build/strata3d_ui_test                      run the checks
//   ./build/strata3d_ui_test --screenshots <dir>  capture the README images
//
// Settings are stored under a separate application name, so the user's own
// Strata3D preferences are not touched.
#include "MainWindow.h"

#include <QActionGroup>
#include <QApplication>
#include <QDir>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QMenuBar>
#include <QMimeData>
#include <QUrl>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QStandardItemModel>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QVTKOpenGLNativeWidget.h>
#include <QtTest/QTest>

#include <vtkCleanPolyData.h>
#include <vtkCommand.h>
#include <vtkMassProperties.h>
#include <vtkPolyDataConnectivityFilter.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSTLReader.h>
#include <vtkTriangleFilter.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

const char *kRealCt = "example/downloads/tcga_thca_headneck_ct";
const char *kPhantom = "example/dental_ct";
const double kPi = 3.14159265358979323846;

int g_failures = 0;

void check(bool ok, const QString &what) {
  std::printf("%s %s\n", ok ? "PASS" : "FAIL", what.toUtf8().constData());
  if (!ok)
    ++g_failures;
}

void pump(int ms) {
  QElapsedTimer t;
  t.start();
  while (t.elapsed() < ms) {
    QApplication::processEvents(QEventLoop::AllEvents, 20);
    QThread::msleep(5);
  }
}

// Clicks the button with this text on the next modal message box
void answerNextBox(const QString &buttonText) {
  QTimer::singleShot(300, [buttonText] {
    auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
    if (!box)
      return;
    for (QAbstractButton *b : box->buttons())
      if (b->text().remove('&') == buttonText) {
        b->click();
        return;
      }
  });
}

// Number of connected pieces (and volume in mm³) of an STL file
int stlPieces(const QString &path, double *volume) {
  vtkNew<vtkSTLReader> reader;
  reader->SetFileName(path.toStdString().c_str());
  vtkNew<vtkCleanPolyData> clean;
  clean->SetInputConnection(reader->GetOutputPort());
  vtkNew<vtkPolyDataConnectivityFilter> pieces;
  pieces->SetInputConnection(clean->GetOutputPort());
  pieces->SetExtractionModeToAllRegions();
  pieces->Update();
  vtkNew<vtkTriangleFilter> triangles;
  triangles->SetInputConnection(reader->GetOutputPort());
  vtkNew<vtkMassProperties> mass;
  mass->SetInputConnection(triangles->GetOutputPort());
  mass->Update();
  *volume = mass->GetVolume();
  return pieces->GetNumberOfExtractedRegions();
}

} // namespace

struct Strata3DUiTest {
  MainWindow &w;
  QString shotDir; // empty: no screenshots

  void shot(const QString &name) {
    if (shotDir.isEmpty())
      return;
    pump(500);
    w.grab().save(QDir(shotDir).filePath(name + ".png"));
    std::printf("     screenshot %s.png\n", name.toUtf8().constData());
  }

  bool waitIdle(int timeoutMs = 300000) {
    QElapsedTimer t;
    t.start();
    pump(200);
    while ((w.m_busy || w.m_loading) && t.elapsed() < timeoutMs)
      pump(50);
    pump(300);
    return !w.m_busy && !w.m_loading;
  }

  bool load(const QString &folder) {
    w.setUnexported(false); // no "export first?" prompt between scenarios
    return w.loadDicomFolder(QDir(folder).absolutePath()) && waitIdle();
  }

  // Real mouse input on the slice view at a patient position on the slice
  void clickAt(double x, double y) {
    vtkRenderer *ren = w.imageViewer->GetRenderer();
    double o[3], sp[3];
    w.m_volume->GetOrigin(o);
    w.m_volume->GetSpacing(sp);
    ren->SetWorldPoint(x, y, o[2] + w.imageViewer->GetSlice() * sp[2], 1.0);
    ren->WorldToDisplay();
    double *d = ren->GetDisplayPoint();
    vtkRenderWindowInteractor *iren = w.m_sliceInteractor;
    iren->SetEventInformation(static_cast<int>(std::lround(d[0])),
                              static_cast<int>(std::lround(d[1])), 0, 0, 0, 0);
    iren->InvokeEvent(vtkCommand::MouseMoveEvent);
    iren->InvokeEvent(vtkCommand::LeftButtonPressEvent);
    iren->InvokeEvent(vtkCommand::LeftButtonReleaseEvent);
  }

  void pressKey(const char *keySym) {
    w.m_sliceInteractor->SetKeySym(keySym);
    w.m_sliceInteractor->InvokeEvent(vtkCommand::KeyPressEvent);
  }

  void keyShortcut(Qt::Key key, Qt::KeyboardModifiers mods) {
    w.activateWindow();
    pump(150);
    if (QApplication::activeWindow() != &w)
      std::printf("     note: window is not active, shortcuts may not "
                  "arrive (another window has focus)\n");
    QTest::keyClick(&w, key, mods);
    pump(200);
  }

  double voxel(int i, int j, int k) const {
    return w.m_volume->GetScalarComponentAsDouble(i, j, k, 0);
  }

  // Axial slice with the most voxels >= threshold (the teeth level)
  int densestSlice(double threshold) const {
    int d[3];
    w.m_volume->GetDimensions(d);
    int bestK = 0;
    long best = -1;
    for (int k = 0; k < d[2]; ++k) {
      long n = 0;
      for (int j = 0; j < d[1]; ++j)
        for (int i = 0; i < d[0]; ++i)
          n += voxel(i, j, k) >= threshold;
      if (n > best) {
        best = n;
        bestK = k;
      }
    }
    return bestK;
  }

  // Centre (in mm) of the largest tooth-sized 2D cross-section on slice k
  bool toothOnSlice(int k, double threshold, double &x, double &y) const {
    int d[3];
    double o[3], sp[3];
    w.m_volume->GetDimensions(d);
    w.m_volume->GetOrigin(o);
    w.m_volume->GetSpacing(sp);
    std::vector<char> seen(static_cast<size_t>(d[0]) * d[1], 0);
    int bestN = 0;
    for (int j0 = 0; j0 < d[1]; ++j0)
      for (int i0 = 0; i0 < d[0]; ++i0) {
        if (seen[j0 * d[0] + i0] || voxel(i0, j0, k) < threshold)
          continue;
        std::vector<std::pair<int, int>> stack{{i0, j0}};
        seen[j0 * d[0] + i0] = 1;
        double si = 0, sj = 0;
        int n = 0;
        while (!stack.empty()) {
          auto [i, j] = stack.back();
          stack.pop_back();
          si += i;
          sj += j;
          ++n;
          for (auto [di, dj] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
            int ni = i + di, nj = j + dj;
            if (ni < 0 || nj < 0 || ni >= d[0] || nj >= d[1] ||
                seen[nj * d[0] + ni] || voxel(ni, nj, k) < threshold)
              continue;
            seen[nj * d[0] + ni] = 1;
            stack.push_back({ni, nj});
          }
        }
        double area = n * sp[0] * sp[1];
        if (area >= 15 && area <= 120 && n > bestN) {
          bestN = n;
          x = o[0] + si / n * sp[0];
          y = o[1] + sj / n * sp[1];
        }
      }
    return bestN > 0;
  }

  // ------------------------------------------------------------ checks

  void testRealCt(const QString &exportRoot) {
    std::printf("=== real CT workflow (%s)\n", kRealCt);
    check(w.ui->viewStack->currentWidget() == w.ui->emptyPage,
          "starts on the empty state");
    check(!w.ui->createMeshButton->isEnabled() &&
              !w.ui->boneButton->isEnabled(),
          "segmentation controls disabled before a scan");
    check(w.menuBar()->actions().size() == 5,
          "menu bar: File Edit View Segment Help");

    check(w.loadDicomFolder(QDir(kRealCt).absolutePath()), "load started");
    check(w.m_loading && w.m_progress->isVisible(),
          "loading runs in the background with progress");
    check(waitIdle(), "load finished");
    check(w.ui->viewStack->currentWidget() == w.ui->viewerPage &&
              !w.m_showing3D,
          "slice view after load");
    check(w.m_hounsfield && w.ui->patientLabel->text() == "TCGA-DE-A4MA",
          "study panel: patient and HU detected");
    check(QSettings().value("recent").toStringList().value(0) ==
              QDir(kRealCt).absolutePath(),
          "folder added to recent scans");

    const double bone = w.m_structures[BoneStructure].lower;
    const double teeth = w.m_structures[TeethStructure].lower;
    check(bone == 400 && teeth == 1500, "CT presets 400 / 1500 HU");
    const int kTeeth = densestSlice(teeth);
    w.ui->sliceSlider->setValue(kTeeth);
    check(w.imageViewer->GetSlice() == kTeeth &&
              w.ui->sliceLabel->text().startsWith(
                  QString("Slice %1 /").arg(kTeeth + 1)),
          "slice slider moves the view and label");

    keyShortcut(Qt::Key_1, Qt::ControlModifier);
    check(w.m_activeStructure == BoneStructure && w.ui->boneButton->isChecked(),
          "Ctrl+1 selects Bone");
    w.ui->settingSpin->setValue(450);
    check(w.ui->settingSlider->value() == 450 &&
              w.m_structures[BoneStructure].lower == 450,
          "spin box updates slider and threshold");
    w.ui->settingSlider->setValue(400);
    check(w.ui->settingSpin->value() == 400, "slider updates spin box");

    w.ui->createMeshButton->click();
    check(waitIdle() && w.m_structures[BoneStructure].mesh && w.m_showing3D,
          "bone mesh built and shown in 3D");
    check(w.isWindowModified(), "window marked modified (unexported mesh)");

    w.ui->teethButton->click();
    w.ui->createMeshButton->click();
    check(waitIdle() && w.m_structures[TeethStructure].mesh,
          "all-teeth mesh built");

    w.ui->toothButton->click();
    w.ui->sliceSlider->setValue(kTeeth);
    w.ui->createMeshButton->click();
    pump(200);
    check(w.ui->hintLabel->property("state").toString() == "error",
          "Create mesh without a tooth click shows an error hint");
    double tx = 0, ty = 0;
    check(toothOnSlice(kTeeth, teeth, tx, ty), "found a tooth to click");
    clickAt(tx, ty);
    check(w.m_hasToothSeed, "click on the slice selects the tooth");
    keyShortcut(Qt::Key_Return, Qt::ControlModifier);
    check(waitIdle() && w.m_structures[ToothStructure].mesh,
          "Ctrl+Enter builds the single-tooth mesh");

    w.ui->nerveButton->click();
    check(w.ui->settingSection->text() == "DIAMETER" &&
              std::fabs(w.ui->settingSpin->value() - 2.5) < 1e-9,
          "Nerve: diameter control, 2.5 mm default");
    double o[3];
    w.m_volume->GetOrigin(o);
    for (int q = 0; q < 4; ++q)
      clickAt(o[0] + 100 + 10 * q, o[1] + 150);
    check(w.m_nervePaths.size() == 1 && w.m_nervePaths[0].size() == 4,
          "4 clicks add 4 nerve points");
    w.m_undoPointAction->trigger();
    pressKey("BackSpace");
    check(w.m_nervePaths[0].size() == 2, "Edit > Undo and Backspace remove points");

    QStandardItem *boneRow = w.m_meshModel->item(0);
    boneRow->setCheckState(Qt::Unchecked);
    check(!w.m_structures[BoneStructure].actor->GetVisibility(),
          "mesh list checkbox hides a mesh");
    boneRow->setCheckState(Qt::Checked);
    answerNextBox("Cancel");
    w.deleteMesh(ToothStructure);
    check(w.m_structures[ToothStructure].mesh != nullptr,
          "Delete mesh > Cancel keeps it");
    answerNextBox("Cancel");
    check(!w.loadDicomFolder(QDir(kPhantom).absolutePath()),
          "opening another scan asks first (Cancel keeps the meshes)");

    QString folder = w.exportStructures(exportRoot);
    check(!folder.isEmpty() && !w.isWindowModified(),
          "export writes the files and clears the modified marker");
    for (const char *name : {"bone.stl", "teeth.stl", "tooth.stl"})
      check(QFileInfo(QDir(folder).filePath(name)).size() > 0,
            QString("%1 written").arg(name));

    w.m_themeGroup->actions()[1]->trigger();
    check(QSettings().value("theme").toString() == "light",
          "View > Theme > Light is remembered");
    w.m_themeGroup->actions()[0]->trigger();

    // The layout must fit a small laptop screen. Whether the window manager
    // applies a programmatic resize is up to the compositor, so the viewer
    // size is only checked when it did.
    QSize minimum = w.minimumSizeHint();
    check(minimum.width() <= 1024 && minimum.height() <= 640,
          QString("layout fits 1024x640 (minimum %1x%2)")
              .arg(minimum.width())
              .arg(minimum.height()));
    w.showNormal();
    w.resize(1024, 640);
    pump(800);
    if (w.width() <= 1100 && w.height() <= 700) {
      QRect view = w.ui->openGLWidget->geometry();
      check(view.width() > 400 && view.height() > 250 &&
                w.ui->sidebarScroll->isVisible(),
            QString("1024x640 window: viewer %1x%2, sidebar visible")
                .arg(view.width())
                .arg(view.height()));
    } else {
      std::printf("     note: the window manager kept %dx%d; small-window "
                  "layout not checked in this run\n",
                  w.width(), w.height());
    }
    w.resize(1600, 950);
    w.writeSettings();
    check(!QSettings().value("geometry").toByteArray().isEmpty(),
          "window geometry saved");
  }

  // Phantom: 12 teeth on a 27 mm arch, canal at z = 11 mm (tools/dental_phantom)
  void testPhantom(const QString &exportRoot) {
    std::printf("=== phantom geometry (%s)\n", kPhantom);
    // open it by dropping the folder onto the window
    discardMeshes();
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(QDir(kPhantom).absolutePath())});
    QDragEnterEvent enter(QPoint(20, 20), Qt::CopyAction, &mime, Qt::LeftButton,
                          Qt::NoModifier);
    QApplication::sendEvent(&w, &enter);
    check(enter.isAccepted(), "window accepts a dragged folder");
    QDropEvent drop(QPointF(20, 20), Qt::CopyAction, &mime, Qt::LeftButton,
                    Qt::NoModifier);
    QApplication::sendEvent(&w, &drop);
    check(waitIdle() && w.ui->patientLabel->text() == "Phantom^DentalCT",
          "dropped folder is loaded");
    w.ui->teethButton->click();
    w.ui->createMeshButton->click();
    waitIdle();

    w.ui->toothButton->click();
    w.ui->sliceSlider->setValue(60); // z = 30 mm, tooth crowns
    double a = kPi * 6.5 / 12;
    double tx = 40 + 27 * std::cos(a), ty = 50 - 27 * std::sin(a);
    clickAt(tx, ty);
    w.ui->createMeshButton->click();
    check(waitIdle() && w.m_structures[ToothStructure].mesh,
          "single tooth built");
    double b[6];
    w.m_structures[ToothStructure].mesh->GetBounds(b);
    check(std::hypot((b[0] + b[1]) / 2 - tx, (b[2] + b[3]) / 2 - ty) < 1.0 &&
              b[1] - b[0] < 7,
          "single tooth is exactly the clicked tooth");

    w.ui->nerveButton->click();
    w.ui->sliceSlider->setValue(22); // z = 11 mm, the canal
    // left canal half, Enter, right canal half: two canals in one mesh
    for (int deg = 15; deg <= 85; deg += 10)
      clickAt(40 + 27 * std::cos(deg * kPi / 180),
              50 - 27 * std::sin(deg * kPi / 180));
    pressKey("Return");
    for (int deg = 95; deg <= 165; deg += 10)
      clickAt(40 + 27 * std::cos(deg * kPi / 180),
              50 - 27 * std::sin(deg * kPi / 180));
    check(w.m_nervePaths.size() == 2 && w.m_nervePaths[0].size() == 8 &&
              w.m_nervePaths[1].size() == 8,
          "Enter starts a second canal");
    w.ui->createMeshButton->click();
    pump(300);
    check(w.m_structures[NerveStructure].mesh != nullptr, "nerve mesh built");
    w.m_structures[NerveStructure].mesh->GetBounds(b);
    check(std::fabs(b[4] - 9.75) < 0.2 && std::fabs(b[5] - 12.25) < 0.2,
          "nerve tube at canal height with 2.5 mm diameter");

    QString folder = w.exportStructures(exportRoot);
    double volume = 0;
    check(stlPieces(QDir(folder).filePath("teeth.stl"), &volume) == 12,
          "teeth.stl has the 12 phantom teeth");
    check(stlPieces(QDir(folder).filePath("tooth.stl"), &volume) == 1,
          "tooth.stl is one piece");
    check(stlPieces(QDir(folder).filePath("nerve.stl"), &volume) == 2,
          "nerve.stl has the two traced canals");
  }

  // so closing the window does not ask to export the test meshes
  void discardMeshes() { w.setUnexported(false); }

  // ------------------------------------------------------- screenshots

  void captureScreenshots() {
    shot("empty-state");

    load(kRealCt);
    const double teeth = w.m_structures[TeethStructure].lower;
    const int kTeeth = densestSlice(teeth);
    w.ui->sliceSlider->setValue(kTeeth);
    w.ui->boneButton->click();
    shot("slices-bone-threshold");
    w.ui->createMeshButton->click();
    waitIdle();
    w.ui->teethButton->click();
    shot("slices-teeth-threshold");
    w.ui->createMeshButton->click();
    waitIdle();
    shot("3d-bone-and-teeth");

    w.ui->toothButton->click();
    w.ui->sliceSlider->setValue(kTeeth);
    double tx = 0, ty = 0;
    if (toothOnSlice(kTeeth, teeth, tx, ty))
      clickAt(tx, ty);
    shot("single-tooth-selection");

    load(kPhantom);
    w.ui->nerveButton->click();
    w.ui->sliceSlider->setValue(22);
    for (int deg = 15; deg <= 165; deg += 25)
      clickAt(40 + 27 * std::cos(deg * kPi / 180),
              50 - 27 * std::sin(deg * kPi / 180));
    shot("nerve-tracing");
    w.ui->createMeshButton->click();
    for (int id : {BoneStructure, TeethStructure}) {
      w.selectStructure(id);
      w.ui->createMeshButton->click();
      waitIdle();
    }
    w.m_meshModel->item(0)->setCheckState(Qt::Unchecked); // hide bone
    w.show3DView();
    shot("3d-teeth-and-nerve");

    w.m_themeGroup->actions()[1]->trigger();
    w.m_meshModel->item(0)->setCheckState(Qt::Checked);
    shot("light-theme");
    w.m_themeGroup->actions()[0]->trigger();
    w.setUnexported(false);
  }
};

int main(int argc, char *argv[]) {
  QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());
  QApplication app(argc, argv);
  QApplication::setOrganizationName("Strata3DUiTest");
  QApplication::setApplicationName("Strata3DUiTest");
  QSettings().clear();

  for (const char *folder : {kRealCt, kPhantom})
    if (!QFileInfo(folder).isDir()) {
      std::fprintf(stderr, "Run from the repository root; missing %s\n",
                   folder);
      return 2;
    }

  QString shotDir;
  const QStringList args = app.arguments();
  int idx = args.indexOf("--screenshots");
  if (idx >= 0 && idx + 1 < args.size()) {
    shotDir = args[idx + 1];
    QDir().mkpath(shotDir);
  }

  MainWindow window;
  window.show();
  window.resize(1600, 950);
  QTemporaryDir exportDir;

  QTimer::singleShot(800, [&] {
    Strata3DUiTest test{window, shotDir};
    if (shotDir.isEmpty()) {
      test.testRealCt(exportDir.path());
      test.testPhantom(exportDir.path());
      std::printf("=== %s (%d failure(s))\n",
                  g_failures ? "FAILED" : "ALL PASSED", g_failures);
    } else {
      test.captureScreenshots();
    }
    test.discardMeshes();
    QSettings().clear();
    app.exit(g_failures ? 1 : 0);
  });
  return app.exec();
}

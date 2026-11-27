#include <MainWindow.h>
#include <QSurfaceFormat>
#include <QVTKOpenGLNativeWidget.h>
#include <QtWidgets/QApplication>

int main(int argc, char *argv[]) {
  // VTK needs an OpenGL 3.2+ core context; this must be set before QApplication
  // exists.
  QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat());

  QApplication app(argc, argv);
  // QSettings location (window layout, theme, recent scans)
  QApplication::setOrganizationName("Strata3D");
  QApplication::setApplicationName("Strata3D");

  MainWindow mainWindow;
  mainWindow.show();

  // Optional: Strata3D <dicom_folder> opens that series on start
  if (argc > 1)
    mainWindow.loadDicomFolder(QString::fromLocal8Bit(argv[1]));

  return app.exec();
}

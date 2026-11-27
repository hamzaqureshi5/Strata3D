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
  QApplication::setOrganizationName("DicomSegmentation");
  QApplication::setApplicationName("DicomSegmentation");

  MainWindow mainWindow;
  mainWindow.show();

  // Optional: DicomSegmentation <dicom_folder> opens that series on start
  if (argc > 1)
    mainWindow.loadDicomFolder(QString::fromLocal8Bit(argv[1]));

  return app.exec();
}

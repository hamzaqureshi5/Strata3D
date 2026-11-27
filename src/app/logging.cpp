#include "app/logging.h"

#include "ui/log.h"

#include <QString>
#include <QtGlobal>

#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkOutputWindow.h>

namespace {

class QtLogOutputWindow : public vtkOutputWindow {
public:
  static QtLogOutputWindow *New();
  vtkTypeMacro(QtLogOutputWindow, vtkOutputWindow);

  void DisplayText(const char *text) override {
    qCInfo(lcVtk).noquote() << QString::fromUtf8(text).trimmed();
  }
  void DisplayErrorText(const char *text) override {
    qCWarning(lcVtk).noquote() << "error:" << QString::fromUtf8(text).trimmed();
  }
  void DisplayWarningText(const char *text) override {
    qCWarning(lcVtk).noquote() << QString::fromUtf8(text).trimmed();
  }
  void DisplayGenericWarningText(const char *text) override {
    DisplayWarningText(text);
  }
  void DisplayDebugText(const char *text) override {
    qCDebug(lcVtk).noquote() << QString::fromUtf8(text).trimmed();
  }
};

vtkStandardNewMacro(QtLogOutputWindow);

} // namespace

void installLogging() {
  qSetMessagePattern("%{time hh:mm:ss.zzz} %{if-warning}W %{endif}"
                     "%{if-critical}E %{endif}%{category}: %{message}");
  vtkNew<QtLogOutputWindow> window;
  vtkOutputWindow::SetInstance(window);
}

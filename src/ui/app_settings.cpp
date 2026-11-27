#include "ui/app_settings.h"

#include <QSettings>

namespace {
const char *kTheme = "theme";
const char *kLastFolder = "lastDir";
const char *kRecent = "recent";
const char *kGeometry = "geometry";
const char *kWindowState = "windowState";
const char *kSplitter = "splitter";
} // namespace

Theme::Mode AppSettings::theme() const {
  return Theme::modeFromString(QSettings().value(kTheme, "dark").toString());
}

void AppSettings::setTheme(Theme::Mode mode) {
  QSettings().setValue(kTheme, Theme::modeToString(mode));
}

QString AppSettings::lastFolder() const {
  return QSettings().value(kLastFolder).toString();
}

void AppSettings::setLastFolder(const QString &folder) {
  QSettings().setValue(kLastFolder, folder);
}

QStringList AppSettings::recentScans() const {
  return QSettings().value(kRecent).toStringList();
}

QStringList AppSettings::addRecentScan(const QString &folder) {
  QStringList recent = recentScans();
  recent.removeAll(folder);
  recent.prepend(folder);
  while (recent.size() > kMaxRecentScans)
    recent.removeLast();
  QSettings().setValue(kRecent, recent);
  return recent;
}

void AppSettings::clearRecentScans() { QSettings().remove(kRecent); }

QByteArray AppSettings::windowGeometry() const {
  return QSettings().value(kGeometry).toByteArray();
}

QByteArray AppSettings::windowState() const {
  return QSettings().value(kWindowState).toByteArray();
}

QByteArray AppSettings::splitterState() const {
  return QSettings().value(kSplitter).toByteArray();
}

void AppSettings::saveWindow(const QByteArray &geometry,
                             const QByteArray &state,
                             const QByteArray &splitter) {
  QSettings s;
  s.setValue(kGeometry, geometry);
  s.setValue(kWindowState, state);
  s.setValue(kSplitter, splitter);
}

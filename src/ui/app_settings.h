#ifndef STRATA3D_UI_APP_SETTINGS_H
#define STRATA3D_UI_APP_SETTINGS_H

#include "ui/theme.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

// Typed access to the persistent preferences (QSettings; on Linux
// ~/.config/<organization>/<application>.conf). Every getter has a default,
// so a missing or damaged settings file just means defaults.
class AppSettings {
public:
  static constexpr int kMaxRecentScans = 8;

  Theme::Mode theme() const;
  void setTheme(Theme::Mode mode);

  QString lastFolder() const;
  void setLastFolder(const QString &folder);

  QStringList recentScans() const;
  // Moves `folder` to the front, keeps at most kMaxRecentScans
  QStringList addRecentScan(const QString &folder);
  void clearRecentScans();

  QByteArray windowGeometry() const;
  QByteArray windowState() const;
  QByteArray splitterState() const;
  void saveWindow(const QByteArray &geometry, const QByteArray &state,
                  const QByteArray &splitter);
};

#endif // STRATA3D_UI_APP_SETTINGS_H

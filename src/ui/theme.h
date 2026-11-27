#ifndef STRATA3D_UI_THEME_H
#define STRATA3D_UI_THEME_H

#include <QColor>
#include <QString>

class QApplication;

// Central design tokens and the application style sheet built from them.
// Widgets opt into variants through dynamic properties instead of their own
// style sheets:
//   QPushButton[role="primary"]   main action of a panel (accent)
//   QLabel[role="section"]        small upper-case section title
//   QLabel[role="muted"]          secondary text
//   QLabel[role="title"]          prominent text (patient name, empty state)
//   QLabel[role="hint"]           contextual instructions; [state="error"]
//   QToolButton[role="structure"] structure picker (checkable)
//   QToolButton[role="segment"]   segmented view switch (checkable)
namespace Theme {

enum class Mode { Dark, Light, System };

struct Tokens {
  QColor window;      // main background
  QColor surface;     // panels (sidebar, bars)
  QColor surfaceAlt;  // controls, list rows
  QColor border;
  QColor text;
  QColor textMuted;
  QColor accent;
  QColor accentHover;
  QColor onAccent;    // text on accent
  QColor danger;
  QColor warning;
  QColor success;
  QColor viewport3D;  // 3D view background (slice view stays black)
};

// Spacing scale in px (multiples of 4) and corner radii
constexpr int kSpaceXs = 4, kSpaceS = 8, kSpaceM = 12, kSpaceL = 16;
constexpr int kRadius = 6, kRadiusSmall = 4;

Mode modeFromString(const QString &name); // "dark" | "light" | "system"
QString modeToString(Mode mode);

// Dark or Light; System resolves to the platform's color scheme
Mode resolve(Mode mode);
const Tokens &tokens(Mode resolvedMode);

// Applies palette + style sheet to the whole application and returns the
// tokens in effect.
const Tokens &apply(QApplication &app, Mode mode);

} // namespace Theme

#endif // STRATA3D_UI_THEME_H

#include "ui/theme.h"

#include <QApplication>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QStyleHints>

namespace Theme {

namespace {

const Tokens kDark{
    QColor("#1b1d21"), // window
    QColor("#23262b"), // surface
    QColor("#2c3036"), // surfaceAlt
    QColor("#3a3f47"), // border
    QColor("#e6e8eb"), // text
    QColor("#a0a7b1"), // textMuted (6.4:1 on surface)
    QColor("#3d8bfd"), // accent
    QColor("#5c9dff"), // accentHover
    QColor("#ffffff"), // onAccent
    QColor("#f27474"), // danger
    QColor("#f0b44c"), // warning
    QColor("#4cc38a"), // success
    QColor("#16181c"), // viewport3D
};

const Tokens kLight{
    QColor("#f3f4f6"), // window
    QColor("#ffffff"), // surface
    QColor("#eceef2"), // surfaceAlt
    QColor("#d3d7de"), // border
    QColor("#1d2127"), // text
    QColor("#59616d"), // textMuted (6.2:1 on surface)
    QColor("#1f6feb"), // accent
    QColor("#3a82f0"), // accentHover
    QColor("#ffffff"), // onAccent
    QColor("#c62f2f"), // danger
    QColor("#9a6300"), // warning
    QColor("#1d7f4e"), // success
    QColor("#e9ecf0"), // viewport3D
};

QString rgba(const QColor &c, int alpha) {
  return QString("rgba(%1, %2, %3, %4)")
      .arg(c.red())
      .arg(c.green())
      .arg(c.blue())
      .arg(alpha);
}

QString styleSheet(const Tokens &t) {
  QString css = R"(
* { outline: none; }
QMainWindow, QDialog { background: @window; }
QWidget { color: @text; }
QToolTip { background: @surfaceAlt; color: @text; border: 1px solid @border;
           padding: 4px 6px; }

/* panels */
#sidebar, QScrollArea#sidebarScroll, QScrollArea#sidebarScroll > QWidget > QWidget {
  background: @surface; }
QScrollArea#sidebarScroll { border: none; border-right: 1px solid @border; }
#viewerArea { background: @window; }
QSplitter::handle { background: @border; }
QSplitter::handle:horizontal { width: 1px; }

/* text roles */
QLabel[role="section"] { color: @muted; font-size: 8pt; font-weight: 600;
  padding-top: 8px; }
QLabel[role="muted"] { color: @muted; }
QLabel[role="title"] { font-size: 12pt; font-weight: 600; }
QLabel[role="hint"] { color: @muted; background: @surfaceAlt;
  border-radius: @radius; padding: 8px; }
QLabel[role="hint"][state="error"] { color: @danger;
  background: @dangerTint; }
QLabel[role="emptyTitle"] { font-size: 16pt; font-weight: 600; }

/* buttons */
QPushButton, QToolButton {
  background: @surfaceAlt; border: 1px solid @border; border-radius: @radius;
  padding: 6px 12px; min-height: 18px; }
QPushButton:hover, QToolButton:hover { border-color: @accent; }
QPushButton:pressed, QToolButton:pressed { background: @border; }
QPushButton:focus, QToolButton:focus { border: 1px solid @accent; }
QPushButton:disabled, QToolButton:disabled { color: @muted;
  background: @surface; border-color: @border; }
QPushButton[role="primary"] { background: @accent; color: @onAccent;
  border: 1px solid @accent; font-weight: 600; padding: 8px 12px; }
QPushButton[role="primary"]:hover { background: @accentHover; }
QPushButton[role="primary"]:focus { border: 2px solid @onAccent; }
QPushButton[role="primary"]:disabled { background: @surfaceAlt;
  color: @muted; border-color: @border; }

QToolButton[role="structure"] { text-align: left; padding: 6px 8px; }
QToolButton[role="structure"]:checked { background: @accentTint;
  border: 1px solid @accent; font-weight: 600; }

QToolButton[role="segment"] { border-radius: 0; padding: 4px 14px; }
QToolButton[role="segment"][position="first"] {
  border-top-left-radius: @radius; border-bottom-left-radius: @radius; }
QToolButton[role="segment"][position="last"] {
  border-top-right-radius: @radius; border-bottom-right-radius: @radius;
  border-left: none; }
QToolButton[role="segment"]:checked { background: @accent; color: @onAccent;
  border-color: @accent; }

/* inputs */
QSpinBox, QDoubleSpinBox, QLineEdit {
  background: @window; border: 1px solid @border; border-radius: @radiusSmall;
  padding: 3px 6px; selection-background-color: @accent; }
QSpinBox:focus, QDoubleSpinBox:focus, QLineEdit:focus { border-color: @accent; }
QSpinBox:disabled, QDoubleSpinBox:disabled { color: @muted; }

QSlider::groove:horizontal { height: 4px; background: @border; border-radius: 2px; }
QSlider::groove:vertical { width: 4px; background: @border; border-radius: 2px; }
QSlider::sub-page:horizontal { background: @accent; border-radius: 2px; }
/* round 14 px knob centred on the 4 px groove */
QSlider::handle:horizontal { background: @text; border: 2px solid @surface;
  width: 14px; margin: -7px 0; border-radius: 9px; }
QSlider::handle:vertical { background: @text; border: 2px solid @surface;
  height: 14px; margin: 0 -7px; border-radius: 9px; }
QSlider::handle:focus { background: @accent; }
QSlider::groove:disabled, QSlider::sub-page:disabled, QSlider::add-page:disabled {
  background: @surfaceAlt; }
QSlider::handle:disabled { background: @muted; }

/* lists */
QListView { background: @window; border: 1px solid @border;
  border-radius: @radius; padding: 2px; }
QListView::item { padding: 4px; border-radius: @radiusSmall; }
QListView::item:hover { background: @surfaceAlt; }
QListView::item:selected { background: @accentTint; color: @text; }
QListView:focus { border-color: @accent; }

/* chrome */
QMenuBar { background: @surface; border-bottom: 1px solid @border; }
QMenuBar::item { padding: 4px 10px; background: transparent; }
QMenuBar::item:selected { background: @surfaceAlt; border-radius: @radiusSmall; }
QMenu { background: @surface; border: 1px solid @border; padding: 4px; }
QMenu::item { padding: 5px 24px 5px 12px; border-radius: @radiusSmall; }
QMenu::item:selected { background: @accent; color: @onAccent; }
QMenu::item:disabled { color: @muted; }
QMenu::separator { height: 1px; background: @border; margin: 4px 6px; }
QToolBar { background: @surface; border: none; border-bottom: 1px solid @border;
  spacing: 4px; padding: 4px 8px; }
QToolBar QToolButton { background: transparent; border: 1px solid transparent;
  padding: 4px 8px; }
QToolBar QToolButton:hover { background: @surfaceAlt; border-color: @border; }
QToolBar QToolButton:checked { background: @accentTint; border-color: @accent; }
QToolBar QToolButton:disabled { background: transparent; border-color: transparent; }
QToolBar::separator { width: 1px; background: @border; margin: 4px 6px; }
QStatusBar { background: @surface; border-top: 1px solid @border; color: @muted; }
QStatusBar QLabel { color: @muted; padding: 0 8px; }
QStatusBar::item { border: none; }
QProgressBar { background: @surfaceAlt; border: none; border-radius: 3px;
  max-height: 6px; min-width: 140px; text-align: center; }
QProgressBar::chunk { background: @accent; border-radius: 3px; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: @border; border-radius: 4px; min-height: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QMessageBox QLabel { color: @text; }
)";
  const QList<QPair<QString, QString>> vars = {
      {"@window", t.window.name()},
      {"@surfaceAlt", t.surfaceAlt.name()},
      {"@surface", t.surface.name()},
      {"@border", t.border.name()},
      {"@text", t.text.name()},
      {"@muted", t.textMuted.name()},
      {"@accentHover", t.accentHover.name()},
      {"@accentTint", rgba(t.accent, 56)},
      {"@accent", t.accent.name()},
      {"@onAccent", t.onAccent.name()},
      {"@dangerTint", rgba(t.danger, 36)},
      {"@danger", t.danger.name()},
      {"@radiusSmall", QString("%1px").arg(kRadiusSmall)},
      {"@radius", QString("%1px").arg(kRadius)},
  };
  // longer names first so "@surfaceAlt" is not hit by "@surface"
  for (const auto &[name, value] : vars)
    css.replace(name, value);
  return css;
}

QPalette palette(const Tokens &t) {
  QPalette p;
  p.setColor(QPalette::Window, t.window);
  p.setColor(QPalette::WindowText, t.text);
  p.setColor(QPalette::Base, t.window);
  p.setColor(QPalette::AlternateBase, t.surfaceAlt);
  p.setColor(QPalette::Text, t.text);
  p.setColor(QPalette::Button, t.surfaceAlt);
  p.setColor(QPalette::ButtonText, t.text);
  p.setColor(QPalette::Highlight, t.accent);
  p.setColor(QPalette::HighlightedText, t.onAccent);
  p.setColor(QPalette::ToolTipBase, t.surfaceAlt);
  p.setColor(QPalette::ToolTipText, t.text);
  p.setColor(QPalette::PlaceholderText, t.textMuted);
  p.setColor(QPalette::Link, t.accent);
  for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
    p.setColor(QPalette::Disabled, role, t.textMuted);
  return p;
}

} // namespace

Mode modeFromString(const QString &name) {
  if (name == "light")
    return Mode::Light;
  if (name == "system")
    return Mode::System;
  return Mode::Dark;
}

QString modeToString(Mode mode) {
  switch (mode) {
  case Mode::Light:
    return "light";
  case Mode::System:
    return "system";
  default:
    return "dark";
  }
}

Mode resolve(Mode mode) {
  if (mode != Mode::System)
    return mode;
  return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Light
             ? Mode::Light
             : Mode::Dark;
}

const Tokens &tokens(Mode resolvedMode) {
  return resolvedMode == Mode::Light ? kLight : kDark;
}

const Tokens &apply(QApplication &app, Mode mode) {
  const Tokens &t = tokens(resolve(mode));
  app.setStyle(QStyleFactory::create("Fusion"));
  app.setPalette(palette(t));
  app.setStyleSheet(styleSheet(t));
  return t;
}

} // namespace Theme

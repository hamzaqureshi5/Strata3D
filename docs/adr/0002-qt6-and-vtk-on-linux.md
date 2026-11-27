# 0002: Qt 6 and VTK render windows owned by Qt

**Status:** accepted (2026-11)

## Context

The original code targeted Qt 5 on Windows. Ubuntu's VTK Qt module is built against
Qt 6, and Qt 5 and Qt 6 cannot be mixed in one program. On a Wayland desktop the app
crashed (`BadAccess` on `X_GLXMakeCurrent`) because VTK created its own X11/GLX window,
and on NVIDIA nothing rendered without a core OpenGL context.

## Decision

- Use Qt 6 and VTK 9 with `QVTKOpenGLNativeWidget`.
- Request `QVTKOpenGLNativeWidget::defaultFormat()` before `QApplication` is created.
- Register a VTK object-factory override so every `vtkRenderWindow` is a
  `vtkGenericOpenGLRenderWindow`, and use the widget's `QVTKInteractor`; never start a
  separate VTK event loop.

## Consequences

- Runs on Wayland and X11 with NVIDIA and Mesa drivers.
- Code must not create standalone VTK windows or interactors in the GUI.

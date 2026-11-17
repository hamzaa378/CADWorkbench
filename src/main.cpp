#include "ui/MainWindow.h"

#include <QApplication>
#include <QStyleFactory>
#include <QSurfaceFormat>

int main(int argc, char** argv) {
    // OCCT owns the GL rendering context in its native child HWND.
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    QApplication::setApplicationName("CADWorkbench");
    QApplication::setOrganizationName("CADWorkbenchProject");
    app.setStyle(QStyleFactory::create("Fusion"));
    // Compact, neutral workbench styling: retain the user's system palette.
    app.setStyleSheet(R"CAD(
        QTabBar::tab { padding: 7px 16px; margin-right: 2px; }
        QTabBar::tab:selected { border-bottom: 2px solid palette(highlight); }
        QDockWidget::title { padding: 6px 7px; font-weight: 600; }
        QToolBar { spacing: 4px; padding: 3px; }
        QToolButton { padding: 3px; border-radius: 4px; }
        QToolButton:hover { background: palette(midlight); }
        QLineEdit#ModelBrowserFilter { padding: 7px; border: 1px solid palette(mid); border-radius: 4px; }
        QListWidget { alternate-background-color: palette(alternate-base); }
    )CAD");
    MainWindow window;
    window.show();
    return app.exec();
}

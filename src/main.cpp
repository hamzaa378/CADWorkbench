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
    MainWindow window;
    window.show();
    return app.exec();
}

#pragma once
#include "core/SceneDocument.h"

#include <QHash>
#include <QPoint>
#include <QStringList>
#include <QWidget>

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>

class QPaintEngine;
class QMouseEvent;
class QWheelEvent;

// All mouse coordinates are converted from logical Qt pixels to physical Win32 pixels.
// The scene remains authoritative in SceneDocument; AIS presentations are disposable views.
class CadViewport final : public QWidget {
    Q_OBJECT
public:
    enum class SelectionMode { Body, Face, Edge, Vertex };
    explicit CadViewport(QWidget* parent = nullptr);
    void updateScene(const QVector<cad::Part>& parts, bool fit = false);
    void selectIds(const QStringList& ids);
    QStringList selectedIds() const;
    QVector<cad::EdgeReference> selectedEdgeReferences() const;
    QVector<cad::FaceReference> selectedFaceReferences() const;
    QVector<cad::VertexReference> selectedVertexReferences() const;
    void showPreviewShape(const TopoDS_Shape& shape);
    void clearPreviewShape();
    void fitAll();
    void fitSelection();
    void isoView();
    void topView();
    void frontView();
    void rightView();
    void setWireframe(bool enabled);
    void setSelectionMode(SelectionMode mode);
    void setGridVisible(bool enabled);
    SelectionMode selectionMode() const noexcept { return selectionMode_; }

signals:
    void selectedObjectsChanged(const QStringList& ids);

protected:
    QPaintEngine* paintEngine() const override { return nullptr; }
    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void initialize();
    void rebuild(bool fit);
    void applySelectionMode();
    QPoint physical(const QPointF& p) const;

    Handle(V3d_Viewer) viewer_;
    Handle(V3d_View) view_;
    Handle(AIS_InteractiveContext) context_;
    QHash<QString, Handle(AIS_Shape)> presentations_;
    Handle(AIS_Shape) previewPresentation_;
    QVector<cad::Part> parts_;
    QPoint lastMouse_;
    QPoint pressMouse_;
    Qt::MouseButton dragMode_ = Qt::NoButton;
    SelectionMode selectionMode_ = SelectionMode::Body;
    bool initialized_ = false;
    bool wireframe_ = false;
    bool gridVisible_ = false;
    bool orbitDrag_ = false;
};

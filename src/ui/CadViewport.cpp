#include "CadViewport.h"

#include <Aspect_DisplayConnection.hxx>
#include <Aspect_GridType.hxx>
#include <Aspect_GridDrawMode.hxx>
#include <Aspect_TypeOfLine.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <Graphic3d_MaterialAspect.hxx>
#include <Prs3d_LineAspect.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Quantity_Color.hxx>
#include <V3d_TypeOfOrientation.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#ifdef Q_OS_WIN
#include <WNT_Window.hxx>
#endif

#include <QColor>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QWheelEvent>
#include <QSet>
#include <QTimer>

#include <algorithm>
#include <cmath>

CadViewport::CadViewport(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(360, 260);
}

QPoint CadViewport::physical(const QPointF& point) const {
    const auto ratio = devicePixelRatioF();
    return QPoint(qRound(point.x() * ratio), qRound(point.y() * ratio));
}

void CadViewport::initialize() {
    if (initialized_) return;
#ifdef Q_OS_WIN
    Handle(Aspect_DisplayConnection) display = new Aspect_DisplayConnection();
    Handle(OpenGl_GraphicDriver) driver = new OpenGl_GraphicDriver(display);
    viewer_ = new V3d_Viewer(driver);
    viewer_->SetDefaultLights();
    viewer_->SetLightOn();
    context_ = new AIS_InteractiveContext(viewer_);
    view_ = viewer_->CreateView();
    Handle(WNT_Window) window = new WNT_Window(reinterpret_cast<Aspect_Handle>(winId()));
    view_->SetWindow(window);
    if (!window->IsMapped()) window->Map();
    view_->SetBackgroundColor(Quantity_NOC_GRAY20);
    view_->SetProj(V3d_XposYnegZpos);
    view_->TriedronDisplay(Aspect_TOTP_RIGHT_LOWER, Quantity_NOC_WHITE, 0.08, V3d_ZBUFFER);
    context_->SetDisplayMode(AIS_Shaded, false);
    initialized_ = true;
    if (gridVisible_) viewer_->ActivateGrid(Aspect_GT_Rectangular, Aspect_GDM_Lines);
    rebuild(true);
#endif
}
void CadViewport::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // Wait for QMainWindow/QDockWidget layout to establish the final native
    // viewport size before creating the OCCT window on this HWND.
    QTimer::singleShot(50, this, [this] {
        if (!initialized_) initialize();
        if (initialized_) {
            view_->MustBeResized();
            view_->Redraw();
        }
    });
}
void CadViewport::paintEvent(QPaintEvent*) { if (initialized_) view_->Redraw(); }
void CadViewport::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (initialized_) { view_->MustBeResized(); view_->Redraw(); }
}
void CadViewport::updateScene(const QVector<cad::Part>& parts, bool fit) {
    clearPreviewShape();
    parts_ = parts;
    if (initialized_) rebuild(fit);
}
void CadViewport::showPreviewShape(const TopoDS_Shape& shape) {
    if (!initialized_ || shape.IsNull()) return;
    clearPreviewShape();
    previewPresentation_ = new AIS_Shape(shape);
    previewPresentation_->SetColor(Quantity_Color(0.95,0.70,0.18,Quantity_TOC_RGB));
    previewPresentation_->SetTransparency(0.55);
    context_->Display(previewPresentation_,false);
    context_->SetDisplayMode(previewPresentation_,AIS_Shaded,false);
    context_->Deactivate(previewPresentation_);
    view_->Redraw();
}
void CadViewport::clearPreviewShape() {
    if (initialized_ && !previewPresentation_.IsNull()) context_->Remove(previewPresentation_,false);
    previewPresentation_.Nullify();
    if (initialized_) view_->Redraw();
}
void CadViewport::rebuild(bool fit) {
    if (!initialized_) return;
    // Before removing presentations, retain selection by document ID.
    const QStringList previouslySelected = selectedIds();
    context_->RemoveAll(false);
    presentations_.clear();
    for (const cad::Part& part : parts_) {
        if (!part.visible) continue;
        Handle(AIS_Shape) visual = new AIS_Shape(part.shape);
        const QColor color(part.color);
        if (color.isValid()) {
            const Quantity_Color faceColor(color.redF(),color.greenF(),color.blueF(),Quantity_TOC_RGB);
            visual->SetColor(faceColor);
            Graphic3d_MaterialAspect material(Graphic3d_NOM_PLASTIC);
            material.SetAmbientColor(faceColor);
            material.SetDiffuseColor(faceColor);
            material.SetSpecularColor(Quantity_Color(0.04,0.04,0.04,Quantity_TOC_RGB));
            material.SetShininess(0.12f);
            visual->SetMaterial(material);
        }
        visual->Attributes()->SetFaceBoundaryDraw(true);
        visual->Attributes()->SetFaceBoundaryAspect(new Prs3d_LineAspect(
            Quantity_Color(0.12,0.15,0.18,Quantity_TOC_RGB),Aspect_TOL_SOLID,1.0));
        context_->Display(visual, false);
        context_->SetDisplayMode(visual, wireframe_ ? AIS_WireFrame : AIS_Shaded, false);
        presentations_.insert(part.id, visual);
    }
    applySelectionMode();
    selectIds(previouslySelected);
    if (fit) fitAll();
    view_->Redraw();
}
QStringList CadViewport::selectedIds() const {
    QStringList out;
    if (!initialized_) return out;
    QSet<QString> seen;
    for (context_->InitSelected(); context_->MoreSelected(); context_->NextSelected()) {
        const Handle(AIS_InteractiveObject) selected = context_->SelectedInteractive();
        for (auto it = presentations_.cbegin(); it != presentations_.cend(); ++it) {
            if (it.value() == selected && !seen.contains(it.key())) {
                out.append(it.key()); seen.insert(it.key()); break;
            }
        }
    }
    return out;
}
QVector<cad::EdgeReference> CadViewport::selectedEdgeReferences() const {
    QVector<cad::EdgeReference> references;
    if (!initialized_ || selectionMode_ != SelectionMode::Edge) return references;
    for (context_->InitSelected(); context_->MoreSelected(); context_->NextSelected()) {
        const Handle(AIS_InteractiveObject) selected = context_->SelectedInteractive();
        QString partId;
        for (auto it = presentations_.cbegin(); it != presentations_.cend(); ++it) {
            if (it.value() == selected) { partId = it.key(); break; }
        }
        if (partId.isEmpty()) continue;
        const auto part = std::find_if(parts_.cbegin(), parts_.cend(), [&](const cad::Part& candidate) {
            return candidate.id == partId;
        });
        if (part == parts_.cend()) continue;
        const TopoDS_Shape selectedShape = context_->SelectedShape();
        if (selectedShape.IsNull() || selectedShape.ShapeType() != TopAbs_EDGE) continue;
        int edgeIndex = 0;
        for (TopExp_Explorer explorer(part->shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
            ++edgeIndex;
            if (explorer.Current().IsSame(selectedShape)) {
                references.append({partId, part->geometryToken, edgeIndex});
                break;
            }
        }
    }
    return references;
}
void CadViewport::selectIds(const QStringList& ids) {
    if (!initialized_) return;
    // An external browser selection chooses whole bodies. In face/edge mode it
    // is intentionally not forced; changing to Body restores whole-body selection.
    if (selectionMode_ != SelectionMode::Body) return;
    context_->ClearSelected(false);
    for (const auto& id : ids) {
        const auto found = presentations_.constFind(id);
        if (found != presentations_.cend()) context_->AddOrRemoveSelected(found.value(),false);
    }
    view_->Redraw();
}
void CadViewport::fitAll() {
    if (!initialized_) return;
    Bnd_Box bounds;
    for (const cad::Part& part : parts_) {
        if (part.visible && presentations_.contains(part.id)) BRepBndLib::Add(part.shape, bounds);
    }
    if (!bounds.IsVoid()) view_->FitAll(bounds, 0.12, false);
    view_->ZFitAll();
    view_->Redraw();
}
void CadViewport::fitSelection() {
    if (!initialized_) return;
    const QStringList ids = selectedIds();
    if (ids.isEmpty()) return;
    QSet<QString> selected;
    for (const QString& id : ids) selected.insert(id);
    Bnd_Box bounds;
    for (const cad::Part& part : parts_) {
        if (part.visible && selected.contains(part.id)) BRepBndLib::Add(part.shape, bounds);
    }
    if (bounds.IsVoid()) return;
    view_->FitAll(bounds, 0.12, false);
    view_->ZFitAll();
    view_->Redraw();
}
void CadViewport::isoView() {
    if (!initialized_) return;
    view_->SetProj(1.0, -1.0, 1.0);
    view_->SetUp(0.0, 0.0, 1.0);
    fitAll();
}
void CadViewport::topView() {
    if (!initialized_) return;
    view_->SetProj(0.0, 0.0, 1.0);
    view_->SetUp(0.0, 1.0, 0.0);
    fitAll();
}
void CadViewport::frontView() {
    if (!initialized_) return;
    view_->SetProj(0.0, -1.0, 0.0);
    view_->SetUp(0.0, 0.0, 1.0);
    fitAll();
}
void CadViewport::rightView() {
    if (!initialized_) return;
    view_->SetProj(1.0, 0.0, 0.0);
    view_->SetUp(0.0, 0.0, 1.0);
    fitAll();
}
void CadViewport::setWireframe(bool enabled) {
    if (wireframe_ == enabled) return;
    wireframe_ = enabled;
    if (initialized_) rebuild(false);
}
void CadViewport::setSelectionMode(SelectionMode mode) {
    if (selectionMode_ == mode) return;
    selectionMode_ = mode;
    if (!initialized_) return;
    context_->ClearSelected(false);
    applySelectionMode();
    view_->Redraw();
    emit selectedObjectsChanged({});
}
void CadViewport::applySelectionMode() {
    if (!initialized_) return;
    const int mode = selectionMode_ == SelectionMode::Body ? 0 :
                     AIS_Shape::SelectionMode(selectionMode_ == SelectionMode::Face ? TopAbs_FACE : TopAbs_EDGE);
    for (auto it = presentations_.cbegin(); it != presentations_.cend(); ++it) {
        context_->Deactivate(it.value());
        context_->Activate(it.value(), mode, false);
    }
}
void CadViewport::setGridVisible(bool enabled) {
    gridVisible_ = enabled;
    if (!initialized_) return;
    if (enabled) viewer_->ActivateGrid(Aspect_GT_Rectangular, Aspect_GDM_Lines);
    else viewer_->DeactivateGrid();
    view_->Redraw();
}
void CadViewport::mousePressEvent(QMouseEvent* event) {
    if (!initialized_) return;
    const QPoint position = physical(event->position());
    pressMouse_ = position;
    lastMouse_ = position;
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
        dragMode_ = event->button();
        orbitDrag_ = true;
        view_->StartRotation(position.x(),position.y());
    } else if (event->button() == Qt::MiddleButton) {
        dragMode_ = Qt::MiddleButton;
        orbitDrag_ = event->modifiers().testFlag(Qt::ShiftModifier);
        if (orbitDrag_) view_->StartRotation(position.x(),position.y());
        else view_->Pan(0,0,1.0,true);
    }
    event->accept();
}
void CadViewport::mouseMoveEvent(QMouseEvent* event) {
    if (!initialized_) return;
    const QPoint position = physical(event->position());
    if (dragMode_ != Qt::NoButton && orbitDrag_ && (event->buttons() & dragMode_)) {
        view_->Rotation(position.x(),position.y()); view_->Redraw();
    } else if (dragMode_ == Qt::MiddleButton && !orbitDrag_ && (event->buttons() & Qt::MiddleButton)) {
        // Pan with displacement from gesture origin; toStart=false uses stored starting view.
        view_->Pan(position.x()-pressMouse_.x(),pressMouse_.y()-position.y(),1.0,false);
        view_->Redraw();
    } else {
        context_->MoveTo(position.x(),position.y(),view_,true);
    }
    lastMouse_ = position;
    event->accept();
}
void CadViewport::mouseReleaseEvent(QMouseEvent* event) {
    if (!initialized_) return;
    const QPoint position = physical(event->position());
    if (event->button() == Qt::LeftButton && (position-pressMouse_).manhattanLength() < 6) {
        context_->MoveTo(position.x(),position.y(),view_,false);
        context_->SelectDetected(event->modifiers().testFlag(Qt::ControlModifier) ?
                                  AIS_SelectionScheme_XOR : AIS_SelectionScheme_Replace);
        view_->Redraw();
        emit selectedObjectsChanged(selectedIds());
    }
    if (event->button() == dragMode_) { dragMode_ = Qt::NoButton; orbitDrag_ = false; }
    event->accept();
}
void CadViewport::wheelEvent(QWheelEvent* event) {
    if (!initialized_) return;
    const double steps = static_cast<double>(event->angleDelta().y()) / 120.0;
    if (steps != 0.0) {
        const double factor = std::pow(1.18,std::clamp(steps,-8.0,8.0));
        double width = 0, height = 0;
        view_->Size(width,height);
        view_->SetSize(std::clamp(std::max(width,height)/factor,0.001,1e9));
        view_->Redraw();
    }
    event->accept();
}

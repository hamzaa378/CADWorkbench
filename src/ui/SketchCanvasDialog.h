#pragma once
#include "core/SketchProfile.h"
#include "core/SketchSolver.h"
#include <QDialog>
#include <QWidget>
#include <QPointF>
#include <QVector>
#include <QStringList>
#include <functional>

class QLabel;
class QDoubleSpinBox;
class QCheckBox;
class QComboBox;
class QPaintEvent;
class QMouseEvent;
class QWheelEvent;
class QKeyEvent;

// An offline 2D XY profile authoring canvas, backed by the same persistent
// SketchModel that the constrained polygon extrusion consumes. No duplicated
// or ephemeral document geometry is committed until the dialog is accepted.
class SketchCanvasWidget final : public QWidget {
public:
    enum class Tool { Select, Polyline, Circle, Arc, Spline, Rectangle, Slot };
    enum class Relation { None, Horizontal, Vertical, Length, Parallel, Perpendicular,
                          Distance, Coincident, Fixed, Unfix, EqualLength, Angle, EqualRadius, Concentric };
    explicit SketchCanvasWidget(QWidget* parent=nullptr);
    void setSketch(const cad::SketchModel& sketch,const QStringList& boundary);
    const cad::SketchModel& sketch() const {return model_;}
    const QStringList& boundary() const {return boundary_;}
    void setTool(Tool tool);
    void setRelation(Relation relation);
    Relation relation() const {return relation_;}
    void setGridSnap(bool state){snapGrid_=state;update();}
    void setPointSnap(bool state){snapPoints_=state;update();}
    void closeBoundary();
    void offsetStraightOutline(double distanceMm);
    void alignEdgeUnderCursor(bool horizontal);
    void removeCurveUnderCursor();
    void clearSketch();
    void undoEdit();
    // User-editable constraints share the sketch's transactional solver, valid
    // profile check and undo history. Invalid edits leave the model untouched.
    QStringList constraintDescriptions() const;
    bool changeConstraintDimension(int index,double newValue);
    bool changeFixedPointAnchor(int index,double xMm,double yMm);
    bool removeConstraint(int index);
    // Two-entity relations use stable sketch IDs; both the canvas click tools
    // and native regression tests exercise these exact transactional paths.
    bool constrainSegmentPair(const QString& firstId,const QString& secondId,
                              cad::SketchConstraintType type,double angleDegrees=0.0);
    bool constrainCirclePair(const QString& firstId,const QString& secondId,
                             cad::SketchConstraintType type);
    void zoomToFit();
    std::function<void(const QString&)> status;
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
private:
    struct Snapshot {cad::SketchModel sketch;QStringList boundary;bool closed=false;};
    void checkpoint();
    QPointF screen(const QPointF& mm) const;
    QPointF world(const QPointF& point) const;
    QPointF snapped(const QPointF& mm,const QString& exclude=QString()) const;
    int hitPoint(const QPointF& cursor) const;
    int hitCircle(const QPointF& cursor) const;
    void fixPoint(const QString& id,const QPointF& p);
    QString newPointId() const;
    void addPoint(const QPointF& p);
    void regenerateBoundarySegments();
    int nearestBoundaryEdge(const QPointF& cursor) const;
    void handleRelationClick(const QPointF& cursor);
    bool tryRelation(cad::SketchModel trial,const QString& description);
    void completeCurvePoint(const QPointF& mm);
    bool closed_=false;
    bool snapGrid_=true;
    bool snapPoints_=true;
    Tool tool_=Tool::Polyline;
    Relation relation_=Relation::None;
    QString firstRelationPoint_;
    QString firstRelationSegment_;
    QString firstRelationCircle_;
    cad::SketchModel model_;
    QStringList boundary_;
    QVector<Snapshot> undo_;
    QPointF center_{0,0};
    double pixelsPerMm_=15;
    int dragging_=-1;
    bool panning_=false;
    QPointF lastCursor_;
    bool circlePending_=false;
    bool rectanglePending_=false;
    QPointF rectangleStart_;
    int slotStep_=0;
    QPointF slotStart_,slotEnd_;
    QPointF pendingCenter_;
    QPointF mouseWorld_;
    int curveEdgeIndex_=-1;
    QVector<QPointF> pendingControls_;
};

class SketchCanvasDialog final : public QDialog {
public:
    explicit SketchCanvasDialog(QWidget* parent=nullptr);
    void setSketch(const cad::SketchModel& sketch,const QStringList& boundary,double depth,
                   const cad::SketchPlane& plane={});
    void setPlane(const cad::SketchPlane& plane);
    cad::SketchPlane plane() const;
    cad::SketchModel sketch() const;
    QStringList boundary() const;
    double depth() const;
private:
    SketchCanvasWidget* canvas_=nullptr;
    QComboBox* toolPicker_=nullptr;
    QLabel* feedback_=nullptr;
    QDoubleSpinBox* depth_=nullptr;
    QComboBox* planePreset_=nullptr;
    QVector<QDoubleSpinBox*> planeFields_;
};

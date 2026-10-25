#pragma once
#include "core/SceneDocument.h"
#include <QMainWindow>
#include <functional>

class CadViewport;
class QTreeWidget;
class QLabel;
class QAction;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
protected:
    void closeEvent(QCloseEvent* event) override;
private:
    void buildUi();
    void refresh(bool fit = false);
    void updateInspector(const QStringList& ids);
    void makeShape(cad::Primitive type);
    void makeOrientedCylinder();
    void makeOrientedCone();
    void makeTorusSegment();
    void makeEngineeringPart(cad::EngineeringPart kind);
    void makeAdvancedPart(cad::AdvancedPart kind);
    void makeSpecializedPart(cad::SpecializedPart kind);
    void makeIndustrialPart(cad::IndustrialPart kind);
    void makeExtendedPart(cad::ExtendedPart kind);
    void makePrecisionPart(cad::PrecisionPart kind);
    void makeProductionPart(cad::ProductionPart kind);
    void makeModelingTool(cad::ModelingTool kind);
    void sketchExtrusion();
    void loftRectangles();
    void revolveRectangleProfile();
    void revolveAboutWorldAxis();
    void moveObject();
    void rotateObject();
    void scaleObject();
    void duplicateObject();
    void renameObject();
    void recolorObject();
    void runBoolean(cad::BooleanOp operation);
    void filletSelectedEdge();
    void chamferSelectedEdge();
    void filletAllBodyEdges();
    void chamferAllBodyEdges();
    void mirrorSelectedBody();
    void mirrorBodyAcrossPlane();
    void rotateBodyAboutAxis();
    void linearPatternSelectedBody();
    void rectangularPatternSelectedBody();
    void circularPatternSelectedBody();
    void holeSelectedBody();
    void holeGridSelectedBody();
    void splitSelectedBody();
    void shellSelectedBody();
    void draftSelectedFace();
    void showMassProperties();
    void removeSelected();
    void newFile();
    void openFile();
    bool saveFile(bool as = false);
    void exportFile(bool step);
    bool maybeSave();
    void confirmPreview(const QString& title, const QString& message, const TopoDS_Shape& shape,
                        std::function<void()> applyAction);
    QStringList selectedIds() const;
    void reportError(const QString& action, const QString& description);
    bool requireBodyMode(const QString& action);

    cad::SceneDocument document_;
    CadViewport* viewport_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QLabel* detail_ = nullptr;
    QAction* undoAction_ = nullptr;
    QAction* redoAction_ = nullptr;
    QAction* bodyAction_ = nullptr;
};

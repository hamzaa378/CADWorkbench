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
    void sweepCircularProfile();
    void sweepRectangularProfile();
    void sweepEllipticalProfile();
    void makeHelicalSweep();
    void makeVariablePitchHelicalSweep();
    void makeTaperedHelicalSweep();
    void makeEllipticalPrism();
    void makeEllipticalTube();
    void makeCircularLoft();
    void makeRectangularToCircularLoft();
    void makeHollowLoftReducer();
    void makeEllipticalLoft();
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
    void revolveAboutCustomAxis();
    void moveObject();
    void rotateObject();
    void scaleObject();
    void scaleNonUniformSelectedBody();
    void duplicateObject();
    void renameObject();
    void recolorObject();
    void assignMaterial();
    void editPartMetadata();
    void showPartsReports();
    void runStandardsCheck();
    void checkDrawingSheet();
    void runBoolean(cad::BooleanOp operation);
    void filletSelectedEdge();
    void chamferSelectedEdge();
    void asymmetricChamferSelectedEdge();
    void filletAllBodyEdges();
    void chamferAllBodyEdges();
    void mirrorSelectedBody();
    void mirrorBodyAcrossPlane();
    void rotateBodyAboutAxis();
    void linearPatternSelectedBody();
    void linearPatternAlongVectorSelectedBody();
    void rectangularPatternSelectedBody();
    void rectangularPatternAlongVectorsSelectedBody();
    void circularPatternSelectedBody();
    void circularPatternAboutAxisSelectedBody();
    void holeSelectedBody();
    void holeGridSelectedBody();
    void circularHolePatternSelectedBody();
    void circularCountersinkPatternSelectedBody();
    void circularCounterborePatternSelectedBody();
    void linearCounterborePatternSelectedBody();
    void linearCountersinkPatternSelectedBody();
    void linearKeyholeSlotPatternSelectedBody();
    void dovetailGrooveSelectedBody();
    void tSlotGrooveSelectedBody();
    void roundedSlotSelectedBody();
    void keyholeSlotSelectedBody();
    void roundedSlotPatternSelectedBody();
    void splitSelectedBody();
    void shellSelectedBody();
    void draftSelectedFace();
    void showMassProperties();
    void diagnoseSelectedBody();
    void measureSelectedBodySection();
    void summarizeSelectedBodySurfaces();
    void measureSelectedFaceArea();
    void measureSelectedEdgeLength();
    void measureSelectedVertexCoordinates();
    void measureSelectedVertexDistance();
    void measureSelectedBodyClearance();
    void measureSelectedBodyInterference();
    void removeSelected();
    void newFile();
    void openFile();
    void importSTEPFile();
    void exportBodyScheduleCsv();
    void exportBillOfMaterialsCsv();
    void exportPartsListCsv();
    void exportDrawingSvg();
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

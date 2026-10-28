#include "MainWindow.h"
#include "core/DrawingSheet.h"
#include "core/SheetMetal.h"
#include "CadViewport.h"

#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>

#include <QAction>
#include <QActionGroup>
#include <QAbstractItemView>
#include <QApplication>
#include <QCloseEvent>
#include <QColorDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDate>
#include <QDateEdit>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFutureWatcher>
#include <QHash>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMenuBar>
#include <QMessageBox>
#include <QInputDialog>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QToolButton>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QtConcurrent/QtConcurrentRun>

#include <optional>
#include <algorithm>
#include <utility>

namespace {
std::optional<QVector<double>> valuesDialog(QWidget* parent, const QString& title,
                                              const QStringList& names, const QVector<double>& defaults,
                                              double min = 0.00001, double max = 1e6) {
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setMinimumWidth(340);
    auto* layout = new QFormLayout(&dialog);
    QVector<QDoubleSpinBox*> fields;
    for (int i=0; i<names.size(); ++i) {
        auto* field = new QDoubleSpinBox(&dialog);
        field->setRange(min,max);
        field->setDecimals(5);
        field->setSingleStep(1.0);
        field->setValue(defaults[i]);
        layout->addRow(names[i],field);
        fields.append(field);
    }
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,&dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    QVector<double> output;
    for (auto* field : fields) output.append(field->value());
    return output;
}
int numberOf(const TopoDS_Shape& shape, TopAbs_ShapeEnum type) {
    int count = 0;
    for (TopExp_Explorer explorer(shape,type); explorer.More(); explorer.Next()) ++count;
    return count;
}

template <typename Preview, typename Builder, typename Completed>
void runPreviewAsync(QObject* owner, QStatusBar* statusBar, const QString& errorAction,
                     const QString& statusMessage, Builder builder, Completed completed) {
    statusBar->showMessage(statusMessage);
    auto* watcher = new QFutureWatcher<Preview>(owner);
    QObject::connect(watcher, &QFutureWatcher<Preview>::finished, owner,
                     [watcher, statusBar, owner, errorAction, completed = std::move(completed)]() mutable {
        const Preview preview = watcher->result();
        watcher->deleteLater();
        statusBar->clearMessage();
        if (preview.shape.IsNull()) {
            QMessageBox::warning(static_cast<QWidget*>(owner), errorAction, preview.error);
            return;
        }
        completed(preview);
    });
    watcher->setFuture(QtConcurrent::run(std::move(builder)));
}
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    buildUi();
    refresh();
    resize(1320,800);
}

void MainWindow::confirmPreview(const QString& title, const QString& message,
                                const TopoDS_Shape& shape, std::function<void()> applyAction) {
    // The geometry job finishes asynchronously, after the parameter dialog's input event.
    QTimer::singleShot(0, this, [this, title, message, shape, applyAction = std::move(applyAction)]() mutable {
        viewport_->showPreviewShape(shape);
        auto* confirmation = new QMessageBox(QMessageBox::Question, title, message,
                                             QMessageBox::Apply | QMessageBox::Cancel, this);
        confirmation->setAttribute(Qt::WA_DeleteOnClose);
        confirmation->setDefaultButton(QMessageBox::Cancel);
        connect(confirmation, &QMessageBox::finished, this,
                [this, applyAction = std::move(applyAction)](int answer) mutable {
            viewport_->clearPreviewShape();
            if (answer == QMessageBox::Apply && applyAction) applyAction();
        });
        confirmation->open();
    });
}

void MainWindow::buildUi() {
    viewport_ = new CadViewport(this);
    setCentralWidget(viewport_);

    tree_ = new QTreeWidget(this);
    tree_->setHeaderLabels({"Bodies / components", "Visible"});
    tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree_->setAlternatingRowColors(true);
    tree_->setRootIsDecorated(false);
    auto* browser = new QDockWidget("Model browser",this);
    browser->setObjectName("ModelBrowserDock");
    browser->setWidget(tree_);
    browser->setMinimumWidth(225);
    addDockWidget(Qt::LeftDockWidgetArea,browser);

    detail_ = new QLabel("Select a body to inspect its geometry.",this);
    detail_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    detail_->setWordWrap(true);
    detail_->setMargin(12);
    auto* properties = new QDockWidget("Properties",this);
    properties->setObjectName("PropertiesDock");
    properties->setWidget(detail_);
    properties->setMinimumWidth(240);
    addDockWidget(Qt::RightDockWidgetArea,properties);

    auto* file = menuBar()->addMenu("&File");
    auto* newAction = file->addAction("&New",this,&MainWindow::newFile,QKeySequence::New);
    auto* openAction = file->addAction("&Open...",this,&MainWindow::openFile,QKeySequence::Open);
    auto* saveAction = file->addAction("&Save",this,[this]{saveFile();},QKeySequence::Save);
    auto* saveAsAction = file->addAction("Save &As...",this,[this]{saveFile(true);},QKeySequence::SaveAs);
    file->addSeparator();
    auto* importStepAction = file->addAction("Import STEP...",this,&MainWindow::importSTEPFile);
    file->addAction("Import IGES solids...",this,&MainWindow::importIGESFile);
    file->addAction("Import closed OBJ mesh as faceted solid...",this,&MainWindow::importOBJMeshFile);
    auto* importStlMeshAction = file->addAction("Import closed STL mesh as faceted solid...",this,&MainWindow::importSTLMeshFile);
    auto* exportStepAction = file->addAction("Export STEP...",this,[this]{exportFile(true);});
    file->addAction("Export IGES...",this,&MainWindow::exportIGESFile);
    auto* exportStlAction = file->addAction("Export STL...",this,[this]{exportFile(false);});
    auto* exportBodyScheduleAction=file->addAction("Export body schedule CSV...",this,&MainWindow::exportBodyScheduleCsv);
    auto* exportBomAction=file->addAction("Export bill of materials CSV...",this,&MainWindow::exportBillOfMaterialsCsv);
    auto* exportPartsListAction=file->addAction("Export parts list CSV...",this,&MainWindow::exportPartsListCsv);
    auto* exportDrawingAction=file->addAction("Export orthographic drawing SVG...",this,&MainWindow::exportDrawingSvg);
    file->addAction("Export orthographic drawing PDF...",this,&MainWindow::exportDrawingPdf);
    auto* arrangeDrawingViewsAction=file->addAction("Arrange saved drawing views...",this,&MainWindow::editDrawingViewLayout);
    auto* editDrawingNotesAction=file->addAction("Edit saved drawing notes...",this,&MainWindow::editDrawingNotes);
    auto* editDrawingDimensionsAction=file->addAction("Edit saved drawing dimensions...",this,&MainWindow::editDrawingDimensions);
    auto* editDrawingPartsListAction=file->addAction("Place linked drawing parts list...",this,&MainWindow::editDrawingPartsList);
    auto* editDrawingBalloonsAction=file->addAction("Edit saved BOM balloons...",this,&MainWindow::editDrawingBalloons);
    auto* editDrawingDetailsAction=file->addAction("Edit saved detail views...",this,&MainWindow::editDrawingDetails);
    auto* editDrawingSectionsAction=file->addAction("Edit saved section views...",this,&MainWindow::editDrawingSections);
    auto* inspectDrawingSourceAction=file->addAction("Check saved drawing source...",this,&MainWindow::inspectSavedDrawingSource);
    file->addAction("Refresh saved drawing source link...",this,&MainWindow::refreshSavedDrawingSource);
    auto* exportFlatPatternAction=file->addAction("Export one-bend sheet-metal flat pattern SVG...",this,&MainWindow::exportSheetMetalFlatPattern);
    auto* exportMultiBendDxfAction=file->addAction("Export multi-bend sheet-metal flat pattern DXF...",this,&MainWindow::exportMultiBendSheetMetalDxf);
    file->addSeparator();
    auto* exitAction = file->addAction("E&xit",this,&QWidget::close);

    auto* edit = menuBar()->addMenu("&Edit");
    undoAction_ = edit->addAction("&Undo",this,[this]{if(document_.undo()) refresh();},QKeySequence::Undo);
    redoAction_ = edit->addAction("&Redo",this,[this]{if(document_.redo()) refresh();},QKeySequence::Redo);
    edit->addSeparator();
    auto* renameAction = edit->addAction("Rename selected...",this,&MainWindow::renameObject,QKeySequence("F2"));
    auto* colorAction = edit->addAction("Body color...",this,&MainWindow::recolorObject);
    auto* materialAction = edit->addAction("Assign material...",this,&MainWindow::assignMaterial);
    auto* metadataAction = edit->addAction("Part number / description / revision...",this,&MainWindow::editPartMetadata);
    auto* editParameterAction = edit->addAction("Edit named parameters...",this,&MainWindow::editNamedParameter);
    auto* featureHistoryAction = edit->addAction("Show selected sketch constraints / feature history...",this,&MainWindow::showSelectedFeatureHistory);
    auto* deleteAction = edit->addAction("Delete selected",this,&MainWindow::removeSelected,QKeySequence::Delete);

    auto* assemblyMenu = menuBar()->addMenu("&Assembly");
    auto* makeComponentAction = assemblyMenu->addAction("Make selected body a component...",this,&MainWindow::makeSelectedComponent);
    auto* insertOccurrenceAction = assemblyMenu->addAction("Insert component occurrence...",this,&MainWindow::insertSelectedComponentOccurrence);
    assemblyMenu->addAction("Synchronize selected component definition to its occurrences...",this,&MainWindow::synchronizeSelectedComponentDefinition);
    auto* rigidJointAction = assemblyMenu->addAction("Create rigid joint (select parent then child)...",this,&MainWindow::createRigidJoint);
    auto* revoluteJointAction = assemblyMenu->addAction("Create revolute joint (select parent then child)...",this,&MainWindow::createRevoluteJoint);
    auto* driveJointAction = assemblyMenu->addAction("Drive selected revolute joint...",this,&MainWindow::driveSelectedRevoluteJoint);
    assemblyMenu->addAction("Create prismatic joint (select parent then child)...",this,&MainWindow::createPrismaticJoint);
    assemblyMenu->addAction("Drive selected prismatic joint...",this,&MainWindow::driveSelectedPrismaticJoint);
    assemblyMenu->addAction("Edit selected joint motion track...",this,&MainWindow::editSelectedJointMotionTrack);
    assemblyMenu->addAction("Evaluate assembly motion study at time...",this,&MainWindow::evaluateAssemblyMotionStudy);

    auto* create = menuBar()->addMenu("&Create");
    auto* boxAction = create->addAction("Box...",this,[this]{makeShape(cad::Primitive::Box);});
    auto* cylAction = create->addAction("Cylinder...",this,[this]{makeShape(cad::Primitive::Cylinder);});
    auto* sphereAction = create->addAction("Sphere...",this,[this]{makeShape(cad::Primitive::Sphere);});
    auto* coneAction = create->addAction("Cone...",this,[this]{makeShape(cad::Primitive::Cone);});
    auto* torusAction = create->addAction("Torus...",this,[this]{makeShape(cad::Primitive::Torus);});
    auto* prismAction = create->addAction("Regular prism...",this,[this]{makeShape(cad::Primitive::RegularPrism);});
    auto* capsuleAction = create->addAction("Capsule...",this,[this]{makeShape(cad::Primitive::Capsule);});
    auto* ellipsoidAction = create->addAction("Ellipsoid...",this,[this]{makeShape(cad::Primitive::Ellipsoid);});
    auto* tubeAction = create->addAction("Tube...",this,[this]{makeShape(cad::Primitive::Tube);});
    auto* wedgeAction = create->addAction("Wedge...",this,[this]{makeShape(cad::Primitive::Wedge);});
    auto* frustumAction = create->addAction("Pyramid frustum...",this,[this]{makeShape(cad::Primitive::PyramidFrustum);});
    auto* nutAction = create->addAction("Hex nut...",this,[this]{makeShape(cad::Primitive::HexNut);});
    auto* orientedCylinderAction=create->addAction("Oriented cylinder...",this,&MainWindow::makeOrientedCylinder);
    auto* orientedConeAction=create->addAction("Oriented cone...",this,&MainWindow::makeOrientedCone);
    auto* torusSegmentAction=create->addAction("Torus segment...",this,&MainWindow::makeTorusSegment);
    auto* sweepCircleAction=create->addAction("Sweep circular profile...",this,&MainWindow::sweepCircularProfile);
    auto* sweepRectangleAction=create->addAction("Sweep rectangular profile...",this,&MainWindow::sweepRectangularProfile);
    auto* sweepEllipseAction=create->addAction("Sweep elliptical profile...",this,&MainWindow::sweepEllipticalProfile);
    auto* helicalSweepAction=create->addAction("Helical sweep...",this,&MainWindow::makeHelicalSweep);
    auto* variablePitchHelixAction=create->addAction("Variable-pitch spring...",this,&MainWindow::makeVariablePitchHelicalSweep);
    auto* taperedHelixAction=create->addAction("Tapered spring...",this,&MainWindow::makeTaperedHelicalSweep);
    auto* ellipticalPrismAction=create->addAction("Elliptical prism...",this,&MainWindow::makeEllipticalPrism);
    auto* ellipticalTubeAction=create->addAction("Elliptical tube...",this,&MainWindow::makeEllipticalTube);
    auto* circularLoftAction=create->addAction("Circular loft...",this,&MainWindow::makeCircularLoft);
    auto* rectToCircleLoftAction=create->addAction("Rectangular to circular transition...",this,&MainWindow::makeRectangularToCircularLoft);
    auto* hollowReducerAction=create->addAction("Hollow loft reducer...",this,&MainWindow::makeHollowLoftReducer);
    auto* ellipticalLoftAction=create->addAction("Elliptical loft...",this,&MainWindow::makeEllipticalLoft);
    auto* extrusionAction = create->addAction("Sketch and extrude profile...",this,&MainWindow::sketchExtrusion);
    auto* parametricExtrusionAction = create->addAction("Constrained rectangle + editable extrusion...",this,&MainWindow::createParametricExtrusion);
    auto* parametricOffsetAction = create->addAction("Constrained offset rectangle + extrusion...",this,&MainWindow::createParametricOffsetExtrusion);
    auto* parametricHoleAction = create->addAction("Add parametric through-hole to selected base...",this,&MainWindow::addParametricThroughHole);
    create->addAction("Pattern parametric through-hole...",this,&MainWindow::addParametricLinearHolePattern);
    auto* loftAction = create->addAction("Loft between rectangles...",this,&MainWindow::loftRectangles);
    auto* revolveAction = create->addAction("Revolve rectangle profile...",this,&MainWindow::revolveRectangleProfile);
    auto* axisRevolveAction = create->addAction("Revolve about X/Y/Z axis...",this,&MainWindow::revolveAboutWorldAxis);
    create->addAction("Revolve about custom axis...",this,&MainWindow::revolveAboutCustomAxis);
    auto* mechanicalMenu=create->addMenu("Mechanical Components");
    QList<QAction*> mechanicalActions;
    for(int i=0;i<static_cast<int>(cad::EngineeringPart::Count);++i){
        const auto kind=static_cast<cad::EngineeringPart>(i);
        const auto spec=cad::engineeringPartSpec(kind);
        auto* action=mechanicalMenu->addAction(spec.name+"...");
        connect(action,&QAction::triggered,this,[this,kind]{makeEngineeringPart(kind);});
        mechanicalActions.append(action);
    }
    auto* advancedMenu=create->addMenu("Advanced Components");
    QList<QAction*> advancedActions;
    for(int i=0;i<static_cast<int>(cad::AdvancedPart::Count);++i){
        const auto kind=static_cast<cad::AdvancedPart>(i);
        const auto spec=cad::advancedPartSpec(kind);
        auto* action=advancedMenu->addAction(spec.name+"...");
        connect(action,&QAction::triggered,this,[this,kind]{makeAdvancedPart(kind);});
        advancedActions.append(action);
    }
    auto* specializedMenu=create->addMenu("Specialized Components");
    QList<QAction*> specializedActions;
    for(int i=0;i<static_cast<int>(cad::SpecializedPart::Count);++i){
        const auto kind=static_cast<cad::SpecializedPart>(i);
        const auto spec=cad::specializedPartSpec(kind);
        auto* action=specializedMenu->addAction(spec.name+"...");
        connect(action,&QAction::triggered,this,[this,kind]{makeSpecializedPart(kind);});
        specializedActions.append(action);
    }
    auto* industrialMenu=create->addMenu("Industrial Components");
    QList<QAction*> industrialActions;
    for(int i=0;i<static_cast<int>(cad::IndustrialPart::Count);++i){
        const auto kind=static_cast<cad::IndustrialPart>(i);
        const auto spec=cad::industrialPartSpec(kind);
        auto* action=industrialMenu->addAction(spec.name+"...");
        connect(action,&QAction::triggered,this,[this,kind]{makeIndustrialPart(kind);});
        industrialActions.append(action);
    }
    auto* extendedMenu=create->addMenu("Extended Components");
    QList<QAction*> extendedActions;
    for(int i=0;i<static_cast<int>(cad::ExtendedPart::Count);++i){
        const auto kind=static_cast<cad::ExtendedPart>(i);
        const auto spec=cad::extendedPartSpec(kind);
        auto* action=extendedMenu->addAction(spec.name+"...");
        connect(action,&QAction::triggered,this,[this,kind]{makeExtendedPart(kind);});
        extendedActions.append(action);
    }

    auto* precisionMenu=create->addMenu("Precision && Utility Components");
    QList<QAction*> precisionActions;
    for(int i=0;i<static_cast<int>(cad::PrecisionPart::Count);++i){
        const auto kind=static_cast<cad::PrecisionPart>(i);
        const auto spec=cad::precisionPartSpec(kind);
        auto* action=precisionMenu->addAction(spec.name+"...");
        connect(action,&QAction::triggered,this,[this,kind]{makePrecisionPart(kind);});
        precisionActions.append(action);
    }

    auto* productionMenu=create->addMenu("Production Components");
    QList<QAction*> productionActions;
    for(int i=0;i<static_cast<int>(cad::ProductionPart::Count);++i){
        const auto kind=static_cast<cad::ProductionPart>(i);
        const auto spec=cad::productionPartSpec(kind);
        auto* action=productionMenu->addAction(spec.name+"...");
        connect(action,&QAction::triggered,this,[this,kind]{makeProductionPart(kind);});
        productionActions.append(action);
    }

    auto* modelingToolsMenu=create->addMenu("Modeling Tools");
    QList<QAction*> modelingToolActions;
    for(int i=0;i<static_cast<int>(cad::ModelingTool::Count);++i){
        const auto kind=static_cast<cad::ModelingTool>(i);
        const auto spec=cad::modelingToolSpec(kind);
        auto* action=modelingToolsMenu->addAction(spec.name+"...");
        connect(action,&QAction::triggered,this,[this,kind]{makeModelingTool(kind);});
        modelingToolActions.append(action);
    }

    auto* modify = menuBar()->addMenu("&Modify");
    auto* moveAction = modify->addAction("Move (offset)...",this,&MainWindow::moveObject);
    auto* rotateAction = modify->addAction("Rotate (world origin)...",this,&MainWindow::rotateObject);
    auto* scaleAction = modify->addAction("Uniform scale (world origin)...",this,&MainWindow::scaleObject);
    auto* nonUniformScaleAction = modify->addAction("Nonuniform scale (world origin)...",this,&MainWindow::scaleNonUniformSelectedBody);
    auto* copyAction = modify->addAction("Duplicate with offset...",this,&MainWindow::duplicateObject,QKeySequence("Ctrl+D"));
    modify->addSeparator();
    auto* fuseAction = modify->addAction("Union two bodies",this,[this]{runBoolean(cad::BooleanOp::Fuse);});
    auto* cutAction = modify->addAction("Subtract B from A...",this,[this]{runBoolean(cad::BooleanOp::Cut);});
    auto* commonAction = modify->addAction("Intersect two bodies",this,[this]{runBoolean(cad::BooleanOp::Common);});
    auto* mirrorAction = modify->addAction("Mirror selected body...",this,&MainWindow::mirrorSelectedBody);
    auto* planeMirrorAction = modify->addAction("Mirror across custom plane...",this,&MainWindow::mirrorBodyAcrossPlane);
    auto* axisRotateAction = modify->addAction("Rotate about custom axis...",this,&MainWindow::rotateBodyAboutAxis);
    auto* patternAction = modify->addAction("Linear pattern selected body...",this,&MainWindow::linearPatternSelectedBody);
    auto* vectorPatternAction = modify->addAction("Linear pattern along vector...",this,&MainWindow::linearPatternAlongVectorSelectedBody);
    auto* rectangularPatternAction = modify->addAction("Rectangular pattern selected body...",this,&MainWindow::rectangularPatternSelectedBody);
    auto* vectorRectangularPatternAction = modify->addAction("Rectangular pattern along vectors...",this,&MainWindow::rectangularPatternAlongVectorsSelectedBody);
    auto* circularPatternAction = modify->addAction("Circular pattern selected body...",this,&MainWindow::circularPatternSelectedBody);
    auto* axisPatternAction = modify->addAction("Circular pattern about custom axis...",this,&MainWindow::circularPatternAboutAxisSelectedBody);
    auto* holeAction = modify->addAction("Hole selected body...",this,&MainWindow::holeSelectedBody);
    auto* holeGridAction = modify->addAction("Hole grid on selected body...",this,&MainWindow::holeGridSelectedBody);
    auto* circularHolePatternAction = modify->addAction("Circular hole pattern...",this,&MainWindow::circularHolePatternSelectedBody);
    auto* circularCountersinkPatternAction = modify->addAction("Circular countersink pattern...",this,&MainWindow::circularCountersinkPatternSelectedBody);
    auto* circularCounterborePatternAction = modify->addAction("Circular counterbore pattern...",this,&MainWindow::circularCounterborePatternSelectedBody);
    auto* linearCounterborePatternAction = modify->addAction("Linear counterbore pattern...",this,&MainWindow::linearCounterborePatternSelectedBody);
    auto* linearCountersinkPatternAction = modify->addAction("Linear countersink pattern...",this,&MainWindow::linearCountersinkPatternSelectedBody);
    auto* linearKeyholePatternAction = modify->addAction("Linear keyhole-slot pattern...",this,&MainWindow::linearKeyholeSlotPatternSelectedBody);
    auto* dovetailGrooveAction = modify->addAction("Dovetail groove...",this,&MainWindow::dovetailGrooveSelectedBody);
    auto* tSlotGrooveAction = modify->addAction("T-slot groove...",this,&MainWindow::tSlotGrooveSelectedBody);
    auto* roundedSlotAction = modify->addAction("Rounded slot...",this,&MainWindow::roundedSlotSelectedBody);
    auto* keyholeSlotAction = modify->addAction("Keyhole slot...",this,&MainWindow::keyholeSlotSelectedBody);
    auto* roundedSlotPatternAction = modify->addAction("Linear rounded-slot pattern...",this,&MainWindow::roundedSlotPatternSelectedBody);
    auto* splitAction = modify->addAction("Split selected body...",this,&MainWindow::splitSelectedBody);
    auto* shellAction = modify->addAction("Shell selected body...",this,&MainWindow::shellSelectedBody);
    auto* draftAction = modify->addAction("Draft planar face...",this,&MainWindow::draftSelectedFace);
    modify->addSeparator();
    auto* filletAction = modify->addAction("Fillet selected edges...",this,&MainWindow::filletSelectedEdge);
    auto* chamferAction = modify->addAction("Chamfer selected edges...",this,&MainWindow::chamferSelectedEdge);
    auto* asymmetricChamferAction = modify->addAction("Asymmetric chamfer selected edge...",this,&MainWindow::asymmetricChamferSelectedEdge);
    auto* filletAllAction = modify->addAction("Fillet all body edges...",this,&MainWindow::filletAllBodyEdges);
    auto* chamferAllAction = modify->addAction("Chamfer all body edges...",this,&MainWindow::chamferAllBodyEdges);

    auto* view = menuBar()->addMenu("&View");
    auto* fitAction = view->addAction("Fit all",viewport_,&CadViewport::fitAll,QKeySequence("F"));
    auto* fitSelectionAction = view->addAction("Fit selected",viewport_,&CadViewport::fitSelection,QKeySequence("Shift+F"));
    auto* isoAction = view->addAction("Isometric",viewport_,&CadViewport::isoView,QKeySequence("1"));
    auto* topAction = view->addAction("Top",viewport_,&CadViewport::topView,QKeySequence("2"));
    auto* frontAction = view->addAction("Front",viewport_,&CadViewport::frontView,QKeySequence("3"));
    auto* rightAction = view->addAction("Right",viewport_,&CadViewport::rightView,QKeySequence("4"));
    auto* wireframeAction = view->addAction("Wireframe");
    wireframeAction->setCheckable(true);
    connect(wireframeAction,&QAction::toggled,viewport_,&CadViewport::setWireframe);
    auto* gridAction = view->addAction("XY reference grid");
    gridAction->setCheckable(true);
    connect(gridAction,&QAction::toggled,viewport_,&CadViewport::setGridVisible);

    auto* inspect = menuBar()->addMenu("&Inspect");
    auto* massPropertiesAction = inspect->addAction("Mass properties / measure body...",this,&MainWindow::showMassProperties);
    auto* clearanceAction = inspect->addAction("Minimum distance between bodies...",this,&MainWindow::measureSelectedBodyClearance);
    auto* interferenceAction = inspect->addAction("Interference volume between bodies...",this,&MainWindow::measureSelectedBodyInterference);
    auto* diagnosticsAction = inspect->addAction("Geometry health and topology...",this,&MainWindow::diagnoseSelectedBody);
    auto* sectionAction = inspect->addAction("Planar section contour...",this,&MainWindow::measureSelectedBodySection);
    auto* surfaceSummaryAction = inspect->addAction("Surface type summary...",this,&MainWindow::summarizeSelectedBodySurfaces);
    auto* faceAreaAction = inspect->addAction("Measure selected face area...",this,&MainWindow::measureSelectedFaceArea);
    auto* faceAngleAction = inspect->addAction("Measure angle between planar faces...",this,&MainWindow::measureAngleBetweenSelectedFaces);
    auto* cylinderMeasureAction = inspect->addAction("Measure selected cylindrical face...",this,&MainWindow::measureSelectedCylindricalFace);
    auto* edgeLengthAction = inspect->addAction("Measure selected edge length...",this,&MainWindow::measureSelectedEdgeLength);
    auto* vertexCoordinatesAction = inspect->addAction("Measure selected vertex coordinates...",this,&MainWindow::measureSelectedVertexCoordinates);
    auto* vertexDistanceAction = inspect->addAction("Measure distance between vertices...",this,&MainWindow::measureSelectedVertexDistance);
    inspect->addSeparator();
    auto* partsReportAction = inspect->addAction("BOM and parts list...",this,&MainWindow::showPartsReports);
    auto* standardsCheckAction = inspect->addAction("Run BOM / ISO / ASME checks...",this,&MainWindow::runStandardsCheck);
    auto* drawingCheckAction = inspect->addAction("Check exported drawing SVG...",this,&MainWindow::checkDrawingSheet);

    auto* select = menuBar()->addMenu("&Select");
    auto* group = new QActionGroup(this);
    group->setExclusive(true);
    auto addMode = [&](const QString& label,CadViewport::SelectionMode mode) {
        auto* action = select->addAction(label);
        action->setCheckable(true);
        group->addAction(action);
        connect(action,&QAction::triggered,this,[this,mode]{viewport_->setSelectionMode(mode);});
        return action;
    };
    bodyAction_ = addMode("Bodies (editable)",CadViewport::SelectionMode::Body);
    bodyAction_->setChecked(true);
    auto* faceAction = addMode("Faces (inspection)",CadViewport::SelectionMode::Face);
    auto* edgeAction = addMode("Edges (inspection)",CadViewport::SelectionMode::Edge);
    auto* vertexAction = addMode("Vertices (inspection)",CadViewport::SelectionMode::Vertex);

    // Tabbed CAD ribbon reuses the same QActions as menus so every command behaves identically.
    auto* ribbon = new QTabWidget(this);
    ribbon->setObjectName("CadCommandRibbon");
    ribbon->setDocumentMode(true);
    ribbon->setMovable(false);
    ribbon->setMinimumHeight(116);
    auto addRibbonTab=[ribbon](const QString& tabName,
                               const QVector<QPair<QString,QList<QAction*>>>& groups) {
        auto* scroll=new QScrollArea(ribbon);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* contents=new QWidget(scroll);
        auto* row=new QHBoxLayout(contents);
        row->setContentsMargins(6,4,6,2);
        row->setSpacing(4);
        for (const auto& group:groups) {
            auto* column=new QWidget(contents);
            auto* layout=new QVBoxLayout(column);
            layout->setContentsMargins(5,1,5,1);
            layout->setSpacing(2);
            auto* buttons=new QHBoxLayout;
            buttons->setContentsMargins(0,0,0,0);
            buttons->setSpacing(2);
            for (QAction* action:group.second) {
                auto* button=new QToolButton(column);
                button->setDefaultAction(action);
                button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
                button->setMinimumWidth(82);
                button->setMinimumHeight(72);
                button->setToolTip(action->text().remove('&'));
                buttons->addWidget(button);
            }
            layout->addLayout(buttons);
            auto* caption=new QLabel(group.first,column);
            caption->setAlignment(Qt::AlignCenter);
            caption->setStyleSheet("color: palette(mid); font-size: 10px;");
            layout->addWidget(caption);
            row->addWidget(column);
            if (&group!=&groups.last()) {
                auto* separator=new QFrame(contents);
                separator->setFrameShape(QFrame::VLine);
                separator->setFrameShadow(QFrame::Sunken);
                row->addWidget(separator);
            }
        }
        row->addStretch(1);
        scroll->setWidget(contents);
        ribbon->addTab(scroll,tabName);
    };
    addRibbonTab("File",{
        {"Document",{newAction,openAction,saveAction,saveAsAction,exitAction}},
        {"Import",{importStepAction,importStlMeshAction}},
        {"Export",{exportStepAction,exportStlAction,exportBodyScheduleAction,exportBomAction,exportPartsListAction,exportDrawingAction,exportFlatPatternAction,exportMultiBendDxfAction}},
        {"Drawing",{arrangeDrawingViewsAction,editDrawingNotesAction,editDrawingDimensionsAction,editDrawingPartsListAction,editDrawingBalloonsAction,editDrawingDetailsAction,editDrawingSectionsAction,inspectDrawingSourceAction}},
        {"History",{undoAction_,redoAction_}}
    });
    addRibbonTab("Create",{
        {"Primitives",{boxAction,cylAction,sphereAction,coneAction,torusAction,prismAction,capsuleAction,ellipsoidAction,tubeAction,wedgeAction,frustumAction,nutAction,orientedCylinderAction,orientedConeAction,torusSegmentAction}},
        {"Profiles",{extrusionAction,parametricExtrusionAction,parametricOffsetAction,parametricHoleAction,loftAction,revolveAction,axisRevolveAction,sweepCircleAction,sweepRectangleAction,sweepEllipseAction,helicalSweepAction,variablePitchHelixAction,taperedHelixAction,ellipticalPrismAction,ellipticalTubeAction,circularLoftAction,rectToCircleLoftAction,hollowReducerAction,ellipticalLoftAction}},
        {"Mechanical",mechanicalActions},
        {"Advanced",advancedActions}
        ,{"Specialized",specializedActions},
        {"Industrial",industrialActions},
        {"Extended",extendedActions},
        {"Precision && Utility",precisionActions},
        {"Production",productionActions},
        {"Modeling Tools",modelingToolActions}
    });
    addRibbonTab("Modify",{
        {"Transform",{moveAction,rotateAction,axisRotateAction,scaleAction,nonUniformScaleAction,copyAction,mirrorAction,planeMirrorAction}},
        {"Combine",{fuseAction,cutAction,commonAction}},
        {"Pattern",{patternAction,vectorPatternAction,rectangularPatternAction,vectorRectangularPatternAction,circularPatternAction,axisPatternAction}},
        {"Features",{holeAction,holeGridAction,circularHolePatternAction,circularCountersinkPatternAction,circularCounterborePatternAction,linearCountersinkPatternAction,linearCounterborePatternAction,dovetailGrooveAction,tSlotGrooveAction,roundedSlotAction,keyholeSlotAction,roundedSlotPatternAction,linearKeyholePatternAction,splitAction,shellAction,draftAction,filletAction,chamferAction,asymmetricChamferAction,filletAllAction,chamferAllAction}},
        {"Manage",{renameAction,colorAction,materialAction,metadataAction,editParameterAction,featureHistoryAction,deleteAction}}
    });
    addRibbonTab("Assembly",{
        {"Components",{makeComponentAction,insertOccurrenceAction,rigidJointAction,revoluteJointAction,driveJointAction}},
        {"Reports",{partsReportAction}}
    });
    addRibbonTab("Inspect",{
        {"Measure",{massPropertiesAction,clearanceAction,interferenceAction,diagnosticsAction,sectionAction,surfaceSummaryAction,faceAreaAction,faceAngleAction,cylinderMeasureAction,edgeLengthAction,vertexCoordinatesAction,vertexDistanceAction}},
        {"Parts data",{partsReportAction,standardsCheckAction,drawingCheckAction}},
        {"Selection",{bodyAction_,faceAction,edgeAction,vertexAction}}
    });
    addRibbonTab("View",{
        {"Camera",{fitAction,fitSelectionAction,isoAction,topAction,frontAction,rightAction}},
        {"Display",{wireframeAction,gridAction}}
    });
    auto* ribbonToolbar=addToolBar("CAD Ribbon");
    ribbonToolbar->setMovable(false);
    ribbonToolbar->setFloatable(false);
    ribbonToolbar->setContextMenuPolicy(Qt::PreventContextMenu);
    ribbonToolbar->addWidget(ribbon);

    // Model browser holds document IDs, never presentation pointers.
    connect(tree_,&QTreeWidget::itemSelectionChanged,this,[this]{
        const auto ids = selectedIds();
        if (!bodyAction_->isChecked()) bodyAction_->setChecked(true);
        {
            const QSignalBlocker blocker(viewport_);
            viewport_->setSelectionMode(CadViewport::SelectionMode::Body);
        }
        viewport_->selectIds(ids);
        updateInspector(ids);
    });
    connect(tree_,&QTreeWidget::itemChanged,this,[this](QTreeWidgetItem* item,int column){
        if (column != 1) return;
        const QString id = item->data(0,Qt::UserRole).toString();
        QString error;
        if (!document_.setVisibility(id,item->checkState(1) == Qt::Checked,&error)) reportError("Visibility",error);
        refresh();
    });
    connect(viewport_,&CadViewport::selectedObjectsChanged,this,[this](const QStringList& ids){
        {
            const QSignalBlocker blocker(tree_);
            tree_->clearSelection();
            for (int i=0; i<tree_->topLevelItemCount(); ++i) {
                auto* item = tree_->topLevelItem(i);
                item->setSelected(ids.contains(item->data(0,Qt::UserRole).toString()));
            }
        }
        updateInspector(ids);
    });
    statusBar()->showMessage("Select: left click / Ctrl+click | Orbit: Shift+middle drag or right drag | Pan: middle drag | Zoom: wheel | Units: mm");
}

QStringList MainWindow::selectedIds() const {
    QStringList ids;
    for (auto* item : tree_->selectedItems()) ids.append(item->data(0,Qt::UserRole).toString());
    return ids;
}
void MainWindow::updateInspector(const QStringList& ids) {
    if (ids.isEmpty()) { detail_->setText("Select a body to inspect its geometry.\n\nUse Select â†’ Face or Edge to inspect subshapes.\n\nMeasurements use millimeters."); return; }
    if (ids.size() > 1) { detail_->setText(QString("%1 bodies selected").arg(ids.size())); return; }
    const auto* part = document_.find(ids.first());
    if (!part) { detail_->clear(); return; }
    GProp_GProps props;
    BRepGProp::VolumeProperties(part->shape,props);
    Bnd_Box box;
    BRepBndLib::AddOptimal(part->shape,box,false,false);
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;
    if (!box.IsVoid()) box.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    const QString materialText=part->materialName.isEmpty()?QStringLiteral("Unassigned"):
        QStringLiteral("%1 (%2 kg/m³)").arg(part->materialName).arg(part->materialDensityKgPerM3,0,'f',1);
    detail_->setText(QString("%1\n\nPart number: %2\nDescription: %3\nRevision: %4\nVolume: %5 mm^3\nBounds X: %6 to %7 mm\nBounds Y: %8 to %9 mm\nBounds Z: %10 to %11 mm\nFaces: %12\nEdges: %13\nVisible: %14\nColor: %15\nMaterial: %16\n\nID: %17")
        .arg(part->name).arg(part->partNumber.isEmpty()?QStringLiteral("(not set)"):part->partNumber)
        .arg(part->description.isEmpty()?QStringLiteral("(not set)"):part->description)
        .arg(part->revision.isEmpty()?QStringLiteral("(not set)"):part->revision)
        .arg(props.Mass(),0,'f',3)
        .arg(xmin,0,'f',2).arg(xmax,0,'f',2)
        .arg(ymin,0,'f',2).arg(ymax,0,'f',2)
        .arg(zmin,0,'f',2).arg(zmax,0,'f',2)
        .arg(numberOf(part->shape,TopAbs_FACE)).arg(numberOf(part->shape,TopAbs_EDGE))
        .arg(part->visible ? "Yes" : "No").arg(part->color).arg(materialText).arg(part->id));
}
void MainWindow::showMassProperties() {
    if (!requireBodyMode("Measure body")) return;
    const auto ids=selectedIds();
    if (ids.size()!=1) { reportError("Measure body","Select exactly one body."); return; }
    const cad::Part* part=document_.find(ids.first());
    if (!part) return;
    bool accepted=false;
    const double defaultDensity=part->materialDensityKgPerM3>0.0?part->materialDensityKgPerM3:7850.0;
    const double density=QInputDialog::getDouble(this,"Mass properties","Material density (kg/m^3)",defaultDensity,0.00001,1000000.0,3,&accepted);
    if(!accepted)return;
    cad::DensityMassProperties massProperties;QString error;
    if(!document_.measureMassFromDensity(part->id,density,&massProperties,&error)){
        reportError("Mass properties",error);return;
    }
    GProp_GProps volumeProperties,areaProperties;
    BRepGProp::VolumeProperties(part->shape,volumeProperties);
    BRepGProp::SurfaceProperties(part->shape,areaProperties);
    Bnd_Box box;
    BRepBndLib::AddOptimal(part->shape,box,false,false);
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;
    if (!box.IsVoid()) box.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    const QString report=QString(
        "%1\n\nBounding size\nX: %2 mm\nY: %3 mm\nZ: %4 mm\n\n"
        "Volume: %5 mm^3\nSurface area: %6 mm^2\nDensity: %7 kg/m^3\nMass: %8 kg\n"
        "Material: %12\nCenter of mass\nX: %9 mm\nY: %10 mm\nZ: %11 mm")
        .arg(part->name)
        .arg(xmax-xmin,0,'f',3).arg(ymax-ymin,0,'f',3).arg(zmax-zmin,0,'f',3)
        .arg(volumeProperties.Mass(),0,'f',3).arg(areaProperties.Mass(),0,'f',3)
        .arg(massProperties.densityKgPerM3,0,'f',3).arg(massProperties.massKg,0,'f',6)
        .arg(massProperties.centerX,0,'f',3).arg(massProperties.centerY,0,'f',3).arg(massProperties.centerZ,0,'f',3)
        .arg(part->materialName.isEmpty()?QStringLiteral("Unassigned (density entered for this measurement)"):part->materialName);
    QMessageBox::information(this,"Mass properties",report);
}

void MainWindow::diagnoseSelectedBody() {
    if(!requireBodyMode("Geometry diagnostics"))return;
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Geometry diagnostics","Select exactly one body in the model browser.");return;}
    cad::BodyDiagnostics result;QString error;
    if(!document_.diagnoseBody(ids.first(),&result,&error)){
        reportError("Geometry diagnostics",error);return;
    }
    const QString report=QString(
        "%1\n\nGeometry: %2\n\nTopology\nSolids: %3\nShells: %4\nFaces: %5\nWires: %6\nEdges: %7\nVertices: %8\n\n"
        "Volume: %9 mm^3\nSurface area: %10 mm^2\nBounds: X %11 to %12, Y %13 to %14, Z %15 to %16 mm\n"
        "Center of mass: (%17, %18, %19) mm")
        .arg(document_.find(ids.first())->name)
        .arg(result.valid?"Valid closed solid":"Invalid or non-solid geometry")
        .arg(result.solids).arg(result.shells).arg(result.faces).arg(result.wires).arg(result.edges).arg(result.vertices)
        .arg(result.volume,0,'f',3).arg(result.surfaceArea,0,'f',3)
        .arg(result.minX,0,'f',3).arg(result.maxX,0,'f',3)
        .arg(result.minY,0,'f',3).arg(result.maxY,0,'f',3)
        .arg(result.minZ,0,'f',3).arg(result.maxZ,0,'f',3)
        .arg(result.centerX,0,'f',3).arg(result.centerY,0,'f',3).arg(result.centerZ,0,'f',3);
    QMessageBox::information(this,"Geometry diagnostics",report);
}

void MainWindow::measureSelectedBodySection() {
    if(!requireBodyMode("Planar section"))return;
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Planar section","Select exactly one body.");return;}
    bool accepted=false;
    const QString axisName=QInputDialog::getItem(this,"Planar section","Plane normal axis",{"X","Y","Z"},2,false,&accepted);
    if(!accepted)return;
    const double offset=QInputDialog::getDouble(this,"Planar section",QString("Plane offset along %1 (mm)").arg(axisName),0.0,-1000000.0,1000000.0,3,&accepted);
    if(!accepted)return;
    cad::SectionMeasurement result;QString error;
    const int axis=axisName=="X"?0:axisName=="Y"?1:2;
    if(!document_.measurePlanarSection(ids.first(),axis,offset,&result,&error)){
        reportError("Planar section",error);return;
    }
    const QString report=QString("%1\n\nPlane normal: %2\nOffset: %3 mm\nSection edges: %4\nTotal contour length: %5 mm")
        .arg(document_.find(ids.first())->name).arg(axisName).arg(offset,0,'f',3)
        .arg(result.edgeCount).arg(result.totalLength,0,'f',3);
    QMessageBox::information(this,"Planar section contour",report);
}

void MainWindow::summarizeSelectedBodySurfaces() {
    if(!requireBodyMode("Surface type summary"))return;
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Surface type summary","Select exactly one body.");return;}
    cad::SurfaceTypeSummary result;QString error;
    if(!document_.summarizeSurfaceTypes(ids.first(),&result,&error)){
        reportError("Surface type summary",error);return;
    }
    const QString report=QString(
        "%1\n\nPlanar: %2 faces, %3 mm^2\nCylindrical: %4 faces, %5 mm^2\n"
        "Conical: %6 faces, %7 mm^2\nSpherical: %8 faces, %9 mm^2\n"
        "Toroidal: %10 faces, %11 mm^2\nOther: %12 faces, %13 mm^2")
        .arg(document_.find(ids.first())->name)
        .arg(result.planarFaces).arg(result.planarArea,0,'f',3)
        .arg(result.cylindricalFaces).arg(result.cylindricalArea,0,'f',3)
        .arg(result.conicalFaces).arg(result.conicalArea,0,'f',3)
        .arg(result.sphericalFaces).arg(result.sphericalArea,0,'f',3)
        .arg(result.toroidalFaces).arg(result.toroidalArea,0,'f',3)
        .arg(result.otherFaces).arg(result.otherArea,0,'f',3);
    QMessageBox::information(this,"Surface type summary",report);
}

void MainWindow::measureSelectedFaceArea() {
    if(viewport_->selectionMode()!=CadViewport::SelectionMode::Face){
        reportError("Face area","Choose Faces (inspection), then select exactly one face in the viewport.");return;
    }
    const QVector<cad::FaceReference> faces=viewport_->selectedFaceReferences();
    if(faces.size()!=1){reportError("Face area","Select exactly one face in the viewport.");return;}
    double area=0.0;QString error;
    if(!document_.measureFaceArea(faces.first(),&area,&error)){
        reportError("Face area",error);return;
    }
    QMessageBox::information(this,"Face area",
        QString("Selected face %1\n\nArea: %2 mm^2")
            .arg(faces.first().faceIndex).arg(area,0,'f',3));
}

void MainWindow::measureAngleBetweenSelectedFaces() {
    if(viewport_->selectionMode()!=CadViewport::SelectionMode::Face){
        reportError("Face angle","Switch to Faces (inspection), then select exactly two planar faces.");return;
    }
    const QVector<cad::FaceReference> faces=viewport_->selectedFaceReferences();
    if(faces.size()!=2){reportError("Face angle","Select exactly two planar faces in the viewport. Hold Ctrl while selecting the second face.");return;}
    cad::FaceAngleMeasurement result;QString error;
    if(!document_.measureAngleBetweenPlanarFaces(faces.at(0),faces.at(1),&result,&error)){
        reportError("Face angle",error);return;
    }
    const QString planeDistance=result.parallel
        ? QString("Parallel-plane separation: %1 mm").arg(result.planeSeparationMm,0,'f',3)
        : QStringLiteral("Plane separation: not defined because the faces are not parallel.");
    QMessageBox::information(this,"Planar face relationship",
        QString("Angle between selected planar-face normals: %1°\n%2\n\nFirst normal: (%3, %4, %5)\nSecond normal: (%6, %7, %8)")
            .arg(result.angleDegrees,0,'f',3).arg(planeDistance)
            .arg(result.firstNormalX,0,'f',4).arg(result.firstNormalY,0,'f',4).arg(result.firstNormalZ,0,'f',4)
            .arg(result.secondNormalX,0,'f',4).arg(result.secondNormalY,0,'f',4).arg(result.secondNormalZ,0,'f',4));
}

void MainWindow::measureSelectedCylindricalFace() {
    if(viewport_->selectionMode()!=CadViewport::SelectionMode::Face){
        reportError("Cylindrical face","Switch to Faces (inspection), then select one cylindrical side face.");return;
    }
    const QVector<cad::FaceReference> faces=viewport_->selectedFaceReferences();
    if(faces.size()!=1){reportError("Cylindrical face","Select exactly one cylindrical side face in the viewport.");return;}
    cad::CylindricalFaceMeasurement result;QString error;
    if(!document_.measureCylindricalFace(faces.first(),&result,&error)){
        reportError("Cylindrical face",error);return;
    }
    QMessageBox::information(this,"Cylindrical face measurement",
        QString("Radius: %1 mm\nDiameter: %2 mm\nAxis direction: (%3, %4, %5)\nAxis origin: (%6, %7, %8) mm")
            .arg(result.radiusMm,0,'f',3).arg(result.diameterMm,0,'f',3)
            .arg(result.axisX,0,'f',4).arg(result.axisY,0,'f',4).arg(result.axisZ,0,'f',4)
            .arg(result.originX,0,'f',3).arg(result.originY,0,'f',3).arg(result.originZ,0,'f',3));
}

void MainWindow::measureSelectedEdgeLength() {
    if(viewport_->selectionMode()!=CadViewport::SelectionMode::Edge){
        reportError("Edge length","Choose Edges (inspection), then select exactly one edge in the viewport.");return;
    }
    const QVector<cad::EdgeReference> edges=viewport_->selectedEdgeReferences();
    if(edges.size()!=1){reportError("Edge length","Select exactly one edge in the viewport.");return;}
    double length=0.0;QString error;
    if(!document_.measureEdgeLength(edges.first(),&length,&error)){
        reportError("Edge length",error);return;
    }
    QMessageBox::information(this,"Edge length",
        QString("Selected edge %1\n\nLength: %2 mm")
            .arg(edges.first().edgeIndex).arg(length,0,'f',3));
}

void MainWindow::measureSelectedVertexCoordinates() {
    if(viewport_->selectionMode()!=CadViewport::SelectionMode::Vertex){
        reportError("Vertex coordinates","Choose Vertices (inspection), then select exactly one vertex in the viewport.");return;
    }
    const auto vertices=viewport_->selectedVertexReferences();
    if(vertices.size()!=1){reportError("Vertex coordinates","Select exactly one vertex in the viewport.");return;}
    double x=0.0,y=0.0,z=0.0;QString error;
    if(!document_.measureVertexCoordinates(vertices.first(),&x,&y,&z,&error)){
        reportError("Vertex coordinates",error);return;
    }
    QMessageBox::information(this,"Vertex coordinates",
        QString("Selected vertex %1\n\nX: %2 mm\nY: %3 mm\nZ: %4 mm")
            .arg(vertices.first().vertexIndex).arg(x,0,'f',3).arg(y,0,'f',3).arg(z,0,'f',3));
}

void MainWindow::measureSelectedVertexDistance() {
    if(viewport_->selectionMode()!=CadViewport::SelectionMode::Vertex){
        reportError("Vertex distance","Choose Vertices (inspection), then select exactly two vertices in the viewport.");return;
    }
    const auto vertices=viewport_->selectedVertexReferences();
    if(vertices.size()!=2){reportError("Vertex distance","Hold Ctrl while selecting; choose exactly two vertices in the viewport.");return;}
    double distance=0.0;QString error;
    if(!document_.measureVertexDistance(vertices[0],vertices[1],&distance,&error)){
        reportError("Vertex distance",error);return;
    }
    QMessageBox::information(this,"Vertex distance",
        QString("Distance between selected vertices: %1 mm").arg(distance,0,'f',3));
}

void MainWindow::measureSelectedBodyClearance() {
    if(!requireBodyMode("Minimum distance"))return;
    const QStringList ids=selectedIds();
    if(ids.size()!=2){reportError("Minimum distance","Select exactly two bodies in the model browser.");return;}
    cad::MinimumDistanceResult measurement;
    QString error;
    if(!document_.measureMinimumDistance(ids[0],ids[1],&measurement,&error)){
        reportError("Minimum distance",error);return;
    }
    const QString report=QStringLiteral("Minimum distance: %1 mm\n\nClosest point on %2: (%3, %4, %5) mm\nClosest point on %6: (%7, %8, %9) mm")
        .arg(measurement.distance,0,'f',4)
        .arg(document_.find(ids[0])->name)
        .arg(measurement.firstX,0,'f',4).arg(measurement.firstY,0,'f',4).arg(measurement.firstZ,0,'f',4)
        .arg(document_.find(ids[1])->name)
        .arg(measurement.secondX,0,'f',4).arg(measurement.secondY,0,'f',4).arg(measurement.secondZ,0,'f',4);
    QMessageBox::information(this,"Minimum distance",report);
}

void MainWindow::measureSelectedBodyInterference() {
    if(!requireBodyMode("Interference volume"))return;
    const QStringList ids=selectedIds();
    if(ids.size()!=2){reportError("Interference volume","Select exactly two bodies in the model browser.");return;}
    cad::InterferenceResult result;QString error;
    if(!document_.measureInterference(ids[0],ids[1],&result,&error)){
        reportError("Interference volume",error);return;
    }
    QString report=QString("Shared volume of %1 and %2: %3 mm^3")
        .arg(document_.find(ids[0])->name,document_.find(ids[1])->name)
        .arg(result.volume,0,'f',4);
    if(result.volume>0.0)report+=QString("\n\nOverlap centroid: (%1, %2, %3) mm")
        .arg(result.centroidX,0,'f',4).arg(result.centroidY,0,'f',4).arg(result.centroidZ,0,'f',4);
    QMessageBox::information(this,"Interference volume",report);
}
void MainWindow::refresh(bool fit) {
    const auto ids = selectedIds();
    {
        const QSignalBlocker blocker(tree_);
        tree_->clear();
        for (const auto& part : document_.parts()) {
            const QString label=part.componentDefinitionId.isEmpty()?part.name:
                QStringLiteral("%1 — occurrence %2").arg(part.componentName,part.occurrenceId.left(8));
            auto* item = new QTreeWidgetItem(tree_,{label,QString()});
            item->setData(0,Qt::UserRole,part.id);
            if(!part.componentDefinitionId.isEmpty()){
                QString tooltip=QStringLiteral("Component: %1\nDefinition ID: %2\nOccurrence ID: %3\nPlacement origin: (%4, %5, %6) mm")
                    .arg(part.componentName,part.componentDefinitionId,part.occurrenceId)
                    .arg(part.placementX,0,'f',3).arg(part.placementY,0,'f',3).arg(part.placementZ,0,'f',3);
                if(!part.assemblyJointType.isEmpty())tooltip+=QStringLiteral("\n%1 joint parent body: %2").arg(part.assemblyJointType,part.assemblyJointParentBodyId);
                if(part.assemblyJointType=="Revolute")tooltip+=QStringLiteral("\nAngle: %1° (limits %2° to %3°)")
                    .arg(part.jointAngleDegrees,0,'f',3).arg(part.jointMinimumDegrees,0,'f',3).arg(part.jointMaximumDegrees,0,'f',3);
                item->setToolTip(0,tooltip);
            }
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(1,part.visible ? Qt::Checked : Qt::Unchecked);
            item->setSelected(ids.contains(part.id));
        }
    }
    viewport_->updateScene(document_.parts(),fit);
    viewport_->selectIds(ids);
    updateInspector(ids);
    undoAction_->setEnabled(document_.canUndo());
    redoAction_->setEnabled(document_.canRedo());
    const QString title = document_.path().isEmpty() ? "Untitled" : QFileInfo(document_.path()).fileName();
    setWindowTitle(QStringLiteral("CADWorkbench v") + QStringLiteral(CADWORKBENCH_VERSION)
                   + QStringLiteral(" - %1%2").arg(title, document_.dirty() ? " *" : ""));
    statusBar()->showMessage(QString("%1 bodies  |  millimeters  |  %2")
                             .arg(document_.parts().size()).arg(document_.dirty() ? "Unsaved changes" : "Saved"),5000);
}
void MainWindow::reportError(const QString& action,const QString& description) { QMessageBox::warning(this,action,description); }
bool MainWindow::requireBodyMode(const QString& action) {
    if (viewport_->selectionMode() == CadViewport::SelectionMode::Body) return true;
    reportError(action,"Face and edge modes are for inspection only. Switch to Select â†’ Bodies before editing.");
    return false;
}
void MainWindow::makeShape(cad::Primitive kind) {
    QString title;
    QStringList labels;
    QVector<double> defaults;
    switch(kind) {
        case cad::Primitive::Box: title="Box"; labels={"Width X (mm)","Depth Y (mm)","Height Z (mm)"}; defaults={40,30,20}; break;
        case cad::Primitive::Cylinder: title="Cylinder"; labels={"Radius (mm)","Height (mm)"}; defaults={10,35}; break;
        case cad::Primitive::Sphere: title="Sphere"; labels={"Radius (mm)"}; defaults={15}; break;
        case cad::Primitive::Cone: title="Cone"; labels={"Bottom radius (mm)","Top radius (mm)","Height (mm)"}; defaults={15,7,30}; break;
        case cad::Primitive::Torus: title="Torus"; labels={"Major radius (mm)","Tube radius (mm)"}; defaults={18,5}; break;
        case cad::Primitive::RegularPrism: title="Regular prism"; labels={"Circumradius (mm)","Height (mm)","Sides (3-64)"}; defaults={10,25,6}; break;
        case cad::Primitive::Capsule: title="Capsule"; labels={"End radius (mm)","Center-to-center span (mm)"}; defaults={8,25}; break;
        case cad::Primitive::Ellipsoid: title="Ellipsoid"; labels={"X radius (mm)","Y radius (mm)","Z radius (mm)"}; defaults={15,10,8}; break;
        case cad::Primitive::Tube: title="Tube"; labels={"Outer radius (mm)","Inner radius (mm)","Height (mm)"}; defaults={12,8,30}; break;
        case cad::Primitive::Wedge: title="Wedge"; labels={"Base width X (mm)","Depth Y (mm)","Height Z (mm)","Top width X (mm)"}; defaults={30,20,15,12}; break;
        case cad::Primitive::PyramidFrustum: title="Pyramid frustum"; labels={"Base width X (mm)","Base depth Y (mm)","Top width X (mm)","Top depth Y (mm)","Height Z (mm)"}; defaults={30,30,12,12,25}; break;
        case cad::Primitive::HexNut: title="Hex nut"; labels={"Outer circumradius (mm)","Bore radius (mm)","Height (mm)"}; defaults={10,5,8}; break;
    }
    const auto input = valuesDialog(this,"Create " + title,labels,defaults);
    if (!input) return;
    QString error;
    if (!document_.addPrimitive(kind,*input,&error)) { reportError(title,error); return; }
    refresh(true);
}

void MainWindow::makeEngineeringPart(cad::EngineeringPart kind) {
    const auto spec=cad::engineeringPartSpec(kind);
    if(spec.name.isEmpty())return;
    const auto dimensions=valuesDialog(this,"Create "+spec.name,spec.labels,spec.defaults);
    if(!dimensions)return;
    QString error;
    if(!document_.addEngineeringPart(kind,*dimensions,&error)){reportError(spec.name,error);return;}
    refresh(true);
}

void MainWindow::makeAdvancedPart(cad::AdvancedPart kind) {
    const auto spec=cad::advancedPartSpec(kind);
    const auto dimensions=valuesDialog(this,"Create "+spec.name,spec.labels,spec.defaults);
    if(!dimensions)return;
    QString error;
    if(!document_.addAdvancedPart(kind,*dimensions,&error)){reportError(spec.name,error);return;}
    refresh(true);
}

void MainWindow::makeSpecializedPart(cad::SpecializedPart kind) {
    const auto spec=cad::specializedPartSpec(kind);
    const auto dimensions=valuesDialog(this,"Create "+spec.name,spec.labels,spec.defaults);
    if(!dimensions)return;
    QString error;
    if(!document_.addSpecializedPart(kind,*dimensions,&error)){reportError(spec.name,error);return;}
    refresh(true);
}

void MainWindow::makeIndustrialPart(cad::IndustrialPart kind) {
    const auto spec=cad::industrialPartSpec(kind);
    const auto dimensions=valuesDialog(this,"Create "+spec.name,spec.labels,spec.defaults);
    if(!dimensions)return;
    QString error;
    if(!document_.addIndustrialPart(kind,*dimensions,&error)){reportError(spec.name,error);return;}
    refresh(true);
}

void MainWindow::makeExtendedPart(cad::ExtendedPart kind) {
    const auto spec=cad::extendedPartSpec(kind);
    const auto dimensions=valuesDialog(this,"Create "+spec.name,spec.labels,spec.defaults);
    if(!dimensions)return;
    QString error;
    if(!document_.addExtendedPart(kind,*dimensions,&error)){reportError(spec.name,error);return;}
    refresh(true);
}

void MainWindow::makePrecisionPart(cad::PrecisionPart kind) {
    const auto spec=cad::precisionPartSpec(kind);
    const auto dimensions=valuesDialog(this,"Create "+spec.name,spec.labels,spec.defaults);
    if(!dimensions)return;
    QString error;
    if(!document_.addPrecisionPart(kind,*dimensions,&error)){reportError(spec.name,error);return;}
    refresh(true);
}

void MainWindow::makeProductionPart(cad::ProductionPart kind) {
    const auto spec=cad::productionPartSpec(kind);
    const auto dimensions=valuesDialog(this,"Create "+spec.name,spec.labels,spec.defaults);
    if(!dimensions)return;
    QString error;
    if(!document_.addProductionPart(kind,*dimensions,&error)){reportError(spec.name,error);return;}
    refresh(true);
}

void MainWindow::makeModelingTool(cad::ModelingTool kind) {
    const auto spec=cad::modelingToolSpec(kind);
    if(spec.name.isEmpty())return;
    const auto dimensions=valuesDialog(this,"Create "+spec.name,spec.labels,spec.defaults);
    if(!dimensions)return;
    QString error;
    if(!document_.addModelingTool(kind,*dimensions,&error)){reportError(spec.name,error);return;}
    refresh(true);
}

void MainWindow::makeOrientedCylinder() {
    const auto v=valuesDialog(this,"Create cylinder on a custom axis",
        {"Radius (mm)","Height (mm)","Origin X","Origin Y","Origin Z","Axis X","Axis Y","Axis Z"},
        {10,30,0,0,0,0,0,1},-1e6,1e6);if(!v)return;
    QString error;if(!document_.addOrientedCylinder((*v)[0],(*v)[1],(*v)[2],(*v)[3],(*v)[4],(*v)[5],(*v)[6],(*v)[7],&error)){
        reportError("Oriented cylinder",error);return;
    }refresh(true);
}

void MainWindow::makeOrientedCone() {
    const auto v=valuesDialog(this,"Create cone on a custom axis",
        {"Bottom radius (mm)","Top radius (mm, zero for point)","Height (mm)","Origin X","Origin Y","Origin Z","Axis X","Axis Y","Axis Z"},
        {10,0,25,0,0,0,0,0,1},-1e6,1e6);if(!v)return;
    QString error;if(!document_.addOrientedCone((*v)[0],(*v)[1],(*v)[2],(*v)[3],(*v)[4],(*v)[5],(*v)[6],(*v)[7],(*v)[8],&error)){
        reportError("Oriented cone",error);return;
    }refresh(true);
}

void MainWindow::makeTorusSegment() {
    const auto v=valuesDialog(this,"Create torus segment",{"Major radius (mm)","Tube radius (mm)","Sweep angle (degrees)"},
        {18,5,90},0.00001,360);if(!v)return;
    QString error;if(!document_.addTorusSegment((*v)[0],(*v)[1],(*v)[2],&error)){reportError("Torus segment",error);return;}
    refresh(true);
}

void MainWindow::sweepCircularProfile() {
    const auto values=valuesDialog(this,"Sweep circular profile",
        {"Profile radius (mm)","Path length (mm)","Lateral offset (mm)","Vertical rise (mm)"},{5,80,20,12},0,1e6);
    if(!values)return;
    QString error;
    if(!document_.sweepCircularProfile((*values)[0],(*values)[1],(*values)[2],(*values)[3],&error))
        reportError("Sweep circular profile",error);
    else refresh(true);
}

void MainWindow::sweepRectangularProfile() {
    const auto values=valuesDialog(this,"Sweep rectangular profile",
        {"Profile width (mm)","Profile height (mm)","Path length (mm)","Lateral offset (mm)","Vertical rise (mm)"},{12,8,80,20,12},0,1e6);
    if(!values)return;
    QString error;
    if(!document_.sweepRectangularProfile((*values)[0],(*values)[1],(*values)[2],(*values)[3],(*values)[4],&error))
        reportError("Sweep rectangular profile",error);
    else refresh(true);
}

void MainWindow::sweepEllipticalProfile() {
    const auto values=valuesDialog(this,"Sweep elliptical profile",
        {"Semi-axis A (mm)","Semi-axis B (mm)","Path length (mm)","Lateral offset (mm)","Vertical rise (mm)"},
        {8,4,80,20,12},0,1e6);
    if(!values)return;
    QString error;
    if(!document_.sweepEllipticalProfile((*values)[0],(*values)[1],(*values)[2],(*values)[3],(*values)[4],&error))
        reportError("Sweep elliptical profile",error);
    else refresh(true);
}

void MainWindow::makeHelicalSweep() {
    const auto values=valuesDialog(this,"Create helical sweep",
        {"Wire radius (mm)","Coil radius (mm)","Pitch (mm)","Turns"},{1.5,12,5,6},0,1e6);
    if(!values)return;
    QString error;
    if(!document_.addHelicalSweep((*values)[0],(*values)[1],(*values)[2],(*values)[3],&error))
        reportError("Helical sweep",error);
    else refresh(true);
}

void MainWindow::makeVariablePitchHelicalSweep() {
    const auto values=valuesDialog(this,"Create variable-pitch spring",
        {"Wire radius (mm)","Coil radius (mm)","Start pitch (mm)","End pitch (mm)","Turns"},
        {1.5,12,4,7,6},0,1e6);
    if(!values)return;
    QString error;
    if(!document_.addVariablePitchHelicalSweep((*values)[0],(*values)[1],(*values)[2],(*values)[3],(*values)[4],&error))
        reportError("Variable-pitch spring",error);
    else refresh(true);
}

void MainWindow::makeTaperedHelicalSweep() {
    const auto values=valuesDialog(this,"Create tapered spring",
        {"Wire radius (mm)","Start coil radius (mm)","End coil radius (mm)","Pitch (mm)","Turns"},
        {1.5,8,16,5,6},0,1e6);
    if(!values)return;
    QString error;
    if(!document_.addTaperedHelicalSweep((*values)[0],(*values)[1],(*values)[2],(*values)[3],(*values)[4],&error))
        reportError("Tapered spring",error);
    else refresh(true);
}

void MainWindow::makeEllipticalPrism() {
    const auto v=valuesDialog(this,"Create elliptical prism",{"Semi-axis A (mm)","Semi-axis B (mm)","Height (mm)"},{18,10,30},0,1e6);
    if(!v)return;QString error;
    if(!document_.addEllipticalPrism((*v)[0],(*v)[1],(*v)[2],&error))reportError("Elliptical prism",error);else refresh(true);
}

void MainWindow::makeEllipticalTube() {
    const auto v=valuesDialog(this,"Create elliptical tube",{"Outer semi-axis A (mm)","Outer semi-axis B (mm)","Inner semi-axis A (mm)","Inner semi-axis B (mm)","Height (mm)"},{18,12,13,8,30},0,1e6);
    if(!v)return;QString error;
    if(!document_.addEllipticalTube((*v)[0],(*v)[1],(*v)[2],(*v)[3],(*v)[4],&error))reportError("Elliptical tube",error);else refresh(true);
}

void MainWindow::makeCircularLoft() {
    const auto v=valuesDialog(this,"Create circular loft",{"Bottom radius (mm)","Top radius (mm)","Height (mm)","Top offset X (mm)","Top offset Y (mm)"},{16,9,35,5,0},0,1e6);
    if(!v)return;QString error;
    if(!document_.loftCircularSections((*v)[0],(*v)[1],(*v)[2],(*v)[3],(*v)[4],&error))reportError("Circular loft",error);else refresh(true);
}

void MainWindow::makeRectangularToCircularLoft() {
    const auto v=valuesDialog(this,"Create rectangular-to-circular transition",{"Rectangle width (mm)","Rectangle depth (mm)","Circle radius (mm)","Height (mm)","Top offset X (mm)","Top offset Y (mm)"},{35,25,12,30,0,0},0,1e6);
    if(!v)return;QString error;
    if(!document_.loftRectangleToCircle((*v)[0],(*v)[1],(*v)[2],(*v)[3],(*v)[4],(*v)[5],&error))reportError("Rectangular-to-circular transition",error);else refresh(true);
}

void MainWindow::makeHollowLoftReducer() {
    const auto v=valuesDialog(this,"Create hollow loft reducer",{"Bottom outer radius (mm)","Bottom inner radius (mm)","Top outer radius (mm)","Top inner radius (mm)","Height (mm)","Top offset X (mm)","Top offset Y (mm)"},{18,12,10,6,35,0,0},0,1e6);
    if(!v)return;QString error;
    if(!document_.loftHollowReducer((*v)[0],(*v)[1],(*v)[2],(*v)[3],(*v)[4],(*v)[5],(*v)[6],&error))reportError("Hollow loft reducer",error);else refresh(true);
}

void MainWindow::makeEllipticalLoft() {
    const auto v=valuesDialog(this,"Create elliptical loft",
        {"Bottom semi-axis A (mm)","Bottom semi-axis B (mm)","Top semi-axis A (mm)",
         "Top semi-axis B (mm)","Height (mm)","Top offset X (mm)","Top offset Y (mm)"},
        {20,12,12,8,35,5,0},0,1e6);
    if(!v)return;QString error;
    if(!document_.loftEllipticalSections((*v)[0],(*v)[1],(*v)[2],(*v)[3],(*v)[4],(*v)[5],(*v)[6],&error))
        reportError("Elliptical loft",error);
    else refresh(true);
}

void MainWindow::sketchExtrusion() {
    const QStringList currentSelection = selectedIds();
    QDialog dialog(this);
    dialog.setWindowTitle("Sketch and extrude profile");
    dialog.setMinimumWidth(370);
    auto* form = new QFormLayout(&dialog);

    auto* profile = new QComboBox(&dialog);
    profile->addItems({"Rectangle", "Circle", "Ellipse", "Slot", "Triangle", "Regular polygon", "Rounded rectangle", "Ring"});
    form->addRow("Profile", profile);

    auto* sketchPlane=new QComboBox(&dialog);
    sketchPlane->addItems({"XY plane","XZ plane","YZ plane","Custom 3D plane"});
    form->addRow("Sketch plane",sketchPlane);

    auto* operation = new QComboBox(&dialog);
    operation->addItem("New Body", static_cast<int>(cad::ExtrusionMode::NewBody));
    if (!document_.parts().isEmpty()) {
        operation->addItem("Join", static_cast<int>(cad::ExtrusionMode::Join));
        operation->addItem("Cut", static_cast<int>(cad::ExtrusionMode::Cut));
    }
    form->addRow("Operation", operation);
    auto* targetBody = new QComboBox(&dialog);
    int selectedTarget = 0;
    for (const auto& part : document_.parts()) {
        targetBody->addItem(part.name, part.id);
        if (!currentSelection.isEmpty() && part.id == currentSelection.first())
            selectedTarget = targetBody->count() - 1;
    }
    if (targetBody->count() > 0) targetBody->setCurrentIndex(selectedTarget);
    targetBody->setEnabled(operation->currentData().toInt() != static_cast<int>(cad::ExtrusionMode::NewBody));
    form->addRow("Target body", targetBody);
    connect(operation, qOverload<int>(&QComboBox::currentIndexChanged), &dialog,
            [operation, targetBody](int) {
        targetBody->setEnabled(operation->currentData().toInt() != static_cast<int>(cad::ExtrusionMode::NewBody));
    });

    auto makeField = [&](double minimum, double maximum, double step, double value) {
        auto* field = new QDoubleSpinBox(&dialog);
        field->setRange(minimum, maximum);
        field->setDecimals(5);
        field->setSingleStep(step);
        field->setValue(value);
        return field;
    };
    auto* originX = makeField(-1e6, 1e6, 1.0, 0.0);
    auto* originY = makeField(-1e6, 1e6, 1.0, 0.0);
    auto* planeOffset = makeField(-1e6, 1e6, 1.0, 0.0);
    auto* sizeX = makeField(0.00001, 1e6, 1.0, 40.0);
    auto* depth = makeField(0.00001, 1e6, 1.0, 30.0);
    auto* height = makeField(0.00001, 1e6, 1.0, 20.0);
    auto* customOriginX=makeField(-1e6,1e6,1.0,0.0);
    auto* customOriginY=makeField(-1e6,1e6,1.0,0.0);
    auto* customOriginZ=makeField(-1e6,1e6,1.0,0.0);
    auto* customNormalX=makeField(-1e6,1e6,0.1,1.0);
    auto* customNormalY=makeField(-1e6,1e6,0.1,1.0);
    auto* customNormalZ=makeField(-1e6,1e6,0.1,1.0);
    auto* customUAxisX=makeField(-1e6,1e6,0.1,1.0);
    auto* customUAxisY=makeField(-1e6,1e6,0.1,0.0);
    auto* customUAxisZ=makeField(-1e6,1e6,0.1,0.0);
    QVector<QWidget*> customFrameFields;
    for(const auto& row:QVector<QPair<QString,QDoubleSpinBox*>>{
            {"Plane origin X (mm)",customOriginX},{"Plane origin Y (mm)",customOriginY},
            {"Plane origin Z (mm)",customOriginZ},{"Normal X",customNormalX},
            {"Normal Y",customNormalY},{"Normal Z",customNormalZ},
            {"U direction X",customUAxisX},{"U direction Y",customUAxisY},{"U direction Z",customUAxisZ}}){
        auto* label=new QLabel(row.first,&dialog);form->addRow(label,row.second);
        customFrameFields.append(label);customFrameFields.append(row.second);
        label->setVisible(false);row.second->setVisible(false);
    }
    connect(sketchPlane,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[customFrameFields](int index){
        for(QWidget* field:customFrameFields)field->setVisible(index==3);
    });
    auto* sizeLabel = new QLabel("Width (mm)",&dialog);
    auto* depthLabel = new QLabel("Depth (mm)",&dialog);
    form->addRow("Origin U (mm)",originX);
    form->addRow("Origin V (mm)",originY);
    form->addRow("Plane offset W (mm)",planeOffset);
    form->addRow(sizeLabel,sizeX);
    form->addRow(depthLabel,depth);
    form->addRow("Extrusion height (mm)",height);
    auto* symmetric=new QCheckBox("Symmetric about sketch plane",&dialog);
    auto* reverseDirection=new QCheckBox("Reverse extrusion direction",&dialog);
    auto* throughAll=new QCheckBox("Through all (Cut)",&dialog);
    auto* tapered=new QCheckBox("Tapered extrusion",&dialog);
    auto* taperAngle=new QDoubleSpinBox(&dialog);taperAngle->setRange(-45,45);taperAngle->setDecimals(3);taperAngle->setValue(5);taperAngle->setEnabled(false);
    form->addRow(symmetric);form->addRow(reverseDirection);form->addRow(throughAll);form->addRow(tapered);form->addRow("Taper angle (degrees)",taperAngle);
    auto* cornerRadius=new QDoubleSpinBox(&dialog);cornerRadius->setRange(0.00001,1e6);cornerRadius->setDecimals(5);cornerRadius->setValue(5);
    auto* cornerRadiusLabel=new QLabel("Corner radius (mm)",&dialog);cornerRadiusLabel->setVisible(false);cornerRadius->setVisible(false);
    form->addRow(cornerRadiusLabel,cornerRadius);
    connect(tapered,&QCheckBox::toggled,taperAngle,&QDoubleSpinBox::setEnabled);
    connect(symmetric,&QCheckBox::toggled,&dialog,[tapered](bool enabled){tapered->setEnabled(!enabled);});
    connect(symmetric,&QCheckBox::toggled,reverseDirection,&QCheckBox::setDisabled);
    connect(throughAll,&QCheckBox::toggled,height,&QDoubleSpinBox::setDisabled);
    auto updateThroughAll = [=](int) {
        const bool allowed=operation->currentData().toInt()==static_cast<int>(cad::ExtrusionMode::Cut)&&
                           !symmetric->isChecked()&&!tapered->isChecked();
        throughAll->setEnabled(allowed);
        if(!allowed)throughAll->setChecked(false);
    };
    connect(operation,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,updateThroughAll);
    connect(symmetric,&QCheckBox::toggled,&dialog,[updateThroughAll](bool){updateThroughAll(0);});
    connect(tapered,&QCheckBox::toggled,&dialog,[updateThroughAll](bool){updateThroughAll(0);});
    updateThroughAll(0);
    auto updateProfileFields = [=](int index) {
        const bool showDepth = index == 0 || index == 2 || index == 3 || index == 5 || index == 6 || index == 7;
        const QString sizeText = index == 0 ? "Width (mm)" :
                                 index == 1 ? "Diameter (mm)" :
                                 index == 2 ? "Major diameter (mm)" :
                                 index == 3 ? "Overall slot width (mm)" :
                                 index == 4 ? "Circumdiameter (mm)" :
                                 index == 7 ? "Outer diameter (mm)" : "Circumdiameter (mm)";
        sizeLabel->setText(sizeText);
        sizeX->setValue(index == 1 ? 20.0 : index == 3 ? 40.0 : 40.0);
        depthLabel->setVisible(showDepth);
        depth->setVisible(showDepth);
        depthLabel->setText(index == 0 ? "Depth (mm)" :
                            index == 2 ? "Minor diameter (mm)" :
                            index == 3 ? "Slot height (mm)" :
                            index == 5 ? "Sides (3-64)" :
                            index == 6 ? "Height Y (mm)" : "Inner diameter (mm)");
        if (index == 2) depth->setValue(25.0);
        else if (index == 3) depth->setValue(12.0);
        else if (index == 5) depth->setValue(6.0);
        else if (index == 6) {sizeX->setValue(40);depth->setValue(30);cornerRadius->setValue(5);}
        else if (index == 7) {sizeX->setValue(30);depth->setValue(20);}
    };
    connect(profile,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,updateProfileFields);
    connect(profile,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[tapered,taperAngle](int index){
        const bool supported=index==0||index==1;
        tapered->setEnabled(supported);if(!supported){tapered->setChecked(false);taperAngle->setEnabled(false);}
    });
    connect(profile,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[cornerRadiusLabel,cornerRadius](int index){
        const bool visible=index==6;cornerRadiusLabel->setVisible(visible);cornerRadius->setVisible(visible);
    });
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,&dialog);
    form->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;

    const auto profileKind = static_cast<cad::SketchProfile>(profile->currentIndex());
    const auto operationKind = static_cast<cad::ExtrusionMode>(operation->currentData().toInt());
    const QString targetId = targetBody->currentData().toString();
    cad::ExtrusionCommand command;
    QString error;
    bool prepared=false;
    if(profileKind==cad::SketchProfile::RoundedRectangle)
        prepared=document_.prepareRoundedRectangleExtrusion(originX->value(),originY->value(),sizeX->value(),depth->value(),
            cornerRadius->value(),height->value(),operationKind,targetId,&command,&error);
    else if(symmetric->isChecked())
        prepared=document_.prepareSymmetricExtrusion(profileKind,originX->value(),originY->value(),sizeX->value(),
            depth->value(),height->value(),operationKind,targetId,&command,&error);
    else if(tapered->isChecked())
        prepared=document_.prepareTaperedExtrusion(profileKind,originX->value(),originY->value(),sizeX->value(),
            depth->value(),height->value(),taperAngle->value(),operationKind,targetId,&command,&error);
    else
        prepared=document_.prepareExtrusion(profileKind,originX->value(),originY->value(),sizeX->value(),
            depth->value(),height->value(),operationKind,targetId,&command,&error);
    if(prepared&&symmetric->isChecked())command.symmetric=true;
    if(prepared){
        command.sketchPlane=static_cast<cad::SketchPlane>(sketchPlane->currentIndex());
        command.planeOffset=planeOffset->value();command.reverseDirection=reverseDirection->isChecked();command.throughAll=throughAll->isChecked();
        command.customOriginX=customOriginX->value();command.customOriginY=customOriginY->value();command.customOriginZ=customOriginZ->value();
        command.customNormalX=customNormalX->value();command.customNormalY=customNormalY->value();command.customNormalZ=customNormalZ->value();
        command.customUAxisX=customUAxisX->value();command.customUAxisY=customUAxisY->value();command.customUAxisZ=customUAxisZ->value();
    }
    if (!prepared) {
        reportError("Sketch extrusion",error);
        return;
    }
    const QString operationLabel = operationKind == cad::ExtrusionMode::Join ? "Join" :
                                   operationKind == cad::ExtrusionMode::Cut ? "Cut" : "New Body";

    runPreviewAsync<cad::ExtrusionPreview>(this, statusBar(), "Sketch extrusion",
        "Building sketch extrusion preview...", [command] {
            return cad::SceneDocument::buildExtrusionPreview(command);
        }, [this, command, operationLabel](const cad::ExtrusionPreview& preview) {
            confirmPreview(QString("Preview extrusion - %1").arg(operationLabel),
                QString("Inspect the translucent gold result. Apply this extrusion as %1?").arg(operationLabel), preview.shape,
                [this, command, preview] {
                    QString commitError;
                    if (!document_.commitExtrusion(command,preview.shape,&commitError))
                        reportError("Sketch extrusion",commitError);
                    else refresh(true);
                });
        });
}

void MainWindow::createParametricExtrusion() {
    QDialog dialog(this);dialog.setWindowTitle("Constrained rectangle extrusion");auto* form=new QFormLayout(&dialog);
    auto makeDimension=[&](const QString& label,double value){auto* spin=new QDoubleSpinBox(&dialog);spin->setRange(0.00001,1000000);spin->setDecimals(3);spin->setValue(value);spin->setSingleStep(1);form->addRow(label,spin);return spin;};
    auto* width=makeDimension("Width parameter (mm)",40);auto* height=makeDimension("Height parameter (mm)",30);auto* depth=makeDimension("Extrusion depth parameter (mm)",20);
    auto* explanation=new QLabel("Creates an XY rectangle anchored at the origin with horizontal/vertical and width/height constraints, then adds a base extrusion. Edit width, height, or depth later from Edit → Edit selected named parameter.",&dialog);
    explanation->setWordWrap(true);form->addRow(explanation);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;if(!document_.addParametricRectangularExtrusion(width->value(),height->value(),depth->value(),&error)){reportError("Create parametric extrusion",error);return;}
    refresh(true);
}
void MainWindow::createParametricOffsetExtrusion() {
    QDialog dialog(this);dialog.setWindowTitle("Constrained offset rectangle extrusion");auto* form=new QFormLayout(&dialog);
    auto makeSpin=[&](const QString& label,double minimum,double maximum,double value){auto* spin=new QDoubleSpinBox(&dialog);spin->setRange(minimum,maximum);spin->setDecimals(3);spin->setValue(value);spin->setSingleStep(1);form->addRow(label,spin);return spin;};
    auto* x=makeSpin("Sketch origin X (mm)",-1000000,1000000,0);auto* y=makeSpin("Sketch origin Y (mm)",-1000000,1000000,0);
    auto* width=makeSpin("Width parameter (mm)",0.00001,1000000,40);auto* height=makeSpin("Height parameter (mm)",0.00001,1000000,30);
    auto* depth=makeSpin("Extrusion depth parameter (mm)",0.00001,1000000,20);
    auto* explanation=new QLabel("Creates an XY rectangle whose sketch origin, width, height, and extrusion depth are named editable parameters. The rectangle remains fully constrained.",&dialog);
    explanation->setWordWrap(true);form->addRow(explanation);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;if(!document_.addParametricRectangularExtrusionAt(x->value(),y->value(),width->value(),height->value(),depth->value(),&error)){
        reportError("Create parametric offset extrusion",error);return;
    }
    refresh(true);
}
void MainWindow::addParametricThroughHole() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Parametric through-hole","Select one current parametric rectangle extrusion.");return;}
    const auto values=valuesDialog(this,"Add a constrained circular sketch and through-hole",
        {"Hole diameter (mm)","Circle center X (mm)","Circle center Y (mm)"},{8,20,15},0.00001,1000000);
    if(!values)return;
    QString error;if(!document_.addParametricThroughHole(ids.first(),(*values)[0],(*values)[1],(*values)[2],&error)){
        reportError("Parametric through-hole",error);return;
    }
    refresh();
}
void MainWindow::addParametricLinearHolePattern() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Parametric hole pattern","Select one body with a current parametric through-hole.");return;}
    QDialog dialog(this);dialog.setWindowTitle("Linear through-hole pattern");auto* form=new QFormLayout(&dialog);
    auto* count=new QSpinBox(&dialog);count->setRange(2,100);count->setValue(4);
    auto* spacing=new QDoubleSpinBox(&dialog);spacing->setRange(0.00001,1000000.0);spacing->setDecimals(3);spacing->setValue(12.0);spacing->setSuffix(" mm");
    auto* axis=new QComboBox(&dialog);axis->addItems({"X","Y"});
    form->addRow("Pattern axis",axis);form->addRow("Hole count",count);form->addRow("Center spacing",spacing);
    auto* note=new QLabel("The pattern extends in the positive direction of the selected sketch axis. Instances must remain inside the profile and cannot overlap.",&dialog);
    note->setWordWrap(true);form->addRow(note);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;if(!document_.addParametricLinearHolePattern(ids.first(),count->value(),spacing->value(),axis->currentText(),&error)){
        reportError("Parametric hole pattern",error);return;
    }
    refresh();
}
void MainWindow::editNamedParameter() {
    const QStringList ids=selectedIds();if(ids.size()!=1){reportError("Edit named parameters","Select one parametric body.");return;}
    const auto parameters=document_.namedParameters(ids.first());if(parameters.isEmpty()){reportError("Edit named parameters","The selected body has no editable parameter definition.");return;}
    QDialog dialog(this);dialog.setWindowTitle("Edit named parameters");auto* form=new QFormLayout(&dialog);
    QMap<QString,QDoubleSpinBox*> editors;
    for(auto it=parameters.cbegin();it!=parameters.cend();++it){
        const bool coordinate=it.key()==QStringLiteral("origin_x")||it.key()==QStringLiteral("origin_y")||
            it.key()==QStringLiteral("hole_x")||it.key()==QStringLiteral("hole_y");
        const bool patternCount=it.key()==QStringLiteral("hole_pattern_count");
        auto* editor=new QDoubleSpinBox(&dialog);editor->setRange(patternCount?2.0:(coordinate?-1000000.0:0.00001),patternCount?100.0:1000000.0);
        editor->setDecimals(patternCount?0:3);editor->setSingleStep(1);editor->setValue(it.value());
        editors.insert(it.key(),editor);form->addRow(it.key()+(patternCount?QStringLiteral(" (instances)"):QStringLiteral(" (mm)")),editor);
    }
    auto* explanation=new QLabel("All dimensions are rebuilt together and saved as one undo step. The hole must remain inside the rectangular profile.",&dialog);
    explanation->setWordWrap(true);form->addRow(explanation);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QMap<QString,double> values;for(auto it=editors.cbegin();it!=editors.cend();++it)values.insert(it.key(),it.value()->value());
    QString error;if(!document_.setNamedParameters(ids.first(),values,&error)){reportError("Edit named parameters",error);return;}refresh();
}
void MainWindow::showSelectedFeatureHistory() {
    const QStringList ids=selectedIds();if(ids.size()!=1){reportError("Feature history","Select one body.");return;}
    const QStringList constraints=document_.sketchConstraints(ids.first());const QStringList features=document_.featureHistory(ids.first());
    if(constraints.isEmpty()&&features.isEmpty()){reportError("Feature history","The selected body has no parametric sketch history.");return;}
    const auto parameters=document_.namedParameters(ids.first());QStringList lines{"Named parameters:"};
    for(auto it=parameters.cbegin();it!=parameters.cend();++it)lines.append(QStringLiteral("  %1 = %2 mm").arg(it.key()).arg(it.value(),0,'f',3));
    lines.append(QString());lines.append("Sketch constraints:");for(const QString& constraint:constraints)lines.append("  "+constraint);
    lines.append(QString());lines.append("Editable features:");for(int i=0;i<features.size();++i)lines.append(QStringLiteral("  %1. %2").arg(i+1).arg(features[i]));
    if(!document_.parametricDefinitionCurrent(ids.first()))lines.append(QStringLiteral("Geometry has changed since this history was authored; parameter editing is disabled for safety."));
    QMessageBox::information(this,"Parametric sketch and feature history",lines.join('\n'));
}

void MainWindow::loftRectangles() {
    const auto values=valuesDialog(this,"Loft between two rectangular sections",
        {"Bottom origin X (mm)","Bottom origin Y (mm)","Bottom width (mm)","Bottom depth (mm)",
         "Top origin X (mm)","Top origin Y (mm)","Top width (mm)","Top depth (mm)","Loft height (mm)"},
        {0,0,30,20,5,5,20,10,40},-1e6,1e6);
    if(!values)return;
    QString error;
    if(!document_.loftRectangles((*values)[0],(*values)[1],(*values)[2],(*values)[3],
                                 (*values)[4],(*values)[5],(*values)[6],(*values)[7],(*values)[8],&error)){
        reportError("Loft",error);return;
    }
    refresh(true);
}

void MainWindow::revolveRectangleProfile() {
    QDialog dialog(this);
    dialog.setWindowTitle("Revolve rectangle profile about Z");
    dialog.setMinimumWidth(370);
    auto* form = new QFormLayout(&dialog);
    auto makeField = [&](double minimum,double maximum,double step,double value) {
        auto* field = new QDoubleSpinBox(&dialog);
        field->setRange(minimum,maximum);
        field->setDecimals(5);
        field->setSingleStep(step);
        field->setValue(value);
        return field;
    };
    auto* radialOffset = makeField(0.0,1e6,1.0,0.0);
    auto* axialOrigin = makeField(-1e6,1e6,1.0,0.0);
    auto* radialWidth = makeField(0.00001,1e6,1.0,10.0);
    auto* axialHeight = makeField(0.00001,1e6,1.0,20.0);
    auto* angle = makeField(1.0,360.0,15.0,360.0);
    form->addRow("Inner radius (mm)",radialOffset);
    form->addRow("Start Z (mm)",axialOrigin);
    form->addRow("Radial thickness (mm)",radialWidth);
    form->addRow("Axial height (mm)",axialHeight);
    form->addRow("Revolve angle (degrees)",angle);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,&dialog);
    form->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if (dialog.exec()!=QDialog::Accepted) return;

    cad::RevolveCommand command;
    QString error;
    if (!document_.prepareRevolve(radialOffset->value(),axialOrigin->value(),radialWidth->value(),
                                  axialHeight->value(),angle->value(),&command,&error)) {
        reportError("Revolve profile",error);
        return;
    }
    runPreviewAsync<cad::RevolvePreview>(this, statusBar(), "Revolve profile",
        "Building revolve preview...", [command] {
            return cad::SceneDocument::buildRevolvePreview(command);
        }, [this, command](const cad::RevolvePreview& preview) {
            confirmPreview("Preview revolved profile",
                "Inspect the translucent gold solid. Add this revolved body?", preview.shape,
                [this, command, preview] {
                    QString commitError;
                    if (!document_.commitRevolve(command,preview.shape,&commitError))
                        reportError("Revolve profile",commitError);
                    else refresh(true);
                });
        });
}

void MainWindow::revolveAboutWorldAxis() {
    const QStringList axes={"X","Y","Z"};bool accepted=false;
    const QString axisName=QInputDialog::getItem(this,"Revolve axis","Axis direction",axes,2,false,&accepted);
    if(!accepted)return;
    const auto v=valuesDialog(this,"Revolve profile about selected axis",
        {"Radial offset (mm)","Axial origin (mm)","Radial width (mm)","Axial height (mm)","Sweep angle (degrees)",
         "Axis origin X (mm)","Axis origin Y (mm)","Axis origin Z (mm)"},{0,0,10,20,360,0,0,0},-1e6,1e6);
    if(!v)return;
    cad::RevolveCommand command;QString error;
    if(!document_.prepareRevolveAboutAxis((*v)[0],(*v)[1],(*v)[2],(*v)[3],(*v)[4],axes.indexOf(axisName),
        (*v)[5],(*v)[6],(*v)[7],&command,&error)){reportError("Revolve",error);return;}
    runPreviewAsync<cad::RevolvePreview>(this,statusBar(),"Revolve about axis","Building revolve preview...",[command]{
        return cad::SceneDocument::buildRevolvePreview(command);
    },[this,command](const cad::RevolvePreview& preview){
        confirmPreview("Preview axis revolve","Inspect the translucent gold solid. Add this revolved body?",preview.shape,
            [this,command,preview]{QString commitError;if(!document_.commitRevolve(command,preview.shape,&commitError))
                reportError("Revolve",commitError);else refresh(true);});
    });
}

void MainWindow::revolveAboutCustomAxis() {
    const auto values=valuesDialog(this,"Revolve profile about a custom 3D axis",
        {"Radial offset (mm)","Axial origin (mm)","Radial width (mm)","Axial height (mm)",
         "Sweep angle (degrees)","Axis origin X (mm)","Axis origin Y (mm)","Axis origin Z (mm)",
         "Axis direction X","Axis direction Y","Axis direction Z"},
        {0,0,10,20,360,0,0,0,1,1,1},-1e6,1e6);
    if(!values)return;
    cad::RevolveCommand command;QString error;
    if(!document_.prepareRevolveAboutCustomAxis((*values)[0],(*values)[1],(*values)[2],(*values)[3],(*values)[4],
        (*values)[5],(*values)[6],(*values)[7],(*values)[8],(*values)[9],(*values)[10],&command,&error)){
        reportError("Revolve about custom axis",error);return;
    }
    runPreviewAsync<cad::RevolvePreview>(this,statusBar(),"Custom-axis revolve","Building custom-axis revolve preview...",
        [command]{return cad::SceneDocument::buildRevolvePreview(command);},[this,command](const cad::RevolvePreview& preview){
            confirmPreview("Preview custom-axis revolve","Inspect the solid around the custom axis. Add this revolved body?",
                preview.shape,[this,command,preview]{QString commitError;
                    if(!document_.commitRevolve(command,preview.shape,&commitError))reportError("Custom-axis revolve",commitError);
                    else refresh(true);
                });
        });
}

void MainWindow::mirrorSelectedBody() {
    if (!requireBodyMode("Mirror")) return;
    const auto ids=selectedIds();
    if (ids.size()!=1) { reportError("Mirror","Select exactly one body."); return; }
    QDialog dialog(this);
    dialog.setWindowTitle("Mirror body across a world plane");
    auto* form=new QFormLayout(&dialog);
    auto* plane=new QComboBox(&dialog);
    plane->addItems({"YZ plane (X = offset)","XZ plane (Y = offset)","XY plane (Z = offset)"});
    form->addRow("Mirror plane",plane);
    auto* offset=new QDoubleSpinBox(&dialog);
    offset->setRange(-1e6,1e6);
    offset->setDecimals(5);
    offset->setSingleStep(1.0);
    form->addRow("Plane offset (mm)",offset);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,&dialog);
    form->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if (dialog.exec()!=QDialog::Accepted) return;
    QString error;
    if (!document_.mirror(ids.first(),static_cast<cad::Axis>(plane->currentIndex()),offset->value(),&error)) {
        reportError("Mirror",error);
        return;
    }
    refresh(true);
}

void MainWindow::mirrorBodyAcrossPlane() {
    if (!requireBodyMode("Mirror across plane")) return;
    const auto ids=selectedIds();
    if(ids.size()!=1){reportError("Mirror across plane","Select exactly one body.");return;}
    const auto values=valuesDialog(this,"Mirror across arbitrary plane",
        {"Normal X","Normal Y","Normal Z","Signed plane offset (mm)"},{1,1,0,0},-1e6,1e6);
    if(!values)return;
    QString error;
    if(!document_.mirrorAcrossPlane(ids.first(),(*values)[0],(*values)[1],(*values)[2],(*values)[3],&error)){
        reportError("Mirror across plane",error);return;
    }
    refresh(true);
}

void MainWindow::rotateBodyAboutAxis() {
    if(!requireBodyMode("Rotate about axis"))return;
    const auto ids=selectedIds();
    if(ids.size()!=1){reportError("Rotate about axis","Select exactly one body.");return;}
    const auto values=valuesDialog(this,"Rotate about a custom axis",
        {"Axis origin X (mm)","Axis origin Y (mm)","Axis origin Z (mm)","Axis direction X","Axis direction Y","Axis direction Z","Angle (degrees)"},
        {0,0,0,0,0,1,90},-1e6,1e6);
    if(!values)return;
    QString error;
    if(!document_.rotateAboutAxis(ids.first(),(*values)[0],(*values)[1],(*values)[2],
                                  (*values)[3],(*values)[4],(*values)[5],(*values)[6],&error)){
        reportError("Rotate about axis",error);return;
    }
    refresh(true);
}

void MainWindow::linearPatternSelectedBody() {
    if (!requireBodyMode("Linear pattern")) return;
    const auto ids=selectedIds();
    if (ids.size()!=1) { reportError("Linear pattern","Select exactly one body."); return; }
    QDialog dialog(this);
    dialog.setWindowTitle("Linear pattern");
    auto* form=new QFormLayout(&dialog);
    auto* axis=new QComboBox(&dialog);
    axis->addItems({"X","Y","Z"});
    form->addRow("Direction axis",axis);
    auto* count=new QSpinBox(&dialog);
    count->setRange(2,1000);
    count->setValue(4);
    form->addRow("Total instances",count);
    auto* spacing=new QDoubleSpinBox(&dialog);
    spacing->setRange(-1e6,1e6);
    spacing->setDecimals(5);
    spacing->setSingleStep(1.0);
    spacing->setValue(50.0);
    form->addRow("Spacing (mm)",spacing);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,&dialog);
    form->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if (dialog.exec()!=QDialog::Accepted) return;
    QString error;
    if (!document_.linearPattern(ids.first(),static_cast<cad::Axis>(axis->currentIndex()),count->value(),spacing->value(),&error)) {
        reportError("Linear pattern",error);
        return;
    }
    refresh(true);
}

void MainWindow::linearPatternAlongVectorSelectedBody() {
    if(!requireBodyMode("Linear pattern along vector"))return;
    const auto ids=selectedIds();
    if(ids.size()!=1){reportError("Linear pattern along vector","Select exactly one body.");return;}
    QDialog dialog(this);dialog.setWindowTitle("Linear pattern along vector");
    auto* form=new QFormLayout(&dialog);
    auto* directionX=new QDoubleSpinBox(&dialog);auto* directionY=new QDoubleSpinBox(&dialog);auto* directionZ=new QDoubleSpinBox(&dialog);
    for(auto* field:{directionX,directionY,directionZ}){field->setRange(-1e6,1e6);field->setDecimals(4);}
    directionX->setValue(1);directionY->setValue(1);
    auto* instances=new QSpinBox(&dialog);instances->setRange(2,1000);instances->setValue(3);
    auto* spacing=new QDoubleSpinBox(&dialog);spacing->setRange(-1e6,1e6);spacing->setDecimals(4);spacing->setValue(25);
    form->addRow("Direction X",directionX);form->addRow("Direction Y",directionY);form->addRow("Direction Z",directionZ);
    form->addRow("Total instances",instances);form->addRow("Spacing (mm)",spacing);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    form->addRow(buttons);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.linearPatternAlongVector(ids.first(),directionX->value(),directionY->value(),directionZ->value(),
                                           instances->value(),spacing->value(),&error)){
        reportError("Linear pattern along vector",error);return;
    }
    refresh(true);
}

void MainWindow::rectangularPatternSelectedBody() {
    if(!requireBodyMode("Rectangular pattern"))return;
    const auto ids=selectedIds();
    if(ids.size()!=1){reportError("Rectangular pattern","Select exactly one body.");return;}
    QDialog dialog(this); dialog.setWindowTitle("Two-direction rectangular pattern");
    auto* form=new QFormLayout(&dialog);
    auto makeAxis=[&](){auto* combo=new QComboBox(&dialog);combo->addItems({"X","Y","Z"});return combo;};
    auto* axis1=makeAxis(); auto* count1=new QSpinBox(&dialog);count1->setRange(1,1000);count1->setValue(3);
    auto* spacing1=new QDoubleSpinBox(&dialog);spacing1->setRange(-1e6,1e6);spacing1->setDecimals(5);spacing1->setValue(30);
    auto* axis2=makeAxis();axis2->setCurrentIndex(1);auto* count2=new QSpinBox(&dialog);count2->setRange(1,1000);count2->setValue(2);
    auto* spacing2=new QDoubleSpinBox(&dialog);spacing2->setRange(-1e6,1e6);spacing2->setDecimals(5);spacing2->setValue(30);
    form->addRow("First axis",axis1);form->addRow("First-axis instances",count1);form->addRow("First-axis spacing (mm)",spacing1);
    form->addRow("Second axis",axis2);form->addRow("Second-axis instances",count2);form->addRow("Second-axis spacing (mm)",spacing2);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.rectangularPattern(ids.first(),static_cast<cad::Axis>(axis1->currentIndex()),count1->value(),spacing1->value(),
                                     static_cast<cad::Axis>(axis2->currentIndex()),count2->value(),spacing2->value(),&error)){
        reportError("Rectangular pattern",error);return;
    }
    refresh(true);
}

void MainWindow::rectangularPatternAlongVectorsSelectedBody(){
    if(!requireBodyMode("Vector rectangular pattern"))return;
    const auto ids=selectedIds();
    if(ids.size()!=1){reportError("Vector rectangular pattern","Select exactly one body.");return;}
    QDialog dialog(this);dialog.setWindowTitle("Two-vector rectangular pattern");dialog.setMinimumWidth(410);
    auto* form=new QFormLayout(&dialog);
    auto makeCoordinate=[&](double value){
        auto* field=new QDoubleSpinBox(&dialog);field->setRange(-1e6,1e6);field->setDecimals(5);field->setValue(value);return field;
    };
    auto* firstX=makeCoordinate(1);auto* firstY=makeCoordinate(0);auto* firstZ=makeCoordinate(0);
    auto* secondX=makeCoordinate(0);auto* secondY=makeCoordinate(1);auto* secondZ=makeCoordinate(0);
    auto* firstCount=new QSpinBox(&dialog);firstCount->setRange(1,1000);firstCount->setValue(3);
    auto* firstSpacing=makeCoordinate(30);
    auto* secondCount=new QSpinBox(&dialog);secondCount->setRange(1,1000);secondCount->setValue(2);
    auto* secondSpacing=makeCoordinate(30);
    form->addRow("First direction X",firstX);form->addRow("First direction Y",firstY);form->addRow("First direction Z",firstZ);
    form->addRow("First-direction instances",firstCount);form->addRow("First-direction spacing (mm)",firstSpacing);
    form->addRow("Second direction X",secondX);form->addRow("Second direction Y",secondY);form->addRow("Second direction Z",secondZ);
    form->addRow("Second-direction instances",secondCount);form->addRow("Second-direction spacing (mm)",secondSpacing);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.rectangularPatternAlongVectors(ids.first(),firstX->value(),firstY->value(),firstZ->value(),
            firstCount->value(),firstSpacing->value(),secondX->value(),secondY->value(),secondZ->value(),
            secondCount->value(),secondSpacing->value(),&error)){
        reportError("Vector rectangular pattern",error);return;
    }
    refresh(true);
}

void MainWindow::circularPatternSelectedBody() {
    if (!requireBodyMode("Circular pattern")) return;
    const auto ids=selectedIds();
    if (ids.size()!=1) { reportError("Circular pattern","Select exactly one body."); return; }
    QDialog dialog(this);
    dialog.setWindowTitle("Circular pattern");
    auto* form=new QFormLayout(&dialog);
    auto* axis=new QComboBox(&dialog);
    axis->addItems({"X","Y","Z"});
    form->addRow("Rotation axis",axis);
    auto makeCoordinate=[&]{
        auto* field=new QDoubleSpinBox(&dialog);
        field->setRange(-1e6,1e6);
        field->setDecimals(5);
        field->setSingleStep(1.0);
        field->setValue(0.0);
        return field;
    };
    auto* centerU=makeCoordinate();
    auto* centerV=makeCoordinate();
    auto* centerULabel=new QLabel("Center X (mm)",&dialog);
    auto* centerVLabel=new QLabel("Center Y (mm)",&dialog);
    form->addRow(centerULabel,centerU);
    form->addRow(centerVLabel,centerV);
    auto* count=new QSpinBox(&dialog);
    count->setRange(2,1000);
    count->setValue(4);
    form->addRow("Total instances",count);
    auto* angle=new QDoubleSpinBox(&dialog);
    angle->setRange(1.0,360.0);
    angle->setDecimals(3);
    angle->setSingleStep(15.0);
    angle->setValue(360.0);
    form->addRow("Total angle (degrees)",angle);
    auto updateCenterLabels=[=](int index){
        if (index==0) { centerULabel->setText("Center Y (mm)"); centerVLabel->setText("Center Z (mm)"); }
        else if (index==1) { centerULabel->setText("Center X (mm)"); centerVLabel->setText("Center Z (mm)"); }
        else { centerULabel->setText("Center X (mm)"); centerVLabel->setText("Center Y (mm)"); }
    };
    connect(axis,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,updateCenterLabels);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,&dialog);
    form->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if (dialog.exec()!=QDialog::Accepted) return;
    QString error;
    if (!document_.circularPattern(ids.first(),static_cast<cad::Axis>(axis->currentIndex()),
                                   centerU->value(),centerV->value(),count->value(),angle->value(),&error)) {
        reportError("Circular pattern",error);
        return;
    }
    refresh(true);
}

void MainWindow::circularPatternAboutAxisSelectedBody() {
    if(!requireBodyMode("Circular pattern about custom axis"))return;
    const auto ids=selectedIds();
    if(ids.size()!=1){reportError("Circular pattern about custom axis","Select exactly one body.");return;}
    QDialog dialog(this);dialog.setWindowTitle("Circular pattern about custom axis");
    auto* form=new QFormLayout(&dialog);
    auto field=[&](double minimum,double maximum,double value){
        auto* spin=new QDoubleSpinBox(&dialog);spin->setRange(minimum,maximum);spin->setDecimals(5);spin->setSingleStep(1);spin->setValue(value);return spin;
    };
    auto* originX=field(-1e6,1e6,0);auto* originY=field(-1e6,1e6,0);auto* originZ=field(-1e6,1e6,0);
    auto* axisX=field(-1e6,1e6,0);auto* axisY=field(-1e6,1e6,0);auto* axisZ=field(-1e6,1e6,1);
    auto* count=new QSpinBox(&dialog);count->setRange(2,1000);count->setValue(4);
    auto* angle=field(1,360,360);
    form->addRow("Axis origin X (mm)",originX);form->addRow("Axis origin Y (mm)",originY);form->addRow("Axis origin Z (mm)",originZ);
    form->addRow("Axis direction X",axisX);form->addRow("Axis direction Y",axisY);form->addRow("Axis direction Z",axisZ);
    form->addRow("Total instances",count);form->addRow("Total angle (degrees)",angle);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.circularPatternAboutAxis(ids.first(),originX->value(),originY->value(),originZ->value(),
        axisX->value(),axisY->value(),axisZ->value(),count->value(),angle->value(),&error)){
        reportError("Circular pattern about custom axis",error);return;
    }
    refresh(true);
}

void MainWindow::holeSelectedBody() {
    if (!requireBodyMode("Hole")) return;
    const auto ids=selectedIds();
    if (ids.size()!=1) { reportError("Hole","Select exactly one body."); return; }
    const cad::Part* part=document_.find(ids.first());
    if (!part) return;
    Bnd_Box bounds;
    BRepBndLib::Add(part->shape,bounds);
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;
    bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);

    QDialog dialog(this);
    dialog.setWindowTitle("Create vertical hole");
    dialog.setMinimumWidth(360);
    auto* form=new QFormLayout(&dialog);
    auto makeField=[&](double minimum,double maximum,double step,double value){
        auto* field=new QDoubleSpinBox(&dialog);
        field->setRange(minimum,maximum);
        field->setDecimals(5);
        field->setSingleStep(step);
        field->setValue(value);
        return field;
    };
    auto* x=makeField(-1e6,1e6,1.0,(xmin+xmax)/2.0);
    auto* y=makeField(-1e6,1e6,1.0,(ymin+ymax)/2.0);
    auto* entryZ=makeField(-1e6,1e6,1.0,zmax);
    auto* diameter=makeField(0.00001,1e6,1.0,5.0);
    auto* depth=makeField(0.00001,1e6,1.0,std::max(1.0,zmax-zmin));
    auto* depthLabel=new QLabel("Blind depth (mm)",&dialog);
    auto* style=new QComboBox(&dialog);style->addItems({"Simple hole","Counterbore","Countersink"});
    auto* headDiameter=makeField(0.00001,1e6,1.0,8.0);
    auto* headDetail=makeField(0.00001,178.0,1.0,3.0);
    auto* headDetailLabel=new QLabel("Counterbore depth (mm)",&dialog);
    auto* headDiameterLabel=new QLabel("Head diameter (mm)",&dialog);
    auto* throughAll=new QCheckBox("Through all",&dialog);
    throughAll->setChecked(true);
    depth->setEnabled(false);
    depthLabel->setEnabled(false);
    form->addRow("Center X (mm)",x);
    form->addRow("Center Y (mm)",y);
    form->addRow("Entry Z (mm)",entryZ);
    form->addRow("Diameter (mm)",diameter);
    form->addRow("Hole type",style);
    form->addRow(headDiameterLabel,headDiameter);
    form->addRow(headDetailLabel,headDetail);
    form->addRow(depthLabel,depth);
    form->addRow(throughAll);
    connect(throughAll,&QCheckBox::toggled,&dialog,[depth,depthLabel](bool enabled){
        depth->setEnabled(!enabled);
        depthLabel->setEnabled(!enabled);
    });
    headDiameterLabel->setVisible(false);headDiameter->setVisible(false);
    headDetailLabel->setVisible(false);headDetail->setVisible(false);
    connect(style,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,
            [headDiameterLabel,headDiameter,headDetailLabel,headDetail](int index){
        const bool enabled=index!=0;
        headDiameterLabel->setVisible(enabled);headDiameter->setVisible(enabled);
        headDetailLabel->setVisible(enabled);headDetail->setVisible(enabled);
        headDetailLabel->setText(index==1?"Counterbore depth (mm)":"Included angle (degrees)");
        headDetail->setRange(index==1?0.00001:1.00001,index==1?1e6:178.99999);
        headDetail->setValue(index==1?3.0:90.0);
    });
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,&dialog);
    form->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if (dialog.exec()!=QDialog::Accepted) return;

    cad::HoleCommand command;
    QString error;
    bool prepared=false;
    if(style->currentIndex()==1)
        prepared=document_.prepareCounterbore(ids.first(),x->value(),y->value(),entryZ->value(),diameter->value(),
            depth->value(),throughAll->isChecked(),headDiameter->value(),headDetail->value(),&command,&error);
    else if(style->currentIndex()==2)
        prepared=document_.prepareCountersink(ids.first(),x->value(),y->value(),entryZ->value(),diameter->value(),
            depth->value(),throughAll->isChecked(),headDiameter->value(),headDetail->value(),&command,&error);
    else
        prepared=document_.prepareHole(ids.first(),x->value(),y->value(),entryZ->value(),diameter->value(),
            depth->value(),throughAll->isChecked(),&command,&error);
    if (!prepared) {
        reportError("Hole",error);
        return;
    }
    runPreviewAsync<cad::HolePreview>(this, statusBar(), "Hole",
        "Building hole preview...", [command] {
            return cad::SceneDocument::buildHolePreview(command);
        }, [this, command](const cad::HolePreview& preview) {
            confirmPreview("Preview hole",
                "Inspect the translucent gold result. Apply this hole to the selected body?", preview.shape,
                [this, command, preview] {
                    QString commitError;
                    if (!document_.commitHole(command,preview.shape,&commitError))
                        reportError("Hole",commitError);
                    else refresh();
                });
        });
}

void MainWindow::holeGridSelectedBody() {
    if(!requireBodyMode("Hole grid"))return;
    const auto ids=selectedIds();if(ids.size()!=1){reportError("Hole grid","Select exactly one body.");return;}
    const cad::Part* part=document_.find(ids.first());if(!part)return;
    Bnd_Box bounds;BRepBndLib::AddOptimal(part->shape,bounds,false,false);
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    QDialog dialog(this);dialog.setWindowTitle("Create rectangular hole grid");auto* form=new QFormLayout(&dialog);
    auto field=[&](double min,double max,double value){auto* spin=new QDoubleSpinBox(&dialog);spin->setRange(min,max);spin->setDecimals(5);spin->setValue(value);return spin;};
    auto* x=field(-1e6,1e6,(xmin+xmax)/2-5);auto* y=field(-1e6,1e6,(ymin+ymax)/2-5);
    auto* z=field(-1e6,1e6,zmax);auto* diameter=field(0.00001,1e6,2);auto* depth=field(0.00001,1e6,zmax-zmin);
    auto* countX=new QSpinBox(&dialog);countX->setRange(1,100);countX->setValue(3);
    auto* spacingX=field(-1e6,1e6,5);auto* countY=new QSpinBox(&dialog);countY->setRange(1,100);countY->setValue(2);
    auto* spacingY=field(-1e6,1e6,5);auto* through=new QCheckBox("Through all",&dialog);through->setChecked(true);
    depth->setEnabled(false);connect(through,&QCheckBox::toggled,depth,&QDoubleSpinBox::setDisabled);
    form->addRow("Start center X (mm)",x);form->addRow("Start center Y (mm)",y);form->addRow("Entry Z (mm)",z);
    form->addRow("Hole diameter (mm)",diameter);form->addRow("Blind depth (mm)",depth);form->addRow(through);
    form->addRow("Columns",countX);form->addRow("Column spacing (mm)",spacingX);
    form->addRow("Rows",countY);form->addRow("Row spacing (mm)",spacingY);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.holePattern(ids.first(),x->value(),y->value(),z->value(),diameter->value(),depth->value(),through->isChecked(),
                              countX->value(),spacingX->value(),countY->value(),spacingY->value(),&error)){
        reportError("Hole grid",error);return;
    }
    refresh();
}

void MainWindow::circularHolePatternSelectedBody() {
    if(!requireBodyMode("Circular hole pattern"))return;
    const auto ids=selectedIds();
    if(ids.size()!=1){reportError("Circular hole pattern","Select exactly one body.");return;}
    QDialog dialog(this);dialog.setWindowTitle("Circular hole pattern");
    auto* form=new QFormLayout(&dialog);
    auto field=[&](double minimum,double maximum,double value){auto* spin=new QDoubleSpinBox(&dialog);spin->setRange(minimum,maximum);spin->setDecimals(4);spin->setValue(value);return spin;};
    auto* centerX=field(-1e6,1e6,40);auto* centerY=field(-1e6,1e6,40);auto* entryZ=field(-1e6,1e6,20);
    auto* boltRadius=field(0.00001,1e6,25);auto* diameter=field(0.00001,1e6,6);auto* depth=field(0.00001,1e6,20);
    auto* count=new QSpinBox(&dialog);count->setRange(2,1000);count->setValue(4);
    auto* angle=field(1,360,360);auto* throughAll=new QCheckBox("Through all",&dialog);throughAll->setChecked(true);
    depth->setEnabled(false);connect(throughAll,&QCheckBox::toggled,depth,&QDoubleSpinBox::setDisabled);
    form->addRow("Center X (mm)",centerX);form->addRow("Center Y (mm)",centerY);form->addRow("Entry Z (mm)",entryZ);
    form->addRow("Bolt-circle radius (mm)",boltRadius);form->addRow("Hole diameter (mm)",diameter);
    form->addRow("Blind depth (mm)",depth);form->addRow("Total instances",count);form->addRow("Pattern angle (degrees)",angle);
    form->addRow(throughAll);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    form->addRow(buttons);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.circularHolePattern(ids.first(),centerX->value(),centerY->value(),entryZ->value(),boltRadius->value(),
                                      diameter->value(),depth->value(),throughAll->isChecked(),count->value(),angle->value(),&error)){
        reportError("Circular hole pattern",error);return;
    }
    refresh(true);
}

void MainWindow::circularCountersinkPatternSelectedBody(){
    if(!requireBodyMode("Circular countersink pattern"))return;
    const auto ids=selectedIds();
    if(ids.size()!=1){reportError("Circular countersink pattern","Select exactly one body.");return;}
    const cad::Part* part=document_.find(ids.first());if(!part)return;
    Bnd_Box bounds;BRepBndLib::AddOptimal(part->shape,bounds,false,false);
    if(bounds.IsVoid()){reportError("Circular countersink pattern","The selected body has no measurable bounds.");return;}
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    const double minSpan=std::min(xmax-xmin,ymax-ymin);
    QDialog dialog(this);dialog.setWindowTitle("Circular countersink pattern");
    auto* form=new QFormLayout(&dialog);
    auto field=[&](double minimum,double maximum,double value){
        auto* spin=new QDoubleSpinBox(&dialog);spin->setRange(minimum,maximum);spin->setDecimals(4);spin->setValue(value);return spin;
    };
    auto* centerX=field(-1e6,1e6,(xmin+xmax)/2);auto* centerY=field(-1e6,1e6,(ymin+ymax)/2);
    auto* entryZ=field(-1e6,1e6,zmax);auto* radius=field(0.00001,1e6,std::max(1.0,std::min(15.0,minSpan*0.25)));
    auto* diameter=field(0.00001,1e6,std::max(1.0,std::min(4.0,minSpan*0.12)));
    auto* headDiameter=field(0.00001,1e6,std::max(2.0,std::min(8.0,minSpan*0.24)));
    auto* depth=field(0.00001,1e6,zmax-zmin);auto* angle=field(2,178,90);
    auto* count=new QSpinBox(&dialog);count->setRange(2,1000);count->setValue(4);
    auto* totalAngle=field(1,360,360);auto* throughAll=new QCheckBox("Through all",&dialog);throughAll->setChecked(true);
    depth->setEnabled(false);connect(throughAll,&QCheckBox::toggled,depth,&QDoubleSpinBox::setDisabled);
    form->addRow("Center X (mm)",centerX);form->addRow("Center Y (mm)",centerY);form->addRow("Entry Z (mm)",entryZ);
    form->addRow("Bolt-circle radius (mm)",radius);form->addRow("Drill diameter (mm)",diameter);
    form->addRow("Countersink diameter (mm)",headDiameter);form->addRow("Included angle (degrees)",angle);
    form->addRow("Blind depth (mm)",depth);form->addRow("Total instances",count);
    form->addRow("Pattern angle (degrees)",totalAngle);form->addRow(throughAll);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.circularCountersinkPattern(ids.first(),centerX->value(),centerY->value(),entryZ->value(),radius->value(),
            diameter->value(),depth->value(),throughAll->isChecked(),headDiameter->value(),angle->value(),count->value(),
            totalAngle->value(),&error)){
        reportError("Circular countersink pattern",error);return;
    }
    refresh(true);
}

void MainWindow::circularCounterborePatternSelectedBody(){
    if(!requireBodyMode("Circular counterbore pattern"))return;
    const auto ids=selectedIds();if(ids.size()!=1){reportError("Circular counterbore pattern","Select exactly one body.");return;}
    const cad::Part* part=document_.find(ids.first());if(!part)return;
    Bnd_Box bounds;BRepBndLib::AddOptimal(part->shape,bounds,false,false);
    if(bounds.IsVoid()){reportError("Circular counterbore pattern","The selected body has no measurable bounds.");return;}
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    const double minSpan=std::min(xmax-xmin,ymax-ymin);
    QDialog dialog(this);dialog.setWindowTitle("Circular counterbore pattern");auto* form=new QFormLayout(&dialog);
    auto field=[&](double minimum,double maximum,double value){auto* spin=new QDoubleSpinBox(&dialog);spin->setRange(minimum,maximum);spin->setDecimals(4);spin->setValue(value);return spin;};
    auto* centerX=field(-1e6,1e6,(xmin+xmax)/2);auto* centerY=field(-1e6,1e6,(ymin+ymax)/2);
    auto* entryZ=field(-1e6,1e6,zmax);auto* radius=field(0.00001,1e6,std::max(1.0,std::min(15.0,minSpan*0.25)));
    auto* diameter=field(0.00001,1e6,std::max(1.0,std::min(4.0,minSpan*0.12)));
    auto* headDiameter=field(0.00001,1e6,std::max(2.0,std::min(8.0,minSpan*0.24)));
    auto* headDepth=field(0.00001,1e6,std::max(0.5,std::min(3.0,(zmax-zmin)*0.15)));
    auto* depth=field(0.00001,1e6,zmax-zmin);auto* count=new QSpinBox(&dialog);count->setRange(2,1000);count->setValue(4);
    auto* totalAngle=field(1,360,360);auto* throughAll=new QCheckBox("Through all",&dialog);throughAll->setChecked(true);
    depth->setEnabled(false);connect(throughAll,&QCheckBox::toggled,depth,&QDoubleSpinBox::setDisabled);
    form->addRow("Center X (mm)",centerX);form->addRow("Center Y (mm)",centerY);form->addRow("Entry Z (mm)",entryZ);
    form->addRow("Bolt-circle radius (mm)",radius);form->addRow("Drill diameter (mm)",diameter);
    form->addRow("Counterbore diameter (mm)",headDiameter);form->addRow("Counterbore depth (mm)",headDepth);
    form->addRow("Blind hole depth (mm)",depth);form->addRow("Total instances",count);form->addRow("Pattern angle (degrees)",totalAngle);
    form->addRow(throughAll);auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.circularCounterborePattern(ids.first(),centerX->value(),centerY->value(),entryZ->value(),radius->value(),
        diameter->value(),depth->value(),throughAll->isChecked(),headDiameter->value(),headDepth->value(),count->value(),
        totalAngle->value(),&error)){
        reportError("Circular counterbore pattern",error);return;
    }
    refresh(true);
}

void MainWindow::linearCounterborePatternSelectedBody(){
    if(!requireBodyMode("Linear counterbore pattern"))return;
    const auto ids=selectedIds();if(ids.size()!=1){reportError("Linear counterbore pattern","Select exactly one body.");return;}
    const cad::Part* part=document_.find(ids.first());if(!part)return;
    Bnd_Box bounds;BRepBndLib::AddOptimal(part->shape,bounds,false,false);
    if(bounds.IsVoid()){reportError("Linear counterbore pattern","The selected body has no measurable bounds.");return;}
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    const double xSpan=xmax-xmin,ySpan=ymax-ymin,zSpan=zmax-zmin;
    QDialog dialog(this);dialog.setWindowTitle("Linear counterbore pattern");auto* form=new QFormLayout(&dialog);
    auto field=[&](double minimum,double maximum,double value){auto* spin=new QDoubleSpinBox(&dialog);spin->setRange(minimum,maximum);spin->setDecimals(4);spin->setValue(value);return spin;};
    auto* count=new QSpinBox(&dialog);count->setRange(2,100);count->setValue(3);
    const double head=std::max(2.0,std::min(8.0,std::min(xSpan,ySpan)*0.24));
    const double drill=std::max(1.0,std::min(4.0,head*0.5));
    const double spacing=std::max(head+1.0,std::min(12.0,xSpan/3.0));
    auto* centerX=field(-1e6,1e6,(xmin+xmax)/2-spacing);auto* centerY=field(-1e6,1e6,(ymin+ymax)/2);
    auto* entryZ=field(-1e6,1e6,zmax);auto* diameter=field(0.0001,1e6,drill);
    auto* headDiameter=field(0.0001,1e6,head);auto* headDepth=field(0.0001,1e6,std::max(0.5,std::min(3.0,zSpan*0.15)));
    auto* depth=field(0.0001,1e6,zSpan);auto* spacingX=field(-1e6,1e6,spacing);auto* spacingY=field(-1e6,1e6,0);
    auto* throughAll=new QCheckBox("Through all",&dialog);throughAll->setChecked(true);depth->setEnabled(false);
    connect(throughAll,&QCheckBox::toggled,depth,&QDoubleSpinBox::setDisabled);
    form->addRow("First center X (mm)",centerX);form->addRow("Center Y (mm)",centerY);form->addRow("Entry Z (mm)",entryZ);
    form->addRow("Drill diameter (mm)",diameter);form->addRow("Counterbore diameter (mm)",headDiameter);
    form->addRow("Counterbore depth (mm)",headDepth);form->addRow("Blind hole depth (mm)",depth);
    form->addRow("Instance count",count);form->addRow("Spacing X (mm)",spacingX);form->addRow("Spacing Y (mm)",spacingY);
    form->addRow(throughAll);auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.linearCounterborePattern(ids.first(),centerX->value(),centerY->value(),entryZ->value(),diameter->value(),
        depth->value(),throughAll->isChecked(),headDiameter->value(),headDepth->value(),count->value(),spacingX->value(),spacingY->value(),&error)){
        reportError("Linear counterbore pattern",error);return;
    }
    refresh(true);
}

void MainWindow::linearCountersinkPatternSelectedBody(){
    if(!requireBodyMode("Linear countersink pattern"))return;
    const auto ids=selectedIds();if(ids.size()!=1){reportError("Linear countersink pattern","Select exactly one body.");return;}
    const cad::Part* part=document_.find(ids.first());if(!part)return;
    Bnd_Box bounds;BRepBndLib::AddOptimal(part->shape,bounds,false,false);
    if(bounds.IsVoid()){reportError("Linear countersink pattern","The selected body has no measurable bounds.");return;}
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    const double xSpan=xmax-xmin,ySpan=ymax-ymin,zSpan=zmax-zmin;
    QDialog dialog(this);dialog.setWindowTitle("Linear countersink pattern");auto* form=new QFormLayout(&dialog);
    auto field=[&](double minimum,double maximum,double value){auto* spin=new QDoubleSpinBox(&dialog);spin->setRange(minimum,maximum);spin->setDecimals(4);spin->setValue(value);return spin;};
    auto* count=new QSpinBox(&dialog);count->setRange(2,100);count->setValue(3);
    const double head=std::max(2.0,std::min(8.0,std::min(xSpan,ySpan)*0.24));
    const double drill=std::max(1.0,std::min(4.0,head*0.5));
    const double spacing=std::max(head+1.0,std::min(12.0,xSpan/3.0));
    auto* centerX=field(-1e6,1e6,(xmin+xmax)/2-spacing);auto* centerY=field(-1e6,1e6,(ymin+ymax)/2);
    auto* entryZ=field(-1e6,1e6,zmax);auto* diameter=field(0.0001,1e6,drill);
    auto* headDiameter=field(0.0001,1e6,head);auto* angle=field(2,178,90);
    auto* depth=field(0.0001,1e6,zSpan);auto* spacingX=field(-1e6,1e6,spacing);auto* spacingY=field(-1e6,1e6,0);
    auto* throughAll=new QCheckBox("Through all",&dialog);throughAll->setChecked(true);depth->setEnabled(false);
    connect(throughAll,&QCheckBox::toggled,depth,&QDoubleSpinBox::setDisabled);
    form->addRow("First center X (mm)",centerX);form->addRow("Center Y (mm)",centerY);form->addRow("Entry Z (mm)",entryZ);
    form->addRow("Drill diameter (mm)",diameter);form->addRow("Countersink diameter (mm)",headDiameter);
    form->addRow("Included angle (degrees)",angle);form->addRow("Blind hole depth (mm)",depth);
    form->addRow("Instance count",count);form->addRow("Spacing X (mm)",spacingX);form->addRow("Spacing Y (mm)",spacingY);
    form->addRow(throughAll);auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.linearCountersinkPattern(ids.first(),centerX->value(),centerY->value(),entryZ->value(),diameter->value(),
        depth->value(),throughAll->isChecked(),headDiameter->value(),angle->value(),count->value(),spacingX->value(),spacingY->value(),&error)){
        reportError("Linear countersink pattern",error);return;
    }
    refresh(true);
}

void MainWindow::dovetailGrooveSelectedBody() {
    if(!requireBodyMode("Dovetail groove"))return;
    const auto ids=selectedIds();
    if(ids.size()!=1){reportError("Dovetail groove","Select exactly one body.");return;}
    const cad::Part* part=document_.find(ids.first());
    if(!part)return;
    Bnd_Box bounds;BRepBndLib::AddOptimal(part->shape,bounds,false,false);
    if(bounds.IsVoid()){reportError("Dovetail groove","The selected body has no measurable bounds.");return;}
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;
    bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    QDialog dialog(this);dialog.setWindowTitle("Cut dovetail groove");dialog.setMinimumWidth(380);
    auto* form=new QFormLayout(&dialog);
    auto field=[&](double minimum,double maximum,double step,double value){
        auto* spin=new QDoubleSpinBox(&dialog);spin->setRange(minimum,maximum);spin->setDecimals(4);
        spin->setSingleStep(step);spin->setValue(value);return spin;
    };
    auto* centerX=field(-1e6,1e6,1.0,(xmin+xmax)/2.0);
    auto* centerY=field(-1e6,1e6,1.0,(ymin+ymax)/2.0);
    auto* entryZ=field(-1e6,1e6,1.0,zmax);
    auto* mouthWidth=field(0.0001,1e6,0.5,std::max(2.0,(xmax-xmin)*0.2));
    auto* bottomWidth=field(0.0002,1e6,0.5,std::max(4.0,(xmax-xmin)*0.35));
    auto* depth=field(0.0001,1e6,0.5,std::max(2.0,(zmax-zmin)*0.25));
    auto* length=field(0.0001,1e6,1.0,std::max(10.0,(ymax-ymin)*0.6));
    auto* angle=field(-360.0,360.0,5.0,0.0);
    form->addRow("Center X (mm)",centerX);form->addRow("Center Y (mm)",centerY);
    form->addRow("Entry Z (mm)",entryZ);form->addRow("Mouth width (mm)",mouthWidth);
    form->addRow("Undercut width (mm)",bottomWidth);form->addRow("Depth (mm)",depth);
    form->addRow("Length (mm)",length);form->addRow("Rotation about Z (degrees)",angle);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    cad::DovetailGrooveCommand command;QString error;
    if(!document_.prepareDovetailGroove(ids.first(),centerX->value(),centerY->value(),entryZ->value(),
          mouthWidth->value(),bottomWidth->value(),depth->value(),length->value(),angle->value(),&command,&error)){
        reportError("Dovetail groove",error);return;
    }
    runPreviewAsync<cad::DovetailGroovePreview>(this,statusBar(),"Dovetail groove",
        "Building dovetail-groove preview...",[command]{return cad::SceneDocument::buildDovetailGroovePreview(command);},
        [this,command](const cad::DovetailGroovePreview& preview){
            confirmPreview("Preview dovetail groove",
                "Inspect the translucent gold result. Apply this groove to the selected body?",preview.shape,
                [this,command,preview]{
                    QString commitError;
                    if(!document_.commitDovetailGroove(command,preview.shape,&commitError))
                        reportError("Dovetail groove",commitError);
                    else refresh();
                });
        });
}

void MainWindow::tSlotGrooveSelectedBody(){
    if(!requireBodyMode("T-slot groove"))return;
    const auto ids=selectedIds();
    if(ids.size()!=1){reportError("T-slot groove","Select exactly one body.");return;}
    const cad::Part* part=document_.find(ids.first());if(!part)return;
    Bnd_Box bounds;BRepBndLib::AddOptimal(part->shape,bounds,false,false);
    if(bounds.IsVoid()){reportError("T-slot groove","The selected body has no measurable bounds.");return;}
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;
    bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    QDialog dialog(this);dialog.setWindowTitle("Cut T-slot groove");dialog.setMinimumWidth(390);
    auto* form=new QFormLayout(&dialog);
    auto field=[&](double minimum,double maximum,double step,double value){
        auto* spin=new QDoubleSpinBox(&dialog);spin->setRange(minimum,maximum);spin->setDecimals(4);
        spin->setSingleStep(step);spin->setValue(value);return spin;
    };
    auto* centerX=field(-1e6,1e6,1.0,(xmin+xmax)/2.0);
    auto* centerY=field(-1e6,1e6,1.0,(ymin+ymax)/2.0);
    auto* entryZ=field(-1e6,1e6,1.0,zmax);
    auto* neckWidth=field(0.0001,1e6,0.5,std::max(2.0,(xmax-xmin)*0.15));
    auto* undercutWidth=field(0.0002,1e6,0.5,std::max(4.0,(xmax-xmin)*0.3));
    auto* depth=field(0.0001,1e6,0.5,std::max(3.0,(zmax-zmin)*0.3));
    auto* undercutHeight=field(0.0001,1e6,0.5,std::max(1.0,(zmax-zmin)*0.1));
    auto* length=field(0.0001,1e6,1.0,std::max(10.0,(ymax-ymin)*0.6));
    auto* angle=field(-360.0,360.0,5.0,0.0);
    form->addRow("Center X (mm)",centerX);form->addRow("Center Y (mm)",centerY);
    form->addRow("Entry Z (mm)",entryZ);form->addRow("Neck width (mm)",neckWidth);
    form->addRow("Undercut width (mm)",undercutWidth);form->addRow("Total depth (mm)",depth);
    form->addRow("Undercut height (mm)",undercutHeight);form->addRow("Length (mm)",length);
    form->addRow("Rotation about Z (degrees)",angle);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    cad::TSlotGrooveCommand command;QString error;
    if(!document_.prepareTSlotGroove(ids.first(),centerX->value(),centerY->value(),entryZ->value(),
          neckWidth->value(),undercutWidth->value(),depth->value(),undercutHeight->value(),length->value(),
          angle->value(),&command,&error)){
        reportError("T-slot groove",error);return;
    }
    runPreviewAsync<cad::TSlotGroovePreview>(this,statusBar(),"T-slot groove",
        "Building T-slot preview...",[command]{return cad::SceneDocument::buildTSlotGroovePreview(command);},
        [this,command](const cad::TSlotGroovePreview& preview){
            confirmPreview("Preview T-slot groove",
                "Inspect the translucent gold result. Apply this T-slot to the selected body?",preview.shape,
                [this,command,preview]{
                    QString commitError;
                    if(!document_.commitTSlotGroove(command,preview.shape,&commitError))
                        reportError("T-slot groove",commitError);
                    else refresh();
                });
        });
}

void MainWindow::roundedSlotSelectedBody(){
    if(!requireBodyMode("Rounded slot"))return;
    const auto ids=selectedIds();
    if(ids.size()!=1){reportError("Rounded slot","Select exactly one body.");return;}
    const cad::Part* part=document_.find(ids.first());if(!part)return;
    Bnd_Box bounds;BRepBndLib::Add(part->shape,bounds);
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    if(bounds.IsVoid()){reportError("Rounded slot","The selected body has no measurable bounds.");return;}
    QDialog dialog(this);dialog.setWindowTitle("Cut rounded slot");dialog.setMinimumWidth(370);
    auto* form=new QFormLayout(&dialog);
    auto field=[&](double minimum,double maximum,double step,double value){
        auto* spin=new QDoubleSpinBox(&dialog);spin->setRange(minimum,maximum);spin->setDecimals(4);
        spin->setSingleStep(step);spin->setValue(value);return spin;
    };
    const double width=std::max(2.0,std::min(12.0,(xmax-xmin)*0.2));
    auto* centerX=field(-1e6,1e6,1.0,(xmin+xmax)/2.0);
    auto* centerY=field(-1e6,1e6,1.0,(ymin+ymax)/2.0);
    auto* entryZ=field(-1e6,1e6,1.0,zmax);
    auto* slotWidth=field(0.0001,1e6,0.5,width);
    auto* length=field(0.0001,1e6,1.0,std::max(width,std::min(24.0,(ymax-ymin)*0.6)));
    auto* depth=field(0.0001,1e6,0.5,std::max(3.0,(zmax-zmin)*0.3));
    auto* angle=field(-360.0,360.0,5.0,0.0);
    form->addRow("Center X (mm)",centerX);form->addRow("Center Y (mm)",centerY);
    form->addRow("Entry Z (mm)",entryZ);form->addRow("Slot width (mm)",slotWidth);
    form->addRow("Overall length (mm)",length);form->addRow("Depth (mm)",depth);
    form->addRow("Rotation about Z (degrees)",angle);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    cad::RoundedSlotCommand command;QString error;
    if(!document_.prepareRoundedSlot(ids.first(),centerX->value(),centerY->value(),entryZ->value(),
          slotWidth->value(),length->value(),depth->value(),angle->value(),&command,&error)){
        reportError("Rounded slot",error);return;
    }
    runPreviewAsync<cad::RoundedSlotPreview>(this,statusBar(),"Rounded slot",
        "Building rounded-slot preview...",[command]{return cad::SceneDocument::buildRoundedSlotPreview(command);},
        [this,command](const cad::RoundedSlotPreview& preview){
            confirmPreview("Preview rounded slot",
                "Inspect the translucent gold result. Apply this rounded slot to the selected body?",preview.shape,
                [this,command,preview]{
                    QString commitError;
                    if(!document_.commitRoundedSlot(command,preview.shape,&commitError))
                        reportError("Rounded slot",commitError);
                    else refresh();
                });
        });
}

void MainWindow::keyholeSlotSelectedBody(){
    if(!requireBodyMode("Keyhole slot"))return;
    const auto ids=selectedIds();
    if(ids.size()!=1){reportError("Keyhole slot","Select exactly one body.");return;}
    const cad::Part* part=document_.find(ids.first());if(!part)return;
    Bnd_Box bounds;BRepBndLib::Add(part->shape,bounds);
    if(bounds.IsVoid()){reportError("Keyhole slot","The selected body has no measurable bounds.");return;}
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    QDialog dialog(this);dialog.setWindowTitle("Cut keyhole slot");dialog.setMinimumWidth(380);
    auto* form=new QFormLayout(&dialog);
    auto field=[&](double minimum,double maximum,double step,double value){
        auto* spin=new QDoubleSpinBox(&dialog);spin->setRange(minimum,maximum);spin->setDecimals(4);
        spin->setSingleStep(step);spin->setValue(value);return spin;
    };
    const double head=std::max(4.0,std::min(10.0,(xmax-xmin)*0.25));
    const double throat=std::max(1.0,std::min(head*0.5,4.0));
    const double throatLength=std::max(4.0,std::min(12.0,(ymax-ymin)*0.4));
    auto* centerX=field(-1e6,1e6,1.0,(xmin+xmax)/2.0);
    auto* centerY=field(-1e6,1e6,1.0,(ymin+ymax)/2.0-throatLength/3.0);
    auto* entryZ=field(-1e6,1e6,1.0,zmax);
    auto* headDiameter=field(0.0001,1e6,0.5,head);
    auto* throatWidth=field(0.0001,1e6,0.5,throat);
    auto* length=field(0.0001,1e6,1.0,throatLength);
    auto* depth=field(0.0001,1e6,0.5,std::max(3.0,(zmax-zmin)*0.3));
    auto* angle=field(-360.0,360.0,5.0,0.0);
    form->addRow("Head center X (mm)",centerX);form->addRow("Head center Y (mm)",centerY);
    form->addRow("Entry Z (mm)",entryZ);form->addRow("Head diameter (mm)",headDiameter);
    form->addRow("Throat width (mm)",throatWidth);form->addRow("Throat length (mm)",length);
    form->addRow("Depth (mm)",depth);form->addRow("Rotation about Z (degrees)",angle);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    cad::KeyholeSlotCommand command;QString error;
    if(!document_.prepareKeyholeSlot(ids.first(),centerX->value(),centerY->value(),entryZ->value(),
          headDiameter->value(),throatWidth->value(),length->value(),depth->value(),angle->value(),
          &command,&error)){
        reportError("Keyhole slot",error);return;
    }
    runPreviewAsync<cad::KeyholeSlotPreview>(this,statusBar(),"Keyhole slot",
        "Building keyhole-slot preview...",[command]{return cad::SceneDocument::buildKeyholeSlotPreview(command);},
        [this,command](const cad::KeyholeSlotPreview& preview){
            confirmPreview("Preview keyhole slot",
                "Inspect the translucent gold result. Apply this keyhole slot to the selected body?",preview.shape,
                [this,command,preview]{
                    QString commitError;
                    if(!document_.commitKeyholeSlot(command,preview.shape,&commitError))
                        reportError("Keyhole slot",commitError);
                    else refresh();
                });
        });
}

void MainWindow::roundedSlotPatternSelectedBody(){
    if(!requireBodyMode("Rounded-slot pattern"))return;
    const auto ids=selectedIds();
    if(ids.size()!=1){reportError("Rounded-slot pattern","Select exactly one body.");return;}
    const cad::Part* part=document_.find(ids.first());if(!part)return;
    Bnd_Box bounds;BRepBndLib::Add(part->shape,bounds);
    if(bounds.IsVoid()){reportError("Rounded-slot pattern","The selected body has no measurable bounds.");return;}
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    QDialog dialog(this);dialog.setWindowTitle("Linear rounded-slot pattern");dialog.setMinimumWidth(390);
    auto* form=new QFormLayout(&dialog);
    auto field=[&](double minimum,double maximum,double step,double value){
        auto* spin=new QDoubleSpinBox(&dialog);spin->setRange(minimum,maximum);spin->setDecimals(4);
        spin->setSingleStep(step);spin->setValue(value);return spin;
    };
    const double width=std::max(2.0,std::min(10.0,(xmax-xmin)*0.2));
    const double spacing=std::max(width+1.0,std::min(12.0,(xmax-xmin)/3.0));
    auto* centerX=field(-1e6,1e6,1.0,(xmin+xmax)/2.0-spacing);
    auto* centerY=field(-1e6,1e6,1.0,(ymin+ymax)/2.0);
    auto* entryZ=field(-1e6,1e6,1.0,zmax);
    auto* slotWidth=field(0.0001,1e6,0.5,width);
    auto* length=field(0.0001,1e6,1.0,std::max(width,std::min(20.0,(ymax-ymin)*0.6)));
    auto* depth=field(0.0001,1e6,0.5,std::max(3.0,(zmax-zmin)*0.3));
    auto* angle=field(-360.0,360.0,5.0,0.0);
    auto* count=new QSpinBox(&dialog);count->setRange(2,100);count->setValue(3);
    auto* spacingX=field(-1e6,1e6,1.0,spacing);auto* spacingY=field(-1e6,1e6,1.0,0.0);
    form->addRow("First center X (mm)",centerX);form->addRow("Center Y (mm)",centerY);
    form->addRow("Entry Z (mm)",entryZ);form->addRow("Slot width (mm)",slotWidth);
    form->addRow("Overall length (mm)",length);form->addRow("Depth (mm)",depth);
    form->addRow("Rotation about Z (degrees)",angle);form->addRow("Instance count",count);
    form->addRow("Spacing X (mm)",spacingX);form->addRow("Spacing Y (mm)",spacingY);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    cad::RoundedSlotPatternCommand command;QString error;
    if(!document_.prepareRoundedSlotPattern(ids.first(),centerX->value(),centerY->value(),entryZ->value(),
          slotWidth->value(),length->value(),depth->value(),angle->value(),count->value(),
          spacingX->value(),spacingY->value(),&command,&error)){
        reportError("Rounded-slot pattern",error);return;
    }
    runPreviewAsync<cad::RoundedSlotPatternPreview>(this,statusBar(),"Rounded-slot pattern",
        "Building rounded-slot pattern preview...",[command]{return cad::SceneDocument::buildRoundedSlotPatternPreview(command);},
        [this,command](const cad::RoundedSlotPatternPreview& preview){
            confirmPreview("Preview rounded-slot pattern",
                "Inspect the translucent gold result. Apply this rounded-slot pattern to the selected body?",preview.shape,
                [this,command,preview]{
                    QString commitError;
                    if(!document_.commitRoundedSlotPattern(command,preview.shape,&commitError))
                        reportError("Rounded-slot pattern",commitError);
                    else refresh();
                });
        });
}

void MainWindow::linearKeyholeSlotPatternSelectedBody(){
    if(!requireBodyMode("Keyhole-slot pattern"))return;
    const auto ids=selectedIds();if(ids.size()!=1){reportError("Keyhole-slot pattern","Select exactly one body.");return;}
    const cad::Part* part=document_.find(ids.first());if(!part)return;
    Bnd_Box bounds;BRepBndLib::AddOptimal(part->shape,bounds,false,false);
    if(bounds.IsVoid()){reportError("Keyhole-slot pattern","The selected body has no measurable bounds.");return;}
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    QDialog dialog(this);dialog.setWindowTitle("Linear keyhole-slot pattern");auto* form=new QFormLayout(&dialog);
    auto field=[&](double minimum,double maximum,double value){auto* spin=new QDoubleSpinBox(&dialog);spin->setRange(minimum,maximum);spin->setDecimals(4);spin->setValue(value);return spin;};
    const double head=std::max(2.0,std::min(10.0,(xmax-xmin)*0.2));
    const double spacing=std::max(head+1.0,std::min(12.0,(xmax-xmin)/3.0));
    auto* centerX=field(-1e6,1e6,(xmin+xmax)/2-spacing);auto* centerY=field(-1e6,1e6,(ymin+ymax)/2);
    auto* entryZ=field(-1e6,1e6,zmax);auto* headDiameter=field(0.0001,1e6,head);
    auto* throatWidth=field(0.0001,1e6,head*0.5);auto* throatLength=field(0.0001,1e6,std::max(5.0,std::min(12.0,(ymax-ymin)*0.4)));
    auto* depth=field(0.0001,1e6,std::max(3.0,(zmax-zmin)*0.3));auto* angle=field(-360,360,0);
    auto* count=new QSpinBox(&dialog);count->setRange(2,100);count->setValue(3);
    auto* spacingX=field(-1e6,1e6,spacing);auto* spacingY=field(-1e6,1e6,0);
    form->addRow("First center X (mm)",centerX);form->addRow("Center Y (mm)",centerY);form->addRow("Entry Z (mm)",entryZ);
    form->addRow("Head diameter (mm)",headDiameter);form->addRow("Throat width (mm)",throatWidth);
    form->addRow("Throat length (mm)",throatLength);form->addRow("Depth (mm)",depth);form->addRow("Rotation about Z (degrees)",angle);
    form->addRow("Instance count",count);form->addRow("Spacing X (mm)",spacingX);form->addRow("Spacing Y (mm)",spacingY);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.linearKeyholeSlotPattern(ids.first(),centerX->value(),centerY->value(),entryZ->value(),headDiameter->value(),
        throatWidth->value(),throatLength->value(),depth->value(),angle->value(),count->value(),spacingX->value(),spacingY->value(),&error)){
        reportError("Keyhole-slot pattern",error);return;
    }
    refresh(true);
}

void MainWindow::splitSelectedBody() {
    if (!requireBodyMode("Split body")) return;
    const auto ids=selectedIds();
    if (ids.size()!=1) { reportError("Split body","Select exactly one body."); return; }
    const cad::Part* part=document_.find(ids.first());
    if (!part) return;
    Bnd_Box bounds;
    BRepBndLib::Add(part->shape,bounds);
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;
    bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    QDialog dialog(this);
    dialog.setWindowTitle("Split body with a world plane");
    auto* form=new QFormLayout(&dialog);
    auto* axis=new QComboBox(&dialog);
    axis->addItems({"X", "Y", "Z"});
    auto* offset=new QDoubleSpinBox(&dialog);
    offset->setRange(-1e6,1e6);
    offset->setDecimals(5);
    offset->setSingleStep(1.0);
    offset->setValue((zmin+zmax)/2.0);
    auto updateOffset=[=](int index){ offset->setValue(index==0?(xmin+xmax)/2.0:index==1?(ymin+ymax)/2.0:(zmin+zmax)/2.0); };
    connect(axis,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,updateOffset);
    form->addRow("Plane normal",axis);
    form->addRow("Plane coordinate (mm)",offset);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    form->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if (dialog.exec()!=QDialog::Accepted) return;
    QString error;
    if (!document_.splitBody(ids.first(),static_cast<cad::Axis>(axis->currentIndex()),offset->value(),&error)) {
        reportError("Split body",error); return;
    }
    refresh(true);
}

void MainWindow::shellSelectedBody() {
    if (!requireBodyMode("Shell")) return;
    const auto ids=selectedIds();
    if (ids.size()!=1) { reportError("Shell","Select exactly one body."); return; }
    QDialog dialog(this);
    dialog.setWindowTitle("Shell body");
    auto* form=new QFormLayout(&dialog);
    form->addRow(new QLabel("Opening faces (select one or more)",&dialog));
    const QStringList openingNames={"-X face", "+X face", "-Y face", "+Y face", "-Z face", "+Z face"};
    QVector<QCheckBox*> openingChecks;
    for(int i=0;i<openingNames.size();++i){
        auto* check=new QCheckBox(openingNames[i],&dialog);
        check->setChecked(i==5);
        openingChecks.append(check);
        form->addRow(QString(),check);
    }
    auto* thickness=new QDoubleSpinBox(&dialog);
    thickness->setRange(0.00001,1e6);
    thickness->setDecimals(5);
    thickness->setSingleStep(0.5);
    thickness->setValue(1.0);
    form->addRow("Wall thickness (mm)",thickness);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    form->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if (dialog.exec()!=QDialog::Accepted) return;
    QVector<cad::ShellOpeningFace> openings;
    for(int i=0;i<openingChecks.size();++i)
        if(openingChecks[i]->isChecked())openings.append({static_cast<cad::Axis>(i/2),(i%2)==1});
    QString error;
    if (!document_.shellBodyWithOpenings(ids.first(),openings,thickness->value(),&error)) {
        reportError("Shell",error); return;
    }
    refresh();
}

void MainWindow::draftSelectedFace() {
    if(!requireBodyMode("Draft face"))return;
    const auto ids=selectedIds();if(ids.size()!=1){reportError("Draft face","Select exactly one body.");return;}
    const cad::Part* part=document_.find(ids.first());if(!part)return;
    QDialog dialog(this);dialog.setWindowTitle("Draft a planar body face");auto* form=new QFormLayout(&dialog);
    int faceCount=0;for(TopExp_Explorer explorer(part->shape,TopAbs_FACE);explorer.More();explorer.Next())++faceCount;
    auto* face=new QSpinBox(&dialog);face->setRange(1,std::max(1,faceCount));face->setValue(1);
    auto* axis=new QComboBox(&dialog);axis->addItems({"X","Y","Z"});axis->setCurrentIndex(2);
    auto* offset=new QDoubleSpinBox(&dialog);offset->setRange(-1e6,1e6);offset->setDecimals(5);offset->setValue(0);
    auto* angle=new QDoubleSpinBox(&dialog);angle->setRange(-45,45);angle->setDecimals(3);angle->setValue(5);
    form->addRow("Face index (1-based)",face);form->addRow("Pull direction",axis);
    form->addRow("Neutral plane offset (mm)",offset);form->addRow("Draft angle (degrees)",angle);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;if(!document_.draftFace(ids.first(),face->value(),static_cast<cad::Axis>(axis->currentIndex()),offset->value(),angle->value(),&error)){
        reportError("Draft face",error);return;
    }
    refresh(true);
}

void MainWindow::moveObject() {
    if (!requireBodyMode("Move")) return;
    const auto ids = selectedIds();
    if (ids.size() != 1) { reportError("Move","Select exactly one body."); return; }
    const auto dims = valuesDialog(this,"Translate (relative offset)",{"Î”X (mm)","Î”Y (mm)","Î”Z (mm)"},{0,0,0},-1e6,1e6);
    if (!dims) return;
    QString error;
    if (!document_.translate(ids.first(),(*dims)[0],(*dims)[1],(*dims)[2],&error)) { reportError("Move",error); return; }
    refresh();
}
void MainWindow::rotateObject() {
    if (!requireBodyMode("Rotate")) return;
    const auto ids = selectedIds();
    if (ids.size() != 1) { reportError("Rotate","Select exactly one body."); return; }
    QDialog dialog(this); dialog.setWindowTitle("Rotate about world origin");
    auto* form = new QFormLayout(&dialog);
    auto* axes = new QComboBox(&dialog); axes->addItems({"X","Y","Z"}); form->addRow("Axis",axes);
    auto* angle = new QDoubleSpinBox(&dialog); angle->setRange(-36000,36000); angle->setDecimals(3); angle->setValue(90); angle->setSuffix(" degrees"); form->addRow("Angle",angle);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,&dialog); form->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    QString error;
    if (!document_.rotate(ids.first(),static_cast<cad::Axis>(axes->currentIndex()),angle->value(),&error)) { reportError("Rotate",error); return; }
    refresh();
}
void MainWindow::scaleObject() {
    if (!requireBodyMode("Scale")) return;
    const auto ids = selectedIds();
    if (ids.size() != 1) { reportError("Scale","Select exactly one body."); return; }
    const auto values = valuesDialog(this,"Uniform scale about world origin",{"Scale multiplier"},{1},0.0001,10000);
    if (!values) return;
    QString error;
    if (!document_.scale(ids.first(),values->first(),&error)) { reportError("Scale",error); return; }
    refresh();
}
void MainWindow::scaleNonUniformSelectedBody() {
    if(!requireBodyMode("Nonuniform scale"))return;
    const auto ids=selectedIds();
    if(ids.size()!=1){reportError("Nonuniform scale","Select exactly one body.");return;}
    const auto values=valuesDialog(this,"Nonuniform scale about world origin",
        {"X scale multiplier","Y scale multiplier","Z scale multiplier"},{1,1,1},0.0001,10000);
    if(!values)return;
    QString error;
    if(!document_.scaleNonUniform(ids.first(),(*values)[0],(*values)[1],(*values)[2],&error)){
        reportError("Nonuniform scale",error);return;
    }
    refresh();
}
void MainWindow::duplicateObject() {
    if (!requireBodyMode("Duplicate")) return;
    const auto ids = selectedIds();
    if (ids.size() != 1) { reportError("Duplicate","Select exactly one body."); return; }
    const auto values = valuesDialog(this,"Duplicate with offset",{"Î”X (mm)","Î”Y (mm)","Î”Z (mm)"},{50,0,0},-1e6,1e6);
    if (!values) return;
    QString error;
    if (!document_.duplicate(ids.first(),(*values)[0],(*values)[1],(*values)[2],&error)) { reportError("Duplicate",error); return; }
    refresh(true);
}

void MainWindow::makeSelectedComponent() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Make component","Select exactly one body.");return;}
    const cad::Part* part=document_.find(ids.first());if(!part)return;
    bool accepted=false;
    const QString name=QInputDialog::getText(this,"Make component","Reusable component name:",QLineEdit::Normal,part->name,&accepted);
    if(!accepted)return;
    QString error;
    if(!document_.makeComponent(ids.first(),name,&error)){reportError("Make component",error);return;}
    refresh();
}

void MainWindow::insertSelectedComponentOccurrence() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Insert component occurrence","Select one component occurrence to use as the source.");return;}
    const cad::Part* source=document_.find(ids.first());
    if(!source||source->componentDefinitionId.isEmpty()){reportError("Insert component occurrence","Make the source body a component first.");return;}
    bool accepted=false;
    const double x=QInputDialog::getDouble(this,"Fixed component placement","X translation offset (mm):",source->placementX+50,-1e9,1e9,3,&accepted);if(!accepted)return;
    const double y=QInputDialog::getDouble(this,"Fixed component placement","Y translation offset (mm):",source->placementY,-1e9,1e9,3,&accepted);if(!accepted)return;
    const double z=QInputDialog::getDouble(this,"Fixed component placement","Z translation offset (mm):",source->placementZ,-1e9,1e9,3,&accepted);if(!accepted)return;
    QString error;
    if(!document_.insertComponentOccurrence(ids.first(),x,y,z,&error)){reportError("Insert component occurrence",error);return;}
    refresh();
}
void MainWindow::synchronizeSelectedComponentDefinition() {
    const QStringList ids=selectedIds();if(ids.size()!=1){reportError("Synchronize component definition","Select one reusable component occurrence as the definition source.");return;}
    QString error;if(!document_.synchronizeComponentOccurrences(ids.first(),&error)){reportError("Synchronize component definition",error);return;}refresh();
}
void MainWindow::createRigidJoint() {
    const QStringList ids=selectedIds();if(ids.size()!=2){reportError("Create rigid joint","Select a parent occurrence first and a child occurrence second.");return;}
    QString error;if(!document_.addRigidJoint(ids[0],ids[1],&error)){reportError("Create rigid joint",error);return;}refresh();
}
void MainWindow::createRevoluteJoint() {
    const QStringList ids=selectedIds();if(ids.size()!=2){reportError("Create revolute joint","Select a parent occurrence first and a child occurrence second.");return;}
    const cad::Part* parent=document_.find(ids[0]);if(!parent){reportError("Create revolute joint","The parent occurrence no longer exists.");return;}
    const auto values=valuesDialog(this,"Revolute joint coordinate frame and limits",
        {"Origin X (mm)","Origin Y (mm)","Origin Z (mm)","Axis X","Axis Y","Axis Z","Minimum angle (degrees)","Maximum angle (degrees)"},
        {parent->placementX,parent->placementY,parent->placementZ,0,0,1,-180,180},-3600,3600);
    if(!values)return;QString error;
    if(!document_.addRevoluteJoint(ids[0],ids[1],(*values)[0],(*values)[1],(*values)[2],(*values)[3],(*values)[4],(*values)[5],(*values)[6],(*values)[7],&error)){
        reportError("Create revolute joint",error);return;
    }
    refresh();
}
void MainWindow::driveSelectedRevoluteJoint() {
    const QStringList ids=selectedIds();if(ids.size()!=1){reportError("Drive revolute joint","Select the child occurrence of one revolute joint.");return;}
    const cad::Part* child=document_.find(ids.first());if(!child||child->assemblyJointType!="Revolute"){
        reportError("Drive revolute joint","The selected occurrence has no revolute joint.");return;
    }
    bool accepted=false;const double angle=QInputDialog::getDouble(this,"Drive revolute joint","Angle (degrees):",child->jointAngleDegrees,
        child->jointMinimumDegrees,child->jointMaximumDegrees,3,&accepted);
    if(!accepted)return;QString error;
    if(!document_.driveRevoluteJoint(ids.first(),angle,&error)){reportError("Drive revolute joint",error);return;}refresh();
}
void MainWindow::createPrismaticJoint() {
    const QStringList ids=selectedIds();if(ids.size()!=2){reportError("Create prismatic joint","Select a parent occurrence first and a child occurrence second.");return;}
    const auto values=valuesDialog(this,"Prismatic joint axis and travel limits",
        {"Axis X","Axis Y","Axis Z","Minimum travel (mm)","Maximum travel (mm)"},{1,0,0,-50,50},-1e6,1e6);
    if(!values)return;QString error;
    if(!document_.addPrismaticJoint(ids[0],ids[1],(*values)[0],(*values)[1],(*values)[2],(*values)[3],(*values)[4],&error)){
        reportError("Create prismatic joint",error);return;
    }
    refresh();
}
void MainWindow::driveSelectedPrismaticJoint() {
    const QStringList ids=selectedIds();if(ids.size()!=1){reportError("Drive prismatic joint","Select the child occurrence of one prismatic joint.");return;}
    const cad::Part* child=document_.find(ids.first());if(!child||child->assemblyJointType!="Prismatic"){
        reportError("Drive prismatic joint","The selected occurrence has no prismatic joint.");return;
    }
    bool accepted=false;const double offset=QInputDialog::getDouble(this,"Drive prismatic joint","Offset (mm):",child->jointTranslationMm,
        child->jointMinimumMm,child->jointMaximumMm,3,&accepted);
    if(!accepted)return;QString error;
    if(!document_.drivePrismaticJoint(ids.first(),offset,&error)){reportError("Drive prismatic joint",error);return;}refresh();
}
void MainWindow::editSelectedJointMotionTrack() {
    const QStringList ids=selectedIds();if(ids.size()!=1){reportError("Edit joint motion track","Select one revolute or prismatic joint child.");return;}
    const cad::Part* child=document_.find(ids.first());if(!child||(child->assemblyJointType!="Revolute"&&child->assemblyJointType!="Prismatic")){
        reportError("Edit joint motion track","The selected occurrence has no motion-capable joint.");return;
    }
    QStringList existing;for(const cad::JointMotionKeyframe& key:child->jointMotionKeyframes)
        existing.append(QString::number(key.timeSeconds,'g',8)+":"+QString::number(key.value,'g',8));
    bool accepted=false;const QString text=QInputDialog::getText(this,"Edit joint motion track",
        "Enter increasing time:value keyframes (seconds:degrees or seconds:mm), separated by commas:",
        QLineEdit::Normal,existing.join(", "),&accepted);
    if(!accepted)return;
    QVector<cad::JointMotionKeyframe> keys;
    for(const QString& entry:text.split(',',Qt::SkipEmptyParts)){
        const QStringList pair=entry.trimmed().split(':');bool timeOkay=false,valueOkay=false;
        if(pair.size()!=2){reportError("Edit joint motion track","Use entries in time:value format, for example 0:0, 1:45.");return;}
        cad::JointMotionKeyframe key;key.timeSeconds=pair[0].trimmed().toDouble(&timeOkay);key.value=pair[1].trimmed().toDouble(&valueOkay);
        if(!timeOkay||!valueOkay){reportError("Edit joint motion track","Each keyframe time and value must be numeric.");return;}
        keys.append(key);
    }
    QString error;if(!document_.setJointMotionKeyframes(ids.first(),keys,&error)){reportError("Edit joint motion track",error);return;}refresh();
}
void MainWindow::evaluateAssemblyMotionStudy() {
    bool accepted=false;const double time=QInputDialog::getDouble(this,"Evaluate assembly motion study","Time (seconds):",0,0,3600,3,&accepted);
    if(!accepted)return;QString error;
    if(!document_.evaluateAssemblyMotionStudy(time,&error)){reportError("Evaluate assembly motion study",error);return;}refresh();
}
void MainWindow::renameObject() {
    if (!requireBodyMode("Rename")) return;
    const auto ids = selectedIds();
    if (ids.size() != 1) { reportError("Rename","Select exactly one body."); return; }
    bool okay = false;
    const QString name = QInputDialog::getText(this,"Rename body","Name:",QLineEdit::Normal,document_.find(ids.first())->name,&okay);
    if (!okay) return;
    QString error;
    if (!document_.rename(ids.first(),name,&error)) reportError("Rename",error);
    else refresh();
}
void MainWindow::recolorObject() {
    if (!requireBodyMode("Recolor")) return;
    const auto ids = selectedIds();
    if (ids.size() != 1) { reportError("Color","Select exactly one body."); return; }
    const QColor color = QColorDialog::getColor(QColor(document_.find(ids.first())->color),this,"Body color");
    if (!color.isValid()) return;
    QString error;
    if (!document_.setColor(ids.first(),color.name(),&error)) reportError("Color",error);
    else refresh();
}
void MainWindow::assignMaterial() {
    if(!requireBodyMode("Assign material"))return;
    const auto ids=selectedIds();
    if(ids.isEmpty()){reportError("Assign material","Select one or more bodies.");return;}
    const cad::Part* part=document_.find(ids.first());if(!part)return;
    QVector<QPair<QString,double>> library={
        {"Unassigned",0.0},{"Aluminum 6061",2700.0},{"Carbon steel",7850.0},
        {"Stainless steel 304",8000.0},{"Brass",8500.0},{"Copper",8960.0},
        {"ABS plastic",1040.0},{"Nylon 6",1150.0}
    };
    for(const cad::Part& body:document_.parts()){
        if(body.materialName.isEmpty())continue;
        const bool known=std::any_of(library.cbegin(),library.cend(),[&body](const auto& choice){return choice.first==body.materialName;});
        if(!known)library.append({body.materialName,body.materialDensityKgPerM3});
    }
    library.append({"Custom...",-1.0});
    QStringList labels;int current=0;
    for(int i=0;i<library.size();++i){labels.append(library[i].first);if(library[i].first==part->materialName)current=i;}
    bool accepted=false;
    const QString selected=QInputDialog::getItem(this,"Assign material","Material:",labels,current,false,&accepted);
    if(!accepted)return;
    QString materialName=selected;
    double density=0.0;
    if(selected=="Custom..."){
        materialName=QInputDialog::getText(this,"Custom material","Material name:",QLineEdit::Normal,{},&accepted).trimmed();
        if(!accepted||materialName.isEmpty())return;
        density=QInputDialog::getDouble(this,"Custom material density","Density (kg/m^3):",
            part->materialName==materialName?part->materialDensityKgPerM3:1000.0,0.00001,1000000.0,4,&accepted);
        if(!accepted)return;
    }else if(selected=="Unassigned"){
        materialName.clear();
    }else{
        const auto preset=std::find_if(library.cbegin(),library.cend(),[&selected](const auto& entry){return entry.first==selected;});
        if(preset==library.cend()||preset->second<0.0)return;
        density=preset->second;
    }
    QString error;
    if(!document_.setMaterial(ids,materialName,density,&error))reportError("Assign material",error);
    else refresh();
}
void MainWindow::editPartMetadata() {
    if (!requireBodyMode("Edit part metadata")) return;
    const auto ids=selectedIds();
    if(ids.size()!=1){reportError("Part metadata","Select exactly one body.");return;}
    const cad::Part* part=document_.find(ids.first());if(!part)return;
    QDialog dialog(this);dialog.setWindowTitle("Part and assembly information");
    auto* form=new QFormLayout(&dialog);
    auto* number=new QLineEdit(part->partNumber,&dialog);number->setMaxLength(32);number->setPlaceholderText("e.g. PLATE-001");
    auto* description=new QLineEdit(part->description,&dialog);description->setMaxLength(200);
    auto* revision=new QLineEdit(part->revision,&dialog);revision->setMaxLength(16);revision->setPlaceholderText("e.g. A");
    auto* assemblyPath=new QLineEdit(part->assemblyPath,&dialog);assemblyPath->setMaxLength(200);assemblyPath->setPlaceholderText("e.g. Pump/Drive");
    auto* procurementType=new QComboBox(&dialog);procurementType->addItems({"Manufactured","Purchased"});procurementType->setCurrentText(part->procurementType);
    auto* supplier=new QLineEdit(part->supplier,&dialog);supplier->setMaxLength(80);supplier->setPlaceholderText("Required for purchased items");
    auto* supplierPartNumber=new QLineEdit(part->supplierPartNumber,&dialog);supplierPartNumber->setMaxLength(64);supplierPartNumber->setPlaceholderText("Supplier catalogue number");
    form->addRow("Part number:",number);form->addRow("Description:",description);form->addRow("Revision:",revision);form->addRow("Assembly path:",assemblyPath);
    form->addRow("Procurement type:",procurementType);form->addRow("Supplier:",supplier);form->addRow("Supplier part number:",supplierPartNumber);
    auto* note=new QLabel("Part numbers use uppercase letters, numbers, dots, underscores, or hyphens in the standards check.",&dialog);
    note->setWordWrap(true);form->addRow(note);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.setPartMetadata(ids.first(),number->text(),description->text(),revision->text(),assemblyPath->text(),
                                  procurementType->currentText(),supplier->text(),supplierPartNumber->text(),&error))reportError("Part metadata",error);
    else refresh();
}
void MainWindow::showPartsReports() {
    QString error;
    const auto bom=document_.billOfMaterials(&error);
    if(bom.isEmpty()){reportError("BOM",error.isEmpty()?"The document has no bodies.":error);return;}
    const auto list=document_.partsList(&error);
    if(list.isEmpty()){reportError("Parts list",error);return;}
    QDialog dialog(this);dialog.setWindowTitle("Bill of materials and parts list");dialog.resize(1450,600);
    auto* layout=new QVBoxLayout(&dialog);auto* tabs=new QTabWidget(&dialog);layout->addWidget(tabs);
    auto makeTable=[&](const QStringList& headers,int rows){
        auto* table=new QTableWidget(rows,headers.size(),tabs);table->setHorizontalHeaderLabels(headers);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setAlternatingRowColors(true);table->horizontalHeader()->setStretchLastSection(true);
        return table;
    };
    auto* bomTable=makeTable({"Item","Part number","Description","Revision","Material","Qty","Unit volume mm³","Total volume mm³","Unit mass kg","Total mass kg","Assembly path","Procurement type","Supplier","Supplier part number","Component ID","Component"},bom.size());
    for(int r=0;r<bom.size();++r){const auto& x=bom[r];const QStringList values={QString::number(x.itemNumber),x.partNumber,x.description,x.revision,x.materialName,QString::number(x.quantity),QString::number(x.unitVolumeMm3,'f',3),QString::number(x.totalVolumeMm3,'f',3),x.unitMassKg>0?QString::number(x.unitMassKg,'f',6):QString(),x.totalMassKg>0?QString::number(x.totalMassKg,'f',6):QString(),x.assemblyPath,x.procurementType,x.supplier,x.supplierPartNumber,x.componentDefinitionId,x.componentName};for(int c=0;c<values.size();++c)bomTable->setItem(r,c,new QTableWidgetItem(values[c]));}
    tabs->addTab(bomTable,"BOM (grouped quantities)");
    auto* partsTable=makeTable({"Item","Body name","Part number","Description","Revision","Material","Qty","Volume mm³","Mass kg","Body ID","Assembly path","Procurement type","Supplier","Supplier part number","Component ID","Component","Occurrence ID","Offset X","Offset Y","Offset Z"},list.size());
    for(int r=0;r<list.size();++r){const auto& x=list[r];const QStringList values={QString::number(x.itemNumber),x.bodyName,x.partNumber,x.description,x.revision,x.materialName,"1",QString::number(x.volumeMm3,'f',3),x.massKg>0?QString::number(x.massKg,'f',6):QString(),x.bodyId,x.assemblyPath,x.procurementType,x.supplier,x.supplierPartNumber,x.componentDefinitionId,x.componentName,x.occurrenceId,QString::number(x.placementX,'f',3),QString::number(x.placementY,'f',3),QString::number(x.placementZ,'f',3)};for(int c=0;c<values.size();++c)partsTable->setItem(r,c,new QTableWidgetItem(values[c]));}
    tabs->addTab(partsTable,"Parts list (one row per body)");
    auto* assemblyTree=new QTreeWidget(tabs);
    assemblyTree->setColumnCount(7);
    assemblyTree->setHeaderLabels({"Assembly / item","Part number","Description","Qty","Total volume mm³","Total mass kg","Material"});
    assemblyTree->setAlternatingRowColors(true);
    assemblyTree->setSelectionBehavior(QAbstractItemView::SelectRows);
    assemblyTree->header()->setStretchLastSection(true);
    QHash<QString,QTreeWidgetItem*> assemblyNodes;
    for(const auto& row:bom){
        QTreeWidgetItem* parent=nullptr;
        QString prefix;
        const QStringList segments=row.assemblyPath.split('/',Qt::SkipEmptyParts);
        for(const QString& segment:segments){
            prefix=prefix.isEmpty()?segment:prefix+'/'+segment;
            auto it=assemblyNodes.find(prefix);
            if(it==assemblyNodes.end()){
                auto* node=parent?new QTreeWidgetItem(parent,{segment}):new QTreeWidgetItem(assemblyTree,{segment});
                node->setText(2,"Assembly roll-up");
                assemblyNodes.insert(prefix,node);
                it=assemblyNodes.find(prefix);
            }
            parent=it.value();
            parent->setData(3,Qt::UserRole,parent->data(3,Qt::UserRole).toInt()+row.quantity);
            parent->setData(4,Qt::UserRole,parent->data(4,Qt::UserRole).toDouble()+row.totalVolumeMm3);
            parent->setData(5,Qt::UserRole,parent->data(5,Qt::UserRole).toDouble()+row.totalMassKg);
        }
        const QString itemName=row.partNumber.isEmpty()?row.description:row.partNumber;
        QStringList values={itemName,row.partNumber,row.description,QString::number(row.quantity),
            QString::number(row.totalVolumeMm3,'f',3),row.totalMassKg>0?QString::number(row.totalMassKg,'f',6):QString(),row.materialName};
        if(parent)new QTreeWidgetItem(parent,values);
        else new QTreeWidgetItem(assemblyTree,values);
    }
    for(QTreeWidgetItem* node:assemblyNodes){
        node->setText(3,QString::number(node->data(3,Qt::UserRole).toInt()));
        node->setText(4,QString::number(node->data(4,Qt::UserRole).toDouble(),'f',3));
        const double mass=node->data(5,Qt::UserRole).toDouble();
        node->setText(5,mass>0.0?QString::number(mass,'f',6):QString());
    }
    assemblyTree->expandAll();
    tabs->addTab(assemblyTree,"Assembly structure (hierarchical roll-up)");
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    dialog.exec();
}
void MainWindow::runStandardsCheck() {
    const QStringList choices={"BOM data quality","ISO drawing readiness","ASME drawing readiness","All available checks"};
    bool accepted=false;const QString choice=QInputDialog::getItem(this,"Standards and data checks","Check profile:",choices,0,false,&accepted);
    if(!accepted)return;
    const QVector<QPair<QString,cad::StandardsProfile>> profiles={
        {choices[0],cad::StandardsProfile::BomData},
        {choices[1],cad::StandardsProfile::IsoDrawingReadiness},
        {choices[2],cad::StandardsProfile::AsmeDrawingReadiness}
    };
    const bool runAll=choice==choices[3];
    int errors=0,warnings=0;
    bool passed=true;
    QStringList details;
    for(const auto& [profileName,profile]:profiles){
        if(!runAll&&choice!=profileName)continue;
        const auto report=document_.checkStandards(profile);
        errors+=report.errors;warnings+=report.warnings;passed=passed&&report.passed();
        if(runAll)details.append(QStringLiteral("%1").arg(profileName));
        if(report.findings.isEmpty())details.append("No findings.");
        for(const auto& finding:report.findings){
            const QString level=finding.severity==cad::FindingSeverity::Error?"ERROR":
                (finding.severity==cad::FindingSeverity::Warning?"WARNING":"INFO");
            details.append(QStringLiteral("%1  %2  %3").arg(level,finding.code,finding.message));
        }
        if(runAll)details.append(QString());
    }
    const QString headline=QStringLiteral("%1\n\nErrors: %2    Warnings: %3\nResult: %4")
        .arg(choice).arg(errors).arg(warnings).arg(passed?"Passed current checks":"Failed");
    QMessageBox box(passed?QMessageBox::Information:QMessageBox::Warning,"Standards check",headline,QMessageBox::Ok,this);
    box.setDetailedText(details.join('\n'));box.exec();
}
void MainWindow::checkDrawingSheet() {
    const QString path=QFileDialog::getOpenFileName(this,"Check exported drawing SVG",QString(),"SVG drawings (*.svg)");
    if(path.isEmpty())return;
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)){reportError("Drawing readiness",QStringLiteral("Could not read the drawing file: %1").arg(file.errorString()));return;}
    const QByteArray svg=file.readAll();
    const QStringList choices={"ISO drawing readiness","ASME drawing readiness","All available checks"};
    bool accepted=false;const QString choice=QInputDialog::getItem(this,"Drawing readiness","Check profile:",choices,0,false,&accepted);
    if(!accepted)return;
    QDialog policyDialog(this);policyDialog.setWindowTitle("Configurable drawing checklist");auto* policyLayout=new QVBoxLayout(&policyDialog);
    auto* guidance=new QLabel("Choose the title-block fields and annotation conventions required for this project. Saved defaults are stored in the project and do not certify compliance with a formal standard.",&policyDialog);
    guidance->setWordWrap(true);policyLayout->addWidget(guidance);
    cad::DrawingReadinessPolicy policy=document_.drawingReadinessPolicy();QVector<QPair<QCheckBox*,QString>> requiredFields,requiredConventions;
    auto addPolicyCheck=[&](const QString& label,const QString& key,QVector<QPair<QCheckBox*,QString>>& list){
        const bool checked=&list==&requiredFields?policy.requiredTitleBlockFields.contains(key):policy.requiredAnnotationConventions.contains(key);
        auto* check=new QCheckBox(label,&policyDialog);check->setChecked(checked);
        policyLayout->addWidget(check);list.append({check,key});
    };
    policyLayout->addWidget(new QLabel("Required title-block fields",&policyDialog));
    addPolicyCheck("Drawing number","drawing_number",requiredFields);addPolicyCheck("Description","description",requiredFields);
    addPolicyCheck("Part number","part_number",requiredFields);addPolicyCheck("Revision","revision",requiredFields);
    addPolicyCheck("Drawn by","drawn_by",requiredFields);addPolicyCheck("Checked by","checked_by",requiredFields);
    addPolicyCheck("Issue date","issue_date",requiredFields);addPolicyCheck("Material","material",requiredFields);
    policyLayout->addWidget(new QLabel("Required annotation conventions",&policyDialog));
    addPolicyCheck("Overall X/Y/Z envelope dimensions","envelope_dimensions",requiredConventions);
    addPolicyCheck("Projection symbol","projection_symbol",requiredConventions);
    addPolicyCheck("Auxiliary view","auxiliary_view",requiredConventions);
    addPolicyCheck("General notes","general_notes",requiredConventions);
    auto* saveProjectPolicy=new QCheckBox("Save selected requirements as this project's defaults",&policyDialog);
    policyLayout->addWidget(saveProjectPolicy);
    const QStringList currentlySelected=selectedIds();
    auto* compareSource=new QCheckBox("Compare export source IDs and geometry revision with selected body/bodies",&policyDialog);
    compareSource->setEnabled(!currentlySelected.isEmpty()&&currentlySelected.size()<=50);policyLayout->addWidget(compareSource);
    auto* policyButtons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&policyDialog);policyLayout->addWidget(policyButtons);
    connect(policyButtons,&QDialogButtonBox::accepted,&policyDialog,&QDialog::accept);connect(policyButtons,&QDialogButtonBox::rejected,&policyDialog,&QDialog::reject);
    if(policyDialog.exec()!=QDialog::Accepted)return;
    cad::DrawingReadinessPolicy selectedPolicy;
    for(const auto& entry:requiredFields)if(entry.first->isChecked())selectedPolicy.requiredTitleBlockFields.append(entry.second);
    for(const auto& entry:requiredConventions)if(entry.first->isChecked())selectedPolicy.requiredAnnotationConventions.append(entry.second);
    if(saveProjectPolicy->isChecked()){
        QString policyError;if(!document_.setDrawingReadinessPolicy(selectedPolicy,&policyError)){reportError("Save drawing checklist",policyError);return;}
    }
    policy=selectedPolicy;
    const QVector<QPair<QString,cad::StandardsProfile>> profiles={
        {choices[0],cad::StandardsProfile::IsoDrawingReadiness},
        {choices[1],cad::StandardsProfile::AsmeDrawingReadiness}
    };
    const bool runAll=choice==choices[2];
    int errors=0,warnings=0;QStringList details;
    for(const auto& [profileName,profile]:profiles){
        if(!runAll&&choice!=profileName)continue;
        const auto report=cad::checkDrawingSvg(svg,profile,policy);
        errors+=report.errors;warnings+=report.warnings;
        if(runAll)details.append(profileName);
        for(const auto& finding:report.findings){
            const QString level=finding.severity==cad::FindingSeverity::Error?"ERROR":
                (finding.severity==cad::FindingSeverity::Warning?"WARNING":"INFO");
            details.append(QStringLiteral("%1  %2  %3").arg(level,finding.code,finding.message));
        }
        if(runAll)details.append(QString());
    }
    if(compareSource->isChecked()){
        QStringList sourceIdsToCompare=currentlySelected;
        if(currentlySelected.size()==1){const cad::Part* selected=document_.find(currentlySelected.first());
            if(selected&&selected->hasDrawingSettings&&!selected->drawingSettings.sourceBodyIds.isEmpty())sourceIdsToCompare=selected->drawingSettings.sourceBodyIds;}
        QVector<cad::Part> currentSources;for(const QString& id:sourceIdsToCompare){const cad::Part* source=document_.find(id);if(source)currentSources.append(*source);}
        const auto sourceReport=cad::checkDrawingSourceSnapshot(currentSources,svg);errors+=sourceReport.errors;warnings+=sourceReport.warnings;
        details.append("MODEL SOURCE SNAPSHOT");
        for(const auto& finding:sourceReport.findings){
            const QString level=finding.severity==cad::FindingSeverity::Error?"ERROR":
                (finding.severity==cad::FindingSeverity::Warning?"WARNING":"INFO");
            details.append(QStringLiteral("%1  %2  %3").arg(level,finding.code,finding.message));
        }
    }
    const QString headline=QStringLiteral("%1\n%2\n\nErrors: %3    Warnings: %4\nResult: %5")
        .arg(choice,QFileInfo(path).fileName()).arg(errors).arg(warnings).arg(errors==0?"No structural errors":"Drawing structure needs attention");
    QMessageBox box(errors==0?QMessageBox::Information:QMessageBox::Warning,"Drawing readiness",headline,QMessageBox::Ok,this);
    box.setDetailedText(details.join('\n'));box.exec();
}
void MainWindow::runBoolean(cad::BooleanOp op) {
    if (!requireBodyMode("Boolean")) return;
    const auto ids = selectedIds();
    if (ids.size() != 2) { reportError("Boolean","Select two bodies in the browser or viewport (Ctrl+click)."); return; }
    QString a = ids[0],b = ids[1];
    if (op == cad::BooleanOp::Cut) {
        const QStringList options = {document_.find(a)->name + " (" + a.left(8) + ")",document_.find(b)->name + " (" + b.left(8) + ")"};
        bool accepted = false;
        const QString target = QInputDialog::getItem(this,"Subtract","Target A (subtract the other body):",options,0,false,&accepted);
        if (!accepted) return;
        if (target == options[1]) std::swap(a,b);
    }
    QString error;
    if (!document_.booleanOperation(op,a,b,&error)) { reportError("Boolean",error); return; }
    refresh(true);
}

void MainWindow::filletSelectedEdge() {
    const auto edges = viewport_->selectedEdgeReferences();
    if (edges.isEmpty()) {
        QMessageBox::information(this,"Fillet edges","Switch to Select â†’ Edges and select one or more edges in the viewport (Ctrl+click to add edges).");
        return;
    }
    const auto radius = valuesDialog(this,"Edge fillet",{"Radius (mm)"},{1.0},0.00001,1e6);
    if (!radius) return;
    cad::FilletCommand command;
    QString error;
    if (!document_.prepareFillet(edges,radius->first(),&command,&error)) {
        reportError("Edge fillet",error);
        return;
    }

    runPreviewAsync<cad::FilletPreview>(this, statusBar(), "Edge fillet",
        QStringLiteral("Calculating fillet preview for %1 edge(s)...").arg(edges.size()), [command] {
            return cad::SceneDocument::buildFilletPreview(command);
        }, [this, command](const cad::FilletPreview& preview) {
            confirmPreview("Preview edge fillet",
                QStringLiteral("Inspect the translucent gold preview. Apply this fillet to the %1 selected edge(s)?").arg(command.edges.size()), preview.shape,
                [this, command, preview] {
                    QString commitError;
                    if (!document_.commitFillet(command,preview.shape,&commitError))
                        reportError("Edge fillet",commitError);
                    else refresh();
                });
        });
}

void MainWindow::chamferSelectedEdge() {
    const auto edges = viewport_->selectedEdgeReferences();
    if (edges.isEmpty()) {
        QMessageBox::information(this,"Chamfer edges","Switch to Select â†’ Edges and select one or more edges in the viewport (Ctrl+click to add edges).");
        return;
    }
    const auto distance = valuesDialog(this,"Edge chamfer",{"Distance (mm)"},{1.0},0.00001,1e6);
    if (!distance) return;
    cad::ChamferCommand command;
    QString error;
    if (!document_.prepareChamfer(edges,distance->first(),&command,&error)) {
        reportError("Edge chamfer",error);
        return;
    }

    runPreviewAsync<cad::ChamferPreview>(this, statusBar(), "Edge chamfer",
        QStringLiteral("Calculating chamfer preview for %1 edge(s)...").arg(edges.size()), [command] {
            return cad::SceneDocument::buildChamferPreview(command);
        }, [this, command](const cad::ChamferPreview& preview) {
            confirmPreview("Preview edge chamfer",
                QStringLiteral("Inspect the translucent gold preview. Apply this chamfer to the %1 selected edge(s)?").arg(command.edges.size()), preview.shape,
                [this, command, preview] {
                    QString commitError;
                    if (!document_.commitChamfer(command,preview.shape,&commitError))
                        reportError("Edge chamfer",commitError);
                    else refresh();
                });
        });
}

void MainWindow::asymmetricChamferSelectedEdge() {
    const auto edges=viewport_->selectedEdgeReferences();
    if(edges.size()!=1){
        QMessageBox::information(this,"Asymmetric chamfer","Switch to Select → Edges and select exactly one edge in the viewport.");
        return;
    }
    const auto distances=valuesDialog(this,"Asymmetric edge chamfer",
        {"Distance on reference face (mm)","Distance on opposite face (mm)"},{1.0,2.0},0.00001,1e6);
    if(!distances)return;
    cad::ChamferCommand command;QString error;
    if(!document_.prepareAsymmetricChamfer(edges.first(),(*distances)[0],(*distances)[1],&command,&error)){
        reportError("Asymmetric chamfer",error);return;
    }
    runPreviewAsync<cad::ChamferPreview>(this,statusBar(),"Asymmetric chamfer",
        "Calculating asymmetric chamfer preview...",[command]{return cad::SceneDocument::buildChamferPreview(command);},
        [this,command](const cad::ChamferPreview& preview){
            confirmPreview("Preview asymmetric chamfer",
                "Inspect the preview. Apply the two-distance chamfer to the selected edge?",preview.shape,
                [this,command,preview]{QString commitError;
                    if(!document_.commitChamfer(command,preview.shape,&commitError))reportError("Asymmetric chamfer",commitError);
                    else refresh();
                });
        });
}

void MainWindow::filletAllBodyEdges() {
    if(!requireBodyMode("Fillet all edges"))return;
    const auto ids=selectedIds();if(ids.size()!=1){reportError("Fillet all edges","Select exactly one body.");return;}
    const auto values=valuesDialog(this,"Fillet all body edges",{"Radius (mm)"},{1.0},0.00001,1e6);if(!values)return;
    QString error;if(!document_.filletAllEdges(ids.first(),values->first(),&error)){reportError("Fillet all edges",error);return;}
    refresh();
}

void MainWindow::chamferAllBodyEdges() {
    if(!requireBodyMode("Chamfer all edges"))return;
    const auto ids=selectedIds();if(ids.size()!=1){reportError("Chamfer all edges","Select exactly one body.");return;}
    const auto values=valuesDialog(this,"Chamfer all body edges",{"Distance (mm)"},{1.0},0.00001,1e6);if(!values)return;
    QString error;if(!document_.chamferAllEdges(ids.first(),values->first(),&error)){reportError("Chamfer all edges",error);return;}
    refresh();
}

void MainWindow::removeSelected() {
    if (!requireBodyMode("Removeselected")) return;
    const auto ids = selectedIds();
    if (ids.isEmpty()) return;
    QString error;
    if (!document_.eraseMany(ids,&error)) reportError("Delete",error);
    else refresh();
}
void MainWindow::newFile() { if (maybeSave()) { document_.newDocument(); refresh(); } }
void MainWindow::openFile() {
    if (!maybeSave()) return;
    const QString path = QFileDialog::getOpenFileName(this,"Open CAD scene",{},"CADWorkbench (*.cadscene)");
    if (path.isEmpty()) return;
    QString error;
    if (!document_.load(path,&error)) reportError("Open",error);
    else refresh(true);
}
void MainWindow::importSTEPFile() {
    const QString path = QFileDialog::getOpenFileName(this,"Import STEP geometry",{},"STEP files (*.step *.stp)");
    if (path.isEmpty()) return;
    QString error;
    if (!document_.importSTEP(path,&error)) { reportError("Import STEP",error); return; }
    refresh(true);
    statusBar()->showMessage(QString("Imported STEP: %1").arg(QFileInfo(path).fileName()),8000);
}
void MainWindow::importIGESFile() {
    const QString path=QFileDialog::getOpenFileName(this,"Import IGES solid geometry",{},"IGES files (*.iges *.igs)");
    if(path.isEmpty())return;
    QString error;if(!document_.importIGES(path,&error)){reportError("Import IGES",error);return;}
    refresh(true);statusBar()->showMessage(QString("Imported IGES solids: %1").arg(QFileInfo(path).fileName()),8000);
}
void MainWindow::importOBJMeshFile() {
    const QString path=QFileDialog::getOpenFileName(this,"Import closed OBJ mesh",{},"OBJ meshes (*.obj)");
    if(path.isEmpty())return;
    QString error;if(!document_.importOBJ(path,0.01,&error)){reportError("Import OBJ mesh",error);return;}
    refresh(true);statusBar()->showMessage(QString("Imported faceted OBJ solid: %1").arg(QFileInfo(path).fileName()),8000);
}
void MainWindow::importSTLMeshFile() {
    const QString path=QFileDialog::getOpenFileName(this,"Import closed STL mesh",{},"STL meshes (*.stl)");
    if(path.isEmpty())return;
    QString error;if(!document_.importSTL(path,0.01,&error)){reportError("Import STL mesh",error);return;}
    refresh(true);statusBar()->showMessage(QString("Imported faceted STL solid: %1").arg(QFileInfo(path).fileName()),8000);
}

void MainWindow::exportBodyScheduleCsv() {
    const QString path=QFileDialog::getSaveFileName(this,"Export body schedule",{},"CSV files (*.csv)");
    if(path.isEmpty())return;
    QString error;
    if(!document_.exportBodyScheduleCsv(path,&error)){reportError("Export body schedule",error);return;}
    statusBar()->showMessage(QString("Body schedule exported: %1").arg(QFileInfo(path).fileName()),8000);
}
void MainWindow::exportBillOfMaterialsCsv() {
    const QString path=QFileDialog::getSaveFileName(this,"Export bill of materials",{},"CSV files (*.csv)");
    if(path.isEmpty())return;
    QString error;
    if(!document_.exportBillOfMaterialsCsv(path,&error)){reportError("Export bill of materials",error);return;}
    statusBar()->showMessage(QString("Bill of materials exported: %1").arg(QFileInfo(path).fileName()),8000);
}
void MainWindow::exportPartsListCsv() {
    const QString path=QFileDialog::getSaveFileName(this,"Export parts list",{},"CSV files (*.csv)");
    if(path.isEmpty())return;
    QString error;
    if(!document_.exportPartsListCsv(path,&error)){reportError("Export parts list",error);return;}
    statusBar()->showMessage(QString("Parts list exported: %1").arg(QFileInfo(path).fileName()),8000);
}
void MainWindow::editDrawingViewLayout() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Arrange drawing views","Select exactly one body to place on a drawing sheet.");return;}
    const cad::Part* part=document_.find(ids.first());
    if(!part){reportError("Arrange drawing views","The selected body could not be found.");return;}
    cad::DrawingSheetSettings settings=part->hasDrawingSettings?part->drawingSettings:cad::DrawingSheetSettings{};
    const bool thirdAngle=settings.projection!="First-angle";
    const bool a3=settings.sheetSize=="A3";
    const double paperScale=a3?420.0/297.0:1.0;
    const double maxX=a3?420.0:297.0,maxY=a3?297.0:210.0;
    const QMap<QString,QPointF> defaults={
        {"TOP",QPointF(thirdAngle?79.0:155.0,thirdAngle?47.0:143.0)},
        {"FRONT",QPointF(thirdAngle?79.0:155.0,thirdAngle?125.0:70.0)},
        {"RIGHT",QPointF(thirdAngle?204.0:45.0,thirdAngle?125.0:70.0)},
        {"AUXILIARY",QPointF(240.0,thirdAngle?47.0:145.0)}};
    QDialog dialog(this);dialog.setWindowTitle("Arrange persistent drawing views");dialog.setMinimumWidth(470);
    auto* form=new QFormLayout(&dialog);
    auto* explanation=new QLabel(QStringLiteral("Set each view center in paper millimeters (%1 × %2). All views share the automatically fitted drawing scale.").arg(maxX,0,'f',0).arg(maxY,0,'f',0),&dialog);
    explanation->setWordWrap(true);form->addRow(explanation);
    if(part->hasDrawingSettings&&!settings.sourceGeometryToken.isEmpty()&&settings.sourceGeometryToken!=part->geometryToken){
        auto* stale=new QLabel("The model changed since this sheet was last saved. Saving this layout will re-link the sheet to the selected body's current geometry.",&dialog);
        stale->setWordWrap(true);stale->setStyleSheet("color:#a65b00");form->addRow(stale);
    }
    QMap<QString,QDoubleSpinBox*> xFields,yFields;
    const QList<QPair<QString,QString>> views={{"TOP","Top"},{"FRONT","Front"},{"RIGHT","Right"},{"AUXILIARY",settings.auxiliaryView=="Section A-A"?"Section A-A":"Isometric"}};
    for(const auto& view:views){
        const QString& key=view.first;const QString& label=view.second;
        const QPointF initial=settings.viewCenters.contains(key)?settings.viewCenters.value(key):defaults.value(key)*paperScale;
        auto* row=new QWidget(&dialog);auto* rowLayout=new QHBoxLayout(row);rowLayout->setContentsMargins(0,0,0,0);
        auto* x=new QDoubleSpinBox(row);x->setRange(10,maxX-10);x->setDecimals(2);x->setSuffix(" mm");x->setValue(initial.x());
        auto* y=new QDoubleSpinBox(row);y->setRange(10,maxY-10);y->setDecimals(2);y->setSuffix(" mm");y->setValue(initial.y());
        rowLayout->addWidget(new QLabel("X",row));rowLayout->addWidget(x);rowLayout->addWidget(new QLabel("Y",row));rowLayout->addWidget(y);
        form->addRow(label+" view center:",row);xFields.insert(key,x);yFields.insert(key,y);
    }
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);form->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    for(const auto& view:views){const QString& key=view.first;settings.viewCenters.insert(key,QPointF(xFields.value(key)->value(),yFields.value(key)->value()));}
    settings.sourceGeometryToken=part->geometryToken;
    QString error;
    if(!document_.setDrawingSheetSettings(ids.first(),settings,&error)){reportError("Save drawing layout",error);return;}
    refresh();statusBar()->showMessage("Drawing sheet and four projected-view placements saved and linked to the current model revision.",8000);
}
void MainWindow::editDrawingNotes() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Edit drawing notes","Select exactly one body to edit its drawing-sheet notes.");return;}
    const cad::Part* part=document_.find(ids.first());
    if(!part){reportError("Edit drawing notes","The selected body could not be found.");return;}
    cad::DrawingSheetSettings settings=part->hasDrawingSettings?part->drawingSettings:cad::DrawingSheetSettings{};
    const bool a3=settings.sheetSize=="A3",thirdAngle=settings.projection!="First-angle";
    const double paperScale=a3?420.0/297.0:1.0,maxX=a3?420.0:297.0,maxY=a3?297.0:210.0;
    const double defaultX=(thirdAngle?12.0:151.0)*paperScale;
    QDialog dialog(this);dialog.setWindowTitle("Edit persistent drawing notes");dialog.setMinimumWidth(500);
    auto* form=new QFormLayout(&dialog);
    auto* info=new QLabel("Up to five single-line notes can be stored with the sheet. Set their paper position in millimeters.",&dialog);
    info->setWordWrap(true);form->addRow(info);
    QVector<QLineEdit*> textFields;QVector<QDoubleSpinBox*> xFields,yFields;
    for(int i=0;i<5;++i){
        QString text;double initialX=defaultX,initialY=(180.0+4.0*i)*paperScale;
        if(i<settings.noteAnnotations.size()){
            const auto& note=settings.noteAnnotations[i];text=note.text;initialX=note.xMm;initialY=note.yMm;
        }else if(i<settings.generalNotes.size())text=settings.generalNotes[i];
        auto* row=new QWidget(&dialog);auto* layout=new QHBoxLayout(row);layout->setContentsMargins(0,0,0,0);
        auto* noteText=new QLineEdit(text,row);noteText->setMaxLength(60);noteText->setPlaceholderText("Leave blank to remove this note");
        auto* x=new QDoubleSpinBox(row);x->setRange(10,maxX-10);x->setDecimals(2);x->setSuffix(" mm");x->setValue(initialX);
        auto* yPosition=new QDoubleSpinBox(row);yPosition->setRange(10,maxY-10);yPosition->setDecimals(2);yPosition->setSuffix(" mm");yPosition->setValue(initialY);
        layout->addWidget(noteText,3);layout->addWidget(new QLabel("X",row));layout->addWidget(x);layout->addWidget(new QLabel("Y",row));layout->addWidget(yPosition);
        form->addRow(QString("Note %1:").arg(i+1),row);textFields.append(noteText);xFields.append(x);yFields.append(yPosition);
    }
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);form->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QVector<cad::DrawingNoteAnnotation> annotations;QStringList noteTexts;
    for(int i=0;i<textFields.size();++i){
        const QString text=textFields[i]->text().trimmed();if(text.isEmpty())continue;
        QString noteId;if(i<settings.noteAnnotations.size())noteId=settings.noteAnnotations[i].id;
        annotations.append({noteId,text,xFields[i]->value(),yFields[i]->value()});noteTexts.append(text);
    }
    settings.generalNotes=noteTexts;settings.noteAnnotations=annotations;settings.sourceGeometryToken=part->geometryToken;
    QString error;
    if(!document_.setDrawingSheetSettings(ids.first(),settings,&error)){reportError("Save drawing notes",error);return;}
    refresh();statusBar()->showMessage("Drawing notes and positions saved with stable IDs and linked to the current model revision.",8000);
}
void MainWindow::editDrawingDimensions() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Edit drawing dimensions","Select exactly one body to edit its drawing dimensions.");return;}
    const cad::Part* part=document_.find(ids.first());
    if(!part){reportError("Edit drawing dimensions","The selected body could not be found.");return;}
    cad::DrawingSheetSettings settings=part->hasDrawingSettings?part->drawingSettings:cad::DrawingSheetSettings{};
    QDialog dialog(this);dialog.setWindowTitle("Edit persistent overall dimensions");dialog.setMinimumWidth(420);
    auto* form=new QFormLayout(&dialog);
    auto* help=new QLabel("Place each overall X/Y/Z dimension line 5–60 mm from its projected view. Geometry values are computed from the linked body.",&dialog);
    help->setWordWrap(true);form->addRow(help);
    QMap<QString,QDoubleSpinBox*> offsets;
    for(const QString& axis:{QStringLiteral("X"),QStringLiteral("Y"),QStringLiteral("Z")}){
        double value=9.0;
        for(const auto& dimension:settings.dimensionAnnotations)if(dimension.axis==axis){value=dimension.offsetMm;break;}
        auto* field=new QDoubleSpinBox(&dialog);field->setRange(5,60);field->setDecimals(2);field->setSuffix(" mm");field->setValue(value);
        form->addRow(axis+" dimension offset:",field);offsets.insert(axis,field);
    }
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);form->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QVector<cad::DrawingDimensionAnnotation> annotations;
    for(const QString& axis:{QStringLiteral("X"),QStringLiteral("Y"),QStringLiteral("Z")}){
        QString id;for(const auto& previous:settings.dimensionAnnotations)if(previous.axis==axis){id=previous.id;break;}
        annotations.append({id,axis,offsets.value(axis)->value()});
    }
    settings.dimensionAnnotations=annotations;settings.sourceGeometryToken=part->geometryToken;
    QString error;
    if(!document_.setDrawingSheetSettings(ids.first(),settings,&error)){reportError("Save drawing dimensions",error);return;}
    refresh();statusBar()->showMessage("Overall drawing dimensions saved with stable IDs and linked to the current model revision.",8000);
}
void MainWindow::editDrawingPartsList() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Place drawing parts list","Select exactly one body to configure its drawing sheet.");return;}
    const cad::Part* part=document_.find(ids.first());
    if(!part){reportError("Place drawing parts list","The selected body could not be found.");return;}
    cad::DrawingSheetSettings settings=part->hasDrawingSettings?part->drawingSettings:cad::DrawingSheetSettings{};
    const bool a3=settings.sheetSize=="A3";const double maxX=a3?420.0:297.0,maxY=a3?297.0:210.0;
    QDialog dialog(this);dialog.setWindowTitle("Link a BOM parts list to this drawing sheet");dialog.setMinimumWidth(440);
    auto* form=new QFormLayout(&dialog);
    auto* help=new QLabel("The exported table is refreshed from the document BOM each time. It includes item, part number, description, quantity, material, and revision. Up to 18 BOM rows fit this table.",&dialog);
    help->setWordWrap(true);form->addRow(help);
    auto* enabled=new QCheckBox("Include linked parts list on drawing export",&dialog);enabled->setChecked(settings.partsListAnnotation.visible);form->addRow(enabled);
    auto* x=new QDoubleSpinBox(&dialog);x->setRange(10,maxX-155);x->setDecimals(2);x->setSuffix(" mm");x->setValue(settings.partsListAnnotation.xMm);
    auto* y=new QDoubleSpinBox(&dialog);y->setRange(10,maxY-108);y->setDecimals(2);y->setSuffix(" mm");y->setValue(settings.partsListAnnotation.yMm);
    form->addRow("Top-left X:",x);form->addRow("Top-left Y:",y);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);form->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    settings.partsListAnnotation.visible=enabled->isChecked();settings.partsListAnnotation.xMm=x->value();settings.partsListAnnotation.yMm=y->value();
    settings.sourceGeometryToken=part->geometryToken;QString error;
    if(!document_.setDrawingSheetSettings(ids.first(),settings,&error)){reportError("Save drawing parts list",error);return;}
    refresh();statusBar()->showMessage(settings.partsListAnnotation.visible
        ? "Linked parts list placement saved; its rows will use the current document BOM when exported."
        : "Linked parts list hidden for this sheet; its identity and placement are retained.",8000);
}
void MainWindow::editDrawingBalloons() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Edit drawing BOM balloons","Select one drawing-sheet master body.");return;}
    const cad::Part* part=document_.find(ids.first());
    if(!part){reportError("Edit drawing BOM balloons","The selected body could not be found.");return;}
    cad::DrawingSheetSettings settings=part->hasDrawingSettings?part->drawingSettings:cad::DrawingSheetSettings{};
    if(settings.sourceBodyIds.isEmpty())settings.sourceBodyIds.append(part->id);
    QDialog dialog(this);dialog.setWindowTitle("Edit saved BOM balloons");dialog.setMinimumSize(570,340);
    auto* layout=new QVBoxLayout(&dialog);
    auto* help=new QLabel("Each balloon is linked to one source body and displays its item number from this sheet's source-scoped BOM. Place balloons in paper-space millimeters.",&dialog);
    help->setWordWrap(true);layout->addWidget(help);
    auto* table=new QTableWidget(&dialog);table->setColumnCount(3);table->setHorizontalHeaderLabels({"Source body","X (mm)","Y (mm)"});
    table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
    auto addRow=[&](const cad::DrawingBalloonAnnotation& balloon){
        const int row=table->rowCount();table->insertRow(row);
        const cad::Part* source=document_.find(balloon.sourceBodyId);auto* bodyItem=new QTableWidgetItem(source?QStringLiteral("%1 (%2)").arg(source->name,source->id.left(8)):balloon.sourceBodyId);
        bodyItem->setFlags(bodyItem->flags()&~Qt::ItemIsEditable);bodyItem->setData(Qt::UserRole,balloon.sourceBodyId);bodyItem->setData(Qt::UserRole+1,balloon.id);table->setItem(row,0,bodyItem);
        table->setItem(row,1,new QTableWidgetItem(QString::number(balloon.xMm,'f',2)));
        table->setItem(row,2,new QTableWidgetItem(QString::number(balloon.yMm,'f',2)));
    };
    for(const auto& balloon:settings.balloonAnnotations)addRow(balloon);
    layout->addWidget(table);
    auto* controls=new QHBoxLayout;auto* addButton=new QPushButton("Add balloon...",&dialog);auto* removeButton=new QPushButton("Remove selected",&dialog);
    controls->addWidget(addButton);controls->addWidget(removeButton);controls->addStretch(1);layout->addLayout(controls);
    connect(addButton,&QPushButton::clicked,&dialog,[&,table]{
        if(table->rowCount()>=50){QMessageBox::warning(&dialog,"BOM balloons","A drawing sheet can contain at most 50 balloons.");return;}
        QStringList labels;for(const QString& sourceId:settings.sourceBodyIds){const cad::Part* source=document_.find(sourceId);if(source)labels.append(QStringLiteral("%1 (%2)").arg(source->name,source->id.left(8)));}
        if(labels.isEmpty()){QMessageBox::warning(&dialog,"BOM balloons","The saved source bodies are missing from this scene.");return;}
        bool accepted=false;const QString selected=QInputDialog::getItem(&dialog,"Add BOM balloon","Source body:",labels,0,false,&accepted);if(!accepted)return;
        const int index=labels.indexOf(selected);int sourceIndex=0;QString sourceId;
        for(const QString& candidate:settings.sourceBodyIds){const cad::Part* source=document_.find(candidate);if(!source)continue;if(sourceIndex++==index){sourceId=candidate;break;}}
        if(sourceId.isEmpty())return;
        const bool duplicate=std::any_of(settings.balloonAnnotations.cbegin(),settings.balloonAnnotations.cend(),[&](const auto& b){return b.sourceBodyId==sourceId;});
        for(int row=0;row<table->rowCount()&&!duplicate;++row)if(table->item(row,0)->data(Qt::UserRole).toString()==sourceId){QMessageBox::information(&dialog,"BOM balloons","This source body already has a balloon on this sheet.");return;}
        if(duplicate){QMessageBox::information(&dialog,"BOM balloons","This source body already has a balloon on this sheet.");return;}
        addRow({QUuid::createUuid().toString(QUuid::WithoutBraces),sourceId,250.0,170.0});
    });
    connect(removeButton,&QPushButton::clicked,&dialog,[table]{const int row=table->currentRow();if(row>=0)table->removeRow(row);});
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QVector<cad::DrawingBalloonAnnotation> balloons;
    for(int row=0;row<table->rowCount();++row){
        QTableWidgetItem* body=table->item(row,0);bool xOk=false,yOk=false;
        const double x=table->item(row,1)->text().toDouble(&xOk),y=table->item(row,2)->text().toDouble(&yOk);
        if(!body||!xOk||!yOk){reportError("Save drawing BOM balloons","Enter valid numeric paper-space X and Y coordinates for every balloon.");return;}
        balloons.append({body->data(Qt::UserRole+1).toString(),body->data(Qt::UserRole).toString(),x,y});
    }
    settings.balloonAnnotations=balloons;settings.sourceBodyIds.removeDuplicates();std::sort(settings.sourceBodyIds.begin(),settings.sourceBodyIds.end());
    QVector<cad::Part> sources;for(const QString& sourceId:settings.sourceBodyIds){const cad::Part* source=document_.find(sourceId);if(source)sources.append(*source);}
    cad::Part composite;QString error;if(!cad::buildDrawingAssemblyPart(sources,&composite,&error)){reportError("Save drawing BOM balloons",error);return;}
    settings.sourceGeometryToken=composite.geometryToken;
    if(!document_.setDrawingSheetSettings(part->id,settings,&error)){reportError("Save drawing BOM balloons",error);return;}
    refresh();statusBar()->showMessage(QString("Saved %1 BOM balloon(s) linked to the drawing source bodies.").arg(balloons.size()),8000);
}
void MainWindow::editDrawingDetails() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Edit drawing detail views","Select one drawing-sheet master body.");return;}
    const cad::Part* part=document_.find(ids.first());
    if(!part){reportError("Edit drawing detail views","The selected body could not be found.");return;}
    cad::DrawingSheetSettings settings=part->hasDrawingSettings?part->drawingSettings:cad::DrawingSheetSettings{};
    QDialog dialog(this);dialog.setWindowTitle("Edit saved drawing detail views");dialog.setMinimumSize(900,370);
    auto* layout=new QVBoxLayout(&dialog);
    auto* help=new QLabel("Add up to four enlarged circular details from TOP, FRONT, or RIGHT. Center coordinates and capture radius use the chosen view's projected millimeters; sheet placement and detail radius use paper millimeters.",&dialog);
    help->setWordWrap(true);layout->addWidget(help);
    auto* table=new QTableWidget(&dialog);table->setColumnCount(7);
    table->setHorizontalHeaderLabels({"Source view","Center U (mm)","Center V (mm)","Capture radius (mm)","Sheet X (mm)","Sheet Y (mm)","Detail radius (mm)"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);layout->addWidget(table);
    auto addRow=[&](const cad::DrawingDetailAnnotation& detail){
        const int row=table->rowCount();table->insertRow(row);
        auto* source=new QTableWidgetItem(detail.sourceView);source->setData(Qt::UserRole,detail.id);table->setItem(row,0,source);
        table->setItem(row,1,new QTableWidgetItem(QString::number(detail.sourceXmm,'f',3)));
        table->setItem(row,2,new QTableWidgetItem(QString::number(detail.sourceYmm,'f',3)));
        table->setItem(row,3,new QTableWidgetItem(QString::number(detail.captureRadiusMm,'f',3)));
        table->setItem(row,4,new QTableWidgetItem(QString::number(detail.xMm,'f',3)));
        table->setItem(row,5,new QTableWidgetItem(QString::number(detail.yMm,'f',3)));
        table->setItem(row,6,new QTableWidgetItem(QString::number(detail.radiusMm,'f',3)));
    };
    for(const cad::DrawingDetailAnnotation& detail:settings.detailAnnotations)addRow(detail);
    auto* buttons=new QHBoxLayout;auto* add=new QPushButton("Add detail",&dialog);auto* remove=new QPushButton("Remove selected",&dialog);
    buttons->addWidget(add);buttons->addWidget(remove);buttons->addStretch();layout->addLayout(buttons);
    auto* dialogButtons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);layout->addWidget(dialogButtons);
    connect(add,&QPushButton::clicked,&dialog,[&]{
        if(table->rowCount()>=4){QMessageBox::information(&dialog,"Detail views","A sheet can contain up to four detail views.");return;}
        cad::DrawingDetailAnnotation detail;detail.id=QUuid::createUuid().toString(QUuid::WithoutBraces);addRow(detail);
    });
    connect(remove,&QPushButton::clicked,&dialog,[&]{const int row=table->currentRow();if(row>=0)table->removeRow(row);});
    connect(dialogButtons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(dialogButtons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QVector<cad::DrawingDetailAnnotation> details;
    for(int row=0;row<table->rowCount();++row){
        cad::DrawingDetailAnnotation detail;const auto* source=table->item(row,0);
        if(!source){reportError("Save drawing detail views","Choose TOP, FRONT, or RIGHT for each detail.");return;}
        detail.id=source->data(Qt::UserRole).toString();detail.sourceView=source->text().trimmed().toUpper();
        bool ok[6]{};double values[6]{};
        for(int column=1;column<7;++column){const auto* cell=table->item(row,column);if(!cell){ok[column-1]=false;continue;}values[column-1]=cell->text().toDouble(&ok[column-1]);}
        if(std::any_of(std::begin(ok),std::end(ok),[](bool valid){return !valid;})){
            reportError("Save drawing detail views","Enter numeric center, capture radius, sheet placement, and detail radius values for every row.");return;
        }
        detail.sourceXmm=values[0];detail.sourceYmm=values[1];detail.captureRadiusMm=values[2];
        detail.xMm=values[3];detail.yMm=values[4];detail.radiusMm=values[5];details.append(detail);
    }
    settings.detailAnnotations=details;
    if(settings.sourceBodyIds.isEmpty())settings.sourceBodyIds.append(part->id);
    QString error;if(!document_.setDrawingSheetSettings(part->id,settings,&error)){reportError("Save drawing detail views",error);return;}
    refresh();statusBar()->showMessage(QString("Saved %1 linked drawing detail view(s).").arg(details.size()),8000);
}
void MainWindow::editDrawingSections() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Edit drawing section views","Select one drawing-sheet master body.");return;}
    const cad::Part* part=document_.find(ids.first());
    if(!part){reportError("Edit drawing section views","The selected body could not be found.");return;}
    cad::DrawingSheetSettings settings=part->hasDrawingSettings?part->drawingSettings:cad::DrawingSheetSettings{};
    QDialog dialog(this);dialog.setWindowTitle("Edit saved section views");dialog.setMinimumSize(660,360);
    auto* layout=new QVBoxLayout(&dialog);
    auto* help=new QLabel("Add up to four separately placed sections. Choose the cut-plane normal (X, Y, or Z), depth as a percentage of that axis's source bounds, and sheet position in paper millimeters.",&dialog);
    help->setWordWrap(true);layout->addWidget(help);
    auto* table=new QTableWidget(&dialog);table->setColumnCount(5);
    table->setHorizontalHeaderLabels({"Plane axis","Cut depth (%)","Sheet X (mm)","Sheet Y (mm)","Section ID"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);layout->addWidget(table);
    auto addRow=[&](const cad::DrawingSectionAnnotation& section){
        const int row=table->rowCount();table->insertRow(row);
        table->setItem(row,0,new QTableWidgetItem(section.planeAxis));
        table->setItem(row,1,new QTableWidgetItem(QString::number(section.depthFraction*100.0,'f',2)));
        table->setItem(row,2,new QTableWidgetItem(QString::number(section.xMm,'f',3)));
        table->setItem(row,3,new QTableWidgetItem(QString::number(section.yMm,'f',3)));
        auto* identity=new QTableWidgetItem(section.id);identity->setData(Qt::UserRole,section.id);table->setItem(row,4,identity);
    };
    for(const cad::DrawingSectionAnnotation& section:settings.sectionAnnotations)addRow(section);
    auto* actions=new QHBoxLayout;auto* add=new QPushButton("Add section",&dialog);auto* remove=new QPushButton("Remove selected",&dialog);
    actions->addWidget(add);actions->addWidget(remove);actions->addStretch();layout->addLayout(actions);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);layout->addWidget(buttons);
    connect(add,&QPushButton::clicked,&dialog,[&]{
        if(table->rowCount()>=4){QMessageBox::information(&dialog,"Section views","A sheet can contain up to four independent section views.");return;}
        cad::DrawingSectionAnnotation section;section.id=QUuid::createUuid().toString(QUuid::WithoutBraces);addRow(section);
    });
    connect(remove,&QPushButton::clicked,&dialog,[&]{const int row=table->currentRow();if(row>=0)table->removeRow(row);});
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QVector<cad::DrawingSectionAnnotation> sections;
    for(int row=0;row<table->rowCount();++row){
        bool depthOk=false,xOk=false,yOk=false;const QString planeAxis=table->item(row,0)->text().trimmed().toUpper();
        const double depth=table->item(row,1)->text().toDouble(&depthOk),x=table->item(row,2)->text().toDouble(&xOk),y=table->item(row,3)->text().toDouble(&yOk);
        if(!depthOk||!xOk||!yOk){reportError("Save drawing section views","Enter numeric cut depths and sheet X/Y positions for every section.");return;}
        sections.append({table->item(row,4)->data(Qt::UserRole).toString(),planeAxis,depth/100.0,x,y});
    }
    settings.sectionAnnotations=sections;if(settings.sourceBodyIds.isEmpty())settings.sourceBodyIds.append(part->id);
    QString error;if(!document_.setDrawingSheetSettings(part->id,settings,&error)){reportError("Save drawing section views",error);return;}
    refresh();statusBar()->showMessage(QString("Saved %1 independently placed section view(s).").arg(sections.size()),8000);
}
void MainWindow::inspectSavedDrawingSource() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Check saved drawing source","Select one drawing-sheet master body.");return;}
    const cad::DrawingSheetSourceStatus status=document_.drawingSheetSourceStatus(ids.first());
    switch(status.state){
        case cad::DrawingSourceState::NoSavedSheet:
            QMessageBox::information(this,"Saved drawing source","The selected body has no saved drawing sheet.");return;
        case cad::DrawingSourceState::Current:
            QMessageBox::information(this,"Saved drawing source",
                QString("Current: the saved drawing links to %1 existing source body/bodies.\n\nA new export will use the current model geometry.").arg(status.sourceBodyIds.size()));return;
        case cad::DrawingSourceState::Stale:
            QMessageBox::warning(this,"Saved drawing source",
                "Stale: one or more source bodies changed since the saved sheet revision. Export the drawing again to refresh its source revision.");return;
        case cad::DrawingSourceState::MissingBodies:
            QMessageBox::warning(this,"Saved drawing source",
                QString("Missing source body/bodies: %1\n\nSelect the intended existing source bodies and export again to replace the saved source set.").arg(status.missingSourceBodyIds.join(", ")));return;
    }
}
void MainWindow::refreshSavedDrawingSource() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Refresh drawing source","Select the body that owns the saved drawing sheet.");return;}
    QString error;if(!document_.refreshDrawingSheetSource(ids.first(),&error)){reportError("Refresh drawing source",error);return;}
    refresh();statusBar()->showMessage("Drawing source links refreshed; export a new SVG or PDF to capture the current model.",6000);
}
void MainWindow::exportDrawingSvg() { exportDrawing(false); }
void MainWindow::exportDrawingPdf() { exportDrawing(true); }
void MainWindow::exportDrawing(bool asPdf) {
    QStringList ids=selectedIds();
    if(ids.isEmpty()||ids.size()>50){reportError("Export orthographic drawing","Select one to 50 bodies for a drawing sheet.");return;}
    std::sort(ids.begin(),ids.end());
    const QString masterId=ids.first();
    const cad::Part* part=document_.find(masterId);
    if(!part){reportError("Export orthographic drawing","The selected body could not be found.");return;}
    const cad::DrawingSheetSettings saved=part->drawingSettings;
    QStringList sourceBodyIds=ids;
    if(ids.size()==1&&part->hasDrawingSettings&&!saved.sourceBodyIds.isEmpty())sourceBodyIds=saved.sourceBodyIds;
    sourceBodyIds.removeDuplicates();std::sort(sourceBodyIds.begin(),sourceBodyIds.end());
    QVector<cad::Part> sourceBodies;sourceBodies.reserve(sourceBodyIds.size());
    for(const QString& sourceId:sourceBodyIds){
        const cad::Part* source=document_.find(sourceId);
        if(!source){reportError("Export orthographic drawing","One or more saved source bodies no longer exist. Select the intended bodies to create a new sheet source.");return;}
        sourceBodies.append(*source);
    }
    cad::Part drawingPart;QString error;
    if(!cad::buildDrawingAssemblyPart(sourceBodies,&drawingPart,&error)){reportError("Prepare drawing source",error);return;}
    bool accepted=false;
    const QString projectionName=QInputDialog::getItem(this,"Drawing projection","Projection convention",
        {"Third-angle projection","First-angle projection"},part->hasDrawingSettings&&saved.projection=="First-angle"?1:0,false,&accepted);
    if(!accepted)return;
    const auto projection=projectionName.startsWith("First-angle")
        ? cad::OrthographicProjection::FirstAngle : cad::OrthographicProjection::ThirdAngle;
    const QString sheetName=QInputDialog::getItem(this,"Drawing sheet","Sheet size",
        {"A4 landscape (297 x 210 mm)","A3 landscape (420 x 297 mm)"},part->hasDrawingSettings&&saved.sheetSize=="A3"?1:0,false,&accepted);
    if(!accepted)return;
    const auto sheetSize=sheetName.startsWith("A3")
        ? cad::DrawingSheetSize::A3Landscape : cad::DrawingSheetSize::A4Landscape;
    const QString auxiliaryName=QInputDialog::getItem(this,"Drawing auxiliary view","Auxiliary view",
        {"Isometric view","Section A-A at body mid-depth"},part->hasDrawingSettings&&saved.auxiliaryView=="Section A-A"?1:0,false,&accepted);
    if(!accepted)return;
    const auto auxiliaryView=auxiliaryName.startsWith("Section")
        ? cad::DrawingAuxiliaryView::SectionA : cad::DrawingAuxiliaryView::Isometric;
    double sectionDepthFraction=0.5;
    if(auxiliaryView==cad::DrawingAuxiliaryView::SectionA){
        const double percent=QInputDialog::getDouble(this,"Section A-A","Section plane through body Y depth (%):",
            part->hasDrawingSettings?saved.sectionDepthFraction*100.0:50.0,1.0,99.0,1,&accepted);
        if(!accepted)return;
        sectionDepthFraction=percent/100.0;
    }
    const QString notesText=QInputDialog::getMultiLineText(this,"General drawing notes",
        "Optional notes (one per line, up to 5 lines of 60 characters):",part->hasDrawingSettings?saved.generalNotes.join('\n'):QString(),&accepted);
    if(!accepted)return;
    QStringList generalNotes;
    for(const QString& line:notesText.split('\n',Qt::SkipEmptyParts))generalNotes.append(line.trimmed());
    QDialog titleBlockDialog(this);titleBlockDialog.setWindowTitle("Drawing title block");titleBlockDialog.setMinimumWidth(390);
    auto* titleForm=new QFormLayout(&titleBlockDialog);
    auto* drawingNumber=new QLineEdit(part->hasDrawingSettings?saved.drawingNumber:part->partNumber,&titleBlockDialog);drawingNumber->setMaxLength(32);drawingNumber->setPlaceholderText("Defaults to part number");
    auto* drawnBy=new QLineEdit(part->hasDrawingSettings?saved.drawnBy:QString(),&titleBlockDialog);drawnBy->setMaxLength(40);drawnBy->setPlaceholderText("Optional");
    auto* checkedBy=new QLineEdit(part->hasDrawingSettings?saved.checkedBy:QString(),&titleBlockDialog);checkedBy->setMaxLength(40);checkedBy->setPlaceholderText("Optional");
    auto* drawingRevision=new QLineEdit(part->hasDrawingSettings?saved.revision:part->revision,&titleBlockDialog);drawingRevision->setMaxLength(16);drawingRevision->setPlaceholderText("Defaults to part revision");
    const QDate savedDate=part->hasDrawingSettings&&!saved.issueDate.isEmpty()?QDate::fromString(saved.issueDate,Qt::ISODate):QDate::currentDate();
    auto* issueDate=new QDateEdit(savedDate,&titleBlockDialog);issueDate->setCalendarPopup(true);issueDate->setDisplayFormat("yyyy-MM-dd");
    titleForm->addRow("Drawing number:",drawingNumber);titleForm->addRow("Drawn by:",drawnBy);
    titleForm->addRow("Checked by:",checkedBy);titleForm->addRow("Revision:",drawingRevision);titleForm->addRow("Issue date:",issueDate);
    auto* titleButtons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&titleBlockDialog);titleForm->addWidget(titleButtons);
    connect(titleButtons,&QDialogButtonBox::accepted,&titleBlockDialog,&QDialog::accept);
    connect(titleButtons,&QDialogButtonBox::rejected,&titleBlockDialog,&QDialog::reject);
    if(titleBlockDialog.exec()!=QDialog::Accepted)return;
    cad::DrawingTitleBlock titleBlock{drawingNumber->text(),drawnBy->text(),checkedBy->text(),issueDate->date().toString(Qt::ISODate),drawingRevision->text()};
    cad::DrawingSheetSettings settings=part->hasDrawingSettings?saved:cad::DrawingSheetSettings{};
    settings.sourceBodyIds=sourceBodyIds;settings.sourceGeometryToken=drawingPart.geometryToken;
    settings.projection=projection==cad::OrthographicProjection::FirstAngle?"First-angle":"Third-angle";
    settings.sheetSize=sheetSize==cad::DrawingSheetSize::A3Landscape?"A3":"A4";
    if(part->hasDrawingSettings&&!settings.viewCenters.isEmpty()){
        const bool oldThird=saved.projection!="First-angle",newThird=settings.projection!="First-angle";
        const double oldPaperScale=saved.sheetSize=="A3"?420.0/297.0:1.0;
        const double newPaperScale=settings.sheetSize=="A3"?420.0/297.0:1.0;
        const auto anchors=[](bool third){return QMap<QString,QPointF>{
            {"TOP",QPointF(third?79.0:155.0,third?47.0:143.0)},
            {"FRONT",QPointF(third?79.0:155.0,third?125.0:70.0)},
            {"RIGHT",QPointF(third?204.0:45.0,third?125.0:70.0)},
            {"AUXILIARY",QPointF(240.0,third?47.0:145.0)}};};
        const auto oldAnchors=anchors(oldThird),newAnchors=anchors(newThird);
        for(auto it=settings.viewCenters.begin();it!=settings.viewCenters.end();++it){
            const QPointF relative=it.value()/oldPaperScale-oldAnchors.value(it.key());
            *it=(newAnchors.value(it.key())+relative)*newPaperScale;
        }
    }
    settings.auxiliaryView=auxiliaryView==cad::DrawingAuxiliaryView::SectionA?"Section A-A":"Isometric";
    settings.sectionDepthFraction=sectionDepthFraction;settings.generalNotes=generalNotes;settings.drawingNumber=titleBlock.drawingNumber;
    settings.drawnBy=titleBlock.drawnBy;settings.checkedBy=titleBlock.checkedBy;settings.issueDate=titleBlock.issueDate;settings.revision=titleBlock.revision;
    const QString path=QFileDialog::getSaveFileName(this,asPdf?"Export orthographic drawing PDF":"Export orthographic drawing SVG",{},
        asPdf?"PDF drawing sheets (*.pdf)":"SVG drawings (*.svg)");
    if(path.isEmpty())return;
    const QVector<cad::BillOfMaterialsRow> bomRows=document_.billOfMaterials(sourceBodyIds,&error);
    if(!error.isEmpty()){reportError("Export orthographic drawing",error);return;}
    if(!document_.setDrawingSheetSettings(masterId,settings,&error)){reportError("Save drawing setup",error);return;}
    part=document_.find(masterId);
    bool exported=false;
    if(asPdf){
        QByteArray svg;
        if(cad::buildOrthographicDrawingSvg(drawingPart,&svg,&error,nullptr,nullptr,nullptr,projection,sheetSize,auxiliaryView,
                sectionDepthFraction,generalNotes,titleBlock,part->drawingSettings.viewCenters,part->drawingSettings.viewIds,
                part->drawingSettings.sheetId,part->drawingSettings.noteAnnotations,part->drawingSettings.dimensionAnnotations,
                part->drawingSettings.partsListAnnotation,bomRows,sourceBodyIds,part->drawingSettings.balloonAnnotations,
                part->drawingSettings.detailAnnotations,part->drawingSettings.sectionAnnotations))
            exported=cad::exportOrthographicDrawingPdf(svg,path,sheetSize,&error);
    }else{
        exported=cad::exportOrthographicDrawingSvg(drawingPart,path,&error,projection,sheetSize,auxiliaryView,sectionDepthFraction,generalNotes,titleBlock,
                part->drawingSettings.viewCenters,part->drawingSettings.viewIds,part->drawingSettings.sheetId,
                part->drawingSettings.noteAnnotations,part->drawingSettings.dimensionAnnotations,
                part->drawingSettings.partsListAnnotation,bomRows,sourceBodyIds,part->drawingSettings.balloonAnnotations,
                part->drawingSettings.detailAnnotations,part->drawingSettings.sectionAnnotations);
    }
    if(!exported){
        document_.undo();refresh();reportError("Export orthographic drawing",error);return;
    }
    refresh();
    statusBar()->showMessage(QString("Orthographic drawing exported for %1 source body/bodies: %2").arg(sourceBodyIds.size()).arg(QFileInfo(path).fileName()),8000);
}
void MainWindow::exportSheetMetalFlatPattern() {
    QDialog dialog(this);dialog.setWindowTitle("One-bend sheet-metal flat pattern");
    auto* form=new QFormLayout(&dialog);
    auto addDimension=[&](const QString& label,double minimum,double maximum,double value,int decimals){
        auto* spin=new QDoubleSpinBox(&dialog);spin->setRange(minimum,maximum);spin->setDecimals(decimals);spin->setValue(value);
        spin->setSingleStep(decimals?0.5:1.0);form->addRow(label,spin);return spin;
    };
    auto* flangeA=addDimension("Outside flange A (mm)",0.00001,1000000,50,3);
    auto* flangeB=addDimension("Outside flange B (mm)",0.00001,1000000,30,3);
    auto* sheetWidth=addDimension("Sheet width (mm)",0.00001,1000000,25,3);
    auto* angle=addDimension("Included bend angle (degrees)",15,165,90,2);
    auto* thickness=addDimension("Thickness (mm)",0.00001,1000000,1.5,3);
    auto* radius=addDimension("Inside bend radius (mm)",0,1000000,1.5,3);
    auto* kFactor=addDimension("K-factor (0–1)",0,1,0.33,4);
    auto* note=new QLabel("Uses bend allowance = angle × (inside radius + K × thickness). One bend only; check the result against material and tooling data.",&dialog);
    note->setWordWrap(true);form->addRow(note);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    form->addRow(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    const QString path=QFileDialog::getSaveFileName(this,"Export sheet-metal flat pattern",{},"SVG files (*.svg)");if(path.isEmpty())return;
    cad::SheetMetalRule rule{thickness->value(),radius->value(),kFactor->value()};cad::SheetMetalFlatPattern pattern;QString error;
    if(!cad::exportOneBendFlatPatternSvg(flangeA->value(),flangeB->value(),sheetWidth->value(),angle->value(),rule,path,&pattern,&error)){
        reportError("Export sheet-metal flat pattern",error);return;
    }
    statusBar()->showMessage(QString("Flat pattern exported: %1 mm developed length; bend allowance %2 mm")
        .arg(pattern.flatLengthMm,0,'f',3).arg(pattern.bendAllowanceMm,0,'f',3),10000);
}
bool MainWindow::saveFile(bool as) {
    QString path = document_.path();
    if (as || path.isEmpty()) path = QFileDialog::getSaveFileName(this,"Save project",path,"CADWorkbench (*.cadscene)");
    if (path.isEmpty()) return false;
    if (!path.endsWith(".cadscene",Qt::CaseInsensitive)) path += ".cadscene";
    QString error;
    if (!document_.saveAs(path,&error)) { reportError("Save",error); return false; }
    refresh(); return true;
}
void MainWindow::exportMultiBendSheetMetalDxf() {
    QDialog dialog(this);dialog.setWindowTitle("Multi-bend flat pattern DXF");auto* form=new QFormLayout(&dialog);
    auto* flanges=new QLineEdit("100,70,60",&dialog);auto* angles=new QLineEdit("90,90",&dialog);
    auto* radii=new QLineEdit("2,3",&dialog);auto* kFactors=new QLineEdit("0.33,0.40",&dialog);
    auto* width=new QDoubleSpinBox(&dialog);width->setRange(0.00001,1000000);width->setDecimals(3);width->setValue(25);
    auto* thickness=new QDoubleSpinBox(&dialog);thickness->setRange(0.00001,1000000);thickness->setDecimals(3);thickness->setValue(2);
    auto* reliefWidth=new QDoubleSpinBox(&dialog);reliefWidth->setRange(0,1000000);reliefWidth->setDecimals(3);reliefWidth->setValue(0);reliefWidth->setSuffix(" mm");
    auto* reliefDepth=new QDoubleSpinBox(&dialog);reliefDepth->setRange(0,1000000);reliefDepth->setDecimals(3);reliefDepth->setValue(0);reliefDepth->setSuffix(" mm");
    form->addRow("Outside flange lengths, in order (mm, comma-separated):",flanges);
    form->addRow("Bend angles, in order (degrees):",angles);form->addRow("Inside radii, in order (mm):",radii);
    form->addRow("K-factors, in order:",kFactors);form->addRow("Blank width (mm):",width);form->addRow("Thickness (mm):",thickness);
    form->addRow("End relief width (0 disables):",reliefWidth);form->addRow("End relief depth (0 disables):",reliefDepth);
    auto* explanation=new QLabel("Enter 2–50 bend stations and one more flange length than bends. Optional rectangular end reliefs are cut into the developed blank at each bend line. Folded solids and springback are not modeled.",&dialog);
    explanation->setWordWrap(true);form->addRow(explanation);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    auto parseList=[](const QString& text,QVector<double>* values){
        values->clear();const QStringList fields=text.split(',',Qt::SkipEmptyParts);if(fields.isEmpty())return false;
        for(const QString& field:fields){bool ok=false;const double value=field.trimmed().toDouble(&ok);if(!ok||!std::isfinite(value))return false;values->append(value);}return true;
    };
    QVector<double> flangeValues,angleValues,radiusValues,kValues;
    if(!parseList(flanges->text(),&flangeValues)||!parseList(angles->text(),&angleValues)||
       !parseList(radii->text(),&radiusValues)||!parseList(kFactors->text(),&kValues)||
       angleValues.size()!=radiusValues.size()||angleValues.size()!=kValues.size()){
        reportError("Multi-bend flat pattern","Enter numeric comma-separated values, with matching angle, radius, and K-factor counts.");return;
    }
    QVector<cad::SheetMetalBend> bendValues;bendValues.reserve(angleValues.size());
    for(int i=0;i<angleValues.size();++i)bendValues.append({angleValues[i],radiusValues[i],kValues[i]});
    const QString path=QFileDialog::getSaveFileName(this,"Export multi-bend flat pattern",{},"DXF files (*.dxf)");if(path.isEmpty())return;
    cad::MultiBendFlatPattern pattern;QString error;
    const cad::SheetMetalRelief relief{reliefWidth->value(),reliefDepth->value()};
    if(!cad::exportMultiBendFlatPatternDxf(flangeValues,width->value(),thickness->value(),bendValues,relief,path,&pattern,&error)){
        reportError("Export multi-bend flat pattern",error);return;
    }
    statusBar()->showMessage(QString("DXF exported: flat length %1 mm, %2 bend lines").arg(pattern.flatLengthMm,0,'f',3).arg(pattern.bendLinePositionsMm.size()),8000);
}
void MainWindow::exportFile(bool step) {
    const QString extension = step ? ".step" : ".stl";
    QString path = QFileDialog::getSaveFileName(this,"Export geometry",{},step ? "STEP (*.step *.stp)" : "STL (*.stl)");
    if (path.isEmpty()) return;
    if (!path.endsWith(extension,Qt::CaseInsensitive) && !(step && path.endsWith(".stp",Qt::CaseInsensitive))) path += extension;
    QString error;
    const bool success = step ? document_.exportSTEP(path,&error) : document_.exportSTL(path,&error);
    if (!success) reportError("Export",error);
    else statusBar()->showMessage("Exported " + path,8000);
}
void MainWindow::exportIGESFile() {
    QString path=QFileDialog::getSaveFileName(this,"Export IGES geometry",{},"IGES files (*.iges *.igs)");
    if(path.isEmpty())return;
    if(!path.endsWith(".iges",Qt::CaseInsensitive)&&!path.endsWith(".igs",Qt::CaseInsensitive))path+=".iges";
    QString error;if(!document_.exportIGES(path,&error)){reportError("Export IGES",error);return;}
    statusBar()->showMessage(QString("Exported IGES solids: %1").arg(QFileInfo(path).fileName()),8000);
}
bool MainWindow::maybeSave() {
    if (!document_.dirty()) return true;
    const auto answer = QMessageBox::warning(this,"Unsaved changes","Save current changes?",QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,QMessageBox::Save);
    if (answer == QMessageBox::Cancel) return false;
    if (answer == QMessageBox::Save) return saveFile();
    return true;
}
void MainWindow::closeEvent(QCloseEvent* event) { if (maybeSave()) event->accept(); else event->ignore(); }


#include "MainWindow.h"
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
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFutureWatcher>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QInputDialog>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>
#include <QTabWidget>
#include <QToolButton>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QtConcurrent/QtConcurrentRun>

#include <optional>
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
    tree_->setHeaderLabels({"Bodies", "Visible"});
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
    auto* exportStepAction = file->addAction("Export STEP...",this,[this]{exportFile(true);});
    auto* exportStlAction = file->addAction("Export STL...",this,[this]{exportFile(false);});
    file->addSeparator();
    auto* exitAction = file->addAction("E&xit",this,&QWidget::close);

    auto* edit = menuBar()->addMenu("&Edit");
    undoAction_ = edit->addAction("&Undo",this,[this]{if(document_.undo()) refresh();},QKeySequence::Undo);
    redoAction_ = edit->addAction("&Redo",this,[this]{if(document_.redo()) refresh();},QKeySequence::Redo);
    edit->addSeparator();
    auto* renameAction = edit->addAction("Rename selected...",this,&MainWindow::renameObject,QKeySequence("F2"));
    auto* colorAction = edit->addAction("Body color...",this,&MainWindow::recolorObject);
    auto* deleteAction = edit->addAction("Delete selected",this,&MainWindow::removeSelected,QKeySequence::Delete);

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
    auto* loftAction = create->addAction("Loft between rectangles...",this,&MainWindow::loftRectangles);
    auto* revolveAction = create->addAction("Revolve rectangle profile...",this,&MainWindow::revolveRectangleProfile);
    auto* axisRevolveAction = create->addAction("Revolve about X/Y/Z axis...",this,&MainWindow::revolveAboutWorldAxis);
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
    auto* edgeLengthAction = inspect->addAction("Measure selected edge length...",this,&MainWindow::measureSelectedEdgeLength);
    auto* vertexCoordinatesAction = inspect->addAction("Measure selected vertex coordinates...",this,&MainWindow::measureSelectedVertexCoordinates);
    auto* vertexDistanceAction = inspect->addAction("Measure distance between vertices...",this,&MainWindow::measureSelectedVertexDistance);

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
        {"Import",{importStepAction}},
        {"Export",{exportStepAction,exportStlAction}},
        {"History",{undoAction_,redoAction_}}
    });
    addRibbonTab("Create",{
        {"Primitives",{boxAction,cylAction,sphereAction,coneAction,torusAction,prismAction,capsuleAction,ellipsoidAction,tubeAction,wedgeAction,frustumAction,nutAction,orientedCylinderAction,orientedConeAction,torusSegmentAction}},
        {"Profiles",{extrusionAction,loftAction,revolveAction,axisRevolveAction,sweepCircleAction,sweepRectangleAction,sweepEllipseAction,helicalSweepAction,variablePitchHelixAction,taperedHelixAction,ellipticalPrismAction,ellipticalTubeAction,circularLoftAction,rectToCircleLoftAction,hollowReducerAction,ellipticalLoftAction}},
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
        {"Manage",{renameAction,colorAction,deleteAction}}
    });
    addRibbonTab("Inspect",{
        {"Measure",{massPropertiesAction,clearanceAction,interferenceAction,diagnosticsAction,sectionAction,surfaceSummaryAction,faceAreaAction,edgeLengthAction,vertexCoordinatesAction,vertexDistanceAction}},
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
    detail_->setText(QString("%1\n\nVolume: %2 mmÂ³\nBounds X: %3 to %4 mm\nBounds Y: %5 to %6 mm\nBounds Z: %7 to %8 mm\nFaces: %9\nEdges: %10\nVisible: %11\nColor: %12\n\nID: %13")
        .arg(part->name).arg(props.Mass(),0,'f',3)
        .arg(xmin,0,'f',2).arg(xmax,0,'f',2)
        .arg(ymin,0,'f',2).arg(ymax,0,'f',2)
        .arg(zmin,0,'f',2).arg(zmax,0,'f',2)
        .arg(numberOf(part->shape,TopAbs_FACE)).arg(numberOf(part->shape,TopAbs_EDGE))
        .arg(part->visible ? "Yes" : "No").arg(part->color).arg(part->id));
}
void MainWindow::showMassProperties() {
    if (!requireBodyMode("Measure body")) return;
    const auto ids=selectedIds();
    if (ids.size()!=1) { reportError("Measure body","Select exactly one body."); return; }
    const cad::Part* part=document_.find(ids.first());
    if (!part) return;
    bool accepted=false;
    const double density=QInputDialog::getDouble(this,"Mass properties","Material density (kg/m^3)",7850.0,0.00001,1000000.0,3,&accepted);
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
        "Center of mass\nX: %9 mm\nY: %10 mm\nZ: %11 mm")
        .arg(part->name)
        .arg(xmax-xmin,0,'f',3).arg(ymax-ymin,0,'f',3).arg(zmax-zmin,0,'f',3)
        .arg(volumeProperties.Mass(),0,'f',3).arg(areaProperties.Mass(),0,'f',3)
        .arg(massProperties.densityKgPerM3,0,'f',3).arg(massProperties.massKg,0,'f',6)
        .arg(massProperties.centerX,0,'f',3).arg(massProperties.centerY,0,'f',3).arg(massProperties.centerZ,0,'f',3);
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
            auto* item = new QTreeWidgetItem(tree_,{part.name,QString()});
            item->setData(0,Qt::UserRole,part.id);
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
    setWindowTitle(QString("CADWorkbench v1.9 â€¢ %1%2").arg(title,document_.dirty() ? " *" : ""));
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
    sketchPlane->addItems({"XY plane","XZ plane","YZ plane"});
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
    if(prepared){command.sketchPlane=static_cast<cad::SketchPlane>(sketchPlane->currentIndex());command.planeOffset=planeOffset->value();command.reverseDirection=reverseDirection->isChecked();command.throughAll=throughAll->isChecked();}
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
bool MainWindow::saveFile(bool as) {
    QString path = document_.path();
    if (as || path.isEmpty()) path = QFileDialog::getSaveFileName(this,"Save project",path,"CADWorkbench (*.cadscene)");
    if (path.isEmpty()) return false;
    if (!path.endsWith(".cadscene",Qt::CaseInsensitive)) path += ".cadscene";
    QString error;
    if (!document_.saveAs(path,&error)) { reportError("Save",error); return false; }
    refresh(); return true;
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
bool MainWindow::maybeSave() {
    if (!document_.dirty()) return true;
    const auto answer = QMessageBox::warning(this,"Unsaved changes","Save current changes?",QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,QMessageBox::Save);
    if (answer == QMessageBox::Cancel) return false;
    if (answer == QMessageBox::Save) return saveFile();
    return true;
}
void MainWindow::closeEvent(QCloseEvent* event) { if (maybeSave()) event->accept(); else event->ignore(); }


#include "MainWindow.h"
#include "CadToolIcons.h"
#include "core/DrawingSheet.h"
#include "core/HoleTable.h"
#include "core/DrillingWorksheet.h"
#include "core/DocumentRevisionHistory.h"
#include "core/DocumentAudit.h"
#include "core/DesignBrowserQuery.h"
#include "core/ManufacturingHandoff.h"
#include "core/ShapeFeatureTree.h"
#include "core/BooleanReferenceLineage.h" // pattern lineage API
#include "core/SheetMetal.h"
#include "core/SurfaceModeling.h"
#include "CadViewport.h"
#include "SketchCanvasDialog.h"
#include "EngineeringWorkbenchDialog.h"
#include "GuidedStudyDialog.h"
#include "WorkshopPlanningDialog.h"
#include "core/ParametricStudy.h"
#include "core/AssemblyFitAudit.h"
#include "core/PlanarMateSelection.h"
#include <QSvgRenderer>
#include <QSizePolicy>
#include <QPainter>
#include <QImage>
#include <QPixmap>
#include <BRepCheck_Analyzer.hxx>
#include "core/SheetMetalBendSequenceAudit.h"
#include "core/AssemblyNetworkStructureAudit.h"
#include "core/AssemblyResiliencePlanner.h"
#include "core/ArticulatedFrameEuler.h"
#include "core/JointDriveCoupling.h"
#include "core/MetricClearanceHoleTable.h"
#include "core/CommandSearchRank.h"
#include "core/CylindricalFitReview.h"
#include "core/GdtDatumReview.h"
#include "core/BendReliefPlanning.h"
#include "core/BendCouponCalibration.h"
#include "core/AssemblyHierarchyAudit.h"
#include "core/MotionDriveLimitReview.h"

#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepGProp.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepTools.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <gp_Vec.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>

#include <QAction>
#include <QActionGroup>
#include <QAbstractItemView>
#include <QApplication>
#include <QCloseEvent>
#include <QColorDialog>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDate>
#include <QDateTime>
#include <QDateEdit>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFutureWatcher>
#include <QHash>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QCryptographicHash>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QInputDialog>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QShortcut>
#include <QSet>
#include <QSize>
#include <QStyle>
#include <QToolBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QToolButton>
#include <QTimer>
#include <QUuid>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <array>
#include <QWidgetAction>
#include <QtConcurrent/QtConcurrentRun>
#include <gp_Pnt.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Vec.hxx>

#include <optional>
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <numbers>
#include <utility>

namespace {
TopoDS_Shape selectedSubshape(const cad::SceneDocument& document,const QString& partId,
                              const QString& geometryToken,int shapeIndex,TopAbs_ShapeEnum type) {
    for(const cad::Part& part:document.parts()){
        // Selection may point to ANY body in the scene, not just the first.
        if(part.id!=partId)continue;
        if(part.geometryToken!=geometryToken||shapeIndex<1)return {};
        int index=0;
        for(TopExp_Explorer explorer(part.shape,type);explorer.More();explorer.Next())
            if(++index==shapeIndex)return explorer.Current();
        return {};
    }
    return {};
}

QString meshImportStatus(const QString& format,const QString& fileName,const cad::MeshImportSummary& summary) {
    return QStringLiteral("Imported faceted %1 %2: %3; welded %4 vertices, removed %5 degenerate/%6 duplicate facets, reoriented %7, filled %8 planar hole(s).")
        .arg(format,summary.importedAsSurfaceBody?QStringLiteral("surface body"):QStringLiteral("solid"),fileName)
        .arg(static_cast<qulonglong>(summary.weldedVertices))
        .arg(static_cast<qulonglong>(summary.removedDegenerateTriangles))
        .arg(static_cast<qulonglong>(summary.removedDuplicateTriangles))
        .arg(static_cast<qulonglong>(summary.reorientedTriangles))
        .arg(static_cast<qulonglong>(summary.filledHoles));
}

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
QString ribbonCommandLabel(QString label) {
    label.remove('&');
    while(label.endsWith('.'))label.chop(1);
    static const QHash<QString,QString> conciseLabels={
        {"Sketch and extrude profile","Extrude"},
        {"Interactive plane sketch canvas","Sketch canvas"},
        {"Sketch on selected planar face","Face sketch"},
        {"General feature tree for any body","Feature graph"},
        {"Constrained rectangle + editable extrusion","Parametric extrude"},
        {"Constrained offset rectangle + extrusion","Offset extrude"},
        {"Add parametric through-hole to selected base","Parametric hole"},
        {"Pattern parametric through-hole","Pattern holes"},
        {"Loft between rectangles","Loft"},
        {"Revolve rectangle profile","Revolve"},
        {"Revolve about X/Y/Z axis","Axis revolve"},
        {"Make selected body a component","Make component"},
        {"Insert component occurrence","Insert occurrence"},
        {"Create rigid joint (select parent then child)","Rigid joint"},
        {"Create revolute joint (select parent then child)","Revolute joint"},
        {"Drive selected revolute joint","Drive revolute"},
        {"Create prismatic joint (select parent then child)","Prismatic joint"},
        {"Drive selected prismatic joint","Drive prismatic"},
        {"Edit selected joint motion track","Motion track"},
        {"Evaluate assembly motion study at time","Motion study"},
        {"BOM and parts list","BOM / parts list"},
        {"Part number / description / revision","Part metadata"},
        {"Edit feature history / view sketch constraints","Edit feature history"},
        {"Fillet selected edges","Fillet edges"},
        {"Chamfer selected edges","Chamfer edges"},
        {"Asymmetric chamfer selected edge","Asymmetric chamfer"},
        {"Fillet all body edges","Fillet all"},
        {"Chamfer all body edges","Chamfer all"},
        {"Edit saved drawing notes","Drawing notes"},
        {"Edit saved drawing dimensions","Drawing dimensions"},
        {"Place linked drawing parts list","Drawing parts list"},
        {"Edit saved BOM balloons","BOM balloons"},
        {"Edit saved detail views","Detail views"},
        {"Edit saved section views","Section views"},
        {"Check saved drawing source","Check drawing link"},
        {"Audit all linked drawing dimensions","Drawing link health"}
    };
    if(conciseLabels.contains(label))return conciseLabels.value(label);
    QString compact=label;
    if(compact.startsWith("Export "))compact.remove(0,7);
    else if(compact.startsWith("Import "))compact.remove(0,7);
    else if(compact.startsWith("Edit saved "))compact.remove(0,11);
    else if(compact.startsWith("Show selected "))compact.remove(0,14);
    else if(compact.startsWith("Add parametric "))compact.remove(0,4);
    compact.replace(" selected body","");
    compact.replace(" selected","");
    return compact.trimmed();
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
    resize(1440,860);
    setMinimumSize(1040,680);
    restoreWorkspaceLayout();
    refresh();
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
    viewport_->setObjectName("CadViewport");
    setCentralWidget(viewport_);

    tree_ = new QTreeWidget(this);
    tree_->setHeaderLabels({"Bodies / components", "Visible"});
    tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree_->setAlternatingRowColors(true);
    tree_->setRootIsDecorated(true);
    tree_->setUniformRowHeights(true);
    tree_->header()->setStretchLastSection(false);
    tree_->header()->setSectionResizeMode(0,QHeaderView::Stretch);
    tree_->header()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    auto* browser = new QDockWidget("DESIGN BROWSER",this);
    browserDock_ = browser;
    browser->setObjectName("ModelBrowserDock");
    // Model browser with local filtering; the full model remains visible in the viewport.
    auto* browserWidget=new QWidget(browser);
    auto* browserLayout=new QVBoxLayout(browserWidget);
    browserLayout->setContentsMargins(5,6,5,5);browserLayout->setSpacing(4);
    auto* browserSearch=new QLineEdit(browserWidget);
    browserFilter_=browserSearch;
    browserSearch->setPlaceholderText("Search: material:steel  kind:solid  state:hidden  feature:fillet");
    browserSearch->setToolTip("AND filters: name:, part:, material:, feature:, kind:solid/surface, state:visible/hidden/stale, has:features. Quote phrases.");
    browserSearch->setClearButtonEnabled(true);
    browserSearch->setObjectName("ModelBrowserFilter");
    browserLayout->addWidget(browserSearch);
    // One-click scope filters are particularly useful for crowded imported
    // assemblies. They operate on the browser view only, never on CAD solids.
    auto* browserScopes=new QWidget(browserWidget);
    auto* scopeRow=new QHBoxLayout(browserScopes);
    scopeRow->setContentsMargins(0,0,0,0);
    scopeRow->setSpacing(3);
    const QList<QPair<QString,QString>> scopes={{"All",QString()},
        {"Hidden","state:hidden"},{"Stale","state:stale"},
        {"Solids","kind:solid"},{"Surfaces","kind:surface"}};
    for(const auto& [label,query]:scopes){
        auto* scope=new QToolButton(browserScopes);
        scope->setText(label);
        scope->setObjectName(QStringLiteral("BrowserScope_%1").arg(label));
        scope->setAutoRaise(true);
        scope->setToolTip(query.isEmpty()?"Show all design-browser bodies":
            QStringLiteral("Filter browser by %1").arg(query));
        connect(scope,&QToolButton::clicked,this,[browserSearch,query]{browserSearch->setText(query);});
        scopeRow->addWidget(scope);
    }
    browserLayout->addWidget(browserScopes);
    browserMatchSummary_=new QLabel("Showing all bodies",browserWidget);
    browserMatchSummary_->setObjectName("BrowserMatchSummary");
    browserMatchSummary_->setToolTip("Browser filters never hide bodies in the 3-D viewport or change saved model state.");
    browserLayout->addWidget(browserMatchSummary_);
    browserLayout->addWidget(tree_,1);
    browser->setWidget(browserWidget);
    connect(browserSearch,&QLineEdit::textChanged,this,[this]{applyBrowserFilter();});
    connect(tree_,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem* item,int){
        if(item&&item->parent()&&item->parent()->data(0,Qt::UserRole).isValid()){
            tree_->clearSelection();item->parent()->setSelected(true);
            editGeneralFeatureTree();
        }
    });
    browser->setMinimumWidth(260);
    addDockWidget(Qt::LeftDockWidgetArea,browser);

    // The assembly navigator is a second VIEW of the same stable body IDs.
    // It never constructs duplicate model occurrences or changes parentage.
    // Present assemblyPath as genuine nested folders, including intermediate
    // paths that may not have an explicit body attached to them.
    assemblyTree_=new QTreeWidget(this);
    assemblyTree_->setObjectName("AssemblyHierarchyNavigator");
    assemblyTree_->setHeaderLabels({"Subassemblies / components"});
    assemblyTree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    assemblyTree_->setUniformRowHeights(true);
    assemblyTree_->setAlternatingRowColors(true);
    assemblyTree_->header()->setStretchLastSection(true);
    assemblyDock_=new QDockWidget("ASSEMBLY NAVIGATOR",this);
    assemblyDock_->setObjectName("AssemblyHierarchyDock");
    assemblyDock_->setWidget(assemblyTree_);
    addDockWidget(Qt::LeftDockWidgetArea,assemblyDock_);
    tabifyDockWidget(browser,assemblyDock_);
    browser->raise();
    connect(assemblyTree_,&QTreeWidget::itemSelectionChanged,this,[this]{
        QStringList ids;
        for(auto* item:assemblyTree_->selectedItems()){
            const QString id=item->data(0,Qt::UserRole).toString();
            if(!id.isEmpty()&&document_.find(id))ids.append(id);
        }
        if(ids.isEmpty())return;
        if(browserFilter_)browserFilter_->clear();
        {
            const QSignalBlocker blocker(tree_);
            tree_->clearSelection();
            for(int i=0;i<tree_->topLevelItemCount();++i){
                auto* body=tree_->topLevelItem(i);
                if(ids.contains(body->data(0,Qt::UserRole).toString())){
                    body->setSelected(true);tree_->scrollToItem(body);
                }
            }
        }
        viewport_->selectIds(ids);
        updateInspector(ids);updateContextActions();updateFeatureTimeline();
    });
    connect(assemblyTree_,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem* item,int){
        if(item&&!item->data(0,Qt::UserRole).toString().isEmpty())viewport_->fitSelection();
    });

    detail_ = new QLabel("Select a body to inspect its geometry.",this);
    detail_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    detail_->setWordWrap(true);
    detail_->setMargin(12);
    auto* properties = new QDockWidget("PROPERTIES & ACTIONS",this);
    propertiesDock_ = properties;
    properties->setObjectName("PropertiesDock");
    auto* propertyScroll=new QScrollArea(properties);
    propertyScroll->setObjectName("PropertiesScrollArea");
    propertyScroll->setFrameShape(QFrame::NoFrame);
    propertyScroll->setWidgetResizable(true);
    propertyScroll->setWidget(detail_);
    // Persistent inspector and a selection-aware command surface share one dock.
    // This avoids repeated menu hunting while preserving existing command handlers.
    auto* inspectorRoot=new QWidget(properties);
    auto* inspectorLayout=new QVBoxLayout(inspectorRoot);
    inspectorLayout->setContentsMargins(7,7,7,5);
    inspectorLayout->setSpacing(6);
    contextSummary_=new QLabel("Select one or more bodies to edit",inspectorRoot);
    contextSummary_->setObjectName("ContextSelectionSummary");
    contextSummary_->setWordWrap(true);
    inspectorLayout->addWidget(contextSummary_);
    auto* contextButtons=new QWidget(inspectorRoot);
    contextButtons->setObjectName("ContextActionsPanel");
    auto* contextGrid=new QGridLayout(contextButtons);
    contextGrid->setContentsMargins(0,0,0,0);
    contextGrid->setHorizontalSpacing(5);
    contextGrid->setVerticalSpacing(5);
    inspectorLayout->addWidget(contextButtons);
    inspectorLayout->addWidget(propertyScroll,1);
    properties->setWidget(inspectorRoot);
    properties->setMinimumWidth(265);
    addDockWidget(Qt::RightDockWidgetArea,properties);

    // A true design-timeline view of the selected body's saved feature nodes.
    // It is an inspector, not an independent feature store or a second source
    // of truth: double-clicking opens the existing atomic feature editor.
    timelineDock_=new QDockWidget("DESIGN TIMELINE",this);
    timelineDock_->setObjectName("FeatureTimelineDock");
    timelineDock_->setAllowedAreas(Qt::BottomDockWidgetArea|Qt::TopDockWidgetArea);
    featureTimeline_=new QListWidget(timelineDock_);
    featureTimeline_->setObjectName("FeatureTimelineList");
    featureTimeline_->setViewMode(QListView::ListMode);
    featureTimeline_->setFlow(QListView::LeftToRight);
    featureTimeline_->setWrapping(false);
    featureTimeline_->setMovement(QListView::Static);
    featureTimeline_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    featureTimeline_->setMinimumHeight(70);
    featureTimeline_->setMaximumHeight(115);
    timelineDock_->setWidget(featureTimeline_);
    addDockWidget(Qt::BottomDockWidgetArea,timelineDock_);
    connect(featureTimeline_,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem* item){
        if(!item||!item->data(Qt::UserRole).isValid())return;
        const QString id=item->data(Qt::UserRole).toString();
        if(!document_.find(id))return;
        {
            const QSignalBlocker blocker(tree_);
            tree_->clearSelection();
            for(int i=0;i<tree_->topLevelItemCount();++i){
                auto* row=tree_->topLevelItem(i);
                if(row->data(0,Qt::UserRole).toString()==id){row->setSelected(true);break;}
            }
        }
        viewport_->selectIds({id});
        editGeneralFeatureTree();
    });

    auto* file = menuBar()->addMenu("&File");
    auto* newAction = file->addAction("&New",this,&MainWindow::newFile,QKeySequence::New);
    auto* openAction = file->addAction("&Open...",this,&MainWindow::openFile,QKeySequence::Open);
    auto* saveAction = file->addAction("&Save",this,[this]{saveFile();},QKeySequence::Save);
    auto* saveAsAction = file->addAction("Save &As...",this,[this]{saveFile(true);},QKeySequence::SaveAs);
    recentProjectsMenu_=file->addMenu("Recent projects");
    recentProjectsMenu_->setObjectName("RecentCadProjectsMenu");
    rebuildRecentProjectsMenu();
    auto* createRevisionAction=file->addAction("Create named document revision...",this,&MainWindow::createDocumentRevision);
    auto* compareRevisionsAction=file->addAction("Compare document revisions...",this,&MainWindow::compareDocumentRevisions);
    auto* restoreRevisionAction=file->addAction("Restore document revision as...",this,&MainWindow::restoreDocumentRevision);
    file->addAction("Tag or prune document revisions...",this,&MainWindow::manageDocumentRevisionHistory);
    auto* autoRevisionAction=file->addAction("Create a revision after each save");
    autoRevisionAction->setCheckable(true);
    QSettings appSettings;
    autoRevisionAction->setChecked(appSettings.value(QStringLiteral("history/automaticCheckpoint"),false).toBool());
    connect(autoRevisionAction,&QAction::toggled,this,[](bool enabled){
        QSettings().setValue(QStringLiteral("history/automaticCheckpoint"),enabled);
    });
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
    file->addAction("Export verified-geometry axial hole table CSV...",this,&MainWindow::exportHoleTableCsv);
    file->addAction("Export source-linked drilling feasibility worksheet CSV...",this,&MainWindow::exportDrillingWorksheetCsv);
    file->addAction("Export feature-linked inspection review CSV...",this,&MainWindow::exportFeatureInspectionNotes);
    auto* exportPartsListAction=file->addAction("Export parts list CSV...",this,&MainWindow::exportPartsListCsv);
    file->addAction("Create UNISSUED manufacturing handoff package...",this,
        &MainWindow::exportManufacturingHandoff);
    auto* exportDrawingAction=file->addAction("Export orthographic drawing SVG...",this,&MainWindow::exportDrawingSvg);
    file->addAction("Export orthographic drawing PDF...",this,&MainWindow::exportDrawingPdf);
    auto* exportDrawingDxfAction=file->addAction("Export orthographic drawing DXF...",this,&MainWindow::exportDrawingDxf);
    auto* arrangeDrawingViewsAction=file->addAction("Arrange saved drawing views...",this,&MainWindow::editDrawingViewLayout);
    auto* editDrawingNotesAction=file->addAction("Edit saved drawing notes...",this,&MainWindow::editDrawingNotes);
    auto* editDrawingDimensionsAction=file->addAction("Edit saved drawing dimensions...",this,&MainWindow::editDrawingDimensions);
    auto* addEdgeDimensionAction=file->addAction("Add linked length, diameter or center distance...",this,&MainWindow::addSelectedEdgeDrawingDimension);
    file->addAction("Edit saved linked dimension and tolerances...",this,&MainWindow::editSelectedEdgeDrawingDimension);
    auto* removeEdgeDimensionAction=file->addAction("Remove saved edge dimension...",this,&MainWindow::removeSelectedEdgeDrawingDimension);
    file->addAction("Edit manually authored geometric tolerance frames...",this,&MainWindow::editDrawingGdtFrames);
    auto* editDrawingPartsListAction=file->addAction("Place linked drawing parts list...",this,&MainWindow::editDrawingPartsList);
    auto* editDrawingBalloonsAction=file->addAction("Edit saved BOM balloons...",this,&MainWindow::editDrawingBalloons);
    auto* editDrawingDetailsAction=file->addAction("Edit saved detail views...",this,&MainWindow::editDrawingDetails);
    auto* editDrawingSectionsAction=file->addAction("Edit saved section views...",this,&MainWindow::editDrawingSections);
    auto* inspectDrawingSourceAction=file->addAction("Check saved drawing source...",this,&MainWindow::inspectSavedDrawingSource);
    file->addAction("Audit all linked drawing dimensions...",this,[this]{
        const QStringList selected=selectedIds();
        if(selected.size()!=1){reportError("Drawing link audit","Select exactly one drawing-sheet owner body.");return;}
        const cad::Part* owner=document_.find(selected.first());
        if(!owner||!owner->hasDrawingSettings){
            reportError("Drawing link audit","The selected body has no saved drawing sheet.");return;
        }
        QVector<cad::Part> sources;
        QSet<QString> uniqueIds;
        for(const auto& annotation:owner->drawingSettings.edgeDimensionAnnotations){
            if(uniqueIds.contains(annotation.sourceBodyId))continue;
            uniqueIds.insert(annotation.sourceBodyId);
            if(const cad::Part* source=document_.find(annotation.sourceBodyId))sources.append(*source);
        }
        cad::DrawingDimensionAudit result;QString reason;
        if(!cad::auditDrawingDimensionLinks(sources,owner->drawingSettings,&result,&reason)){
            reportError("Drawing link audit",reason);return;
        }
        QStringList lines{
            "LINKED DRAWING DIMENSION AUDIT — READ ONLY",
            QStringLiteral("Current: %1 | Changed but resolvable: %2 | Broken: %3 | Missing source: %4")
                .arg(result.current).arg(result.needsRegeneration).arg(result.broken).arg(result.missing),
            result.readyForRegeneration()?QStringLiteral("Geometry links are ready for explicit drawing regeneration and review."):
                QStringLiteral("Fix missing or ambiguous dimension references before issuing a drawing."),
            QString()
        };
        for(const auto& entry:result.entries){
            lines<<QStringLiteral("%1 | %2 | %3 | %4 | %5")
                .arg(entry.annotationId,entry.sourceId,entry.kind,entry.view,entry.status)
                 <<QStringLiteral("  %1").arg(entry.detail);
            if(entry.hasMeasurement)
                lines<<QStringLiteral("  CURRENT GEOMETRY: %1 mm (manual tolerance interval %2 to %3 mm)")
                    .arg(entry.nominalMm,0,'f',3).arg(entry.minimumMm,0,'f',3)
                    .arg(entry.maximumMm,0,'f',3);
        }
        if(result.entries.isEmpty())lines<<"No selected-edge dimensions are saved on this sheet.";
        // Datum semantics are read-only advice. Old saved GD&T frames remain
        // untouched; no drawing revision is silently acknowledged or approved.
        std::vector<cad::drawing::GdtFrame> gdts;
        for(const auto& f:owner->drawingSettings.gdtFrames)
            gdts.push_back({f.id.toStdString(),f.characteristic.toStdString(),
                f.datumA.toStdString(),f.datumB.toStdString(),
                f.datumC.toStdString(),f.toleranceMm});
        std::vector<cad::drawing::GdtAdvice> advisories;
        std::string datumError;
        if(!cad::drawing::reviewGdtDatumPolicy(gdts,&advisories,&datumError)){
            lines<<QStringLiteral("Manual GD&T datum review unavailable: %1")
                   .arg(QString::fromStdString(datumError));
        }else{
            lines<<QString()<<QStringLiteral("MANUAL GD&T DATUM REVIEW: %1 advisory item(s)")
                    .arg(static_cast<int>(advisories.size()));
            for(const auto& advisory:advisories)
                lines<<QStringLiteral("  %1: %2")
                    .arg(QString::fromStdString(advisory.frameId),
                         QString::fromStdString(advisory.warning));
        }
        lines<<QString()<<"This is a link-preflight, not a GD&T or engineering drawing certification.";
        QDialog review(this);review.setWindowTitle("Linked drawing dimension health");review.resize(820,500);
        auto* layout=new QVBoxLayout(&review);
        auto* body=new QPlainTextEdit(lines.join('\n'),&review);body->setReadOnly(true);layout->addWidget(body);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,&review);
        auto* exportButton=buttons->addButton("Export audit CSV...",QDialogButtonBox::ActionRole);
        layout->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::rejected,&review,&QDialog::reject);
        connect(exportButton,&QPushButton::clicked,&review,[&,this]{
            QString path=QFileDialog::getSaveFileName(&review,"Save drawing link audit",
                "drawing-link-audit.csv","CSV (*.csv)");
            if(path.isEmpty())return;
            if(!path.endsWith(".csv",Qt::CaseInsensitive))path+=".csv";
            const auto csvField=[](QString field){field.replace('"',"\"\"");return QStringLiteral("\"")+field+QStringLiteral("\"");};
            QStringList csv{"annotation_id,source_id,kind,view,status,details,current_nominal_mm,manual_min_mm,manual_max_mm"};
            for(const auto& item:result.entries){
                QStringList row;
                for(const auto& field:{item.annotationId,item.sourceId,item.kind,item.view,item.status,item.detail})
                    row<<csvField(field);
                row<<(item.hasMeasurement?QString::number(item.nominalMm,'f',3):QString());
                row<<(item.hasMeasurement?QString::number(item.minimumMm,'f',3):QString());
                row<<(item.hasMeasurement?QString::number(item.maximumMm,'f',3):QString());
                csv<<row.join(',');
            }
            const QByteArray bytes=(csv.join('\n')+"\n").toUtf8();
            QSaveFile file(path);
            if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit()){
                file.cancelWriting();reportError("Drawing link audit","Could not write the audit CSV.");return;
            }
            statusBar()->showMessage("Linked drawing dimension audit exported without changing the model.",7000);
        });
        review.exec();
    });
    file->addAction("Review witnessed drawing edge revisions across sheets...",this,[this]{
        QMap<QString,QStringList> requested;
        const QStringList highlighted=selectedIds();
        const bool oneSheet=highlighted.size()==1&&document_.find(highlighted.first())&&
            document_.find(highlighted.first())->hasDrawingSettings;
        for(const cad::Part& owner:document_.parts()){
            if(!owner.hasDrawingSettings||(oneSheet&&owner.id!=highlighted.first()))continue;
            const auto state=document_.drawingSheetSourceStatus(owner.id);
            for(const auto& dimension:owner.drawingSettings.edgeDimensionAnnotations){
                if(state.staleAnnotationIds.contains(dimension.id))
                    requested[owner.id].append(dimension.id);
            }
        }
        if(requested.isEmpty()){
            QMessageBox::information(this,"Drawing revision review",
                "No stale witnessed edge dimensions were found in the chosen sheet set. "
                "This does not validate other GD&T annotations or certify a drawing.");return;
        }
        QString report,issue;
        if(!document_.previewDrawingEdgeReviewBatch(requested,&report,&issue)){
            reportError("Drawing revision review blocked",issue);return;
        }
        if(QMessageBox::question(this,"Review source-linked edge revisions",
            report.left(3000)+"\n\nReview and acknowledge only these independently resolved edge dimensions "
            "as ONE undoable edit? GD&T, the aggregate sheet revision and drawing issue status "
            "will NOT be approved.",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        if(!document_.reviewDrawingEdgeReviewBatch(requested,&report,&issue)){
            reportError("Drawing revision review rejected",issue);return;
        }
        refresh(true);
        statusBar()->showMessage("Selected witnessed drawing revisions reviewed together; drawing remains unissued.",8500);
    });
    file->addAction("Export all saved drawing revision issues CSV...",this,[this]{
        QString path=QFileDialog::getSaveFileName(this,"Export drawing revision review register",
            "drawing-recovery-review.csv","CSV (*.csv)");
        if(path.isEmpty())return;
        if(!path.endsWith(".csv",Qt::CaseInsensitive))path+=".csv";
        QString issue;
        if(!document_.exportDrawingRecoveryCsv(path,&issue)){
            reportError("Export drawing review",issue);return;
        }
        statusBar()->showMessage("Unissued drawing revision register exported.",7000);
    });
    file->addAction("Regenerate saved drawing SVG...",this,&MainWindow::refreshSavedDrawingSource);
    file->addAction("Regenerate all saved drawings (strict atomic SVG batch)...",this,[this]{
        QStringList owners;
        for(const cad::Part& body:document_.parts())if(body.hasDrawingSettings)owners<<body.id;
        if(owners.isEmpty()||owners.size()>12){
            reportError("Strict drawing regeneration","Select a document with 1-12 saved sheets.");return;
        }
        const QString parent=QFileDialog::getExistingDirectory(this,
            "Choose parent for a NEW, unissued drawing batch folder");
        if(parent.isEmpty())return;
        const QString output=QDir(parent).filePath(QStringLiteral("CADWorkbench_DrawingReview_%1")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz")));
        QString report,issue;
        if(!document_.exportSavedDrawingSvgBatch(owners,output,&report,&issue)){
            reportError("Strict drawing batch",issue);return;
        }
        QMessageBox::information(this,"Linked drawing set exported",
            report+"\n\nSaved unissued SVG batch: "+output+
            "\n\nReview each sheet before issuing it. Saved source revisions remain untouched.");
    });
    file->addAction("Regenerate ALL saved drawing previews (not issued)...",this,[this]{
        // F08: stage every sheet from CURRENT geometry before writing ANY file.
        // The resulting previews are intentionally UNISSUED; no sheet revision,
        // source token, history entry, or previous published SVG is modified.
        struct Preview {QString masterId,drawingNumber;QByteArray svg;
                        cad::DrawingSheetSize paper;QString modelGeometryToken;};
        QVector<Preview> staged;
        QStringList failures;
        for(const cad::Part& owner:document_.parts()){
            if(!owner.hasDrawingSettings)continue;
            cad::DrawingSheetSettings prepared=owner.drawingSettings;
            QStringList ids=prepared.sourceBodyIds;
            if(ids.isEmpty())ids.append(owner.id);
            QSet<QString> distinctSourceIds;
            for(const QString& id:ids)distinctSourceIds.insert(id);
            if(ids.size()>50||ids.size()!=distinctSourceIds.size()){
                failures<<QStringLiteral("%1: invalid duplicate/oversized source list").arg(owner.id);
                continue;
            }
            std::sort(ids.begin(),ids.end());
            QVector<cad::Part> sources;
            bool missing=false;
            for(const QString& id:ids){
                if(const auto* source=document_.find(id))sources.append(*source);
                else{failures<<QStringLiteral("%1: source %2 is missing").arg(owner.id,id);missing=true;}
            }
            if(missing)continue;
            QString issue;
            cad::DrawingDimensionAudit dimensions;
            if(!cad::auditDrawingDimensionLinks(sources,prepared,&dimensions,&issue)||
               !dimensions.readyForRegeneration()){
                failures<<QStringLiteral("%1: drawing dimensions are missing/ambiguous: %2")
                    .arg(owner.id,issue);continue;
            }
            cad::Part composite;
            if(!cad::buildDrawingAssemblyPart(sources,&composite,&issue)){
                failures<<QStringLiteral("%1: %2").arg(owner.id,issue);continue;
            }
            prepared.sourceBodyIds=ids;
            prepared.sourceGeometryToken=composite.geometryToken;
            prepared.sourceDataToken=document_.drawingSourceDataToken(ids,&issue);
            cad::DrawingSheetSettings valid;
            if(!issue.isEmpty()||!document_.prepareDrawingSheetSettings(owner.id,prepared,&valid,&issue)){
                failures<<QStringLiteral("%1: %2").arg(owner.id,issue);continue;
            }
            const auto projection=valid.projection=="First-angle"
                ?cad::OrthographicProjection::FirstAngle:cad::OrthographicProjection::ThirdAngle;
            const auto paper=valid.sheetSize=="A3"
                ?cad::DrawingSheetSize::A3Landscape:cad::DrawingSheetSize::A4Landscape;
            const auto auxiliary=valid.auxiliaryView=="Section A-A"
                ?cad::DrawingAuxiliaryView::SectionA:cad::DrawingAuxiliaryView::Isometric;
            const cad::DrawingTitleBlock title{valid.drawingNumber,valid.drawnBy,
                valid.checkedBy,valid.issueDate,valid.revision};
            const auto bom=document_.billOfMaterials(ids,&issue);
            if(!issue.isEmpty()){
                failures<<QStringLiteral("%1: %2").arg(owner.id,issue);continue;
            }
            QByteArray svg;
            if(!cad::buildOrthographicDrawingSvg(composite,&svg,&issue,nullptr,nullptr,nullptr,
                projection,paper,auxiliary,valid.sectionDepthFraction,
                valid.generalNotes,title,valid.viewCenters,valid.viewIds,valid.sheetId,
                valid.noteAnnotations,valid.dimensionAnnotations,valid.partsListAnnotation,
                bom,ids,valid.balloonAnnotations,valid.detailAnnotations,valid.sectionAnnotations,
                valid.edgeDimensionAnnotations,sources,valid.gdtFrames)||
               !QSvgRenderer(svg).isValid()){
                failures<<QStringLiteral("%1: drawing render failed (%2)").arg(owner.id,issue);continue;
            }
            staged.append({owner.id,valid.drawingNumber,svg,paper,valid.sourceGeometryToken});
        }
        if(!failures.isEmpty()){
            // Any bad sheet blocks the entire batch, rather than delivering a
            // misleading partly updated drawing set.
            reportError("Batch drawing regeneration blocked",failures.join("\n"));return;
        }
        if(staged.isEmpty()){
            QMessageBox::information(this,"Drawing previews","There are no saved drawing sheets.");return;
        }
        const bool includePdf=QMessageBox::question(this,"Preview formats",
            "Create matching UNISSUED PDF copies as well as SVG previews? "
            "Neither format changes a saved drawing revision.",
            QMessageBox::Yes|QMessageBox::No,QMessageBox::Yes)==QMessageBox::Yes;
        const QString parent=QFileDialog::getExistingDirectory(this,
            "Choose folder for unissued regenerated drawing previews");
        if(parent.isEmpty())return;
        const QString newFolder=QStringLiteral("CADWorkbench_Unissued_Drawings_%1")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz"));
        const QString path=QDir(parent).filePath(newFolder);
        if(QDir(path).exists()||!QDir().mkpath(path)){
            reportError("Batch drawing previews","Could not create a new output directory.");return;
        }
        QStringList written;
        QVector<QString> svgHashes, pdfHashes;
        for(int index=0;index<staged.size();++index){
            // Index-only names are safe even for externally loaded body IDs.
            // A separate manifest maps previews to their original sheet IDs.
            const QString name=QStringLiteral("drawing_%1_UNISSUED.svg")
                .arg(index+1,3,10,QChar('0'));
            const QString fileName=QDir(path).filePath(name);
            QSaveFile file(fileName);
            if(!file.open(QIODevice::WriteOnly)||
               file.write(staged[index].svg)!=staged[index].svg.size()||!file.commit()){
                file.cancelWriting();
                for(const QString& created:written)QFile::remove(created);
                QDir().rmdir(path);
                reportError("Batch drawing previews","Writing failed; incomplete previews were removed.");return;
            }
            written.append(fileName);
            svgHashes.append(QString::fromLatin1(QCryptographicHash::hash(
                staged[index].svg,QCryptographicHash::Sha256).toHex()));
            pdfHashes.append(QString());
            if(includePdf){
                const QString pdf=QDir(path).filePath(QStringLiteral("drawing_%1_UNISSUED.pdf")
                    .arg(index+1,3,10,QChar('0')));
                QString pdfError;
                if(!cad::exportOrthographicDrawingPdf(staged[index].svg,pdf,
                        staged[index].paper,&pdfError)){
                    // This directory did not exist before the operation and
                    // contains only this batch's own outputs. Do not leave
                    // an incomplete set of PDFs and SVGs for fabrication.
                    QDir(path).removeRecursively();
                    reportError("Batch drawing PDF previews",
                        "PDF generation failed; the entire unissued batch was removed: "+pdfError);
                    return;
                }
                // Digest the committed file, not an in-memory PDF approximation.
                QFile pdfContents(pdf);
                if(!pdfContents.open(QIODevice::ReadOnly)){
                    QDir(path).removeRecursively();
                    reportError("Batch drawing PDF previews",
                        "PDF could not be reopened for integrity checking; batch removed.");return;
                }
                pdfHashes[index]=QString::fromLatin1(QCryptographicHash::hash(
                    pdfContents.readAll(),QCryptographicHash::Sha256).toHex());
                written.append(pdf);
            }
        }
        // Keep a human-readable manifest so the preview is identifiable
        // without embedding untrusted part numbers into filesystem paths.
        const auto csvQuote=[](QString value){
            value.replace('"',QStringLiteral("\"\""));
            return QStringLiteral("\"")+value+QStringLiteral("\"");
        };
        QStringList indexRows{"preview_file,sheet_owner_id,drawing_number,issue_status,pdf_file,current_geometry_token,svg_sha256,pdf_sha256"};
        for(int index=0;index<staged.size();++index){
            const QString name=QStringLiteral("drawing_%1_UNISSUED.svg")
                .arg(index+1,3,10,QChar('0'));
            const QString pdfName=includePdf?QStringLiteral("drawing_%1_UNISSUED.pdf")
                .arg(index+1,3,10,QChar('0')):QString();
            indexRows<<QStringList{csvQuote(name),csvQuote(staged[index].masterId),
                csvQuote(staged[index].drawingNumber),csvQuote("UNISSUED REVIEW REQUIRED"),
                csvQuote(pdfName),csvQuote(staged[index].modelGeometryToken),
                csvQuote(svgHashes[index]),csvQuote(pdfHashes[index])}.join(',');
        }
        const QByteArray indexBytes=(indexRows.join('\n')+'\n').toUtf8();
        QSaveFile indexFile(QDir(path).filePath("drawing_index.csv"));
        if(!indexFile.open(QIODevice::WriteOnly)||
           indexFile.write(indexBytes)!=indexBytes.size()||!indexFile.commit()){
            indexFile.cancelWriting();
            for(const QString& created:written)QFile::remove(created);
            QDir().rmdir(path);
            reportError("Batch drawing previews","Could not write the preview index; batch was rolled back.");return;
        }
        QMessageBox::information(this,"Unissued drawing previews",
            QStringLiteral("Generated %1 current-geometry SVG%3 previews in:\n%2\n\n"
                "These are NOT approved drawing revisions. Verify each sheet, "
                "then issue an individual reviewed revision with the existing drawing command.")
                .arg(staged.size()).arg(path).arg(includePdf?QStringLiteral(" and PDF"):QString()));
    });

    auto* exportFlatPatternAction=file->addAction("Export one-bend sheet-metal flat pattern SVG...",this,&MainWindow::exportSheetMetalFlatPattern);
    auto* exportMultiBendDxfAction=file->addAction("Export multi-bend sheet-metal flat pattern DXF...",this,&MainWindow::exportMultiBendSheetMetalDxf);
    auto* exportFoldedSheetDxfAction=file->addAction("Export selected folded body flat pattern DXF...",this,&MainWindow::exportSelectedFoldedSheetFlatPatternDxf);
    file->addAction("Export saved associative flat blank (SVG/DXF)...",this,
        &MainWindow::exportSelectedAssociativeFlatPattern);
    file->addAction("Export associative flat manufacturing package (SVG/DXF/CSV/manifest)...",this,
        &MainWindow::exportSelectedAssociativeFlatPackage);
    file->addAction("Export selected associative flat blanks as ONE manufacturing job...",this,
        &MainWindow::exportSelectedAssociativeFlatJobBatch);
    auto* exportFoldedBendTableAction=file->addAction("Export selected folded body bend table CSV...",this,&MainWindow::exportSelectedFoldedSheetBendTableCsv);
    file->addSeparator();
    auto* exitAction = file->addAction("E&xit",this,&QWidget::close);

    auto* edit = menuBar()->addMenu("&Edit");
    undoAction_ = edit->addAction("&Undo",this,[this]{if(document_.undo()) refresh();},QKeySequence::Undo);
    redoAction_ = edit->addAction("&Redo",this,[this]{if(document_.redo()) refresh();},QKeySequence::Redo);
    edit->addSeparator();
    auto* renameAction = edit->addAction("Rename selected...",this,&MainWindow::renameObject,QKeySequence("F2"));
    batchRenameAction_=edit->addAction("Batch rename selected bodies...",this,
        &MainWindow::batchRenameSelectedBodies,QKeySequence("Ctrl+Shift+R"));
    batchRenameAction_->setObjectName("BatchRenameSelectedAction");
    batchRenameAction_->setToolTip("Preview and rename multiple bodies as one atomic operation (Ctrl+Shift+R).");
    auto* colorAction = edit->addAction("Body color...",this,&MainWindow::recolorObject);
    auto* materialAction = edit->addAction("Assign material...",this,&MainWindow::assignMaterial);
    auto* metadataAction = edit->addAction("Part number / description / revision...",this,&MainWindow::editPartMetadata);
    auto* editParameterAction = edit->addAction("Edit named parameters...",this,&MainWindow::editNamedParameter);
    edit->addAction("Edit named dimensions across selected parametric bodies...",this,
        &MainWindow::editNamedParameterBatch);
    edit->addAction("Coordinated feature suppression/restore across bodies...",this,
        &MainWindow::editParametricFeatureSuppressionBatch);
    edit->addAction("Explore geometry-rebuilt parametric design alternatives...",this,
        &MainWindow::exploreParametricDesignAlternatives);
    edit->addAction("Edit selected constrained polygon sketch...",this,&MainWindow::editConstrainedPolygonExtrusion);
    edit->addAction("Edit linked sketch source, depth and offset...",this,&MainWindow::editLinkedSketchExtrusion);
    edit->addAction("Add/edit in-place external sketch boss or pocket...",this,&MainWindow::editInPlaceSketchFeature);
    edit->addAction("Edit selected sketch on canvas...",this,&MainWindow::editInteractiveSketch);
    auto* generalTreeAction=edit->addAction("General feature tree for any body...",this,&MainWindow::editGeneralFeatureTree);
    edit->addAction("Batch finishing of selected bodies (atomic fillet/chamfer)...",this,[this]{
        const QStringList ids=selectedIds();
        if(ids.size()<2||ids.size()>16){
            reportError("Batch engineering finish","Select 2-16 distinct independent solid bodies in the parts browser.");return;
        }
        QStringList names;
        for(const QString& id:ids){
            const cad::Part* part=document_.find(id);
            if(!part){reportError("Batch engineering finish","A selected body is no longer available.");return;}
            names<<part->name;
        }
        bool accepted=false;
        const QString kind=QInputDialog::getItem(this,"Batch finishing",
            "Select the edge treatment to add to ALL selected bodies:",
            {"Fillet all edges","Chamfer all edges"},0,false,&accepted);
        if(!accepted)return;
        const double size=QInputDialog::getDouble(this,"Batch finishing",
            kind.startsWith("Fillet")?"Radius (mm)":"Chamfer distance (mm)",
            .5,.00001,10000,5,&accepted);
        if(!accepted)return;
        if(QMessageBox::question(this,"Review atomic finishing job",
             QStringLiteral("Apply %1 (%2 mm) to %3 selected bodies?\n\n%4\n\n"
                 "All bodies and linked dependents must rebuild successfully; otherwise nothing changes. "
                 "This creates ONE undoable feature-tree transaction.")
                .arg(kind).arg(size,0,'g',12).arg(ids.size()).arg(names.join(", ")),
             QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        QMap<QString,QVector<cad::ShapeFeatureNode>> proposals;
        for(const QString& id:ids){
            const cad::Part* part=document_.find(id);
            QVector<cad::ShapeFeatureNode> nodes=part->shapeFeatureNodes;
            cad::ShapeFeatureNode finish;
            finish.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
            finish.type=kind;
            finish.parameters.insert(kind.startsWith("Fillet")?"radius":"distance",size);
            nodes.append(finish);
            proposals.insert(id,nodes);
        }
        QString issue;
        if(!document_.setShapeFeatureTreesBatch(proposals,&issue)){
            reportError("Atomic finishing job",issue);return;
        }
        refresh();
        statusBar()->showMessage(QStringLiteral("Committed one undoable %1 job across %2 bodies.")
            .arg(kind).arg(ids.size()),10000);
    });
    edit->addAction("Mixed associative feature history (branches + sketches)...",this,
        &MainWindow::editInterleavedFeatureHistory);

    edit->addAction("Edit selected sketch geometric constraints...",this,&MainWindow::editConstrainedSketchRelations);
    auto* editSketchCirclesAction=edit->addAction("Add/remove constrained sketch circles...",this,&MainWindow::editConstrainedSketchCircles);
    edit->addAction("Design configurations and variants...",this,&MainWindow::showConfigurationStudy);
    edit->addAction("Edit feature-linked engineering/inspection notes...",this,&MainWindow::editFeatureInspectionNotes);
    edit->addAction("Edit named parameter links...",this,&MainWindow::editParameterLinks);
    edit->addAction("Add parametric all-edge chamfer...",this,&MainWindow::addParametricAllEdgeChamfer);
    edit->addAction("Add parametric all-edge fillet...",this,&MainWindow::addParametricAllEdgeFillet);
    auto* sheetEditAction=edit->addAction("Edit folded sheet-metal body...",this,&MainWindow::editFoldedSheetMetal);
    auto* sheetDevelopmentAction=edit->addAction("Edit linked flat blank K-factors and relief...",this,&MainWindow::editLinkedFoldedFlatDevelopment);
    auto* featureHistoryAction = edit->addAction("Edit feature history / view sketch constraints...",this,&MainWindow::showSelectedFeatureHistory);
    auto* deleteAction = edit->addAction("Delete selected",this,&MainWindow::removeSelected,QKeySequence::Delete);

    auto* assemblyMenu = menuBar()->addMenu("&Assembly");
    auto* makeComponentAction = assemblyMenu->addAction("Make selected body a component...",this,&MainWindow::makeSelectedComponent);
    auto* insertOccurrenceAction = assemblyMenu->addAction("Insert component occurrence...",this,&MainWindow::insertSelectedComponentOccurrence);
    assemblyMenu->addAction("Orient selected free occurrence (roll, pitch, yaw)...",this,&MainWindow::orientSelectedComponentOccurrence);
    assemblyMenu->addAction("Synchronize selected component definition to its occurrences...",this,&MainWindow::synchronizeSelectedComponentDefinition);
    assemblyMenu->addAction("Linear pattern of component occurrences...",this,&MainWindow::linearPatternSelectedComponentOccurrence);
    assemblyMenu->addAction("Circular Z-axis pattern of component occurrences...",this,&MainWindow::circularPatternSelectedComponentOccurrence);
    auto* rigidJointAction = assemblyMenu->addAction("Create rigid joint (select parent then child)...",this,&MainWindow::createRigidJoint);
    assemblyMenu->addAction("Solve assembly frame mates (three point pairs)...",this,&MainWindow::solveAssemblyFrameMates);
    assemblyMenu->addAction("Inspect/remove individual live rigid mate links...",this,
                            &MainWindow::manageLiveRigidMates);
    assemblyMenu->addAction("Audit saved rigid mate structure and cycle links...",this,
                            &MainWindow::auditLiveRigidMateStructure);
    assemblyMenu->addAction("Refresh saved live rigid mates",this,[this]{
        QString issue;
        if(!document_.refreshLiveRigidMateNetwork(&issue)){
            reportError("Refresh live rigid mates",issue);return;
        }
        refresh(true);
        statusBar()->showMessage("Saved rigid mates verified and refreshed.",8000);
    });
    assemblyMenu->addAction("Remove saved live rigid mates",this,[this]{
        if(!document_.removeLiveRigidMateNetwork()){
            reportError("Live rigid mates","This document has no saved live rigid mate network.");return;
        }
        refresh(true);
        statusBar()->showMessage("Saved rigid mate definitions removed (undo available).",8000);
    });
    assemblyMenu->addSeparator();
    assemblyMenu->addAction("Save live articulated pair (bounded mechanism)...",this,
                            &MainWindow::saveLiveArticulatedPair);
    assemblyMenu->addAction("Edit multiple articulated drives (atomic preview)...",this,
                            &MainWindow::editSavedArticulatedDrives);
    assemblyMenu->addAction("Edit articulated joint axis, limits and reference offset...",this,
                            &MainWindow::editSavedArticulatedJointDefinition);
    assemblyMenu->addAction("Edit ALL saved articulated mate frame offsets (atomic)...",this,[this]{
        if(!document_.hasLiveArticulatedNetwork()){
            reportError("Batch mate-frame editor","Save a live articulated network first.");return;
        }
        const auto original=document_.liveArticulatedJointDefinitions();
        if(original.isEmpty()||original.size()>64){
            reportError("Batch mate-frame editor","The saved joint count is outside 1-64.");return;
        }
        QDialog dialog(this);dialog.setWindowTitle("Atomic articulated mate frame editor");
        dialog.resize(700,500);
        auto* layout=new QVBoxLayout(&dialog);
        auto* instructions=new QLabel(
            "Edit zero-pose frame translations (mm) for multiple joints. "
            "Axis orientation, joint type and travel limits remain unchanged. "
            "Every changed frame is solved together; all solid placements are rolled back "
            "if any joint or constraint fails. Continuous motion clearance is NOT certified.",&dialog);
        instructions->setWordWrap(true);layout->addWidget(instructions);
        auto* table=new QTableWidget(original.size(),4,&dialog);
        table->setHorizontalHeaderLabels({"Saved connection","Zero X (mm)","Zero Y (mm)","Zero Z (mm)"});
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        for(int i=0;i<original.size();++i){
            const auto& joint=original[i];
            auto* identity=new QTableWidgetItem(QString("%1: %2 -> %3")
                .arg(i+1).arg(joint.parentBodyId.left(8),joint.childBodyId.left(8)));
            identity->setFlags(identity->flags()&~Qt::ItemIsEditable);
            table->setItem(i,0,identity);
            const double xyz[]={joint.childAtZero.translation.x,
                                joint.childAtZero.translation.y,
                                joint.childAtZero.translation.z};
            for(int j=0;j<3;++j)table->setItem(i,j+1,
                new QTableWidgetItem(QString::number(xyz[j],'g',15)));
        }
        layout->addWidget(table);
        auto* strict=new QCheckBox("Reject nominal interference / insufficient clearance",&dialog);
        strict->setChecked(false);layout->addWidget(strict);
        auto* controls=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
        layout->addWidget(controls);
        connect(controls,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
        connect(controls,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        if(dialog.exec()!=QDialog::Accepted)return;
        QMap<int,cad::SceneDocument::ArticulatedNetworkJoint> proposed;
        for(int i=0;i<original.size();++i){
            auto next=original[i];
            double* fields[]={&next.childAtZero.translation.x,
                              &next.childAtZero.translation.y,
                              &next.childAtZero.translation.z};
            for(int j=0;j<3;++j){
                bool ok=false;
                const auto* cell=table->item(i,j+1);
                const double value=cell?cell->text().trimmed().toDouble(&ok):0.0;
                if(!ok||!std::isfinite(value)||std::abs(value)>100000){
                    reportError("Batch mate-frame editor",QString("Joint %1 has an invalid offset.").arg(i+1));return;
                }
                *fields[j]=value;
            }
            if(next.childAtZero.translation.x!=original[i].childAtZero.translation.x||
               next.childAtZero.translation.y!=original[i].childAtZero.translation.y||
               next.childAtZero.translation.z!=original[i].childAtZero.translation.z)
                proposed.insert(i,next);
        }
        if(proposed.isEmpty()){
            statusBar()->showMessage("No mate-frame offsets changed.",5000);return;
        }
        QString preview,issue;
        if(!document_.previewLiveArticulatedJointDefinitionBatch(proposed,&preview,&issue,
                0.0,strict->isChecked())){
            reportError("Atomic mate-frame preview",issue);return;
        }
        if(QMessageBox::question(this,"Review complete mate network",
            preview+"\n\nCommit all changed frame definitions as one undo checkpoint?",
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        if(!document_.replaceLiveArticulatedJointDefinitionBatch(proposed,&issue,
                0.0,strict->isChecked())){
            reportError("Atomic mate-frame commit",issue);return;
        }
        refresh(true);
        statusBar()->showMessage("All selected mate frames committed atomically.",8000);
    });
    assemblyMenu->addAction("Rebind saved articulated joint endpoints (full-solve preview)...",this,
                            &MainWindow::rebindSavedArticulatedJoint);
    assemblyMenu->addAction("Edit saved articulated joint driving coordinate...",this,
                            &MainWindow::editSavedArticulatedJoint);
    assemblyMenu->addAction("Refresh saved articulated mechanism",this,[this]{
        QString issue;
        if(!document_.refreshLiveArticulatedNetwork(&issue)){
            reportError("Refresh articulated mechanism",issue);return;
        }
        refresh(true);
        statusBar()->showMessage("Saved articulated mechanism refreshed against the fixed frame.",8500);
    });
    assemblyMenu->addAction("Remove saved articulated mechanism",this,[this]{
        if(!document_.removeLiveArticulatedNetwork()){
            reportError("Articulated mechanism","No live articulated mechanism is saved.");return;
        }
        refresh(true);
        statusBar()->showMessage("Saved articulated definitions removed; undo available.",7000);
    });
    auto* revoluteJointAction = assemblyMenu->addAction("Create revolute joint (select parent then child)...",this,&MainWindow::createRevoluteJoint);
    auto* driveJointAction = assemblyMenu->addAction("Drive selected revolute joint...",this,&MainWindow::driveSelectedRevoluteJoint);
    assemblyMenu->addAction("Create prismatic joint (select parent then child)...",this,&MainWindow::createPrismaticJoint);
    assemblyMenu->addAction("Drive selected prismatic joint...",this,&MainWindow::driveSelectedPrismaticJoint);
    assemblyMenu->addAction("Edit selected joint motion track...",this,&MainWindow::editSelectedJointMotionTrack);
    assemblyMenu->addAction("Retime all saved joint motion tracks (one undo)...",this,[this]{
        QMap<QString,QVector<cad::JointMotionKeyframe>> proposed;
        bool accepted=false;
        const double multiplier=QInputDialog::getDouble(this,"Retime assembly motion",
            "Time multiplier for ALL tracks (2.0 = half speed, 0.5 = double speed):",
            1.0,0.1,10.0,2,&accepted);
        if(!accepted)return;
        if(std::abs(multiplier-1.0)<1e-12){
            statusBar()->showMessage("Motion tracks unchanged.",5000);return;
        }
        for(const auto& part:document_.parts()){
            if(part.jointMotionKeyframes.isEmpty())continue;
            QVector<cad::JointMotionKeyframe> keys=part.jointMotionKeyframes;
            for(auto& key:keys)key.timeSeconds*=multiplier;
            proposed.insert(part.id,std::move(keys));
        }
        if(proposed.isEmpty()){
            reportError("Assembly motion retiming","No saved joint motion tracks found.");return;
        }
        QString report,issue;
        if(!document_.previewJointMotionKeyframesBatch(proposed,&report,&issue)){
            reportError("Assembly motion retiming",issue);return;
        }
        if(QMessageBox::question(this,"Retime all joint tracks",
            report+"\n\nCommit all track changes in one undoable operation?",
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        if(!document_.setJointMotionKeyframesBatch(proposed,&issue)){
            reportError("Assembly motion retiming",issue);return;
        }
        refresh(true);
        statusBar()->showMessage("All saved joint motion timelines retimed together; undo available.",8500);
    });

    assemblyMenu->addAction("Review selected joint drive limits (read-only)...",this,[this]{
        const auto ids=selectedIds();
        const cad::Part* child=ids.size()==1?document_.find(ids.first()):nullptr;
        if(!child||(child->assemblyJointType!="Revolute"&&child->assemblyJointType!="Prismatic")||
           child->jointMotionKeyframes.size()<2){
            reportError("Motion drive review","Select one revolute/prismatic occurrence with at least two saved keyframes.");return;
        }
        bool accepted=false;
        const double speedLimit=QInputDialog::getDouble(this,"Motion drive worksheet",
            "Maximum commanded coordinate speed (degrees/s or mm/s):",100.0,0.0,1e8,4,&accepted);
        if(!accepted)return;
        const double accelerationLimit=QInputDialog::getDouble(this,"Motion drive worksheet",
            "Maximum commanded coordinate acceleration (degrees/s² or mm/s²):",500.0,0.0,1e8,4,&accepted);
        if(!accepted)return;
        double maximumActualLinearSpeed=0,maximumSmoothSpeed=0,maximumSmoothAcceleration=0;
        QStringList problems;
        const double lower=child->assemblyJointType=="Revolute"?child->jointMinimumDegrees:child->jointMinimumMm;
        const double upper=child->assemblyJointType=="Revolute"?child->jointMaximumDegrees:child->jointMaximumMm;
        const auto& keys=child->jointMotionKeyframes;
        for(int i=1;i<keys.size();++i){
            const auto& a=keys[i-1];const auto& b=keys[i];
            const double duration=b.timeSeconds-a.timeSeconds;
            if(!std::isfinite(duration)||duration<=0){
                reportError("Motion drive review","The saved motion track has invalid keyframe times.");return;
            }
            maximumActualLinearSpeed=std::max(maximumActualLinearSpeed,
                                              std::abs(b.value-a.value)/duration);
            std::vector<cad::joint_motion::DriveEnvelope> envelopes;
            std::string issue;
            const std::vector<cad::joint_motion::Drive> segment{{1,a.value,b.value,lower,upper}};
            if(!cad::joint_motion::reviewDriveLimits(segment,duration,
                cad::joint_motion::Progression::Smoothstep,{{1,speedLimit,accelerationLimit}},
                &envelopes,&issue)){
                problems<<QStringLiteral("segment %1-%2: %3").arg(i).arg(i+1)
                          .arg(QString::fromStdString(issue));
            }
            const double travel=std::abs(b.value-a.value);
            maximumSmoothSpeed=std::max(maximumSmoothSpeed,1.5*travel/duration);
            maximumSmoothAcceleration=std::max(maximumSmoothAcceleration,6.0*travel/(duration*duration));
        }
        const QString unit=child->assemblyJointType=="Revolute"?"degrees":"mm";
        QMessageBox::information(this,"Read-only motion drive review",
            QStringLiteral("Saved piecewise-LINEAR motion peak speed: %1 %2/s\n"
               "Speed policy: %3 %2/s\n"
               "Saved linear speed policy: %8\n\n"
               "Hypothetical SMOOTHSTEP trajectory: peak speed %4 %2/s, "
               "peak acceleration %5 %2/s².\n"
               "Smoothstep policy: %6\n\n"
               "The existing saved track is LINEAR, not smoothstep; its acceleration "
               "can jump at keyframe boundaries. This is a commanded coordinate "
               "review, not a solid-body motion or collision certificate. No edits were applied.\n%7")
            .arg(maximumActualLinearSpeed,0,'g',12).arg(unit)
            .arg(speedLimit,0,'g',12).arg(maximumSmoothSpeed,0,'g',12)
            .arg(maximumSmoothAcceleration,0,'g',12)
            .arg(problems.isEmpty()?"within limits":"outside limits")
            .arg(problems.join("\n"))
            .arg(maximumActualLinearSpeed<=speedLimit+1e-10?"within limits":"EXCEEDS LIMIT"));
    });
    assemblyMenu->addAction("Audit explicit occurrence hierarchy CSV (read-only)...",this,[this]{
        const QString path=QFileDialog::getOpenFileName(this,"Select explicit occurrence hierarchy",
            {},"Simple hierarchy CSV (*.csv)");
        if(path.isEmpty())return;
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly)||file.size()>1024*1024){
            reportError("Hierarchy review","Could not open a hierarchy CSV under 1 MB.");return;
        }
        const QStringList rows=QString::fromUtf8(file.readAll()).split('\n',Qt::SkipEmptyParts);
        if(rows.size()<2||rows.size()>1025||rows.first().trimmed()!="id,parent_id"){
            reportError("Hierarchy review","Expected 1-1024 rows with header id,parent_id. Quoted CSV fields are not supported.");return;
        }
        std::vector<std::string> names,parents;
        QMap<QString,int> byId;
        for(int i=1;i<rows.size();++i){
            const QStringList cells=rows[i].trimmed().split(',',Qt::KeepEmptyParts);
            if(cells.size()!=2||cells[0].trimmed().isEmpty()||byId.contains(cells[0].trimmed())){
                reportError("Hierarchy review",QStringLiteral("Invalid or duplicate hierarchy CSV row %1.").arg(i+1));return;
            }
            const QString id=cells[0].trimmed();
            const cad::Part* occurrence=document_.find(id);
            if(!occurrence||occurrence->componentDefinitionId.isEmpty()){
                reportError("Hierarchy review",QStringLiteral("Occurrence %1 is not a loaded assembly component.").arg(id));return;
            }
            byId.insert(id,static_cast<int>(names.size()));
            names.push_back(id.toStdString());
            parents.push_back(cells[1].trimmed().toStdString());
        }
        std::vector<int> parentIndices;
        for(const auto& parent:parents){
            if(parent.empty()){parentIndices.push_back(-1);continue;}
            const auto pos=byId.constFind(QString::fromStdString(parent));
            if(pos==byId.cend()){
                reportError("Hierarchy review","A parent occurrence is missing from the explicit CSV.");return;
            }
            parentIndices.push_back(pos.value());
        }
        cad::assembly::HierarchyReport result;
        std::string reason;
        if(!cad::assembly::auditHierarchy(names,parentIndices,&result,&reason)){
            reportError("Hierarchy review",QString::fromStdString(reason));return;
        }
        QStringList description{
            "EXPLICIT ASSEMBLY HIERARCHY — READ-ONLY REVIEW",
            QStringLiteral("Occurrences: %1 | Independent roots: %2 | Maximum depth: %3")
                .arg(static_cast<int>(names.size())).arg(static_cast<int>(result.rootCount))
                .arg(static_cast<int>(result.maximumDepth)),
            "This CSV is external design intent, NOT inferred from mate links or saved into the scene."
        };
        if(result.rootCount!=1)description<<"Review multiple roots; this is a forest, not one assembly tree.";
        for(std::size_t i=0;i<names.size();++i)
            description<<QStringLiteral("%1 | parent %2 | depth %3")
                .arg(QString::fromStdString(names[i]),
                     parentIndices[i]<0?QStringLiteral("ROOT"):
                         QString::fromStdString(names[static_cast<std::size_t>(parentIndices[i])]))
                .arg(static_cast<int>(result.depthByOccurrence[i]));
        QMessageBox::information(this,"Assembly hierarchy audit",description.join('\n'));
    });
    assemblyMenu->addAction("Evaluate assembly motion study at time...",this,&MainWindow::evaluateAssemblyMotionStudy);
    auto* motionInterferenceAction=assemblyMenu->addAction("Scan selected bodies for motion interference...",this,&MainWindow::scanSelectedMotionInterference);
    auto* allPairsMotionAction=assemblyMenu->addAction("Scan all assembly bodies for motion interference...",this,&MainWindow::scanAllAssemblyMotionInterference);

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
    create->addAction("Constrained trapezoid extrusion...",this,&MainWindow::createConstrainedTrapezoidExtrusion);
    create->addAction("Constrained triangle from side lengths...",this,&MainWindow::createConstrainedTriangleExtrusion);
    create->addAction("Constrained polygon from vertices...",this,&MainWindow::createConstrainedPolygonFromVertices);
    create->addAction("Associative extrusion from selected body's sketch...",this,&MainWindow::createLinkedSketchExtrusion);
    create->addAction("Associative pocket from another body's sketch...",this,&MainWindow::createLinkedSketchPocket);
    create->addAction("Associative boss from another body's sketch...",this,&MainWindow::createLinkedSketchBoss);
    auto* sketchCanvasAction=create->addAction("Interactive plane sketch canvas...",this,&MainWindow::createInteractiveSketch);
    auto* faceSketchAction=create->addAction("Sketch on selected planar face...",this,&MainWindow::createSketchOnSelectedFace);

    auto* metricClearanceHoleAction=create->addAction("ISO 273 metric fastener clearance hole...",this,&MainWindow::addMetricFastenerClearanceHole);
    metricClearanceHoleAction->setToolTip("Select M2-M20 fastener and fine/medium/coarse nominal clearance; create an editable through-hole.");
    auto* parametricHoleAction = create->addAction("Add parametric through-hole to selected base...",this,&MainWindow::addParametricThroughHole);
    create->addAction("Add parametric blind hole to selected base...",this,&MainWindow::addParametricBlindHole);
    create->addAction("Pattern parametric through/blind hole...",this,&MainWindow::addParametricLinearHolePattern);
    auto* surfaceLoftAction=create->addAction("Lofted surface through rectangular sections...",this,&MainWindow::createLoftedSurfaceBody);
    auto* selectedFaceLoftAction=create->addAction("Loft surface between selected face boundaries...",this,&MainWindow::loftSelectedFaceBoundaries);
    selectedFaceLoftAction->setToolTip("Select two or three faces (different polygons and curved boundaries allowed); loft their outer boundary wires into an open surface.");
    create->addAction("Extrude selected edge as a surface...",this,&MainWindow::extrudeSelectedEdgeSurface);
    create->addAction("Revolve selected edge as a surface...",this,&MainWindow::revolveSelectedEdgeSurface);
    auto* surfaceOffsetAction=create->addAction("Offset selected surface face...",this,&MainWindow::offsetSelectedSurface);
    create->addAction("Extend selected surface face...",this,&MainWindow::extendSelectedSurface);
    auto* surfaceThickenAction=create->addAction("Thicken selected surface body into a solid...",this,&MainWindow::thickenSelectedSurface);
    auto* surfaceStitchAction=create->addAction("Stitch selected surface bodies into solids...",this,&MainWindow::stitchSelectedSurfaces);
    create->addAction("Sew selected surfaces into one shell...",this,&MainWindow::sewSelectedSurfaces);
    create->addAction("Trim surface with a solid...",this,&MainWindow::trimSelectedSurface);
    create->addAction("Split surface with a solid...",this,&MainWindow::splitSelectedSurface);
    auto* sheetCreateAction=create->addAction("Folded sheet-metal body...",this,&MainWindow::createFoldedSheetMetal);
    auto* sheetBlankAction=create->addAction("Associative flat blank from selected folded sheet...",this,&MainWindow::createLinkedFoldedFlatBlank);
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
    modify->addAction("Associative union (keep sources)",this,[this]{runLinkedBoolean(cad::BooleanOp::Fuse);});
    modify->addAction("Associative subtract (keep sources)",this,[this]{runLinkedBoolean(cad::BooleanOp::Cut);});
    modify->addAction("Associative intersection (keep sources)",this,[this]{runLinkedBoolean(cad::BooleanOp::Common);});
    modify->addAction("Edit associative Boolean result...",this,&MainWindow::editLinkedBoolean);
    modify->addAction("Edit multiple associative results (atomic)...",this,&MainWindow::editLinkedBooleanBatch);
    modify->addAction("Inspect associative body dependencies...",this,&MainWindow::inspectLinkedBooleanGraph);
    modify->addAction("Preview associative rebuild impact...",this,&MainWindow::previewLinkedBodyImpact);
    modify->addAction("Refresh associative bodies",this,[this]{
        QString error;if(!document_.refreshLinkedBooleanBodies(&error))reportError("Linked bodies",error);
        else{refresh(true);statusBar()->showMessage("Associative body outputs checked and refreshed.",6000);}
    });
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
    auto* commandSearchAction = view->addAction("Find command...",this,&MainWindow::openCommandSearch,QKeySequence("Ctrl+K"));
    view->addAction(assemblyDock_->toggleViewAction());
    view->addSeparator();
    isolateAction_=view->addAction("Isolate selected bodies",this,&MainWindow::isolateSelectedBodies,
                                   QKeySequence("Ctrl+Shift+I"));
    hideSelectedAction_=view->addAction("Hide selected bodies",this,&MainWindow::hideSelectedBodies,
                                        QKeySequence("Ctrl+Shift+H"));
    showAllAction_=view->addAction("Show all bodies",this,&MainWindow::showAllBodies,
                                   QKeySequence("Ctrl+Shift+J"));
    view->addSeparator();
    view->addAction(browser->toggleViewAction());
    view->addAction(properties->toggleViewAction());
    view->addAction(timelineDock_->toggleViewAction());
    auto* resetLayoutAction=view->addAction("Reset workspace layout",this,&MainWindow::resetWorkspaceLayout);
    auto* gridAction = view->addAction("XY reference grid");
    gridAction->setCheckable(true);
    connect(gridAction,&QAction::toggled,viewport_,&CadViewport::setGridVisible);

    auto* inspect = menuBar()->addMenu("&Inspect");
    inspect->addAction("Engineering workbench: 50 preliminary calculations...",this,&MainWindow::showEngineeringWorkbench);
    inspect->addAction("Guided engineering design studies...",this,&MainWindow::showGuidedEngineeringStudies);
    inspect->addAction("Manufacturing workshop: sections, machining, fits, cylinders...",this,&MainWindow::showWorkshopPlanning);
    inspect->addAction("Engineering design review and export...",this,&MainWindow::showEngineeringDesignReview);
    auto* massPropertiesAction = inspect->addAction("Mass properties / measure body...",this,&MainWindow::showMassProperties);
    auto* assemblyMassAction = inspect->addAction("Assembly center of gravity and inertia...",this,&MainWindow::showAssemblyMassProperties);
    auto* clearanceAction = inspect->addAction("Minimum distance between bodies...",this,&MainWindow::measureSelectedBodyClearance);
    auto* interferenceAction = inspect->addAction("Interference volume between bodies...",this,&MainWindow::measureSelectedBodyInterference);
    inspect->addAction("Assembly fit audit: all pairs, clearance and tolerance bounds...",this,&MainWindow::showAssemblyFitAudit);
    auto* diagnosticsAction = inspect->addAction("Geometry health and topology...",this,&MainWindow::diagnoseSelectedBody);
    auto* documentAuditAction = inspect->addAction("Audit whole project (sketch, features, joints, drawings)...",this,&MainWindow::showDocumentAudit);
    inspect->addAction("Integrated project readiness: geometry, drawings, refold and joint rates...",this,[this]{
        QByteArray evidence;QString issue;
        if(!document_.buildIntegratedEngineeringReadinessCsv(&evidence,&issue)){
            reportError("Integrated engineering readiness",issue);return;
        }
        if(QMessageBox::question(this,"Integrated engineering preflight",
            QString::fromUtf8(evidence).left(6500)+
            "\n\nAll results are UNISSUED. Save this review register?",
            QMessageBox::Yes|QMessageBox::No,QMessageBox::Yes)!=QMessageBox::Yes)return;
        QString destination=QFileDialog::getSaveFileName(this,
            "Save integrated engineering readiness", "CADWorkbench-unissued-readiness.csv", "CSV (*.csv)");
        if(destination.isEmpty())return;
        if(!destination.endsWith(".csv",Qt::CaseInsensitive))destination+=".csv";
        if(!document_.exportIntegratedEngineeringReadinessCsv(destination,&issue)){
            reportError("Integrated engineering readiness",issue);return;
        }
        statusBar()->showMessage("Read-only engineering readiness exported. Human approval remains required.",9000);
    });

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
    auto* foldedShopCheckAction=inspect->addAction("Check selected folded sheet against shop limits...",this,&MainWindow::checkSelectedFoldedSheetShopLimits);
    inspect->addAction("Audit stored folded refold geometry and OCCT volume...",this,[this]{
        const QStringList chosen=selectedIds();
        if(chosen.size()!=1){
            reportError("Refold audit","Select exactly one saved folded sheet-metal source.");return;
        }
        QByteArray evidence;QString issue;
        if(!document_.auditFoldedSheetRefoldGeometry(chosen.first(),&evidence,&issue)){
            reportError("Refold audit",issue);return;
        }
        if(QMessageBox::question(this,"Geometric refold audit",
            QString::fromUtf8(evidence).left(5000)+
            "\n\nExport this unissued inspection evidence as CSV?",
            QMessageBox::Yes|QMessageBox::No,QMessageBox::Yes)!=QMessageBox::Yes)return;
        QString path=QFileDialog::getSaveFileName(this,"Export refold geometry check",
            "refold-geometry-review.csv","CSV (*.csv)");
        if(path.isEmpty())return;
        if(!path.endsWith(".csv",Qt::CaseInsensitive))path+=".csv";
        QSaveFile output(path);
        if(!output.open(QIODevice::WriteOnly)||
           output.write(evidence)!=evidence.size()||!output.commit()){
            output.cancelWriting();reportError("Refold audit","Could not save the refold report.");return;
        }
        statusBar()->showMessage("OCCT refold conservation reviewed; not a manufacturing certification.",8500);
    });
    inspect->addAction("Audit selected folded body bend sequence...",this,
                       &MainWindow::auditSelectedFoldedSheetBendSequence);

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
    commandRibbon_=ribbon;
    ribbon->setObjectName("CadCommandRibbon");
    ribbon->setDocumentMode(true);
    ribbon->setMovable(false);
    ribbon->setMinimumHeight(116);
    auto addRibbonTab=[ribbon](const QString& tabName,
                               const QVector<QPair<QString,QList<QAction*>>>& groups) {
        auto* scroll=new QScrollArea(ribbon);
        scroll->setObjectName("RibbonScrollArea");
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* contents=new QWidget(scroll);
        contents->setObjectName("RibbonPage");
        auto* row=new QHBoxLayout(contents);
        row->setContentsMargins(6,4,6,2);
        row->setSpacing(4);
        for (const auto& group:groups) {
            auto* column=new QWidget(contents);
            column->setObjectName("RibbonGroup");
            auto* layout=new QVBoxLayout(column);
            layout->setContentsMargins(5,1,5,1);
            layout->setSpacing(2);
            auto* buttons=new QHBoxLayout;
            buttons->setContentsMargins(0,0,0,0);
            buttons->setSpacing(2);
            const int groupActionCount=static_cast<int>(group.second.size());
            int directActionCount=groupActionCount;
            if(group.first=="Primitives")directActionCount=std::min(6,groupActionCount);
            else if(group.first=="Profiles")directActionCount=std::min(7,groupActionCount);
            else if(group.first=="Features"||group.first=="Measure")directActionCount=std::min(5,groupActionCount);
            else if(groupActionCount>8)directActionCount=0;
            auto addCommandButton=[&](QAction* action){
                auto* button=new QToolButton(column);
                // Consistent, scalable original icons across every workbench tab.
                if(action->icon().isNull())action->setIcon(cad::ui::commandGlyph(action->text()));
                button->setDefaultAction(action);
                button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
                button->setMinimumWidth(90);
                button->setMaximumWidth(124);
                button->setMinimumHeight(68);
                button->setIconSize(QSize(24,24));
                QString fullLabel=action->text();fullLabel.remove('&');
                button->setText(ribbonCommandLabel(fullLabel));
                button->setToolTip(fullLabel);
                button->setStatusTip(fullLabel);
                buttons->addWidget(button);
            };
            for(int i=0;i<directActionCount;++i)addCommandButton(group.second[i]);
            if(directActionCount<groupActionCount){
                auto* overflow=new QToolButton(column);
                overflow->setText("More…");
                overflow->setMinimumWidth(76);
                overflow->setMaximumWidth(96);
                overflow->setMinimumHeight(68);
                overflow->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
                overflow->setToolTip(QString("More %1 commands").arg(group.first));
                overflow->setStatusTip(overflow->toolTip());
                auto* overflowMenu=new QMenu(overflow);
                for(int i=directActionCount;i<groupActionCount;++i)overflowMenu->addAction(group.second[i]);
                overflow->setMenu(overflowMenu);
                overflow->setPopupMode(QToolButton::InstantPopup);
                buttons->addWidget(overflow);
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
        {"Export",{exportStepAction,exportStlAction,exportBodyScheduleAction,exportBomAction,exportPartsListAction,exportDrawingAction,exportDrawingDxfAction,exportFlatPatternAction,exportMultiBendDxfAction,exportFoldedSheetDxfAction,exportFoldedBendTableAction}},
        {"Drawing",{arrangeDrawingViewsAction,editDrawingNotesAction,editDrawingDimensionsAction,addEdgeDimensionAction,removeEdgeDimensionAction,editDrawingPartsListAction,editDrawingBalloonsAction,editDrawingDetailsAction,editDrawingSectionsAction,inspectDrawingSourceAction}},
        {"History",{undoAction_,redoAction_,createRevisionAction,compareRevisionsAction,restoreRevisionAction}}
    });
    addRibbonTab("Create",{
        {"Primitives",{boxAction,cylAction,sphereAction,coneAction,torusAction,prismAction,capsuleAction,ellipsoidAction,tubeAction,wedgeAction,frustumAction,nutAction,orientedCylinderAction,orientedConeAction,torusSegmentAction}},
        {"Profiles",{sketchCanvasAction,faceSketchAction,extrusionAction,parametricExtrusionAction,parametricOffsetAction,parametricHoleAction,metricClearanceHoleAction,loftAction,surfaceLoftAction,selectedFaceLoftAction,revolveAction,axisRevolveAction,sweepCircleAction,sweepRectangleAction,sweepEllipseAction,helicalSweepAction,variablePitchHelixAction,taperedHelixAction,ellipticalPrismAction,ellipticalTubeAction,circularLoftAction,rectToCircleLoftAction,hollowReducerAction,ellipticalLoftAction}},
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
        {"Manage",{generalTreeAction,renameAction,colorAction,materialAction,metadataAction,editParameterAction,editSketchCirclesAction,featureHistoryAction,deleteAction}}
    });
    addRibbonTab("Assembly",{
        {"Components",{makeComponentAction,insertOccurrenceAction,rigidJointAction,revoluteJointAction,driveJointAction}},
        {"Reports",{partsReportAction,motionInterferenceAction,allPairsMotionAction}}
    });
    addRibbonTab("Inspect",{
        {"Measure",{massPropertiesAction,assemblyMassAction,clearanceAction,interferenceAction,diagnosticsAction,sectionAction,surfaceSummaryAction,faceAreaAction,faceAngleAction,cylinderMeasureAction,edgeLengthAction,vertexCoordinatesAction,vertexDistanceAction}},
        {"Parts data",{documentAuditAction,partsReportAction,standardsCheckAction,drawingCheckAction,foldedShopCheckAction}},
        {"Selection",{bodyAction_,faceAction,edgeAction,vertexAction}}
    });
    addRibbonTab("View",{
        {"Camera",{fitAction,fitSelectionAction,isoAction,topAction,frontAction,rightAction}},
        {"Display",{commandSearchAction,wireframeAction,gridAction}}
    });
    // Dedicated workflow pages avoid using File as the entire drawing workspace.
    // These reuse existing QActions: no duplicate geometry/export implementations.
    addRibbonTab("Drawing",{
        {"Views",{arrangeDrawingViewsAction,inspectDrawingSourceAction,exportDrawingAction,exportDrawingDxfAction}},
        {"Annotations",{editDrawingDimensionsAction,addEdgeDimensionAction,removeEdgeDimensionAction,
                         editDrawingNotesAction,editDrawingPartsListAction,editDrawingBalloonsAction}},
        {"Details",{editDrawingDetailsAction,editDrawingSectionsAction}},
        {"Review",{drawingCheckAction,documentAuditAction,exportPartsListAction}}
    });
    addRibbonTab("Sheet Metal",{
        {"Folded design",{sheetCreateAction,sheetEditAction,sheetBlankAction,sheetDevelopmentAction}},
        {"Manufacturing",{exportFoldedSheetDxfAction,exportFoldedBendTableAction,
                            exportMultiBendDxfAction,exportFlatPatternAction}},
        {"Shop checks",{foldedShopCheckAction,documentAuditAction}}
    });
    addRibbonTab("Surface",{
        {"Create",{surfaceLoftAction,selectedFaceLoftAction,surfaceOffsetAction}},
        {"Convert",{surfaceThickenAction,surfaceStitchAction}},
        {"Inspect",{surfaceSummaryAction,diagnosticsAction}}
    });

    auto* quickAccess=addToolBar("Quick Access");
    quickAccess->setObjectName("QuickAccessToolbar");
    quickAccess->setMovable(false);
    quickAccess->setFloatable(false);
    quickAccess->setIconSize(QSize(18,18));
    quickAccess->setToolButtonStyle(Qt::ToolButtonIconOnly);
    quickAccess->setContextMenuPolicy(Qt::PreventContextMenu);
    newAction->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    openAction->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    saveAction->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    undoAction_->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    redoAction_->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    quickAccess->addAction(newAction);
    quickAccess->addAction(openAction);
    quickAccess->addAction(saveAction);
    quickAccess->addSeparator();
    quickAccess->addAction(undoAction_);
    quickAccess->addAction(redoAction_);
    quickAccess->addSeparator();
    auto* homeAction=quickAccess->addAction(cad::ui::commandGlyph("Model"),"Fit model",viewport_,&CadViewport::fitAll);
    homeAction->setToolTip("Fit all geometry (F)");
    auto* workspaceLabel=new QLabel("WORKSPACE",quickAccess);
    workspaceLabel->setObjectName("WorkspaceLabel");
    quickAccess->addWidget(workspaceLabel);
    auto* workspacePicker=new QComboBox(quickAccess);
    workspaceSelector_=workspacePicker;
    workspacePicker->setObjectName("WorkspaceSelector");
    workspacePicker->addItems({"Design","Assembly","Drawing","Inspect","View","Sheet Metal","Surface"});
    workspacePicker->setMinimumWidth(128);
    quickAccess->addWidget(workspacePicker);
    connect(workspacePicker,qOverload<int>(&QComboBox::currentIndexChanged),this,[ribbon](int index){
        static const int tabForWorkspace[]={1,3,6,4,5,7,8};
        if(index>=0&&index<7)ribbon->setCurrentIndex(tabForWorkspace[index]);
    });
    connect(ribbon,&QTabWidget::currentChanged,this,[workspacePicker](int tabIndex){
        const int workspaceForTab=tabIndex==6?2:tabIndex==7?5:tabIndex==8?6:
            tabIndex==3?1:tabIndex==4?3:tabIndex==5?4:0;
        const QSignalBlocker blocker(workspacePicker);
        workspacePicker->setCurrentIndex(workspaceForTab);
    });
    auto* commandSearch=new QLineEdit(quickAccess);
    commandSearch->setObjectName("CommandSearch");
    commandSearch->setClearButtonEnabled(true);
    commandSearch->setPlaceholderText("Find parts / tools...  Ctrl+K");
    commandSearch->setToolTip("Search parts, tools or engineering terms. Ctrl+K opens the full searchable catalog.");
    commandSearch->setMinimumWidth(225);
    commandSearch->setMaximumWidth(300);
    quickAccess->addWidget(commandSearch);

    QVector<QPair<QString,QAction*>> commandEntries;
    QSet<QAction*> seenCommands;
    std::function<void(QMenu*)> collectMenuActions=[&](QMenu* menu){
        if(!menu)return;
        for(QAction* action:menu->actions()){
            if(action->menu())collectMenuActions(action->menu());
            else if(!action->isSeparator()&&!seenCommands.contains(action)){
                const QString label=action->text().remove('&').trimmed();
                if(!label.isEmpty()){seenCommands.insert(action);commandEntries.append({label,action});}
            }
        }
    };
    for(QAction* menuAction:menuBar()->actions())collectMenuActions(menuAction->menu());
    QStringList commandLabels;
    for(const auto& entry:commandEntries)commandLabels.append(entry.first);
    std::sort(commandLabels.begin(),commandLabels.end(),[](const QString& a,const QString& b){return a.compare(b,Qt::CaseInsensitive)<0;});
    commandLabels.removeDuplicates();
    auto* commandCompleter=new QCompleter(commandLabels,commandSearch);
    commandCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    commandCompleter->setFilterMode(Qt::MatchContains);
    commandCompleter->setCompletionMode(QCompleter::PopupCompletion);
    commandSearch->setCompleter(commandCompleter);
    auto triggerCommand=[commandEntries,commandSearch,this](const QString& query){
        const QString needle=query.trimmed();if(needle.isEmpty())return;
        QVector<QAction*> exact;
        for(const auto& entry:commandEntries){
            if(entry.second&&entry.second->isEnabled()&&entry.second->isVisible()&&
               entry.first.compare(needle,Qt::CaseInsensitive)==0)exact.append(entry.second);
        }
        if(exact.size()==1){
            commandSearch->clear();
            exact.front()->trigger();
            return;
        }
        // Ambiguous, partial, and engineering-synonym queries go to the
        // full ranked catalog. Never guess and run a destructive command.
        commandSearch->clear();
        openCommandSearchWithQuery(needle);
    };
    connect(commandSearch,&QLineEdit::returnPressed,this,[commandSearch,triggerCommand]{triggerCommand(commandSearch->text());});
    connect(commandCompleter,qOverload<const QString&>(&QCompleter::activated),this,triggerCommand);
    auto* focusSearch=new QShortcut(QKeySequence("Ctrl+F"),this);
    connect(focusSearch,&QShortcut::activated,this,[commandSearch]{commandSearch->setFocus();commandSearch->selectAll();});
    auto* findTools=new QPushButton("Find tools...",quickAccess);
    findTools->setObjectName("FindToolsButton");
    findTools->setToolTip("Browse all CAD commands and component families (Ctrl+K)");
    connect(findTools,&QPushButton::clicked,this,&MainWindow::openCommandSearch);
    quickAccess->addWidget(findTools);
    ribbon->setCurrentIndex(1);
    workspacePicker->setCurrentIndex(0);

    // Reuse actual menu actions in a contextual inspector and model browser.
    // Use the grid created above; do not redeclare contextGrid in this constructor.
    // This keeps the selection actions in their original Qt-owned layout.
    const QList<QPair<QString,QAction*>> contextCommands={
        {"Feature history",generalTreeAction},
        {"Rename",renameAction},
        {"Batch rename",batchRenameAction_},
        {"Appearance",colorAction},
        {"Material",materialAction},
        {"Part data",metadataAction},
        {"Fillet",filletAction},
        {"Chamfer",chamferAction},
        {"Fit",fitSelectionAction},
        {"Isolate",isolateAction_},
        {"Hide",hideSelectedAction_},
        {"Show all",showAllAction_},
        {"Properties",massPropertiesAction},
    };
    for(int i=0;i<contextCommands.size();++i){
        auto* button=new QToolButton(contextGrid->parentWidget());
        button->setObjectName(QStringLiteral("ContextActionButton_%1").arg(i));
        button->setDefaultAction(contextCommands[i].second);
        button->setText(contextCommands[i].first);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIconSize(QSize(18,18));
        button->setMinimumHeight(33);
        button->setToolTip(contextCommands[i].second->text());
        if(button->icon().isNull())button->setIcon(cad::ui::commandGlyph(contextCommands[i].first));
        contextGrid->addWidget(button,i/2,i%2);
    }
    singleBodyActions_={generalTreeAction,renameAction,colorAction,metadataAction,
                        filletAction,chamferAction,massPropertiesAction};
    multipleBodyActions_={materialAction,fitSelectionAction};
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree_,&QWidget::customContextMenuRequested,this,
            [this,generalTreeAction,renameAction,colorAction,materialAction,metadataAction,
             filletAction,chamferAction,fitSelectionAction,massPropertiesAction](const QPoint& pos){
        auto* target=tree_->itemAt(pos);
        if(target){
            while(target->parent())target=target->parent();
            if(!target->isSelected()){
                tree_->clearSelection();target->setSelected(true);
            }
        }
        QMenu menu(tree_);
        menu.addAction(generalTreeAction);
        menu.addAction(renameAction);
        menu.addAction(batchRenameAction_);
        menu.addAction(metadataAction);
        menu.addAction(colorAction);
        menu.addAction(materialAction);
        menu.addSeparator();
        menu.addAction(filletAction);menu.addAction(chamferAction);
        menu.addAction(massPropertiesAction);
        menu.addSeparator();
        menu.addAction(fitSelectionAction);
        menu.addAction(isolateAction_);
        menu.addAction(hideSelectedAction_);
        menu.addAction(showAllAction_);
        menu.exec(tree_->viewport()->mapToGlobal(pos));
    });

    addToolBarBreak(Qt::TopToolBarArea);
    auto* ribbonToolbar=addToolBar("CAD Ribbon");
    ribbonToolbar->setObjectName("CadRibbonToolbar");
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
        updateContextActions();
        updateFeatureTimeline();
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
        updateContextActions();
        updateFeatureTimeline();
    });
    // Persistent heads-up navigation: useful when the main ribbon is hidden.
    auto* navigation=addToolBar("View and selection");
    navigation->setObjectName("CadNavigationToolbar");
    navigation->setMovable(false);navigation->setFloatable(false);
    navigation->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    navigation->setIconSize(QSize(17,17));
    addToolBar(Qt::BottomToolBarArea,navigation);
    navigation->addAction(fitAction);navigation->addAction(fitSelectionAction);
    navigation->addAction(isoAction);navigation->addAction(topAction);
    navigation->addAction(frontAction);navigation->addAction(rightAction);
    navigation->addSeparator();navigation->addAction(bodyAction_);
    navigation->addAction(faceAction);navigation->addAction(edgeAction);navigation->addAction(vertexAction);
    view->addAction(navigation->toggleViewAction());
    auto* showRibbonAction=view->addAction("Show command ribbon");
    showRibbonAction->setCheckable(true);
    showRibbonAction->setChecked(true);
    connect(showRibbonAction,&QAction::toggled,ribbonToolbar,&QWidget::setVisible);
    connect(ribbonToolbar,&QToolBar::visibilityChanged,showRibbonAction,&QAction::setChecked);
    statusBar()->showMessage("Left click: select | Ctrl+click: multi-select | Right drag: orbit | Middle drag: pan | Wheel: zoom | F: fit | Ctrl+K: tools | Units: mm");
    setStyleSheet(R"QSS(
        QMainWindow { background: #e8edf1; color: #26343d; }
        QLabel#ContextSelectionSummary { background: #edf5f8; color: #174760; border-left: 3px solid #1684aa; border-radius: 3px; padding: 8px; font-weight: 600; }
        QListWidget#FeatureTimelineList { background: #f6f9fb; border: none; color: #1e3946; }
        QListWidget#FeatureTimelineList::item { background: #ffffff; border: 1px solid #c8d5dc; border-radius: 5px; padding: 11px 14px; margin: 5px 3px; }
        QListWidget#FeatureTimelineList::item:selected { background: #d8edf6; border: 2px solid #1684aa; }
        QWidget#ContextActionsPanel QToolButton { background: #ffffff; color: #2a424e; border: 1px solid #c0ced6; border-radius: 5px; padding: 5px 7px; text-align: left; }
        QWidget#ContextActionsPanel QToolButton:hover { border-color: #1684aa; background: #e5f2f7; }
        QWidget#ContextActionsPanel QToolButton:disabled { background: #ecf0f2; color: #899ba4; border-color: #d8e0e5; }
        QToolBar#CadNavigationToolbar { background: #f6f8fa; border-top: 1px solid #bdcbd3; padding: 4px 10px; }
        QMenuBar { background: #34434e; color: #f4f7f9; border-bottom: 1px solid #25323b; padding: 2px 4px; }
        QMenuBar::item { padding: 6px 10px; border-radius: 4px; }
        QMenuBar::item:selected { background: #176b91; color: #ffffff; }
        QMenu { background: #ffffff; color: #26343d; border: 1px solid #9eacb6; padding: 4px; }
        QMenu::item { padding: 6px 26px 6px 22px; border-radius: 3px; }
        QMenu::item:selected { background: #d8edf6; color: #173b4d; }
        QToolBar { background: #e8edf1; color: #26343d; border: none; spacing: 4px; padding: 4px 8px; }
        QToolBar#QuickAccessToolbar { background: #dfe6eb; border-bottom: 1px solid #b8c4cc; }
        QToolBar#CadRibbonToolbar { background: #edf1f4; border-bottom: 1px solid #b8c4cc; }
        QToolButton { background: transparent; color: #26343d; border: 1px solid transparent; border-radius: 4px; padding: 5px; }
        QToolButton:hover { background: #d7e7ef; border-color: #8eb6c9; }
        QToolButton:pressed, QToolButton:checked { background: #c5e5f1; border-color: #3188a8; }
        QTabWidget#CadCommandRibbon::pane { background: #edf1f4; border: 1px solid #aebbc5; border-top: 2px solid #1684aa; }
        QScrollArea#RibbonScrollArea { background: #edf1f4; border: none; }
        QWidget#RibbonPage { background: #edf1f4; }
        QWidget#RibbonGroup { background: #ffffff; border: 1px solid #c2ccd3; border-radius: 5px; }
        QWidget#RibbonGroup QLabel { color: #536674; font-size: 10px; border: none; background: transparent; }
        QWidget#RibbonGroup QToolButton { background: transparent; color: #25343e; border: 1px solid transparent; padding: 4px; font-size: 11px; font-weight: 500; }
        QWidget#RibbonGroup QToolButton:hover { background: #e5f2f7; border-color: #8eb6c9; }
        QWidget#RibbonGroup QToolButton:pressed { background: #c5e5f1; border-color: #3188a8; }
        QWidget#RibbonGroup QToolButton::menu-indicator { image: none; }
        QTabBar::tab { background: #dce4e9; color: #3d505c; padding: 7px 15px; margin-right: 2px; border: 1px solid #b8c4cc; border-bottom: none; }
        QTabBar::tab:selected { background: #ffffff; color: #173b4d; border-top: 2px solid #1684aa; }
        QDockWidget { color: #f5f7f8; font-weight: 600; }
        QDockWidget::title { background: #40525e; padding: 7px 9px; border: 1px solid #34444f; }
        QTreeWidget { background: #ffffff; alternate-background-color: #f1f5f7; color: #24333d; border: 1px solid #c2ccd3; selection-background-color: #c6e6f2; selection-color: #163b4e; outline: none; }
        QHeaderView::section { background: #e3e9ed; color: #314752; padding: 5px; border: none; border-right: 1px solid #c2ccd3; border-bottom: 1px solid #c2ccd3; }
        QLabel { color: #26343d; }
        QLabel#WorkspaceLabel { color: #4c626f; font-size: 9px; font-weight: 700; padding-left: 4px; }
        QLineEdit, QComboBox { background: #ffffff; color: #26343d; border: 1px solid #9eacb6; border-radius: 4px; padding: 5px 8px; selection-background-color: #b8e0ef; }
        QLineEdit:focus, QComboBox:focus { border: 1px solid #1684aa; }
        QComboBox QAbstractItemView { background: #ffffff; color: #26343d; selection-background-color: #c6e6f2; }
        QStatusBar { background: #34434e; color: #f0f4f6; border-top: 1px solid #25323b; }
        QScrollBar:horizontal { background: #dce4e9; height: 10px; margin: 0; }
        QScrollBar::handle:horizontal { background: #8fa2ae; min-width: 28px; border-radius: 4px; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
    )QSS");
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
    if(part->isSurfaceBody){
        GProp_GProps area;BRepGProp::SurfaceProperties(part->shape,area);
        Bnd_Box surfaceBounds;BRepBndLib::AddOptimal(part->shape,surfaceBounds,false,false);
        double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;
        if(!surfaceBounds.IsVoid())surfaceBounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
        detail_->setText(QStringLiteral("%1 — Surface body\n\nSurface area: %2 mm²\nBounds X: %3 to %4 mm\nBounds Y: %5 to %6 mm\nBounds Z: %7 to %8 mm\nFaces: %9\nEdges: %10\nVisible: %11\nColor: %12\n\nID: %13")
            .arg(part->name).arg(area.Mass(),0,'f',3)
            .arg(xmin,0,'f',2).arg(xmax,0,'f',2).arg(ymin,0,'f',2).arg(ymax,0,'f',2)
            .arg(zmin,0,'f',2).arg(zmax,0,'f',2).arg(numberOf(part->shape,TopAbs_FACE))
            .arg(numberOf(part->shape,TopAbs_EDGE)).arg(part->visible?QStringLiteral("Yes"):QStringLiteral("No"))
            .arg(part->color,part->id));
        return;
    }
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
    if(part->hasLinkedBoolean){
        const QString status=document_.linkedBooleanCurrent(part->id)?"Current":"STALE";
        detail_->setText(detail_->text()+QStringLiteral("\n\nAssociative Boolean: %1  [%2]\nPrimary: %3\nTool: %4")
            .arg(part->linkedOperation,status,part->linkedPrimaryId,part->linkedToolId));
    }
    if(part->hasLinkedSketchExtrusion){
        const QString state=document_.linkedSketchExtrusionCurrent(part->id)?
            QStringLiteral("Current"):QStringLiteral("STALE");
        detail_->setText(detail_->text()+QStringLiteral(
            "\n\nAssociative sketch extrusion: %1\nSource body: %2\nDepth: %3 mm\nNormal offset: %4 mm")
            .arg(state,part->linkedSketchSourceId)
            .arg(part->linkedSketchDepthMm,0,'g',8)
            .arg(part->linkedSketchOffsetMm,0,'g',8));
    }
    if(part->hasShapeFeatureTree){
        const QString state=document_.shapeFeatureTreeCurrent(part->id)?
            QStringLiteral("Current"):QStringLiteral("STALE: direct geometry edit detected");
        detail_->setText(detail_->text()+QStringLiteral("\n\nParametric modifiers: %1\nHistory: %2")
            .arg(part->shapeFeatureNodes.size()).arg(state));
    }
}
void MainWindow::showEngineeringDesignReview() {
    // Read-only report: combines existing source geometry, part metadata,
    // BOM, material data and current project audit without acknowledging stale
    // drawing references or changing the document's undo/redo state.
    QString issue;
    const auto bill=document_.billOfMaterials(&issue);
    const auto audit=cad::runDocumentAudit(document_);
    QStringList lines;
    lines<<"CADWORKBENCH | ENGINEERING DESIGN REVIEW"
         <<"PRELIMINARY DESIGN DOCUMENT — NOT AN APPROVAL FOR MANUFACTURE"
         <<"All quantities in stated SI-derived units (mm, mm^3, kg)."
         <<"Model data can be incomplete; verify materials, loading and drawing dimensions independently."
         <<""
         <<QStringLiteral("Bodies: %1; audit errors: %2; warnings: %3; information: %4")
            .arg(audit.bodyCount).arg(audit.errorCount).arg(audit.warningCount).arg(audit.informationCount)
         <<""<<"GROUPED BILL OF MATERIALS";
    if(!issue.isEmpty())lines<<QStringLiteral("BOM diagnostic: ")+issue;
    for(const auto& row:bill){
        lines<<QStringLiteral("%1 | %2 | Qty %3 | %4 | Rev %5 | material %6 | estimated mass %7 kg | %8")
            .arg(row.itemNumber).arg(row.partNumber.isEmpty()?QStringLiteral("(unassigned)"):row.partNumber)
            .arg(row.quantity).arg(row.description).arg(row.revision)
            .arg(row.materialName.isEmpty()?QStringLiteral("(unknown)"):row.materialName)
            .arg(row.totalMassKg,0,'g',12).arg(row.assemblyPath);
        if(row.materialName.isEmpty()||row.totalMassKg<=0.0)
            lines<<QStringLiteral("  WARNING: material properties or mass may not be defined.");
    }
    lines<<""<<"PART AND FEATURE DEPENDENCIES";
    for(const auto& part:document_.parts()){
        lines<<QStringLiteral("%1 [%2] | %3 | %4")
            .arg(part.name,part.id,
                 part.isSurfaceBody?QStringLiteral("surface"):QStringLiteral("solid"),
                 part.visible?QStringLiteral("visible"):QStringLiteral("hidden"));
        if(part.hasShapeFeatureTree)
            lines<<QStringLiteral("  Feature stack: %1 nodes (%2)")
                .arg(part.shapeFeatureNodes.size())
                .arg(document_.shapeFeatureTreeCurrent(part.id)?QStringLiteral("current"):QStringLiteral("STALE"));
        if(part.hasLinkedBoolean)
            lines<<QStringLiteral("  Linked Boolean %1: %2 + %3 (%4)")
                .arg(part.linkedOperation,part.linkedPrimaryId,part.linkedToolId,
                    document_.linkedBooleanCurrent(part.id)?QStringLiteral("current"):QStringLiteral("STALE"));
        if(part.hasDrawingSettings)
            lines<<QStringLiteral("  Drawing settings saved; verify linked drawing source and revision before export.");
    }
    lines<<""<<"PROJECT READINESS FINDINGS"<<audit.toPlainText()
         <<""<<"FINAL DESIGN VERIFICATION REQUIRED"
         <<"Check applied loads and boundary conditions, fatigue, thermal loading, local design codes,"
         <<"material certification, manufacturing tolerance stack, assembly clearance and drawing signoff."
         <<"This summary is not a finite-element simulation, code-compliance assessment or certified drawing.";
    const QString report=lines.join('\n')+'\n';
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Engineering design review · %1 bodies · %2 audit issues")
        .arg(audit.bodyCount).arg(audit.errorCount+audit.warningCount));
    dialog.resize(980,670);
    auto* layout=new QVBoxLayout(&dialog);
    auto* description=new QLabel(QStringLiteral("Combines live BOM, material assignments, feature/link status and project audit. Read-only, with explicit uncertainty."),&dialog);
    description->setWordWrap(true);layout->addWidget(description);
    auto* text=new QPlainTextEdit(&dialog);text->setReadOnly(true);text->setPlainText(report);
    layout->addWidget(text,1);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);
    auto* exportButton=buttons->addButton(QStringLiteral("Export design review TXT..."),QDialogButtonBox::ActionRole);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(exportButton,&QPushButton::clicked,&dialog,[&]{
        const QString path=QFileDialog::getSaveFileName(&dialog,QStringLiteral("Export engineering design review"),
            QStringLiteral("cad-design-review.txt"),QStringLiteral("Text files (*.txt)"));
        if(path.isEmpty())return;
        QSaveFile file(path);
        const QByteArray bytes=report.toUtf8();
        if(!file.open(QIODevice::WriteOnly|QIODevice::Text)||file.write(bytes)!=bytes.size()||!file.commit())
            QMessageBox::warning(&dialog,QStringLiteral("Export failed"),QStringLiteral("Could not save the design review."));
    });
    layout->addWidget(buttons);
    dialog.exec();
}

void MainWindow::showGuidedEngineeringStudies() {
    EngineeringBodyContext context;
    const QStringList ids=selectedIds();
    if(ids.size()==1){
        const cad::Part* part=document_.find(ids.first());
        if(part&&!part->shape.IsNull()&&!part->isSurfaceBody){
            Bnd_Box box;
            BRepBndLib::AddOptimal(part->shape,box,false,false);
            if(!box.IsVoid()){
                double x0=0,y0=0,z0=0,x1=0,y1=0,z1=0;
                box.Get(x0,y0,z0,x1,y1,z1);
                context.name=part->name;
                context.xSizeMm=x1-x0;context.ySizeMm=y1-y0;context.zSizeMm=z1-z0;
                context.valid=std::isfinite(context.xSizeMm)&&std::isfinite(context.ySizeMm)&&
                    std::isfinite(context.zSizeMm)&&context.xSizeMm>0&&context.ySizeMm>0&&context.zSizeMm>0;
            }
        }
    }
    GuidedStudyDialog dialog(context,this);
    dialog.exec();
}

void MainWindow::showWorkshopPlanning() {
    const QStringList ids=selectedIds();
    if(ids.size()>1){
        reportError("Manufacturing workshop","Select at most one CAD body to inspect. Calculators work without selection.");
        return;
    }
    const cad::Part* body=ids.size()==1?document_.find(ids.first()):nullptr;
    WorkshopPlanningDialog dialog(body,this);
    dialog.exec();
}

void MainWindow::showEngineeringWorkbench() {
    EngineeringBodyContext context;
    const QStringList ids=selectedIds();
    if(ids.size()==1){
        const cad::Part* part=document_.find(ids.first());
        if(part&&!part->shape.IsNull()){
            Bnd_Box box;
            BRepBndLib::AddOptimal(part->shape,box,false,false);
            if(!box.IsVoid()){
                double x0=0,y0=0,z0=0,x1=0,y1=0,z1=0;
                box.Get(x0,y0,z0,x1,y1,z1);
                GProp_GProps properties;
                BRepGProp::VolumeProperties(part->shape,properties);
                context.name=part->name;
                context.xSizeMm=x1-x0;
                context.ySizeMm=y1-y0;
                context.zSizeMm=z1-z0;
                context.volumeMm3=properties.Mass();
                context.assignedDensityKgPerM3=part->materialDensityKgPerM3;
                context.valid=std::isfinite(context.xSizeMm)&&std::isfinite(context.ySizeMm)&&
                    std::isfinite(context.zSizeMm)&&std::isfinite(context.volumeMm3)&&
                    context.xSizeMm>0&&context.ySizeMm>0&&context.zSizeMm>0;
            }
        }
    }
    EngineeringWorkbenchDialog dialog(context,this);
    dialog.exec();
}

void MainWindow::showMassProperties() {
    if(!requireBodyMode("Measure body"))return;
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Mass properties","Select exactly one solid body.");return;}
    const cad::Part* part=document_.find(ids.first());
    if(!part)return;
    bool accepted=false;
    const double defaultDensity=part->materialDensityKgPerM3>0.0?part->materialDensityKgPerM3:7850.0;
    const double density=QInputDialog::getDouble(this,"Mass properties","Material density (kg/m^3)",
                                                 defaultDensity,0.00001,1000000.0,3,&accepted);
    if(!accepted)return;
    cad::tensor::Report mass;QString error;
    if(!document_.measureMassTensor(part->id,density,&mass,&error)){
        reportError("Mass properties",error);return;
    }
    GProp_GProps surface;
    BRepGProp::SurfaceProperties(part->shape,surface);
    Bnd_Box bounds;
    BRepBndLib::AddOptimal(part->shape,bounds,false,false);
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;
    if(!bounds.IsVoid())bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    QStringList lines;
    lines<<QStringLiteral("CADWorkbench | Engineering Mass & Inertia Report")
         <<QStringLiteral("Body: %1").arg(part->name)
         <<QStringLiteral("Material: %1").arg(part->materialName.isEmpty()?QStringLiteral("Unassigned; entered density used"):
                                              part->materialName)
         <<QStringLiteral("Density: %1 kg/m^3").arg(mass.densityKgPerM3,0,'g',12)
         <<QStringLiteral("Bounds XYZ: %1 x %2 x %3 mm")
               .arg(xmax-xmin,0,'g',12).arg(ymax-ymin,0,'g',12).arg(zmax-zmin,0,'g',12)
         <<QStringLiteral("Volume: %1 mm^3").arg(mass.volumeMm3,0,'g',12)
         <<QStringLiteral("Surface area: %1 mm^2").arg(surface.Mass(),0,'g',12)
         <<QStringLiteral("Mass: %1 kg").arg(mass.massKg,0,'g',12)
         <<QStringLiteral("Center of mass XYZ (mm): %1, %2, %3")
               .arg(mass.centerMm[0],0,'g',12).arg(mass.centerMm[1],0,'g',12).arg(mass.centerMm[2],0,'g',12)
         <<QStringLiteral("")
         <<QStringLiteral("CENTROIDAL INERTIA TENSOR (kg*mm^2), global XYZ axes");
    for(int row=0;row<3;++row)
        lines<<QStringLiteral("[%1   %2   %3]")
            .arg(mass.inertiaKgMm2[3*row],0,'g',12)
            .arg(mass.inertiaKgMm2[3*row+1],0,'g',12)
            .arg(mass.inertiaKgMm2[3*row+2],0,'g',12);
    lines<<QStringLiteral("")<<QStringLiteral("PRINCIPAL MOMENTS (ascending)");
    for(int index=0;index<3;++index){
        const auto& direction=mass.principalAxes[index];
        lines<<QStringLiteral("I%1 = %2 kg*mm^2  (%3 kg*m^2); radius = %4 mm")
            .arg(index+1)
            .arg(mass.principalMomentsKgMm2[index],0,'g',12)
            .arg(mass.principalMomentsKgMm2[index]*1e-6,0,'g',12)
            .arg(mass.radiiOfGyrationMm[index],0,'g',12)
            <<QStringLiteral("    Axis XYZ: (%1, %2, %3)")
              .arg(direction[0],0,'g',10).arg(direction[1],0,'g',10).arg(direction[2],0,'g',10);
    }
    if(!mass.principalAxesUnique)
        lines<<QStringLiteral("NOTE: Repeated principal moments: the corresponding principal directions are not unique.");
    lines<<QStringLiteral("")
         <<QStringLiteral("Assumptions: homogeneous solid; uniform entered density; no voids beyond modeled geometry.")
         <<QStringLiteral("Inertia is about the center of mass. Engineering review required before load design.");
    const QString report=lines.join(QLatin1Char('\n'));
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Mass, center of gravity and principal inertia — %1").arg(part->name));
    dialog.resize(760,660);
    auto* layout=new QVBoxLayout(&dialog);
    auto* editor=new QPlainTextEdit(&dialog);
    editor->setReadOnly(true);
    editor->setPlainText(report);
    layout->addWidget(editor);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);
    auto* exportButton=buttons->addButton(QStringLiteral("Export report..."),QDialogButtonBox::ActionRole);
    connect(exportButton,&QPushButton::clicked,&dialog,[&dialog,report]{
        const QString fileName=QFileDialog::getSaveFileName(&dialog,QStringLiteral("Export mass and inertia report"),
                                                           QStringLiteral("mass_inertia_report.txt"),QStringLiteral("Text report (*.txt)"));
        if(fileName.isEmpty())return;
        QSaveFile output(fileName);
        if(!output.open(QIODevice::WriteOnly|QIODevice::Text)||
           output.write(report.toUtf8())<0||!output.commit()){
            QMessageBox::warning(&dialog,QStringLiteral("Export failed"),
                                 QStringLiteral("Could not write the report to the selected file."));
        }
    });
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}

void MainWindow::showAssemblyMassProperties() {
    if(!requireBodyMode("Assembly mass properties"))return;
    QStringList ids=selectedIds();
    const bool selected=!ids.isEmpty();
    if(!selected){
        // Linked Boolean outputs are construction results, not extra physical components.
        for(const cad::Part& part:document_.parts())
            if(part.visible&&!part.isSurfaceBody&&!part.hasLinkedBoolean&&!part.shape.IsNull())ids<<part.id;
    }
    if(ids.isEmpty()){
        reportError("Assembly mass properties","Select physical solid bodies or make the components visible.");return;
    }
    cad::tensor::Report tensor;QString error;
    if(!document_.measureAssemblyMassTensor(ids,&tensor,&error)){
        reportError("Assembly mass properties",error);return;
    }
    QStringList lines;
    lines<<QStringLiteral("CADWorkbench | Assembly Mass / Center of Gravity / Inertia")
         <<QStringLiteral("Scope: %1").arg(selected?QStringLiteral("Selected bodies"):QStringLiteral("Visible non-derived solids"))
         <<QStringLiteral("Physical components counted: %1").arg(ids.size())
         <<QStringLiteral("Combined material mass: %1 kg").arg(tensor.massKg,0,'g',12)
         <<QStringLiteral("Summed modeled volumes: %1 mm^3").arg(tensor.volumeMm3,0,'g',12)
         <<QStringLiteral("Center of gravity XYZ: (%1, %2, %3) mm")
            .arg(tensor.centerMm[0],0,'g',12).arg(tensor.centerMm[1],0,'g',12).arg(tensor.centerMm[2],0,'g',12)
         <<QStringLiteral("")<<QStringLiteral("COMPONENTS (each uses its assigned material density)");
    for(const QString& id:ids){
        const cad::Part* part=document_.find(id);
        if(part)lines<<QStringLiteral("%1 | %2 | %3 kg/m^3")
                     .arg(part->name,part->materialName.isEmpty()?QStringLiteral("Unnamed material"):part->materialName)
                     .arg(part->materialDensityKgPerM3,0,'g',12);
    }
    lines<<QStringLiteral("")<<QStringLiteral("COMBINED INERTIA TENSOR, centroidal (kg*mm^2)");
    for(int row=0;row<3;++row)
        lines<<QStringLiteral("[%1  %2  %3]")
              .arg(tensor.inertiaKgMm2[3*row],0,'g',12)
              .arg(tensor.inertiaKgMm2[3*row+1],0,'g',12)
              .arg(tensor.inertiaKgMm2[3*row+2],0,'g',12);
    lines<<QStringLiteral("")<<QStringLiteral("PRINCIPAL MOMENTS AND DIRECTIONS");
    for(int axis=0;axis<3;++axis){
        const auto& direction=tensor.principalAxes[axis];
        lines<<QStringLiteral("I%1: %2 kg*mm^2 / %3 kg*m^2; gyration radius %4 mm")
             .arg(axis+1).arg(tensor.principalMomentsKgMm2[axis],0,'g',12)
             .arg(tensor.principalMomentsKgMm2[axis]*1e-6,0,'g',12)
             .arg(tensor.radiiOfGyrationMm[axis],0,'g',12)
             <<QStringLiteral("  Direction XYZ: (%1, %2, %3)")
               .arg(direction[0],0,'g',10).arg(direction[1],0,'g',10).arg(direction[2],0,'g',10);
    }
    if(!tensor.principalAxesUnique)
        lines<<QStringLiteral("NOTE: Principal directions are not unique when moments are repeated.");
    lines<<QStringLiteral("")
         <<QStringLiteral("Each selected component is counted once using its modeled global placement.")
         <<QStringLiteral("Overlapping bodies count their mass twice; choose physical components, not construction results.")
         <<QStringLiteral("This is a rigid-assembly, homogeneous-per-component calculation, not a dynamics simulation.");
    const QString report=lines.join(QLatin1Char('\n'));
    QDialog dialog(this);dialog.setWindowTitle(QStringLiteral("Assembly center of gravity and principal inertia"));
    dialog.resize(800,680);
    auto* layout=new QVBoxLayout(&dialog);
    auto* viewer=new QPlainTextEdit(&dialog);
    viewer->setReadOnly(true);viewer->setPlainText(report);layout->addWidget(viewer);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);
    auto* exportButton=buttons->addButton(QStringLiteral("Export report..."),QDialogButtonBox::ActionRole);
    connect(exportButton,&QPushButton::clicked,&dialog,[&dialog,report]{
        const QString path=QFileDialog::getSaveFileName(&dialog,QStringLiteral("Export assembly inertia report"),
                             QStringLiteral("assembly_inertia_report.txt"),QStringLiteral("Text report (*.txt)"));
        if(path.isEmpty())return;
        QSaveFile output(path);
        if(!output.open(QIODevice::WriteOnly|QIODevice::Text)||output.write(report.toUtf8())<0||!output.commit())
            QMessageBox::warning(&dialog,QStringLiteral("Export failed"),QStringLiteral("Could not save the report."));
    });
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
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
void MainWindow::showAssemblyFitAudit() {
    if(!requireBodyMode("Assembly fit audit"))return;
    QStringList ids=selectedIds();
    // A deliberate multi-selection is honored; an empty selection scans the scene.
    if(ids.isEmpty()){
        for(const auto& part:document_.parts())if(part.visible&&!part.isSurfaceBody)ids.append(part.id);
    }
    if(ids.size()<2||ids.size()>32){
        reportError("Assembly fit audit","Select 2–32 solid bodies, or clear the selection to scan all visible solids (max 32).");return;
    }
    QDialog settings(this);
    settings.setWindowTitle("Assembly clearance and manufacturing deviation bounds");
    settings.resize(660,480);
    auto* settingsLayout=new QVBoxLayout(&settings);
    auto* guidance=new QLabel(
        "Specify the required functional gap and a maximum outward geometric deviation "
        "for EACH solid. Results use a conservative worst-case clearance bound, not a "
        "statistical tolerance stack. Enter 0 only when checking nominal geometry.",&settings);
    guidance->setWordWrap(true);settingsLayout->addWidget(guidance);
    auto* gapRow=new QHBoxLayout();
    gapRow->addWidget(new QLabel("Minimum required operating gap (mm):",&settings));
    auto* clearance=new QDoubleSpinBox(&settings);
    clearance->setRange(0.0,1000000.0);clearance->setDecimals(4);
    clearance->setValue(1.0);clearance->setSuffix(" mm");
    gapRow->addWidget(clearance);settingsLayout->addLayout(gapRow);
    auto* table=new QTableWidget(static_cast<int>(ids.size()),2,&settings);
    table->setHorizontalHeaderLabels({"Solid body", "Max outward deviation (mm)"});
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    QVector<QDoubleSpinBox*> deviationInputs;
    deviationInputs.reserve(ids.size());
    for(int index=0;index<static_cast<int>(ids.size());++index){
        const cad::Part* part=document_.find(ids[index]);
        auto* label=new QTableWidgetItem(part?part->name:ids[index]);
        label->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
        table->setItem(index,0,label);
        auto* bound=new QDoubleSpinBox(table);
        bound->setDecimals(4);bound->setRange(0.0,1000000.0);
        bound->setValue(0.0);bound->setSuffix(" mm");
        table->setCellWidget(index,1,bound);
        deviationInputs.append(bound);
    }
    settingsLayout->addWidget(table);
    auto* settingsButtons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&settings);
    connect(settingsButtons,&QDialogButtonBox::accepted,&settings,&QDialog::accept);
    connect(settingsButtons,&QDialogButtonBox::rejected,&settings,&QDialog::reject);
    settingsLayout->addWidget(settingsButtons);
    if(settings.exec()!=QDialog::Accepted)return;
    const double required=clearance->value();
    QVector<double> deviations;deviations.reserve(deviationInputs.size());
    for(const auto* input:deviationInputs)deviations.append(input->value());
    QVector<cad::AssemblyFitPair> pairs;
    QString error;
    if(!cad::inspectAssemblyFitWithTolerances(document_,ids,required,deviations,&pairs,&error)){
        reportError("Assembly fit audit",error);return;
    }
    const QString report=cad::assemblyFitAuditText(pairs,required);
    const QString csv=cad::assemblyFitAuditCsv(pairs,required);
    QDialog dialog(this);dialog.setWindowTitle("Assembly fit and clearance audit");dialog.resize(800,570);
    auto* layout=new QVBoxLayout(&dialog);
    auto* description=new QLabel("Read-only static B-rep measurements. Contact and true solid overlap are reported separately.",&dialog);
    description->setWordWrap(true);layout->addWidget(description);
    auto* display=new QPlainTextEdit(&dialog);display->setReadOnly(true);display->setPlainText(report);
    layout->addWidget(display);
    auto* controls=new QHBoxLayout();
    auto* copy=new QPushButton("Copy report",&dialog);
    auto* exportCsv=new QPushButton("Export CSV...",&dialog);
    auto* close=new QPushButton("Close",&dialog);
    controls->addWidget(copy);controls->addWidget(exportCsv);controls->addStretch();controls->addWidget(close);
    layout->addLayout(controls);
    connect(copy,&QPushButton::clicked,&dialog,[report]{QApplication::clipboard()->setText(report);});
    connect(exportCsv,&QPushButton::clicked,&dialog,[&dialog,csv]{
        const QString filename=QFileDialog::getSaveFileName(&dialog,"Save assembly fit audit",QStringLiteral("assembly_fit_audit.csv"),"CSV files (*.csv)");
        if(filename.isEmpty())return;
        QSaveFile destination(filename);
        if(!destination.open(QIODevice::WriteOnly|QIODevice::Text)){
            QMessageBox::warning(&dialog,"Export failed",destination.errorString());return;
        }
        const QByteArray bytes=csv.toUtf8();
        if(destination.write(bytes)!=bytes.size()||!destination.commit()){
            QMessageBox::warning(&dialog,"Export failed",destination.errorString());
        }
    });
    connect(close,&QPushButton::clicked,&dialog,&QDialog::accept);
    dialog.exec();
}

void MainWindow::rebuildAssemblyNavigator() {
    if(!assemblyTree_)return;
    const QSignalBlocker blocker(assemblyTree_);
    assemblyTree_->clear();
    QMap<QString,QTreeWidgetItem*> folders;
    auto* ungrouped=new QTreeWidgetItem(assemblyTree_,{"Ungrouped bodies"});
    ungrouped->setFlags(ungrouped->flags()&~Qt::ItemIsSelectable);
    folders.insert(QString(),ungrouped);
    for(const auto& part:document_.parts()){
        QString path=part.assemblyPath.trimmed();
        QString normalized;
        QTreeWidgetItem* parent=ungrouped;
        for(const QString& segment:path.split('/',Qt::SkipEmptyParts)){
            const QString partName=segment.trimmed();
            if(partName.isEmpty())continue;
            normalized=normalized.isEmpty()?partName:normalized+"/"+partName;
            if(!folders.contains(normalized)){
                auto* folder=new QTreeWidgetItem(parent,{partName});
                folder->setFlags(folder->flags()&~Qt::ItemIsSelectable);
                folder->setToolTip(0,normalized);
                folder->setExpanded(true);
                folders.insert(normalized,folder);
            }
            parent=folders.value(normalized);
        }
        const QString caption=part.componentName.isEmpty()?part.name:
            QStringLiteral("%1 · %2").arg(part.componentName,part.name);
        auto* body=new QTreeWidgetItem(parent,{caption});
        body->setData(0,Qt::UserRole,part.id);
        body->setToolTip(0,QStringLiteral("Body ID: %1\nAssembly: %2").arg(part.id,path));
        if(!part.visible)body->setForeground(0,QColor(139,151,160));
    }
    if(ungrouped->childCount()==0)delete ungrouped;
    syncAssemblyNavigatorSelection(selectedIds());
}

void MainWindow::syncAssemblyNavigatorSelection(const QStringList& ids) {
    if(!assemblyTree_)return;
    const QSignalBlocker blocker(assemblyTree_);
    for(QTreeWidgetItemIterator it(assemblyTree_);*it;++it){
        auto* item=*it;
        const QString id=item->data(0,Qt::UserRole).toString();
        if(!id.isEmpty())item->setSelected(ids.contains(id));
    }
}

void MainWindow::applyBrowserFilter() {
    if(!tree_||!browserFilter_)return;
    const QString text=browserFilter_->text().trimmed();
    const auto query=text.toUtf8().toStdString();
    const auto& parts=document_.parts();
    int visibleRows=0;
    const int count=std::min(tree_->topLevelItemCount(),static_cast<int>(parts.size()));
    for(int i=0;i<count;++i){
        const auto& part=parts[i];
        auto* item=tree_->topLevelItem(i);
        QStringList features;
        for(const auto& node:part.shapeFeatureNodes)features.append(node.type);
        for(int j=0;j<item->childCount();++j)features.append(item->child(j)->text(0));
        const bool stale=(part.hasShapeFeatureTree&&!document_.shapeFeatureTreeCurrent(part.id))||
            (part.hasLinkedSketchExtrusion&&!document_.linkedSketchExtrusionCurrent(part.id))||
            (part.hasLinkedBoolean&&!document_.linkedBooleanCurrent(part.id));
        const cad::browserquery::BodyIndex index={
            part.name.toUtf8().toStdString(),part.partNumber.toUtf8().toStdString(),
            part.materialName.toUtf8().toStdString(),part.id.toUtf8().toStdString(),
            features.join(' ').toUtf8().toStdString(),part.visible,part.isSurfaceBody,stale};
        const bool hit=cad::browserquery::matches(index,query);
        item->setHidden(!hit);
        if(hit)++visibleRows;
        for(int j=0;j<item->childCount();++j)item->child(j)->setHidden(false);
        if(hit&&!text.isEmpty())item->setExpanded(true);
    }
    if(browserMatchSummary_)
        browserMatchSummary_->setText(QStringLiteral("Showing %1 of %2 bodies%3")
           .arg(visibleRows).arg(parts.size())
           .arg(text.isEmpty()?QString():QStringLiteral(" · search is view-only")));
}

void MainWindow::refresh(bool fit) {
    const auto ids = selectedIds();
    {
        const QSignalBlocker blocker(tree_);
        tree_->clear();
        for (const auto& part : document_.parts()) {
            const QString label=part.isSurfaceBody?QStringLiteral("%1 — Surface").arg(part.name):
                part.componentDefinitionId.isEmpty()?part.name:
                QStringLiteral("%1 — occurrence %2").arg(part.componentName,part.occurrenceId.left(8));
            auto* item = new QTreeWidgetItem(tree_,{label,QString()});
            item->setData(0,Qt::UserRole,part.id);
            if(!part.componentDefinitionId.isEmpty()){
                QString tooltip=QStringLiteral("Component: %1\nDefinition ID: %2\nOccurrence ID: %3\nPlacement origin: (%4, %5, %6) mm")
                    .arg(part.componentName,part.componentDefinitionId,part.occurrenceId)
                    .arg(part.placementX,0,'f',3).arg(part.placementY,0,'f',3).arg(part.placementZ,0,'f',3);
                tooltip+=QStringLiteral("\nOrientation R/P/Y: %1° / %2° / %3°")
                    .arg(part.placementRollDegrees,0,'f',2).arg(part.placementPitchDegrees,0,'f',2)
                    .arg(part.placementYawDegrees,0,'f',2);
                if(!part.assemblyJointType.isEmpty())tooltip+=QStringLiteral("\n%1 joint parent body: %2").arg(part.assemblyJointType,part.assemblyJointParentBodyId);
                if(part.assemblyJointType=="Revolute")tooltip+=QStringLiteral("\nAngle: %1° (limits %2° to %3°)")
                    .arg(part.jointAngleDegrees,0,'f',3).arg(part.jointMinimumDegrees,0,'f',3).arg(part.jointMaximumDegrees,0,'f',3);
                item->setToolTip(0,tooltip);
            }
            if(part.hasLinkedBoolean){
                auto* source=new QTreeWidgetItem(item,{
                    QStringLiteral("%1 from %2 / %3").arg(part.linkedOperation,
                        part.linkedPrimaryId.left(8),part.linkedToolId.left(8)),QString()});
                source->setFlags(source->flags()&~Qt::ItemIsSelectable);
                if(!document_.linkedBooleanCurrent(part.id))item->setText(0,"⚠ "+label+" [stale linked body]");
            }
            if(part.hasLinkedSketchExtrusion){
                auto* source=new QTreeWidgetItem(item,{
                    QStringLiteral("External sketch from %1 · depth %2 mm · offset %3 mm")
                        .arg(part.linkedSketchSourceId.left(8))
                        .arg(part.linkedSketchDepthMm,0,'g',8)
                        .arg(part.linkedSketchOffsetMm,0,'g',8),QString()});
                source->setFlags(source->flags()&~Qt::ItemIsSelectable);
                if(!document_.linkedSketchExtrusionCurrent(part.id))
                    item->setText(0,QStringLiteral("⚠ %1 [stale external sketch]").arg(label));
            }
            if(part.hasShapeFeatureTree){
                auto* base=new QTreeWidgetItem(item,{"Frozen B-rep source",QString()});
                base->setFlags(base->flags()&~Qt::ItemIsSelectable);
                if(!document_.shapeFeatureTreeCurrent(part.id)){
                    item->setText(0,QStringLiteral("⚠ %1 [stale history]").arg(label));
                    item->setToolTip(0,"Geometry changed outside its saved feature history. Rebase or undo the edit before regenerating.");
                }
                for(int i=0;i<part.shapeFeatureNodes.size();++i){
                    const auto& node=part.shapeFeatureNodes[i];
                    QStringList dimensions;
                    for(auto it=node.parameters.cbegin();it!=node.parameters.cend();++it)
                        dimensions.append(QStringLiteral("%1=%2").arg(it.key()).arg(it.value(),0,'g',6));
                    auto* child=new QTreeWidgetItem(item,{
                        QStringLiteral("%1. %2%3  [%4]").arg(i+1).arg(node.type,
                          node.suppressed?QStringLiteral(" (suppressed)"):QString(),dimensions.join(", ")),QString()});
                    child->setFlags(child->flags()&~Qt::ItemIsSelectable);
                    child->setToolTip(0,QStringLiteral("Stable feature ID: %1\nInputs: %2")
                        .arg(node.id,node.inputs.isEmpty()?QStringLiteral("previous output (sequential)"):
                                                  node.inputs.join(", ")));
                }
                item->setExpanded(part.shapeFeatureNodes.size()<=8);
            }
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(1,part.visible ? Qt::Checked : Qt::Unchecked);
            item->setSelected(ids.contains(part.id));
        }
    }
    applyBrowserFilter();
    rebuildAssemblyNavigator();
    viewport_->updateScene(document_.parts(),fit);
    viewport_->selectIds(ids);
    updateInspector(ids);
    updateContextActions();
    updateFeatureTimeline();
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
        command.sketchPlane=static_cast<cad::ExtrusionSketchPlane>(sketchPlane->currentIndex());
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
void MainWindow::createConstrainedTrapezoidExtrusion() {
    QDialog dialog(this);dialog.setWindowTitle("Constrained trapezoid extrusion");auto* form=new QFormLayout(&dialog);
    auto field=[&](const QString& label,double initial,double minimum=0.00001){
        auto* box=new QDoubleSpinBox(&dialog);box->setRange(minimum,1000000);box->setDecimals(3);
        box->setValue(initial);box->setSuffix(" mm");form->addRow(label,box);return box;
    };
    auto* bottom=field("Bottom width",50);auto* top=field("Top width",30);
    auto* height=field("Profile height",25);auto* offset=field("Top left X offset",10,-1000000);
    auto* depth=field("Extrusion depth",10);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    cad::SketchModel sketch;
    sketch.points={{"a",0,0},{"b",bottom->value(),0},
                   {"c",offset->value()+top->value(),height->value()},{"d",offset->value(),height->value()}};
    sketch.segments={{"bottom","a","b"},{"top","d","c"}};
    auto fixed=[&](const QString& point,double x,double y){cad::SketchConstraint c;c.type=cad::SketchConstraintType::FixedPoint;c.pointA=point;c.fixedX=x;c.fixedY=y;return c;};
    auto horizontal=[&](const QString& segment){cad::SketchConstraint c;c.type=cad::SketchConstraintType::Horizontal;c.segmentA=segment;return c;};
    auto distance=[&](const QString& a,const QString& b,double value){cad::SketchConstraint c;c.type=cad::SketchConstraintType::Distance;c.pointA=a;c.pointB=b;c.value=value;return c;};
    sketch.constraints={fixed("a",0,0),fixed("d",offset->value(),height->value()),
                        horizontal("bottom"),horizontal("top"),distance("a","b",bottom->value()),
                        distance("d","c",top->value())};
    QString error;
    if(!document_.addConstrainedPolygonExtrusion(sketch,{"a","b","c","d"},depth->value(),&error)){
        reportError("Constrained trapezoid",error);return;
    }
    refresh(true);
}
void MainWindow::createConstrainedTriangleExtrusion() {
    QDialog dialog(this);dialog.setWindowTitle("Constrained triangle from side lengths");auto* form=new QFormLayout(&dialog);
    auto field=[&](const QString& label,double initial){auto* box=new QDoubleSpinBox(&dialog);
        box->setRange(0.00001,1000000);box->setDecimals(3);box->setValue(initial);box->setSuffix(" mm");form->addRow(label,box);return box;};
    auto* base=field("Base A–B",40);auto* left=field("Side A–C",30);
    auto* right=field("Side B–C",35);auto* depth=field("Extrusion depth",10);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    const double ab=base->value(),ac=left->value(),bc=right->value();
    if(ab+ac<=bc||ab+bc<=ac||ac+bc<=ab){reportError("Constrained triangle","The three lengths do not form a triangle.");return;}
    const double cx=(ab*ab+ac*ac-bc*bc)/(2*ab);
    const double cy=std::sqrt(std::max(0.0,ac*ac-cx*cx));
    cad::SketchModel sketch;sketch.points={{"a",0,0},{"b",ab,0},{"c",cx,cy}};
    sketch.segments={{"base","a","b"},{"left","a","c"},{"right","b","c"}};
    cad::SketchConstraint anchor;anchor.type=cad::SketchConstraintType::FixedPoint;anchor.pointA="a";
    cad::SketchConstraint horizontal;horizontal.type=cad::SketchConstraintType::Horizontal;horizontal.segmentA="base";
    auto distance=[&](const QString& a,const QString& b,double value){cad::SketchConstraint c;c.type=cad::SketchConstraintType::Distance;c.pointA=a;c.pointB=b;c.value=value;return c;};
    sketch.constraints={anchor,horizontal,distance("a","b",ab),distance("a","c",ac),distance("b","c",bc)};
    QString error;
    if(!document_.addConstrainedPolygonExtrusion(sketch,{"a","b","c"},depth->value(),&error)){
        reportError("Constrained triangle",error);return;
    }
    refresh(true);
}
void MainWindow::createInteractiveSketch(){
    SketchCanvasDialog dialog(this);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.addConstrainedPolygonExtrusion(dialog.sketch(),dialog.boundary(),dialog.depth(),&error,dialog.plane())){
        reportError("Interactive sketch extrusion",error);return;
    }
    refresh(true);
    statusBar()->showMessage("Created a plane-aware editable sketch extrusion.",8000);
}
void MainWindow::createSketchOnSelectedFace(){
    const auto selection=viewport_->selectedFaceReferences();
    if(selection.size()!=1){
        reportError("Sketch on face","Switch to Select > Faces, then click one planar face in the viewport.");return;
    }
    const auto faceShape=selectedSubshape(document_,selection.first().partId,
        selection.first().geometryToken,selection.first().faceIndex,TopAbs_FACE);
    if(faceShape.IsNull()){
        reportError("Sketch on face","Selected face is stale. Select it again.");return;
    }
    const TopoDS_Face face=TopoDS::Face(faceShape);
    const BRepAdaptor_Surface surface(face);
    if(surface.GetType()!=GeomAbs_Plane){
        reportError("Sketch on face","Only planar faces are supported as sketch planes.");return;
    }
    cad::SketchPlane plane;
    GProp_GProps area;
    BRepGProp::SurfaceProperties(face,area);
    const gp_Pnt origin=area.CentreOfMass();
    gp_Vec normal(surface.Plane().Axis().Direction());
    const cad::Part* part=document_.find(selection.first().partId);
    if(part&&!part->isSurfaceBody){
        GProp_GProps solid;BRepGProp::VolumeProperties(part->shape,solid);
        if(normal.Dot(gp_Vec(solid.CentreOfMass(),origin))<0)normal.Reverse();
    }
    plane.originX=origin.X();plane.originY=origin.Y();plane.originZ=origin.Z();
    plane.normalX=normal.X();plane.normalY=normal.Y();plane.normalZ=normal.Z();
    SketchCanvasDialog dialog(this);dialog.setPlane(plane);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.addConstrainedPolygonExtrusion(dialog.sketch(),dialog.boundary(),dialog.depth(),&error,dialog.plane())){
        reportError("Sketch on face",error);return;
    }
    refresh(true);
    statusBar()->showMessage("Created a new body on the selected planar face. It is not yet fused with its source.",9000);
}
namespace {
bool linkedSketchDimensionsDialog(QWidget* parent,double* depth,double* offset){
    if(!depth||!offset)return false;
    QDialog dialog(parent);
    dialog.setWindowTitle("Linked sketch extrusion — parameters");
    auto* form=new QFormLayout(&dialog);
    auto* explanation=new QLabel(
        "Creates a separate associative solid using the selected body's saved constrained sketch. "
        "An upstream sketch edit updates this solid and any linked Boolean descendants. "
        "Offset is measured along the source sketch-plane normal.",&dialog);
    explanation->setWordWrap(true);form->addRow(explanation);
    auto* depthField=new QDoubleSpinBox(&dialog);
    depthField->setDecimals(4);depthField->setRange(0.0001,1000000.0);
    depthField->setSuffix(" mm");depthField->setValue(*depth);
    form->addRow("Extrusion depth",depthField);
    auto* offsetField=new QDoubleSpinBox(&dialog);
    offsetField->setDecimals(4);offsetField->setRange(-1000000.0,1000000.0);
    offsetField->setSuffix(" mm");offsetField->setValue(*offset);
    form->addRow("Normal offset",offsetField);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    form->addRow(buttons);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return false;
    *depth=depthField->value();*offset=offsetField->value();return true;
}
}
void MainWindow::createLinkedSketchExtrusion(){
    if(!requireBodyMode("Linked sketch extrusion"))return;
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Linked sketch extrusion","Select exactly one source body.");return;}
    const cad::Part* source=document_.find(ids.first());
    if(!source||!source->hasConstrainedPolygonDefinition||
       source->constrainedPolygonGeometryToken!=source->geometryToken){
        reportError("Linked sketch extrusion","Select a body with a current editable constrained sketch.");return;
    }
    double depth=source->constrainedPolygonDepthMm,offset=0.0;
    if(!linkedSketchDimensionsDialog(this,&depth,&offset))return;
    QString error;
    if(!document_.addLinkedSketchExtrusion(ids.first(),depth,offset,&error)){
        reportError("Linked sketch extrusion",error);return;
    }
    refresh(true);
    statusBar()->showMessage("Created a live sketch-derived solid. Source sketch edits rebuild the dependent geometry.",9000);
}
void MainWindow::editLinkedSketchExtrusion(){
    if(!requireBodyMode("Linked sketch extrusion"))return;
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Linked sketch extrusion","Select exactly one linked sketch extrusion.");return;}
    const cad::Part* derived=document_.find(ids.first());
    if(!derived||!derived->hasLinkedSketchExtrusion){
        reportError("Linked sketch extrusion","Select a body created from an external constrained sketch.");return;
    }
    // Choose a new sketch source by stable body ID. Retargeting and parameter
    // changes are validated and committed as one dependency-graph transaction.
    QDialog dialog(this);dialog.setWindowTitle("Edit associative sketch source and dimensions");
    auto* form=new QFormLayout(&dialog);
    auto* sourceField=new QComboBox(&dialog);
    for(const cad::Part& part:document_.parts()){
        if(part.hasConstrainedPolygonDefinition&&!part.isSurfaceBody&&
           part.constrainedPolygonGeometryToken==part.geometryToken)
            sourceField->addItem(part.name+QStringLiteral(" [")+part.id.left(8)+QStringLiteral("]"),part.id);
    }
    if(sourceField->count()==0){reportError("Linked sketch extrusion","No current source sketches exist.");return;}
    const int oldSource=sourceField->findData(derived->linkedSketchSourceId);
    if(oldSource>=0)sourceField->setCurrentIndex(oldSource);
    form->addRow("Constrained sketch source",sourceField);
    auto* depthField=new QDoubleSpinBox(&dialog);
    depthField->setDecimals(4);depthField->setRange(0.0001,1000000.0);
    depthField->setSuffix(" mm");depthField->setValue(derived->linkedSketchDepthMm);
    form->addRow("Extrusion depth",depthField);
    auto* offsetField=new QDoubleSpinBox(&dialog);
    offsetField->setDecimals(4);offsetField->setRange(-1000000.0,1000000.0);
    offsetField->setSuffix(" mm");offsetField->setValue(derived->linkedSketchOffsetMm);
    form->addRow("Normal offset",offsetField);
    QStringList affected;QString previewError;
    if(document_.linkedBodyImpact(ids.first(),&affected,&previewError)){
        auto* impact=new QLabel(QStringLiteral("This edit will update up to %1 linked bodies, including this one.")
                                  .arg(affected.size()),&dialog);
        impact->setWordWrap(true);form->addRow(impact);
    }
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.retargetLinkedSketchExtrusion(ids.first(),sourceField->currentData().toString(),
                                                 depthField->value(),offsetField->value(),&error)){
        reportError("Linked sketch extrusion",error);return;
    }
    refresh(true);
    statusBar()->showMessage("Retargeted linked sketch and rebuilt its Boolean descendants atomically.",9000);
}

void MainWindow::createLinkedSketchPocket(){
    if(!requireBodyMode("Associative sketch pocket"))return;
    QDialog dialog(this);dialog.setWindowTitle("Associative cross-body sketch pocket");
    auto* form=new QFormLayout(&dialog);
    auto* explanation=new QLabel(
        "Creates a hidden sketch-linked cutter and a visible associative Cut result. "
        "The original target and source remain unchanged. The pocket follows subsequent source sketch edits.",&dialog);
    explanation->setWordWrap(true);form->addRow(explanation);
    auto* sourceField=new QComboBox(&dialog);
    auto* targetField=new QComboBox(&dialog);
    const QStringList selected=selectedIds();
    for(const cad::Part& part:document_.parts()){
        if(part.hasConstrainedPolygonDefinition&&!part.isSurfaceBody&&
           part.constrainedPolygonGeometryToken==part.geometryToken)
            sourceField->addItem(part.name,part.id);
        if(!part.isSurfaceBody)targetField->addItem(part.name,part.id);
    }
    if(sourceField->count()==0||targetField->count()==0){
        reportError("Associative sketch pocket","A current sketch source and a solid target are required.");return;
    }
    for(const QString& selectedId:selected){
        const int sourceIndex=sourceField->findData(selectedId);
        const int targetIndex=targetField->findData(selectedId);
        if(sourceIndex>=0)sourceField->setCurrentIndex(sourceIndex);
        if(targetIndex>=0)targetField->setCurrentIndex(targetIndex);
    }
    form->addRow("Constrained sketch source",sourceField);
    form->addRow("Solid body to cut",targetField);
    auto* depthField=new QDoubleSpinBox(&dialog);
    depthField->setDecimals(4);depthField->setRange(0.0001,1000000.0);
    depthField->setValue(5.0);depthField->setSuffix(" mm");
    form->addRow("Pocket tool depth",depthField);
    auto* offsetField=new QDoubleSpinBox(&dialog);
    offsetField->setDecimals(4);offsetField->setRange(-1000000.0,1000000.0);
    offsetField->setSuffix(" mm");offsetField->setValue(0.0);
    form->addRow("Tool normal offset",offsetField);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.addLinkedSketchPocket(sourceField->currentData().toString(),
                                        targetField->currentData().toString(),
                                        depthField->value(),offsetField->value(),&error)){
        reportError("Associative sketch pocket",error);return;
    }
    refresh(true);
    statusBar()->showMessage("Created associative pocket and hidden live cutter in one undo step.",9000);
}

void MainWindow::createLinkedSketchBoss(){
    if(!requireBodyMode("Associative sketch boss"))return;
    QDialog dialog(this);dialog.setWindowTitle("Associative cross-body sketch boss");
    auto* form=new QFormLayout(&dialog);
    auto* explanation=new QLabel(
        "Creates a hidden sketch-linked additive tool and a visible associative Fuse result. "
        "A boss must add volume AND remain connected to the target. "
        "The original source and target bodies remain unchanged.",&dialog);
    explanation->setWordWrap(true);form->addRow(explanation);
    auto* sourceField=new QComboBox(&dialog);
    auto* targetField=new QComboBox(&dialog);
    const QStringList selected=selectedIds();
    for(const cad::Part& part:document_.parts()){
        if(part.hasConstrainedPolygonDefinition&&!part.isSurfaceBody&&
           part.constrainedPolygonGeometryToken==part.geometryToken)
            sourceField->addItem(part.name,part.id);
        if(!part.isSurfaceBody)targetField->addItem(part.name,part.id);
    }
    if(sourceField->count()==0||targetField->count()==0){
        reportError("Associative sketch boss","A current constrained sketch and a solid target are required.");return;
    }
    for(const QString& selectedId:selected){
        const int sourceIndex=sourceField->findData(selectedId);
        const int targetIndex=targetField->findData(selectedId);
        if(sourceIndex>=0)sourceField->setCurrentIndex(sourceIndex);
        if(targetIndex>=0)targetField->setCurrentIndex(targetIndex);
    }
    form->addRow("Constrained sketch source",sourceField);
    form->addRow("Solid target body",targetField);
    auto* depthField=new QDoubleSpinBox(&dialog);
    depthField->setDecimals(4);depthField->setRange(0.0001,1000000.0);
    depthField->setValue(5.0);depthField->setSuffix(" mm");
    form->addRow("Boss extrusion depth",depthField);
    auto* offsetField=new QDoubleSpinBox(&dialog);
    offsetField->setDecimals(4);offsetField->setRange(-1000000.0,1000000.0);
    offsetField->setSuffix(" mm");offsetField->setValue(8.0);
    form->addRow("Tool normal offset",offsetField);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.addLinkedSketchBoss(sourceField->currentData().toString(),
                                      targetField->currentData().toString(),
                                      depthField->value(),offsetField->value(),&error)){
        reportError("Associative sketch boss",error);return;
    }
    refresh(true);
    statusBar()->showMessage("Created a connected associative sketch boss and hidden tool in one undo step.",9000);
}

void MainWindow::editInPlaceSketchFeature(){
    if(!requireBodyMode("In-place associative sketch feature"))return;
    QDialog dialog(this);dialog.setWindowTitle("Edit in-place associative sketch feature");
    auto* form=new QFormLayout(&dialog);
    auto* explanation=new QLabel(
        "Apply a source constrained sketch directly to an existing solid body ID. "
        "The original target B-rep is frozen, and downstream Boolean bodies regenerate. "
        "Up to eight ordered sketch modifiers per ordinary target are supported. "
        "Each stage can reference a different external constrained sketch. "
        "All downstream Boolean dependencies regenerate atomically.",&dialog);
    explanation->setWordWrap(true);form->addRow(explanation);
    auto* source=new QComboBox(&dialog);
    auto* target=new QComboBox(&dialog);
    const QStringList selected=selectedIds();
    for(const cad::Part& part:document_.parts()){
        if(part.hasConstrainedPolygonDefinition&&!part.isSurfaceBody&&
           part.constrainedPolygonGeometryToken==part.geometryToken)
            source->addItem(part.name,part.id);
        if(!part.isSurfaceBody&&!part.hasLinkedBoolean&&!part.hasLinkedSketchExtrusion&&
           !part.hasShapeFeatureTree&&!part.hasConstrainedPolygonDefinition&&
           part.componentDefinitionId.isEmpty()&&part.assemblyJointType.isEmpty())
            target->addItem(part.name,part.id);
    }
    if(source->count()==0||target->count()==0){
        reportError("In-place sketch feature","Need a constrained sketch and an ordinary target solid.");return;
    }
    for(const QString& id:selected){
        const int i=target->findData(id);if(i>=0)target->setCurrentIndex(i);
    }
    form->addRow("Source sketch",source);form->addRow("Existing target body",target);
    auto* stage=new QComboBox(&dialog);
    stage->addItem("Edit first modifier",0);
    stage->addItem("Append second modifier",1);
    stage->addItem("Edit second modifier",2);
    stage->addItem("Remove second modifier",3);
    stage->addItem("Remove entire modifier stack",4);
    stage->addItem("Append next modifier (stages 3-8)",5);
    stage->addItem("Edit stage 3-8",6);
    stage->addItem("Remove last stage 3-8",7);
    form->addRow("Modifier stage",stage);
    auto* additionalIndex=new QSpinBox(&dialog);
    additionalIndex->setRange(3,8);additionalIndex->setValue(3);
    additionalIndex->setToolTip("Select an existing stage from 3 to 8 for editing. Append always creates the next stage.");
    form->addRow("Stage to edit",additionalIndex);
    auto* operation=new QComboBox(&dialog);
    operation->addItem("Add material (boss)",QStringLiteral("Fuse"));
    operation->addItem("Remove material (pocket)",QStringLiteral("Cut"));
    form->addRow("Operation",operation);
    auto* depth=new QDoubleSpinBox(&dialog);
    depth->setRange(0.0001,1000000.0);depth->setDecimals(4);depth->setValue(5);depth->setSuffix(" mm");
    auto* offset=new QDoubleSpinBox(&dialog);
    offset->setRange(-1000000.0,1000000.0);offset->setDecimals(4);offset->setSuffix(" mm");
    form->addRow("Tool depth",depth);form->addRow("Normal offset",offset);
    const auto loadExisting=[&]{
        const auto* part=document_.find(target->currentData().toString());
        const int mode=stage->currentData().toInt();
        const bool second=mode==2||mode==3;
        const bool hasFirst=part&&part->hasInPlaceSketchFeature;
        const bool hasSecond=hasFirst&&part->hasInPlaceSketchFollowUp;
        if(!hasFirst)return;
        if((mode==6||mode==7)&&part->extraInPlaceSketchStages.size()>additionalIndex->value()-3&&
           additionalIndex->value()>=3){
            const auto& extra=part->extraInPlaceSketchStages[additionalIndex->value()-3];
            const int idx=source->findData(extra.sourceId);if(idx>=0)source->setCurrentIndex(idx);
            operation->setCurrentIndex(extra.operation=="Cut"?1:0);
            depth->setValue(extra.depthMm);offset->setValue(extra.offsetMm);
            return;
        }
        const QString src=(second&&hasSecond)?part->inPlaceFollowSourceId:part->inPlaceSketchSourceId;
        const int idx=source->findData(src);if(idx>=0)source->setCurrentIndex(idx);
        const QString op=(second&&hasSecond)?part->inPlaceFollowOperation:part->inPlaceSketchOperation;
        operation->setCurrentIndex(op=="Cut"?1:0);
        depth->setValue((second&&hasSecond)?part->inPlaceFollowDepthMm:part->inPlaceSketchDepthMm);
        offset->setValue((second&&hasSecond)?part->inPlaceFollowOffsetMm:part->inPlaceSketchOffsetMm);
    };
    connect(target,QOverload<int>::of(&QComboBox::currentIndexChanged),&dialog,[&](int){loadExisting();});
    connect(stage,QOverload<int>::of(&QComboBox::currentIndexChanged),&dialog,[&](int){loadExisting();});
    connect(additionalIndex,QOverload<int>::of(&QSpinBox::valueChanged),&dialog,[&](int){loadExisting();});
    loadExisting();
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    form->addRow(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;bool ok=false;
    const int mode=stage->currentData().toInt();
    const QString targetId=target->currentData().toString();
    if(mode==7)ok=document_.removeLastInPlaceSketchStage(targetId,&error);
    else if(mode==6)ok=document_.updateInPlaceSketchStage(targetId,additionalIndex->value()-3,
        source->currentData().toString(),
        operation->currentData().toString()=="Cut"?cad::BooleanOp::Cut:cad::BooleanOp::Fuse,
        depth->value(),offset->value(),&error);
    else if(mode==5)ok=document_.appendInPlaceSketchStage(targetId,source->currentData().toString(),
        operation->currentData().toString()=="Cut"?cad::BooleanOp::Cut:cad::BooleanOp::Fuse,
        depth->value(),offset->value(),&error);
    else if(mode==4)ok=document_.removeInPlaceSketchFeature(targetId,&error);
    else if(mode==3)ok=document_.removeInPlaceSketchFollowUp(targetId,&error);
    else if(mode==2)ok=document_.updateInPlaceSketchFollowUp(targetId,source->currentData().toString(),
        operation->currentData().toString()=="Cut"?cad::BooleanOp::Cut:cad::BooleanOp::Fuse,
        depth->value(),offset->value(),&error);
    else if(mode==1)ok=document_.appendInPlaceSketchFeature(targetId,source->currentData().toString(),
        operation->currentData().toString()=="Cut"?cad::BooleanOp::Cut:cad::BooleanOp::Fuse,
        depth->value(),offset->value(),&error);
    else ok=document_.setInPlaceSketchFeature(targetId,source->currentData().toString(),
        operation->currentData().toString()=="Cut"?cad::BooleanOp::Cut:cad::BooleanOp::Fuse,
        depth->value(),offset->value(),&error);
    if(!ok){reportError("In-place associative sketch feature",error);return;}
    refresh(true);
    statusBar()->showMessage("Updated existing body and associative downstream results in one undo step.",9000);
}

void MainWindow::saveLiveArticulatedPair(){
    if(!requireBodyMode("Articulated mechanism"))return;
    const QStringList ids=selectedIds();
    if(ids.size()!=2){
        reportError("Articulated mechanism","Select exactly two free component occurrences, fixed first.");return;
    }
    const cad::Part* fixed=document_.find(ids[0]);
    const cad::Part* moving=document_.find(ids[1]);
    if(!fixed||!moving||fixed->componentDefinitionId.isEmpty()||
       moving->componentDefinitionId.isEmpty()||!fixed->assemblyJointType.isEmpty()||
       !moving->assemblyJointType.isEmpty()){
        reportError("Articulated mechanism","Both selections must be independent free component occurrences.");return;
    }
    QDialog dialog(this);dialog.setWindowTitle("Save a bounded articulated pair");
    auto* form=new QFormLayout(&dialog);
    auto* info=new QLabel("This source-backed mechanism stores a fixed reference pose and "
        "a driven relative correction. Its joint axis passes through the fixed "
        "component's reference-frame origin; face-driven hinge picking and multi-link "
        "authoring are not yet available in this dialog.",&dialog);
    info->setWordWrap(true);form->addRow(info);
    auto* kind=new QComboBox(&dialog);
    kind->addItem("Fixed frame",0);
    kind->addItem("Driven revolute (Z axis, degrees)",1);
    kind->addItem("Driven prismatic (Z axis, mm)",2);
    form->addRow("Constraint",kind);
    auto* initial=new QDoubleSpinBox(&dialog);
    initial->setRange(-180,180);initial->setDecimals(3);initial->setValue(0);
    form->addRow("Requested coordinate",initial);
    connect(kind,QOverload<int>::of(&QComboBox::currentIndexChanged),&dialog,
            [kind,initial](int){
        const int mode=kind->currentData().toInt();
        initial->setEnabled(mode!=0);initial->setValue(0);
        initial->setRange(mode==2?-1000:-180,mode==2?1000:180);
        initial->setSuffix(mode==1?QStringLiteral(" deg"):mode==2?QStringLiteral(" mm"):QString());
    });
    initial->setEnabled(false);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    cad::SceneDocument::ArticulatedNetworkJoint joint;
    joint.parentBodyId=ids[0];joint.childBodyId=ids[1];
    joint.kind=static_cast<cad::articulated::JointKind>(kind->currentData().toInt());
    joint.axis={0,0,1};
    joint.initial=joint.kind==cad::articulated::JointKind::Revolute?
        initial->value()*std::numbers::pi/180.0:initial->value();
    joint.minimum=joint.initial;joint.maximum=joint.initial;joint.driven=true;
    QString issue;
    if(!document_.setLiveArticulatedNetwork(ids[0],{joint},&issue)){
        reportError("Save articulated pair",issue);return;
    }
    refresh(true);
    statusBar()->showMessage("Saved live articulated pair; reference frames persist across reopen.",9000);
}

void MainWindow::editSavedArticulatedJoint(){
    if(!document_.hasLiveArticulatedNetwork()){
        reportError("Saved articulated mechanism","No saved articulated network is available.");return;
    }
    const auto joints=document_.liveArticulatedJointDefinitions();
    QStringList labels;QVector<int> editable;
    for(int i=0;i<joints.size();++i){
        const auto& j=joints[i];
        if(!j.driven||j.kind==cad::articulated::JointKind::Fixed)continue;
        const bool revolute=j.kind==cad::articulated::JointKind::Revolute;
        labels.append(QStringLiteral("%1. %2 -> %3 (%4)")
            .arg(i+1).arg(j.parentBodyId,j.childBodyId,
                revolute?QStringLiteral("revolute, degrees"):
                         QStringLiteral("prismatic, mm")));
        editable.append(i);
    }
    if(labels.isEmpty()){
        reportError("Saved articulated mechanism","No driven revolute or prismatic joint is editable.");return;
    }
    bool selected=false;
    const QString choice=QInputDialog::getItem(this,"Edit mechanism driving coordinate",
        "Saved driven joint",labels,0,false,&selected);
    if(!selected)return;
    const int entry=labels.indexOf(choice);
    if(entry<0)return;
    const int index=editable[entry];
    const auto& joint=joints[index];
    const bool revolute=joint.kind==cad::articulated::JointKind::Revolute;
    const double unit=revolute?180.0/std::numbers::pi:1.0;
    bool accepted=false;
    const double number=QInputDialog::getDouble(this,"Edit driven coordinate",
        revolute?QStringLiteral("Angle (degrees)"):QStringLiteral("Translation (mm)"),
        joint.initial*unit,revolute?-360.0:-1000.0,
        revolute?360.0:1000.0,3,&accepted);
    if(!accepted)return;
    bool clearanceAccepted=false;
    const double requiredGap=QInputDialog::getDouble(this,"Articulated geometry clearance",
        "Required nominal clearance (mm, zero allows contact):",
        0.0,0.0,1000000.0,4,&clearanceAccepted);
    if(!clearanceAccepted)return;
    QString issue,preview;
    if(!document_.previewLiveArticulatedJointCoordinate(index,number/unit,
            &preview,&issue,requiredGap)){
        reportError("Articulated joint preview",issue);return;
    }
    if(std::abs(number/unit-joint.initial)<=1e-12){
        QMessageBox::information(this,"Read-only articulated pose preview",preview);
        return;
    }
    QDialog review(this);review.setWindowTitle("Review proposed articulated motion");
    auto* layout=new QVBoxLayout(&review);
    auto* reportText=new QPlainTextEdit(&review);
    reportText->setReadOnly(true);reportText->setPlainText(preview);
    reportText->setMinimumSize(640,290);layout->addWidget(reportText);
    auto* reviewButtons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&review);
    layout->addWidget(reviewButtons);
    connect(reviewButtons,&QDialogButtonBox::accepted,&review,&QDialog::accept);
    connect(reviewButtons,&QDialogButtonBox::rejected,&review,&QDialog::reject);
    if(review.exec()!=QDialog::Accepted)return;
    if(!document_.updateLiveArticulatedJointCoordinate(index,number/unit,&issue)){
        reportError("Edit driven mechanism",issue);return;
    }
    refresh(true);
    statusBar()->showMessage("Driven joint updated atomically; undo and project persistence retained.",9000);
}

void MainWindow::editSavedArticulatedJointDefinition(){
    if(!document_.hasLiveArticulatedNetwork()){
        reportError("Joint definition editor","Save a live articulated mechanism first.");return;
    }
    const auto joints=document_.liveArticulatedJointDefinitions();
    QStringList names;QVector<int> editable;
    for(int i=0;i<joints.size();++i){
        const auto& j=joints[i];
        if(!j.driven||j.kind==cad::articulated::JointKind::Fixed)continue;
        names<<QString("%1. %2 -> %3 (%4)").arg(i+1).arg(j.parentBodyId,j.childBodyId,
            j.kind==cad::articulated::JointKind::Revolute?"revolute":"prismatic");
        editable.append(i);
    }
    if(names.isEmpty()){
        reportError("Joint definition editor","No driven revolute or prismatic joints are saved.");return;
    }
    bool picked=false;
    const QString selection=QInputDialog::getItem(this,"Edit saved articulated joint",
        "Saved joint",names,0,false,&picked);
    if(!picked)return;
    const int row=names.indexOf(selection);if(row<0)return;
    const int index=editable[row];
    auto proposed=joints[index];
    const bool angle=proposed.kind==cad::articulated::JointKind::Revolute;
    const double factor=angle?180.0/std::numbers::pi:1.0;
    QDialog dialog(this);dialog.setWindowTitle("Articulated joint definition and travel limits");
    auto* layout=new QVBoxLayout(&dialog);
    auto* note=new QLabel("Edit a saved joint without changing its endpoints or type. Axis values are normalized. "
        "A complete loop solve and static solid preview run before any change is committed. "
        "Limits constrain subsequent batch drive edits.",&dialog);
    note->setWordWrap(true);layout->addWidget(note);
    auto* form=new QFormLayout;layout->addLayout(form);
    auto make=[&](double value,double low,double high,const QString& suffix=QString()){
        auto* box=new QDoubleSpinBox(&dialog);box->setDecimals(12);box->setRange(low,high);
        box->setValue(value);box->setSuffix(suffix);return box;
    };
    auto* ax=make(proposed.axis.x,-1,1),*ay=make(proposed.axis.y,-1,1),
        *az=make(proposed.axis.z,-1,1);
    form->addRow("Axis X",ax);form->addRow("Axis Y",ay);form->addRow("Axis Z",az);
    auto* ox=make(proposed.childAtZero.translation.x,-100000,100000," mm");
    auto* oy=make(proposed.childAtZero.translation.y,-100000,100000," mm");
    auto* oz=make(proposed.childAtZero.translation.z,-100000,100000," mm");
    form->addRow("Zero frame offset X",ox);form->addRow("Zero frame offset Y",oy);
    form->addRow("Zero frame offset Z",oz);
    cad::joint_frame::Euler savedFrame;
    if(!cad::joint_frame::toEuler(proposed.childAtZero.rotation,&savedFrame)){
        reportError("Joint definition editor","Saved joint frame is not a valid proper rotation matrix.");return;
    }
    const double radToDeg=180.0/std::numbers::pi;
    auto* yawBox=make(savedFrame.yaw*radToDeg,-180,180," deg");
    auto* pitchBox=make(savedFrame.pitch*radToDeg,-90,90," deg");
    auto* rollBox=make(savedFrame.roll*radToDeg,-180,180," deg");
    form->addRow("Zero-frame yaw Z",yawBox);
    form->addRow("Zero-frame pitch Y",pitchBox);
    form->addRow("Zero-frame roll X",rollBox);
    const double cap=angle?360.0:1000.0;
    const QString suffix=angle?QStringLiteral(" deg"):QStringLiteral(" mm");
    auto* coordinate=make(proposed.initial*factor,-cap,cap,suffix);
    auto* lower=make(proposed.minimum*factor,-cap,cap,suffix);
    auto* upper=make(proposed.maximum*factor,-cap,cap,suffix);
    auto* clearance=make(0,0,1000000," mm");
    form->addRow("Driven coordinate",coordinate);
    form->addRow("Lower travel limit",lower);form->addRow("Upper travel limit",upper);
    form->addRow("Required nominal clearance",clearance);
    auto* controls=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    layout->addWidget(controls);
    connect(controls,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(controls,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    dialog.resize(580,570);if(dialog.exec()!=QDialog::Accepted)return;
    const double norm=std::sqrt(ax->value()*ax->value()+ay->value()*ay->value()+az->value()*az->value());
    if(!std::isfinite(norm)||norm<1e-8){
        reportError("Joint axis","Joint axis must be a nonzero finite direction.");return;
    }
    proposed.axis={ax->value()/norm,ay->value()/norm,az->value()/norm};
    proposed.childAtZero.translation={ox->value(),oy->value(),oz->value()};
    if(!cad::joint_frame::fromEuler({yawBox->value()/radToDeg,
        pitchBox->value()/radToDeg,rollBox->value()/radToDeg},
        &proposed.childAtZero.rotation)){
        reportError("Joint frame","Invalid yaw, pitch or roll for saved joint.");return;
    }
    proposed.initial=coordinate->value()/factor;
    proposed.minimum=lower->value()/factor;proposed.maximum=upper->value()/factor;
    if(proposed.minimum>proposed.initial||proposed.maximum<proposed.initial){
        reportError("Joint limits","The driven coordinate must remain within the selected travel limits.");return;
    }
    QString issue,report;
    if(!document_.previewLiveArticulatedJointDefinitionEdit(index,proposed,&report,&issue,clearance->value())){
        reportError("Articulated definition preview",issue);return;
    }
    if(QMessageBox::question(this,"Confirm articulated joint definition",report+"\n\nCommit as one undoable edit?",
             QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
    if(!document_.replaceLiveArticulatedJointDefinition(index,proposed,&issue)){
        reportError("Joint definition update",issue);return;
    }
    refresh(true);statusBar()->showMessage("Articulated joint definition updated atomically.",8500);
}

void MainWindow::rebindSavedArticulatedJoint(){
    if(!document_.hasLiveArticulatedNetwork()){
        reportError("Articulated rebind","Save an articulated network before rebinding a joint.");return;
    }
    const auto joints=document_.liveArticulatedJointDefinitions();
    if(joints.isEmpty())return;
    QStringList choices;
    QStringList members{document_.liveArticulatedFixedBodyId()};
    for(int i=0;i<joints.size();++i){
        const auto& joint=joints[i];
        choices<<QString("%1. %2 -> %3").arg(i+1).arg(joint.parentBodyId,joint.childBodyId);
        if(!members.contains(joint.parentBodyId))members<<joint.parentBodyId;
        if(!members.contains(joint.childBodyId))members<<joint.childBodyId;
    }
    bool chosen=false;
    const QString value=QInputDialog::getItem(this,"Rebind an articulated joint",
        "Saved joint connection",choices,0,false,&chosen);
    if(!chosen)return;
    const int index=choices.indexOf(value);
    if(index<0)return;
    const auto& joint=joints[index];
    QDialog dialog(this);
    dialog.setWindowTitle("Rebind joint to existing mechanism members");
    auto* layout=new QVBoxLayout(&dialog);
    auto* description=new QLabel("Select an existing parent and child. The complete mechanism must remain connected "
        "and uniquely solvable. The joint type, index, axis, limits and saved zero frame remain unchanged. "
        "The new graph is solved and its solids inspected before commitment.",&dialog);
    description->setWordWrap(true);layout->addWidget(description);
    auto* form=new QFormLayout;layout->addLayout(form);
    auto* parent=new QComboBox(&dialog);
    auto* child=new QComboBox(&dialog);
    parent->addItems(members);child->addItems(members);
    parent->setCurrentText(joint.parentBodyId);child->setCurrentText(joint.childBodyId);
    form->addRow("Parent occurrence",parent);
    form->addRow("Child occurrence",child);
    auto* clearance=new QDoubleSpinBox(&dialog);
    clearance->setRange(0,1000000);clearance->setDecimals(4);clearance->setSuffix(" mm");
    form->addRow("Required nominal clearance",clearance);
    auto* controls=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    layout->addWidget(controls);
    connect(controls,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(controls,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString issue,preview;
    if(!document_.previewLiveArticulatedJointRebind(index,parent->currentText(),
            child->currentText(),&preview,&issue,clearance->value())){
        reportError("Articulated graph rebind preview",issue);return;
    }
    if(QMessageBox::question(this,"Confirm articulated joint rebind",
        preview+"\n\nCommit this joint rebind as one undoable edit?",
        QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
    if(!document_.rebindLiveArticulatedJoint(index,parent->currentText(),
            child->currentText(),&issue)){
        reportError("Articulated graph rebind",issue);return;
    }
    refresh(true);
    statusBar()->showMessage("Saved articulated joint rebound and mechanism regenerated atomically.",8500);
}

void MainWindow::editSavedArticulatedDrives(){
    if(!document_.hasLiveArticulatedNetwork()){
        reportError("Articulated drive editor","Save an articulated network first.");return;
    }
    const auto joints=document_.liveArticulatedJointDefinitions();
    QDialog dialog(this);dialog.setWindowTitle("Edit articulated drives together");
    auto* layout=new QVBoxLayout(&dialog);
    auto* explanation=new QLabel("Choose any combination of driven coordinates. Preview solves the entire articulated network in one transaction; cancel does not change the scene.",&dialog);
    explanation->setWordWrap(true);layout->addWidget(explanation);
    auto* table=new QTableWidget(0,4,&dialog);
    table->setHorizontalHeaderLabels({"Joint","Driven","Current","Proposed"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    QVector<int> editable;
    for(int i=0;i<joints.size();++i){
        const auto& joint=joints[i];
        if(!joint.driven||joint.kind==cad::articulated::JointKind::Fixed)continue;
        const bool revolute=joint.kind==cad::articulated::JointKind::Revolute;
        const double unit=revolute?180.0/std::numbers::pi:1.0;
        const int row=table->rowCount();table->insertRow(row);editable.append(i);
        const auto locked=[](const QString& value){auto* item=new QTableWidgetItem(value);
            item->setFlags(item->flags()&~Qt::ItemIsEditable);return item;};
        table->setItem(row,0,locked(QString("%1 → %2").arg(joint.parentBodyId,joint.childBodyId)));
        table->setItem(row,1,locked(revolute?"Revolute (deg)":"Prismatic (mm)"));
        table->setItem(row,2,locked(QString::number(joint.initial*unit,'g',15)));
        auto* control=new QDoubleSpinBox(table);
        control->setDecimals(9);control->setRange(revolute?-360:-1000,revolute?360:1000);
        control->setValue(joint.initial*unit);table->setCellWidget(row,3,control);
    }
    if(editable.isEmpty()){
        reportError("Articulated drive editor","No driven coordinates are editable.");return;
    }
    layout->addWidget(table);
    auto* required=new QDoubleSpinBox(&dialog);required->setRange(0,1000000);
    required->setDecimals(4);required->setSuffix(" mm");
    auto* form=new QFormLayout();form->addRow("Required nominal clearance",required);
    auto* sweep=new QCheckBox("Preview full sampled motion path before committing",&dialog);
    sweep->setChecked(true);
    auto* rejectUnsafe=new QCheckBox("Reject unsafe final and sampled poses (if enabled)",&dialog);
    rejectUnsafe->setChecked(true);
    auto* frames=new QSpinBox(&dialog);frames->setRange(2,12);frames->setValue(6);
    frames->setSuffix(" base intervals");
    auto* refinement=new QSpinBox(&dialog);
    refinement->setRange(1,4);refinement->setValue(2);
    refinement->setSuffix(" samples per base interval");
    form->addRow(sweep);
    form->addRow(rejectUnsafe);
    form->addRow("Motion preview resolution",frames);
    form->addRow("Sub-interval sampling",refinement);
    auto* trajectoryTiming=new QComboBox(&dialog);
    trajectoryTiming->addItems({"Linear drive progress", "Ease in/out (smoothstep)"});
    trajectoryTiming->setToolTip("Controls interpolation timing only. It does not model acceleration, forces, or continuous collisions.");
    form->addRow("Drive progression",trajectoryTiming);
    connect(sweep,&QCheckBox::toggled,trajectoryTiming,&QComboBox::setEnabled);
    connect(sweep,&QCheckBox::toggled,frames,&QSpinBox::setEnabled);
    connect(sweep,&QCheckBox::toggled,refinement,&QSpinBox::setEnabled);
    // One-edit solver-checked ratio graph. Relations are transient; no
    // persistent gear or rack mate is authored by this dialog.
    auto* coupled=new QCheckBox("Enable leader–follower ratio relations for this edit",&dialog);
    auto* leadDrive=new QComboBox(&dialog);
    auto* followDrive=new QComboBox(&dialog);
    for(int row=0;row<editable.size();++row){
        const int jointIndex=editable[row];
        const auto& joint=joints[jointIndex];
        const QString title=QString("Joint %1: %2 -> %3 (%4)")
            .arg(jointIndex+1).arg(joint.parentBodyId,joint.childBodyId)
            .arg(joint.kind==cad::articulated::JointKind::Revolute?"degrees":"mm");
        leadDrive->addItem(title,jointIndex);
        followDrive->addItem(title,jointIndex);
    }
    if(editable.size()>1)followDrive->setCurrentIndex(1);
    auto* ratio=new QDoubleSpinBox(&dialog);
    ratio->setDecimals(6);ratio->setRange(-1000,1000);ratio->setValue(1);
    ratio->setToolTip("Follower displacement = ratio × leader displacement + offset");
    auto* offset=new QDoubleSpinBox(&dialog);
    offset->setDecimals(6);offset->setRange(-100000,100000);
    offset->setToolTip("Angle degrees for revolute, mm for prismatic, applied to follower displacement");
    auto* couplingForm=new QFormLayout();
    couplingForm->addRow(coupled);couplingForm->addRow("Leader",leadDrive);
    couplingForm->addRow("Follower",followDrive);
    couplingForm->addRow("Follower / leader displacement ratio",ratio);
    couplingForm->addRow("Follower displacement offset",offset);
    // The first relation remains immediately editable above. Additional
    // rows permit a graph of chained ratios in one atomic mechanism solve;
    // the shared solver rejects cycles, duplicate followers and mixed units.
    auto* additionalCouplings=new QTableWidget(0,4,&dialog);
    additionalCouplings->setHorizontalHeaderLabels({"Leader", "Follower", "Ratio", "Offset (follower units)"});
    additionalCouplings->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    auto* addCoupling=new QPushButton("Add another ratio relation",&dialog);
    auto* removeCoupling=new QPushButton("Remove selected relation",&dialog);
    auto* linkControls=new QHBoxLayout();
    linkControls->addWidget(addCoupling);linkControls->addWidget(removeCoupling);
    auto insertRatioRow=[&](){
        if(additionalCouplings->rowCount()>=std::min(16,static_cast<int>(editable.size())-2))return;
        const int row=additionalCouplings->rowCount();
        additionalCouplings->insertRow(row);
        auto* leaderBox=new QComboBox(additionalCouplings);
        auto* followerBox=new QComboBox(additionalCouplings);
        for(int i=0;i<leadDrive->count();++i){
            leaderBox->addItem(leadDrive->itemText(i),leadDrive->itemData(i));
            followerBox->addItem(followDrive->itemText(i),followDrive->itemData(i));
        }
        if(leaderBox->count()>1)leaderBox->setCurrentIndex(row+1<leaderBox->count()?row+1:0);
        if(followerBox->count()>2)followerBox->setCurrentIndex(row+2<followerBox->count()?row+2:0);
        auto* ratioBox=new QDoubleSpinBox(additionalCouplings);
        ratioBox->setDecimals(6);ratioBox->setRange(-1000,1000);ratioBox->setValue(1);
        auto* offsetBox=new QDoubleSpinBox(additionalCouplings);
        offsetBox->setDecimals(6);offsetBox->setRange(-100000,100000);
        additionalCouplings->setCellWidget(row,0,leaderBox);
        additionalCouplings->setCellWidget(row,1,followerBox);
        additionalCouplings->setCellWidget(row,2,ratioBox);
        additionalCouplings->setCellWidget(row,3,offsetBox);
    };
    connect(addCoupling,&QPushButton::clicked,&dialog,insertRatioRow);
    connect(removeCoupling,&QPushButton::clicked,&dialog,[&](){
        if(additionalCouplings->currentRow()>=0)
            additionalCouplings->removeRow(additionalCouplings->currentRow());
    });
    auto enableCoupling=[&](bool enabled){
        leadDrive->setEnabled(enabled);followDrive->setEnabled(enabled);
        ratio->setEnabled(enabled);offset->setEnabled(enabled);
        additionalCouplings->setEnabled(enabled);
        addCoupling->setEnabled(enabled&&editable.size()>2);
        removeCoupling->setEnabled(enabled);
    };
    coupled->setEnabled(editable.size()>1);
    enableCoupling(false);
    connect(coupled,&QCheckBox::toggled,&dialog,enableCoupling);
    layout->addLayout(couplingForm);
    layout->addWidget(additionalCouplings);
    layout->addLayout(linkControls);
    layout->addLayout(form);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    dialog.resize(850,510);
    if(dialog.exec()!=QDialog::Accepted)return;
    QMap<int,double> targets;
    for(int row=0;row<editable.size();++row){
        const int index=editable[row];
        const bool revolute=joints[index].kind==cad::articulated::JointKind::Revolute;
        const auto* control=qobject_cast<QDoubleSpinBox*>(table->cellWidget(row,3));
        if(!control)return;
        const double unit=revolute?std::numbers::pi/180.0:1.0;
        targets.insert(index,control->value()*unit);
    }
    QString coupledTravelReport;
    if(coupled->isChecked()){
        std::vector<cad::drive_coupling::Drive> driveInputs;
        for(const int index:editable){
            const auto& joint=joints[index];
            driveInputs.push_back({index,
                joint.kind==cad::articulated::JointKind::Revolute?
                    cad::drive_coupling::Kind::Revolute:
                    cad::drive_coupling::Kind::Prismatic,
                joint.initial,targets.value(index),joint.minimum,joint.maximum});
        }
        const int leader=leadDrive->currentData().toInt();
        const int follower=followDrive->currentData().toInt();
        const bool revolute=joints[follower].kind==cad::articulated::JointKind::Revolute;
        const double offsetUnit=revolute?std::numbers::pi/180.0:1.0;
        std::vector<cad::drive_coupling::Relation> relations{
            {leader,follower,ratio->value(),offset->value()*offsetUnit}};
        for(int row=0;row<additionalCouplings->rowCount();++row){
            auto* leaderBox=qobject_cast<QComboBox*>(additionalCouplings->cellWidget(row,0));
            auto* followerBox=qobject_cast<QComboBox*>(additionalCouplings->cellWidget(row,1));
            auto* ratioBox=qobject_cast<QDoubleSpinBox*>(additionalCouplings->cellWidget(row,2));
            auto* offsetBox=qobject_cast<QDoubleSpinBox*>(additionalCouplings->cellWidget(row,3));
            if(!leaderBox||!followerBox||!ratioBox||!offsetBox)return;
            const int followerId=followerBox->currentData().toInt();
            const double unit=joints[followerId].kind==cad::articulated::JointKind::Revolute?
                std::numbers::pi/180.0:1.0;
            relations.push_back({leaderBox->currentData().toInt(),followerId,
                ratioBox->value(),offsetBox->value()*unit});
        }
        cad::drive_coupling::Solution result;
        std::string couplingIssue;
        if(!cad::drive_coupling::solve(driveInputs,relations,
            &result,&couplingIssue)){
            reportError("Articulated drive ratio coupling",QString::fromStdString(couplingIssue));return;
        }
        for(const auto& value:result.targets)targets[value.first]=value.second;
        cad::drive_coupling::RangeStudy feasible;
        if(!cad::drive_coupling::admissibleRootRanges(driveInputs,relations,
                &feasible,&couplingIssue)){
            reportError("Articulated drive ratio travel ranges",
                QString::fromStdString(couplingIssue));return;
        }
        QStringList limits{"RATIO-COUPLED ROOT TRAVEL CORRIDORS (joint limits only):"};
        for(const auto& [root,range]:feasible.independentRoots){
            const bool rotary=joints[root].kind==cad::articulated::JointKind::Revolute;
            const double unit=rotary?180.0/std::numbers::pi:1.0;
            limits<<QString("Joint %1: %2 .. %3 %4 (constrained followers: %5)")
                .arg(root+1)
                .arg(range.lowerBounded?QString::number(range.minimum*unit,'g',9):"unbounded")
                .arg(range.upperBounded?QString::number(range.maximum*unit,'g',9):"unbounded")
                .arg(rotary?"deg":"mm").arg(static_cast<qulonglong>(range.constrainedFollowers));
        }
        limits<<"Travel corridors do not certify collision-free intermediate mechanism poses.";
        coupledTravelReport=limits.join('\n');
    }
    QString issue,preview;
    bool unsafeFinal=false;
    if(!document_.previewLiveArticulatedCoordinates(targets,&preview,&issue,required->value(),&unsafeFinal)){
        reportError("Articulated batch preflight",issue);return;
    }
    if(rejectUnsafe->isChecked()&&unsafeFinal){
        reportError("Articulated batch clearance",
            "The proposed final pose has interference, inadequate clearance, or unverified fit pairs. "
            "Adjust the drives or explicitly disable strict sampled-fit rejection.");return;
    }
    if(sweep->isChecked()){
        QString pathReport;
        if(!document_.previewLiveArticulatedTrajectory(targets,frames->value(),
                  &pathReport,&issue,required->value(),rejectUnsafe->isChecked(),
                  refinement->value(),trajectoryTiming->currentIndex()==1)){
            reportError("Sampled articulated motion preflight",issue);return;
        }
        preview=pathReport+"\n\nFINAL POSE SUMMARY\n"+preview;
    }
    if(!coupledTravelReport.isEmpty())preview=coupledTravelReport+"\n\n"+preview;
    QDialog review(this);review.setWindowTitle("Review all proposed articulated drives");
    auto* reviewLayout=new QVBoxLayout(&review);
    auto* report=new QPlainTextEdit(&review);report->setReadOnly(true);
    report->setPlainText(preview);reviewLayout->addWidget(report);
    auto* reviewButtons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&review);
    reviewLayout->addWidget(reviewButtons);
    auto* exportCsv=reviewButtons->addButton("Export sampled motion CSV...",QDialogButtonBox::ActionRole);
    connect(exportCsv,&QPushButton::clicked,&review,[&]{
        QByteArray csv;QString issue;
        if(!document_.buildLiveArticulatedMotionCsv(targets,frames->value(),
            refinement->value(),trajectoryTiming->currentIndex()==1,required->value(),
            rejectUnsafe->isChecked(),&csv,&issue)){
            reportError("Sampled articulated motion CSV",issue);return;
        }
        QString path=QFileDialog::getSaveFileName(&review,"Export audited articulated motion",
            QStringLiteral("articulated-motion.csv"),"CSV (*.csv)");
        if(path.isEmpty())return;
        if(!path.endsWith(".csv",Qt::CaseInsensitive))path+=".csv";
        QSaveFile output(path);
        if(!output.open(QIODevice::WriteOnly)||output.write(csv)!=csv.size()||!output.commit()){
            output.cancelWriting();reportError("Sampled articulated motion CSV","Cannot save the motion evidence CSV.");return;
        }
        statusBar()->showMessage("Read-only sampled mechanism evidence exported without changing joint coordinates.",9000);
    });
    auto* exportEnvelope=reviewButtons->addButton("Export sampled workspace + same-time proximity CSV...",
        QDialogButtonBox::ActionRole);
    connect(exportEnvelope,&QPushButton::clicked,&review,[&]{
        QByteArray envelope;QString issue;
        if(!document_.buildLiveArticulatedEnvelopeCsv(targets,frames->value(),
            refinement->value(),trajectoryTiming->currentIndex()==1,&envelope,&issue)){
            reportError("Moving-body swept-space envelopes",issue);return;
        }
        QString path=QFileDialog::getSaveFileName(&review,"Export sampled moving-body workspace",
            QStringLiteral("articulated-workspace.csv"),"CSV (*.csv)");
        if(path.isEmpty())return;
        if(!path.endsWith(".csv",Qt::CaseInsensitive))path+=".csv";
        QSaveFile output(path);
        if(!output.open(QIODevice::WriteOnly)||output.write(envelope)!=envelope.size()||!output.commit()){
            output.cancelWriting();reportError("Swept-space envelopes","Cannot save envelope CSV.");return;
        }
        statusBar()->showMessage("Workspace and same-pose AABB diagnostics exported; not a continuous collision test.",9000);
    });
    connect(reviewButtons,&QDialogButtonBox::accepted,&review,&QDialog::accept);
    connect(reviewButtons,&QDialogButtonBox::rejected,&review,&QDialog::reject);
    review.resize(760,460);
    if(review.exec()!=QDialog::Accepted)return;
    if(!document_.commitLiveArticulatedCoordinatesChecked(targets,required->value(),
            rejectUnsafe->isChecked(),sweep->isChecked()?frames->value():0,
            refinement->value(),trajectoryTiming->currentIndex()==1,&issue)){
        reportError("Articulated batch commit",issue);return;
    }
    refresh(true);statusBar()->showMessage("Articulated drives committed as one undoable transaction.",9000);
}

void MainWindow::auditLiveRigidMateStructure(){
    if(!document_.hasLiveRigidMateNetwork()){
        reportError("Rigid-mate structure audit","No persistent rigid mate network has been saved.");return;
    }
    const auto links=document_.liveRigidMateDefinitions();
    QStringList names;QMap<QString,std::size_t> indices;
    const auto registerNode=[&](const QString& id){
        if(!indices.contains(id)){
            indices.insert(id,static_cast<std::size_t>(names.size()));
            const auto* body=document_.find(id);
            names.append(body?body->name+QStringLiteral(" [%1]").arg(id.left(8)):id);
        }
        return indices.value(id);
    };
    const QString rootId=document_.liveRigidMateFixedBodyId();
    const std::size_t rootIndex=registerNode(rootId);
    std::vector<cad::mates::StructuralMateLink> raw;
    raw.reserve(static_cast<std::size_t>(links.size()));
    for(const auto& link:links){
        const auto from=registerNode(link.parentBodyId);
        const auto to=registerNode(link.childBodyId);
        raw.push_back({from,to,link.constraints.size()});
    }
    cad::mates::StructuralMateAudit audit;std::string issue;
    if(!cad::mates::auditMateStructure(static_cast<std::size_t>(names.size()),rootIndex,raw,&audit,&issue)){
        reportError("Rigid-mate structure audit",QString::fromStdString(issue));return;
    }
    QStringList lines;
    lines<<"STRUCTURAL MATE NETWORK AUDIT (READ ONLY)"
         <<QStringLiteral("Fixed occurrence: %1").arg(names[static_cast<qsizetype>(rootIndex)])
         <<QStringLiteral("Participating occurrences: %1").arg(names.size())
         <<QStringLiteral("Connections: %1").arg(static_cast<int>(raw.size()))
         <<QStringLiteral("Connected components: %1").arg(static_cast<int>(audit.connectedComponents))
         <<QStringLiteral("Independent graph cycles: %1").arg(static_cast<int>(audit.independentCycles))
         <<QStringLiteral("Connectivity: %1").arg(audit.connected?QStringLiteral("all participating nodes linked"):
                                                           QStringLiteral("DISCONNECTED"))
         <<QStringLiteral("Graph bridge links: %1").arg(static_cast<int>(audit.bridgeLinkIndices.size()))
         <<QStringLiteral("Cycle-participating links: %1").arg(static_cast<int>(audit.cycleLinkIndices.size()))<<QString();
    if(!audit.unreachableNodes.empty()){
        lines<<"Occurrences not connected to the fixed occurrence:";
        for(const auto index:audit.unreachableNodes)lines<<"  "+names[static_cast<qsizetype>(index)];
    }
    lines<<QString()<<"Independent fixed-frame paths per occurrence (edge-disjoint; capped at 3):";
    for(qsizetype i=0;i<names.size();++i){
        if(static_cast<std::size_t>(i)==rootIndex)continue;
        const unsigned char paths=audit.independentPathsToFixed[static_cast<std::size_t>(i)];
        const QString score=paths==3?QStringLiteral("3+"):
            QString::number(static_cast<int>(paths));
        lines<<QStringLiteral("  %1: %2 independent link path(s)%3")
            .arg(names[i],score,paths==0?QStringLiteral(" — NOT CONNECTED"):
                paths==1?QStringLiteral(" — single-link vulnerability"):QString());
    }
    if(!audit.bridgeLinkIndices.empty()){
        lines<<"Critical connectivity links (removing one disconnects a branch):";
        for(const auto index:audit.bridgeLinkIndices)
            lines<<QStringLiteral("  Link %1: %2 -> %3").arg(static_cast<int>(index+1))
                .arg(names[static_cast<qsizetype>(raw[index].parent)],names[static_cast<qsizetype>(raw[index].child)]);
    }
    if(!audit.bridgeImpacts.empty()){
        lines<<QString()<<"Fixed-frame isolation if a critical connection is removed:";
        for(const auto& impact:audit.bridgeImpacts){
            QStringList detached;
            for(const auto index:impact.detachedFromFixed)
                detached<<names[static_cast<qsizetype>(index)];
            lines<<QStringLiteral("  Link %1: %2 occurrence(s) lose their fixed-frame path: %3")
                .arg(static_cast<int>(impact.linkIndex+1))
                .arg(detached.size())
                .arg(detached.isEmpty()?QStringLiteral("none in fixed component"):detached.join(", "));
        }
    }
    if(!audit.criticalOccurrenceImpacts.empty()){
        lines<<QString()<<"Critical occurrences (removing one isolates other occurrences):";
        for(const auto& impact:audit.criticalOccurrenceImpacts){
            QStringList detached;
            for(const auto index:impact.detachedFromFixed)
                detached<<names[static_cast<qsizetype>(index)];
            lines<<QStringLiteral("  %1: %2 other occurrence(s) lose the fixed-frame path: %3")
                .arg(names[static_cast<qsizetype>(impact.occurrenceIndex)])
                .arg(detached.size()).arg(detached.join(", "));
        }
    }
    cad::mates::StructuralReinforcement reinforcement;
    std::string reinforcementError;
    if(cad::mates::planStructuralReinforcement(static_cast<std::size_t>(names.size()),
             rootIndex,raw,&reinforcement,&reinforcementError)){
        lines<<QString()<<"GRAPH-ONLY REINFORCEMENT CANDIDATES (NOT MATE COMMANDS)"
             <<QStringLiteral("Fixed-frame bridge links: %1; after simulated proposals: %2")
                   .arg(static_cast<int>(reinforcement.vulnerableFixedFrameLinksBefore))
                   .arg(static_cast<int>(reinforcement.vulnerableFixedFrameLinksAfter));
        for(const auto& endpoints:reinforcement.suggestedConnections){
            lines<<QStringLiteral("  Consider a distinct physical support path between %1 and %2")
                .arg(names[static_cast<qsizetype>(endpoints.first)],
                     names[static_cast<qsizetype>(endpoints.second)]);
        }
        if(reinforcement.suggestedConnections.empty())
            lines<<"  No additional graph links needed for the currently anchored component.";
        if(!reinforcement.alreadyUnreachable.empty())
            lines<<"  Disconnected occurrences are excluded; attach them with valid real mates separately.";
        lines<<"Suggested pairs are not guaranteed to have compatible faces, axes, clearance or stiffness."
             <<"Do not create redundant duplicate observations as a substitute for real independent support.";
    }else{
        lines<<QString()<<"Reinforcement analysis unavailable: "
             +QString::fromStdString(reinforcementError);
    }
    lines<<QString()<<"Structural loops are not proof of geometric consistency or mechanical redundancy."
         <<"Use the persistent mate solver and motion/clearance audits for separate checks."
         <<"This command does not change the model, its history or its saved mate observations.";
    QDialog dialog(this);dialog.setWindowTitle("Saved rigid-mate connectivity audit");dialog.resize(750,470);
    auto* layout=new QVBoxLayout(&dialog);
    auto* report=new QPlainTextEdit(lines.join('\n'),&dialog);report->setReadOnly(true);
    layout->addWidget(report);
    auto* controls=new QHBoxLayout;
    auto* exportAudit=new QPushButton("Save audit report...",&dialog);
    controls->addWidget(exportAudit);controls->addStretch();
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    controls->addWidget(buttons);
    connect(exportAudit,&QPushButton::clicked,&dialog,[&]{
        const QString path=QFileDialog::getSaveFileName(&dialog,"Save rigid-mate structure audit",{},"Text reports (*.txt)");
        if(path.isEmpty())return;
        const QByteArray bytes=lines.join('\n').toUtf8();
        QSaveFile file(path);
        if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit()){
            file.cancelWriting();QMessageBox::warning(&dialog,"Export failed","Cannot save complete rigid-mate audit report.");return;
        }
        statusBar()->showMessage("Rigid-mate structure audit exported.",6000);
    });
    layout->addLayout(controls);dialog.exec();
}

void MainWindow::manageLiveRigidMates(){
    if(!document_.hasLiveRigidMateNetwork()){
        reportError("Saved rigid mate links","There is no saved live rigid mate network.");return;
    }
    const auto links=document_.liveRigidMateDefinitions();
    QStringList descriptions;
    for(int i=0;i<links.size();++i){
        const auto& link=links[i];
        const auto* parent=document_.find(link.parentBodyId);
        const auto* child=document_.find(link.childBodyId);
        descriptions.append(QStringLiteral("%1. %2 -> %3 (%4 constraints)")
            .arg(i+1).arg(parent?parent->name:link.parentBodyId)
            .arg(child?child->name:link.childBodyId).arg(link.constraints.size()));
    }
    bool accepted=false;
    const QString choice=QInputDialog::getItem(this,"Manage live rigid mate connections",
        QStringLiteral("Fixed occurrence: %1. Select a connection to edit or remove.")
            .arg(document_.liveRigidMateFixedBodyId()),descriptions,0,false,&accepted);
    if(!accepted)return;
    const int index=descriptions.indexOf(choice);
    if(index<0)return;
    QMessageBox operation(this);
    operation.setWindowTitle("Live rigid mate connection");
    operation.setText("Edit the saved reference observations, or remove this connection.");
    auto* editButton=operation.addButton("Edit constraint frames...",QMessageBox::AcceptRole);
    auto* removeButton=operation.addButton("Remove connection",QMessageBox::DestructiveRole);
    operation.addButton(QMessageBox::Cancel);
    operation.exec();
    if(operation.clickedButton()==removeButton){
        if(QMessageBox::question(this,"Remove saved mate connection",
            QStringLiteral("Remove connection %1? This can be undone.").arg(index+1),
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        QString issue;
        if(!document_.removeLiveRigidMateConnection(index,&issue)){
            reportError("Remove saved mate connection",issue);return;
        }
        refresh(true);
        statusBar()->showMessage("Mate connection removed; document undo is available.",7000);
        return;
    }
    if(operation.clickedButton()!=editButton)return;
    QVector<cad::SceneDocument::AssemblyNetworkMate> world;
    QString issue;
    if(!document_.liveRigidMateWorldDefinitions(&world,&issue)){
        reportError("Edit live rigid mate",issue);return;
    }
    if(index>=world.size())return;
    QDialog editor(this);editor.setWindowTitle("Edit persistent rigid mate observations");
    editor.resize(780,560);
    auto* layout=new QVBoxLayout(&editor);
    auto* explanation=new QLabel("Coordinates are in the CURRENT world frame (mm). "
        "Directions are unit vectors. Existing component identities stay fixed. "
        "The entire network must remain solvable; failed edits change nothing.",&editor);
    explanation->setWordWrap(true);layout->addWidget(explanation);
    auto* previewClearance=new QDoubleSpinBox(&editor);
    previewClearance->setRange(0.0,1000000.0);
    previewClearance->setDecimals(4);
    previewClearance->setValue(0.0);
    previewClearance->setSuffix(" mm required clearance");
    previewClearance->setToolTip("Optional nominal minimum gap for exact predicted solid checks. "
        "Does not alter or prevent a mate edit; intentional overlap may be allowed.");
    layout->addWidget(previewClearance);
    auto* scroll=new QScrollArea(&editor);scroll->setWidgetResizable(true);
    auto* content=new QWidget(scroll);auto* rowsLayout=new QVBoxLayout(content);
    struct Fields { std::array<QDoubleSpinBox*,12> vectors{}; QDoubleSpinBox* planeOffset=nullptr; };
    QVector<Fields> inputs;
    bool changedByUser=false;
    inputs.reserve(static_cast<qsizetype>(world[index].constraints.size()));
    const auto& current=world[index].constraints;
    for(std::size_t n=0;n<current.size();++n){
        const auto& c=current[n];
        const QString type=c.kind==cad::mates::Kind::Point?"Point":
            c.kind==cad::mates::Kind::CoaxialAxis?"Axis":"Plane";
        auto* title=new QLabel(QStringLiteral("Constraint %1: %2").arg(n+1).arg(type),content);
        rowsLayout->addWidget(title);
        auto* line=new QWidget(content);auto* grid=new QGridLayout(line);
        const std::array<cad::mates::Vec3,4> values={
            c.parentPoint,c.childPoint,c.parentDirection,c.childDirection};
        const QStringList labels={"Parent anchor","Child anchor","Parent direction","Child direction"};
        Fields row;
        for(int point=0;point<(c.kind==cad::mates::Kind::Point?2:4);++point){
            grid->addWidget(new QLabel(labels[point],line),point,0);
            const double coordinates[3]={values[point].x,values[point].y,values[point].z};
            for(int axis=0;axis<3;++axis){
                auto* box=new QDoubleSpinBox(line);
                box->setRange(-1000000.0,1000000.0);
                box->setDecimals(12); // avoid rounding old mate definitions on display
                box->setSingleStep(point<2?1.0:0.1);
                box->setValue(coordinates[axis]);
                connect(box,QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                        &editor,[&changedByUser](double){changedByUser=true;});
                row.vectors[3*point+axis]=box;
                grid->addWidget(box,point,axis+1);
            }
        }
        if(c.kind==cad::mates::Kind::CoincidentPlane){
            row.planeOffset=new QDoubleSpinBox(line);
            row.planeOffset->setRange(-1000000.0,1000000.0);
            row.planeOffset->setDecimals(8);
            row.planeOffset->setSuffix(" mm plane offset");
            row.planeOffset->setValue(c.offsetMm);
            grid->addWidget(new QLabel("Signed plane separation",line),4,0);
            grid->addWidget(row.planeOffset,4,1,1,3);
            connect(row.planeOffset,QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    &editor,[&changedByUser](double){changedByUser=true;});
        }
        rowsLayout->addWidget(line);
        inputs.append(row);
    }
    rowsLayout->addStretch();scroll->setWidget(content);layout->addWidget(scroll);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&editor);
    layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&editor,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&editor,&QDialog::reject);
    if(editor.exec()!=QDialog::Accepted)return;
    if(!changedByUser&&previewClearance->value()==0.0){
        statusBar()->showMessage("Mate observations unchanged; no revision created.",5000);
        return;
    }
    auto changed=world[index];
    for(std::size_t n=0;n<changed.constraints.size();++n){
        auto& c=changed.constraints[n];
        const auto& row=inputs[static_cast<qsizetype>(n)];
        const auto vec=[&](int point){return cad::mates::Vec3{
            row.vectors[3*point]->value(),row.vectors[3*point+1]->value(),
            row.vectors[3*point+2]->value()};};
        c.parentPoint=vec(0);c.childPoint=vec(1);
        if(c.kind!=cad::mates::Kind::Point){
            c.parentDirection=vec(2);c.childDirection=vec(3);
        }
        if(row.planeOffset)c.offsetMm=row.planeOffset->value();
    }
    QString preview;
    if(!document_.previewLiveRigidMateConnectionEdit(index,changed,&preview,&issue,
            previewClearance->value())){
        reportError("Mate edit validation",issue);return;
    }
    if(!changedByUser){
        QMessageBox::information(this,"Read-only mate clearance preview",preview);
        return; // a report-only request must never consume an undo entry
    }
    if(QMessageBox::question(this,"Review rigid mate edit",preview+"\n\nCommit this edit?",
                             QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
    if(!document_.replaceLiveRigidMateConnection(index,changed,&issue)){
        reportError("Edit live rigid mate",issue);return;
    }
    refresh(true);
    statusBar()->showMessage("Rigid mate frames updated atomically; undo/save/reopen supported.",9000);
}

void MainWindow::solveAssemblyFrameMates(){
    if(!requireBodyMode("Assembly frame mate solve"))return;
    const QStringList ids=selectedIds();
    if(ids.size()!=2){
        reportError("Assembly mates","Select exactly two component occurrences: parent first, child second.");return;
    }
    const cad::Part* parent=document_.find(ids[0]);
    const cad::Part* child=document_.find(ids[1]);
    if(!parent||!child||parent->componentDefinitionId.isEmpty()||child->componentDefinitionId.isEmpty()){
        reportError("Assembly mates","Both selections must be component occurrences.");return;
    }
    QDialog dialog(this);dialog.setWindowTitle("Assembly six-DOF mate solver");
    auto* form=new QFormLayout(&dialog);
    auto* hint=new QLabel("Choose a fully constraining set of world-frame observations. "
        "Each row stores a parent/child point or a UNIT axis/plane normal. "
        "Conflicting and underconstrained mate sets do not move the assembly.",&dialog);
    hint->setWordWrap(true);form->addRow(hint);
    auto* mateMode=new QComboBox(&dialog);
    mateMode->addItem("Three noncollinear point pairs",0);
    mateMode->addItem("One point and two oriented axes",1);
    mateMode->addItem("One point, one axis and one plane",2);
    form->addRow("Mate constraint set",mateMode);
    auto* facePrefill=new QPushButton("Prefill from two selected planar faces...",&dialog);
    facePrefill->setToolTip("The selected faces supply physical centers and normals. "
        "A world-referenced in-plane guide is an explicitly imposed twist lock, "
        "not a constraint inferred from a plane alone.");
    form->addRow("Geometry observations",facePrefill);
    auto* cylinderPrefill=new QPushButton("Prefill from two selected cylindrical faces...",&dialog);
    cylinderPrefill->setToolTip("The two cylinder axes supply physical line observations. "
        "Axial sliding and twist remain free until the user explicitly locks them.");
    form->addRow("Coaxial observations",cylinderPrefill);
    auto* fitReview=new QPushButton("Review selected cylindrical fit (read only)...",&dialog);
    fitReview->setToolTip("Identify the hole and shaft explicitly, then enter their dimensional deviations. A B-rep cylinder alone does not identify a fit class.");
    form->addRow("Mechanical fit",fitReview);
    connect(fitReview,&QPushButton::clicked,&dialog,[this,&dialog,ids]{
        const auto references=viewport_->selectedFaceReferences();
        if(references.size()!=2){
            QMessageBox::warning(&dialog,"Cylindrical fit","Select one cylindrical face on each selected occurrence.");return;
        }
        cad::mates::CylindricalFaceObservation observed[2];
        for(int side=0;side<2;++side){
            int count=0;
            for(const auto& ref:references){
                if(ref.partId!=ids[side])continue;
                ++count;
                const TopoDS_Shape shape=selectedSubshape(document_,ref.partId,
                    ref.geometryToken,ref.faceIndex,TopAbs_FACE);
                QString issue;
                if(shape.IsNull()||!cad::mates::describeSelectedCylindricalFace(
                        TopoDS::Face(shape),&observed[side],&issue)){
                    QMessageBox::warning(&dialog,"Cylindrical fit",
                        issue.isEmpty()?QStringLiteral("Invalid or stale cylindrical selection."):issue);return;
                }
            }
            if(count!=1){
                QMessageBox::warning(&dialog,"Cylindrical fit","Exactly one cylinder per selected occurrence is required.");return;
            }
        }
        const auto choice=QMessageBox::question(&dialog,"Identify bore and shaft",
            "Is the PARENT selected cylinder the bore (hole)? Choose No if the CHILD cylinder is the bore. "
            "This is a user identification, not inferred from face normals.",
            QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel,QMessageBox::Cancel);
        if(choice==QMessageBox::Cancel)return;
        const int bore=choice==QMessageBox::Yes?0:1,shaft=1-bore;
        const auto deviations=valuesDialog(&dialog,"Fit dimensional deviations (mm)",
            {"Hole lower deviation","Hole upper deviation","Shaft lower deviation","Shaft upper deviation"},
            {0.0,0.0,0.0,0.0},-1000.0,1000.0);
        if(!deviations)return;
        cad::mates::CylindricalFitReview result;
        std::string issue;
        if(!cad::mates::reviewCylindricalFit(2*observed[bore].radiusMm,
                2*observed[shaft].radiusMm,(*deviations)[0],(*deviations)[1],
                (*deviations)[2],(*deviations)[3],&result,&issue)){
            QMessageBox::warning(&dialog,"Cylindrical fit",QString::fromStdString(issue));return;
        }
        const QString type=result.alwaysClearance?"CLEARANCE":
            result.alwaysInterference?"INTERFERENCE":"TRANSITION / LIMIT CONTACT";
        QMessageBox::information(&dialog,"Read-only diametral fit worksheet",
            QStringLiteral("Classification: %1\nNominal clearance: %2 mm\n"
               "Minimum possible: %3 mm\nMaximum possible: %4 mm\n\n"
               "This uses only the manually identified bore/shaft and entered tolerances. "
               "It does not create a mate or certify manufacturability.")
               .arg(type).arg(result.nominalDiametralClearanceMm,0,'g',12)
               .arg(result.smallestPossibleClearanceMm,0,'g',12)
               .arg(result.largestPossibleClearanceMm,0,'g',12));
    });
    QDoubleSpinBox* fields[3][2][3]{};
    QLabel* lineLabels[3][2]{};
    const double origins[2][3]={{parent->placementX,parent->placementY,parent->placementZ},
                                {child->placementX,child->placementY,child->placementZ}};
    for(int point=0;point<3;++point)for(int side=0;side<2;++side){
        auto* line=new QWidget(&dialog);auto* layout=new QHBoxLayout(line);
        layout->setContentsMargins(0,0,0,0);
        for(int axis=0;axis<3;++axis){
            auto* spin=new QDoubleSpinBox(line);spin->setDecimals(3);
            spin->setRange(-1000000,1000000);spin->setSuffix(" mm");
            spin->setValue(origins[side][axis]+(point==1&&axis==0?10:0)+
                           (point==2&&axis==1?10:0));
            fields[point][side][axis]=spin;layout->addWidget(spin);
        }
        auto* label=new QLabel(&dialog);
        lineLabels[point][side]=label;
        form->addRow(label,line);
    }
    auto* planeOffset=new QDoubleSpinBox(&dialog);
    planeOffset->setRange(-1000000.0,1000000.0);
    planeOffset->setDecimals(4);planeOffset->setSuffix(" mm signed offset");
    planeOffset->setToolTip("Signed distance from the parent plane toward its normal to the child plane. "
        "The other constraints must lock all six degrees of freedom.");
    form->addRow("Third plane offset (mode 3)",planeOffset);
    const auto updateMateMode=[&](int){
        const int mode=mateMode->currentData().toInt();
        planeOffset->setEnabled(mode==2);
        if(mode!=2)planeOffset->setValue(0.0);
        for(int point=0;point<3;++point){
            const QString type=point==0?QStringLiteral("Point"):
                mode==0?QStringLiteral("Point"):
                (point==2&&mode==2)?QStringLiteral("Plane normal"):QStringLiteral("Axis direction");
            for(int side=0;side<2;++side){
                lineLabels[point][side]->setText(QStringLiteral("%1 %2 %3")
                    .arg(type).arg(point+1).arg(side==0?"parent":"child"));
                for(int axis=0;axis<3;++axis){
                    auto* field=fields[point][side][axis];
                    const bool direction=mode!=0&&point>0;
                    field->setSuffix(direction?QString():QStringLiteral(" mm"));
                    field->setDecimals(direction?5:3);
                    field->setValue(direction?(axis==(point==1?0:1)?1.0:0.0):
                        origins[side][axis]+(point==1&&axis==0?10:0)+
                        (point==2&&axis==1?10:0));
                }
            }
        }
    };
    connect(mateMode,QOverload<int>::of(&QComboBox::currentIndexChanged),&dialog,updateMateMode);
    updateMateMode(0);
    connect(facePrefill,&QPushButton::clicked,&dialog,[this,&dialog,ids,mateMode,fields,planeOffset]{
        const auto references=viewport_->selectedFaceReferences();
        if(references.size()!=2){
            QMessageBox::warning(&dialog,"Planar mate prefill",
                "Select exactly one planar face on each of the two selected component occurrences.");return;
        }
        cad::mates::PlanarFaceObservation observations[2];
        for(int side=0;side<2;++side){
            const auto it=std::find_if(references.cbegin(),references.cend(),
                [&](const auto& reference){return reference.partId==ids[side];});
            if(it==references.cend()){
                QMessageBox::warning(&dialog,"Planar mate prefill",
                    "Selected faces must belong to the chosen parent and child occurrences.");return;
            }
            const TopoDS_Shape shape=selectedSubshape(document_,it->partId,
                it->geometryToken,it->faceIndex,TopAbs_FACE);
            QString issue;
            if(shape.IsNull()||!cad::mates::describeSelectedPlanarFace(
                    TopoDS::Face(shape),&observations[side],&issue)){
                QMessageBox::warning(&dialog,"Planar mate prefill",
                    issue.isEmpty()?QStringLiteral("Face selection is stale."):issue);return;
            }
        }
        // Explicitly key the in-plane twist and both translations: planar
        // contact alone is NOT six-DOF constrained. No geometry is moved here.
        if(QMessageBox::question(&dialog,"Constrain remaining planar freedoms",
            "Two planar faces alone permit sliding and twisting. Prefill will add "
            "a world-aligned in-plane guide and center-to-center lock. Review or "
            "edit these observations before solving. Continue?",
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        mateMode->setCurrentIndex(2);
        planeOffset->setValue(0.0);
        for(int side=0;side<2;++side){
            const auto& obs=observations[side];
            const auto setRow=[&](int row,const cad::mates::Vec3& v){
                fields[row][side][0]->setValue(v.x);
                fields[row][side][1]->setValue(v.y);
                fields[row][side][2]->setValue(v.z);
            };
            setRow(0,obs.center);
            setRow(1,obs.worldGuide);
            cad::mates::Vec3 faceNormal=obs.normal;
            if(side==1){faceNormal.x=-faceNormal.x;faceNormal.y=-faceNormal.y;faceNormal.z=-faceNormal.z;}
            setRow(2,faceNormal);
        }
        QMessageBox::information(&dialog,"Geometry prefill complete",
            "Physical face centers and normals were loaded. The guide imposes an "
            "additional user-defined twist lock, and the point pair locks sliding. "
            "Inspect the full constraint set and any signed offset before committing.");
    });
    connect(cylinderPrefill,&QPushButton::clicked,&dialog,
            [this,&dialog,ids,mateMode,fields,planeOffset]{
        const auto references=viewport_->selectedFaceReferences();
        if(references.size()!=2){
            QMessageBox::warning(&dialog,"Concentric mate prefill",
                "Select exactly one cylindrical face on each selected component occurrence.");return;
        }
        cad::mates::CylindricalFaceObservation observed[2];
        for(int side=0;side<2;++side){
            int found=0;
            for(const auto& reference:references){
                if(reference.partId!=ids[side])continue;
                ++found;
                if(found!=1)break;
                const TopoDS_Shape shape=selectedSubshape(document_,reference.partId,
                    reference.geometryToken,reference.faceIndex,TopAbs_FACE);
                QString issue;
                if(shape.IsNull()||!cad::mates::describeSelectedCylindricalFace(
                        TopoDS::Face(shape),&observed[side],&issue)){
                    QMessageBox::warning(&dialog,"Concentric mate prefill",
                        issue.isEmpty()?QStringLiteral("Cylindrical face selection is stale."):issue);
                    return;
                }
            }
            if(found!=1){
                QMessageBox::warning(&dialog,"Concentric mate prefill",
                    "Select one cylindrical face on the parent and one on the child.");return;
            }
        }
        if(QMessageBox::question(&dialog,"Lock remaining concentric freedoms",
            QStringLiteral("Concentric cylinders alone allow axial sliding and rotation. "
                "The prefill adds an explicit axial center lock and a world-guide "
                "twist lock. These are extra user constraints, not properties of "
                "the cylinders. Radii: %1 mm and %2 mm. Review clearance and "
                "all entries before solving. Continue?")
                .arg(observed[0].radiusMm,0,'f',4)
                .arg(observed[1].radiusMm,0,'f',4),
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        mateMode->setCurrentIndex(1); // Point + two oriented axes (fully locked)
        planeOffset->setValue(0.0);
        for(int side=0;side<2;++side){
            const auto& item=observed[side];
            const auto setRow=[&](int row,const cad::mates::Vec3& value){
                fields[row][side][0]->setValue(value.x);
                fields[row][side][1]->setValue(value.y);
                fields[row][side][2]->setValue(value.z);
            };
            setRow(0,item.axisPoint);
            setRow(1,item.axisDirection);
            setRow(2,item.worldGuide);
        }
        QMessageBox::information(&dialog,"Concentric observations loaded",
            "Selected B-rep cylinder axes were measured. Axial center alignment "
            "and guide rotation are additional user-imposed locks. They can be "
            "edited or cancelled before any geometry is changed.");
    });
    auto* locked=new QCheckBox("Keep solved placement as a persistent rigid parent-child joint",&dialog);
    locked->setChecked(true);form->addRow(locked);
    auto* retainLive=new QCheckBox(
        "Store editable local-frame mates instead (refreshable after moving components)",&dialog);
    form->addRow(retainLive);
    if(document_.hasLiveRigidMateNetwork()){
        auto* networkHint=new QLabel("An existing live mate graph will be EXTENDED (not replaced). "
            "To change a connection, remove it through the Assembly menu first.",&dialog);
        networkHint->setWordWrap(true);form->addRow(networkHint);
    }
    connect(retainLive,&QCheckBox::toggled,&dialog,[locked](bool enabled){
        if(enabled)locked->setChecked(false);
        locked->setEnabled(!enabled);
    });
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    form->addRow(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    std::vector<cad::mates::Constraint> constraints;
    const auto vec=[&](int point,int side){return cad::mates::Vec3{
        fields[point][side][0]->value(),fields[point][side][1]->value(),fields[point][side][2]->value()};};
    const int mode=mateMode->currentData().toInt();
    cad::mates::Vec3 parentOrigin=vec(0,0);
    // In the point+axis+plane preset, the point also pins translation.
    // Shift its target along the chosen parent plane normal to make a nonzero
    // plane offset consistent rather than introducing contradictory mates.
    if(mode==2){
        const auto normal=vec(2,0);
        parentOrigin.x+=normal.x*planeOffset->value();
        parentOrigin.y+=normal.y*planeOffset->value();
        parentOrigin.z+=normal.z*planeOffset->value();
    }
    constraints.push_back({cad::mates::Kind::Point,parentOrigin,vec(0,1),{},{} });
    for(int point=1;point<3;++point){
        if(mode==0){
            constraints.push_back({cad::mates::Kind::Point,vec(point,0),vec(point,1),{},{} });
        }else{
            const auto kind=point==2&&mode==2?cad::mates::Kind::CoincidentPlane:
                                                        cad::mates::Kind::CoaxialAxis;
            // Direction observations share the first point as their axis/plane anchor.
            cad::mates::Constraint mate{kind,mode==2&&point==1?parentOrigin:vec(0,0),
                                        vec(0,1),vec(point,0),vec(point,1)};
            if(kind==cad::mates::Kind::CoincidentPlane)mate.offsetMm=planeOffset->value();
            constraints.push_back(mate);
        }
    }
    QString error;
    if(retainLive->isChecked()){
        cad::SceneDocument::AssemblyNetworkMate mate;
        mate.parentBodyId=ids[0];mate.childBodyId=ids[1];
        mate.constraints=constraints;
        const bool extending=document_.hasLiveRigidMateNetwork();
        if(extending){
            if(!document_.appendLiveRigidMateConnection(mate,&error)){
                reportError("Extend live frame mate network",error);return;
            }
        }else if(!document_.setLiveRigidMateNetwork(ids[0],{mate},&error)){
            reportError("Save live frame mates",error);return;
        }
        refresh(true);
        statusBar()->showMessage(extending?
            "Extended saved rigid mate graph; inspect/remove individual links in Assembly menu.":
            "Saved local-frame mate network; add other selected occurrence pairs using this dialog.",9000);
    }else{
        if(!document_.solveAssemblyMates(ids[0],ids[1],constraints,locked->isChecked(),&error)){
            reportError("Assembly mate solve",error);return;
        }
        refresh(true);
        statusBar()->showMessage("Solved assembly mates in a single undo step.",9000);
    }
}

void MainWindow::editInteractiveSketch(){
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Sketch canvas","Select one constrained polygon extrusion.");return;}
    const cad::Part* part=document_.find(ids.first());
    if(!part||!part->hasConstrainedPolygonDefinition){
        reportError("Sketch canvas","This body does not own a constrained polygon sketch.");return;
    }
    if(part->constrainedPolygonGeometryToken!=part->geometryToken){
        reportError("Sketch canvas","The sketch link is stale after a direct edit. The existing geometry is preserved.");return;
    }
    SketchCanvasDialog dialog(this);
    dialog.setSketch(part->constrainedPolygonSketch,part->constrainedPolygonBoundary,
                     part->constrainedPolygonDepthMm,part->constrainedPolygonPlane);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.updateConstrainedPolygonExtrusion(ids.first(),dialog.sketch(),dialog.boundary(),dialog.depth(),&error,dialog.plane())){
        reportError("Update sketch extrusion",error);return;
    }
    refresh();statusBar()->showMessage("Updated constrained sketch and extrusion in one undo step.",8000);
}

// Compact authoring UI for the most frequently used mixed-DAG operations.
// The document API supports the full validated ShapeFeatureTree operator set;
// this dialog deliberately exposes a conservative subset without requiring
// users to edit raw JSON or write a plugin.
void MainWindow::editInterleavedFeatureHistory(){
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Mixed feature history","Select exactly one solid body in the browser.");return;}
    const QString target=ids.first();
    const cad::Part* part=document_.find(target);
    if(!part||part->isSurfaceBody||part->hasConstrainedPolygonDefinition||
       part->hasShapeFeatureTree||part->hasLinkedBoolean||part->hasLinkedSketchExtrusion||
       part->hasInPlaceSketchFeature||part->hasParametricDefinition||
       part->hasFoldedSheetMetalDefinition||!part->componentDefinitionId.isEmpty()||
       !part->assemblyJointType.isEmpty()){
        reportError("Mixed feature history",
            "Choose an ordinary solid without another feature definition, assembly occurrence, or joint.");
        return;
    }
    QVector<cad::InterleavedHistoryStep> steps=part->interleavedSteps;
    QDialog dialog(this);dialog.setWindowTitle("Mixed associative history — "+part->name);
    dialog.resize(930,530);
    auto* layout=new QVBoxLayout(&dialog);
    auto* help=new QLabel("Create live sketch bosses/pockets and branch solid operations. "
        "Each step can use the previous result, the frozen base, or an earlier named step. "
        "The final step becomes the model. Failed rebuilds leave the current body untouched.",&dialog);
    help->setWordWrap(true);layout->addWidget(help);
    auto* list=new QListWidget(&dialog);list->setAlternatingRowColors(true);layout->addWidget(list,1);
    auto* row=new QHBoxLayout;
    auto* add=new QPushButton("Add step",&dialog);
    auto* editStep=new QPushButton("Edit step",&dialog);
    auto* batchParameters=new QPushButton("Batch edit driving parameters",&dialog);
    auto* erase=new QPushButton("Remove step",&dialog);
    auto* moveUp=new QPushButton("Move up",&dialog);
    auto* moveDown=new QPushButton("Move down",&dialog);
    for(auto* button:{add,editStep,batchParameters,erase,moveUp,moveDown})row->addWidget(button);
    layout->addLayout(row);
    auto* stagedAudit=new QPushButton("Preflight staged history (read-only)",&dialog);
    stagedAudit->setToolTip("Rebuilds staged steps in memory; checks B-rep validity, stage volume and first failing prefix without editing the document.");
    layout->addWidget(stagedAudit);
    auto* feedback=new QLabel("At least one external constrained-sketch step is required.",&dialog);
    feedback->setWordWrap(true);layout->addWidget(feedback);
    auto* controls=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);
    layout->addWidget(controls);
    const auto redraw=[&](int selected){
        list->clear();
        for(int i=0;i<steps.size();++i){
            const auto& step=steps[i];
            const QString type=step.kind==QStringLiteral("Shape")?step.feature.type:step.kind;
            const QString parents=step.inputs.isEmpty()?QStringLiteral("previous"):
                step.inputs.join(QStringLiteral(" + "));
            QString details;
            if(step.kind!=QStringLiteral("Shape")){
                const cad::Part* sketchSource=document_.find(step.sketchSourceId);
                details=QStringLiteral("  sketch %1  depth %2 mm  offset %3 mm")
                    .arg(sketchSource?sketchSource->name:step.sketchSourceId)
                    .arg(step.depthMm,0,'g',6).arg(step.offsetMm,0,'g',6);
            }
            if(!step.referenceSourceId.isEmpty())
                details+=QStringLiteral("  tracked from %1").arg(step.referenceSourceId.left(10));
            if(step.referencePatternOccurrence!=0)
                details+=QStringLiteral("  legacy occurrence %1").arg(step.referencePatternOccurrence);
            if(!step.referencePatternOccurrences.isEmpty()){
                QStringList instances;
                for(auto it=step.referencePatternOccurrences.cbegin();
                    it!=step.referencePatternOccurrences.cend();++it)
                    instances.append(QStringLiteral("%1:%2").arg(it.key().left(8)).arg(it.value()));
                details+=QStringLiteral("  nested occurrences [%1]").arg(instances.join(","));
            }
            list->addItem(QStringLiteral("%1. %2  [%3]  from %4%5")
                .arg(i+1).arg(type,step.id.left(8),parents,details));
        }
        if(selected>=0&&selected<steps.size())list->setCurrentRow(selected);
        feedback->setText(QStringLiteral("%1 steps staged. Save rebuilds this body and its dependents atomically.")
            .arg(steps.size()));
    };
    // Restrict bulk parameter editing to numeric driving inputs. Face/edge
    // indices, kernel witnesses and pattern-reference identities are not
    // dimensions, even when their serialized representation is numeric.
    const auto topologyKey=[](const QString& key){
        return key.contains("anchor_")||key.contains("witness_")||
            key.startsWith("curved_")||key.startsWith("reference_")||
            key.startsWith("face_")||key.startsWith("edge_")||
            key.endsWith("_index")||key.endsWith("_version");
    };
    const auto openEditor=[&](int index)->bool{
        if(index>=steps.size())return false;
        if(steps.size()>=40&&index<0){
            QMessageBox::warning(&dialog,"History limit","A mixed feature history supports at most 40 steps.");return false;
        }
        const bool existing=index>=0;
        const cad::InterleavedHistoryStep original=existing?steps[index]:cad::InterleavedHistoryStep{};
        const QString selectedType=!existing?QStringLiteral("SketchFuse"):
            original.kind==QStringLiteral("Shape")?original.feature.type:original.kind;
        const QStringList operations={"SketchFuse","SketchCut","Translate","Rotate Z",
            "Uniform scale","Linear pattern X","Linear pattern Y","Linear pattern Z",
            "Circular pattern Z","Fuse branches","Subtract branches","Intersect branches",
            "Fillet selected edge","Chamfer selected edge",
            "Fillet selected edges","Chamfer selected edges",
            "Face normal pocket","Face normal boss","Open-face shell",
            "Sweep circle path","Loft rectangles"};
        if(existing&&!operations.contains(selectedType)){
            // Preserve advanced operator identity, history edges and topology
            // witnesses. The compact editor only knows a subset of operation
            // families, but every saved numeric driving parameter can be
            // edited through a guarded table without direct JSON modification.
            QDialog advanced(&dialog);
            advanced.setWindowTitle("Edit advanced feature parameters: "+selectedType);
            advanced.resize(710,510);
            auto* advancedLayout=new QVBoxLayout(&advanced);
            auto* caution=new QLabel(
                "Edit driving numeric parameters only. Stable feature identity, "
                "dependency inputs and captured topology witnesses stay unchanged. "
                "A full read-only rebuild must succeed before this edit is staged. "
                "Some topology-changing edits still require reference reselection.",&advanced);
            caution->setWordWrap(true);advancedLayout->addWidget(caution);
            auto* parameterTable=new QTableWidget(&advanced);
            parameterTable->setColumnCount(2);
            parameterTable->setHorizontalHeaderLabels({"Parameter", "Numeric value"});
            parameterTable->horizontalHeader()->setStretchLastSection(true);
            parameterTable->setRowCount(original.feature.parameters.size());
            QStringList parameterKeys;
            int rowIndex=0;
            for(auto it=original.feature.parameters.cbegin();
                it!=original.feature.parameters.cend();++it,++rowIndex){
                const QString key=it.key();parameterKeys.append(key);
                auto* name=new QTableWidgetItem(key);
                name->setFlags(name->flags()&~Qt::ItemIsEditable);
                parameterTable->setItem(rowIndex,0,name);
                auto* value=new QTableWidgetItem(QString::number(it.value(),'g',17));
                const bool witness=topologyKey(key);
                if(witness){
                    value->setFlags(value->flags()&~Qt::ItemIsEditable);
                    value->setToolTip("Topology witness: automatically captured, not a driving dimension.");
                }
                parameterTable->setItem(rowIndex,1,value);
            }
            advancedLayout->addWidget(parameterTable);
            auto* advancedButtons=new QDialogButtonBox(
                QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&advanced);
            advancedLayout->addWidget(advancedButtons);
            connect(advancedButtons,&QDialogButtonBox::accepted,&advanced,&QDialog::accept);
            connect(advancedButtons,&QDialogButtonBox::rejected,&advanced,&QDialog::reject);
            if(advanced.exec()!=QDialog::Accepted)return false;
            auto proposed=steps;
            QMap<QString,double> nextParameters=original.feature.parameters;
            for(int row=0;row<parameterKeys.size();++row){
                const QString key=parameterKeys[row];
                const auto* field=parameterTable->item(row,1);
                bool ok=false;
                const double numeric=field?field->text().trimmed().toDouble(&ok):0.0;
                if(!ok||!std::isfinite(numeric)||std::abs(numeric)>1e9){
                    QMessageBox::warning(&dialog,"Invalid feature parameter",
                        QStringLiteral("Parameter '%1' must be a finite number within ±1e9.")
                            .arg(key));return false;
                }
                // Always preserve witness metadata exactly, even if table
                // data is changed indirectly or a future widget permits it.
                const bool witness=topologyKey(key);
                if(!witness)nextParameters[key]=numeric;
            }
            if(nextParameters==original.feature.parameters)return false;
            proposed[index].feature.parameters=std::move(nextParameters);
            QMap<QString,TopoDS_Shape> stagedShapes;
            QString why;
            if(!document_.previewInterleavedFeatureHistory(target,proposed,&stagedShapes,&why)){
                QMessageBox::warning(&dialog,"Advanced edit rejected",
                    why+"\n\nThe saved solid and staged history are unchanged.");return false;
            }
            const TopoDS_Shape finalShape=stagedShapes.value(proposed.last().id);
            if(finalShape.IsNull()||!BRepCheck_Analyzer(finalShape).IsValid()){
                QMessageBox::warning(&dialog,"Invalid rebuilt feature",
                    "The edited timeline did not produce a valid final B-rep.");return false;
            }
            steps=std::move(proposed);
            redraw(index);
            feedback->setText("Advanced parameter edit staged and verified; Save commits the full history atomically.");
            return true;
        }
        QDialog editor(&dialog);editor.setWindowTitle(existing?"Edit mixed feature step":"Add mixed feature step");
        // The full engineering parameter set must fit laptop displays. Keep
        // the confirmation controls visible while the form itself scrolls.
        editor.resize(680,700);
        auto* shellLayout=new QVBoxLayout(&editor);
        auto* scroll=new QScrollArea(&editor);
        scroll->setWidgetResizable(true);
        scroll->setMinimumHeight(340);
        auto* formBody=new QWidget(scroll);
        auto* form=new QFormLayout(formBody);
        scroll->setWidget(formBody);
        shellLayout->addWidget(scroll,1);
        auto* operation=new QComboBox(&editor);operation->addItems(operations);
        operation->setCurrentText(selectedType);form->addRow("Operation",operation);
        auto* parent=new QComboBox(&editor);parent->addItem("Previous result",QString());
        parent->addItem("Frozen base",QStringLiteral("@base"));
        auto* second=new QComboBox(&editor);second->addItem("Choose second parent",QString());
        second->addItem("Frozen base",QStringLiteral("@base"));
        const int upper=existing?index:steps.size();
        for(int i=0;i<upper;++i){
            const auto& previous=steps[i];
            const QString label=QStringLiteral("%1  %2").arg(i+1).arg(previous.id.left(10));
            parent->addItem(label,previous.id);second->addItem(label,previous.id);
        }
        form->addRow("First parent",parent);form->addRow("Second parent (Boolean only)",second);
        if(existing&&!original.inputs.isEmpty()){
            const int first=parent->findData(original.inputs.first());
            if(first>=0)parent->setCurrentIndex(first);
            if(original.inputs.size()>1){
                const int other=second->findData(original.inputs[1]);
                if(other>=0)second->setCurrentIndex(other);
            }
        }
        auto* sketch=new QComboBox(&editor);
        for(const cad::Part& candidate:document_.parts())
            if(candidate.id!=target&&candidate.hasConstrainedPolygonDefinition&&
               candidate.constrainedPolygonGeometryToken==candidate.geometryToken)
                sketch->addItem(candidate.name,candidate.id);
        if(existing){const int found=sketch->findData(original.sketchSourceId);
            if(found>=0)sketch->setCurrentIndex(found);}
        form->addRow("Constrained sketch source",sketch);
        const auto number=[&](const QString& text,double min,double max,double initial){
            auto* input=new QDoubleSpinBox(&editor);input->setRange(min,max);input->setDecimals(4);
            input->setValue(initial);form->addRow(text,input);return input;
        };
        const QMap<QString,double> parameters=original.feature.parameters;
        auto* x=number("X (mm)",-1e6,1e6,parameters.value("x"));
        auto* y=number("Y (mm)",-1e6,1e6,parameters.value("y"));
        auto* z=number("Z (mm)",-1e6,1e6,parameters.value("z"));
        auto* angle=number("Rotation (degrees)",-360,360,parameters.value("degrees"));
        auto* factor=number("Uniform scale",0.0001,1000,parameters.value("factor",1.0));
        auto* patternCount=new QSpinBox(&editor);patternCount->setRange(2,24);
        patternCount->setValue(int(parameters.value("count",3)));form->addRow("Pattern instances",patternCount);
        auto* patternSpacing=number("Linear pattern spacing (mm)",-1e6,1e6,
            parameters.value("spacing",20.0));
        auto* patternSweep=number("Circular pattern sweep (degrees)",0.001,360,
            parameters.value("sweep_degrees",360.0));
        auto* depth=number("Sketch depth (mm)",0.0001,1e6,existing?original.depthMm:5.0);
        auto* offset=number("Sketch normal offset (mm)",-1e6,1e6,existing?original.offsetMm:8.0);
        auto* edgeSize=number("Edge fillet radius / chamfer size (mm)",0.00001,1e6,
            parameters.value("radius",parameters.value("distance",0.2)));
        auto* faceU=number("Face U location (mm)",-1e6,1e6,parameters.value("u"));
        auto* faceV=number("Face V location (mm)",-1e6,1e6,parameters.value("v"));
        auto* faceRadius=number("Face hole/boss radius (mm)",0.00001,1e6,
            parameters.value("radius",1.0));
        auto* faceDepth=number("Face hole/boss depth (mm)",0.00001,1e6,
            parameters.value("depth",2.0));
        auto* shellThickness=number("Shell wall thickness (mm)",0.00001,1e6,
            parameters.value("thickness",1.0));
        auto* sweepRadius=number("Sweep profile radius (mm)",0.00001,1e6,
            parameters.value("radius",2.0));
        auto* sweepLength=number("Sweep path length (mm)",0.00001,1e6,
            parameters.value("path_length",30.0));
        auto* sweepLateral=number("Sweep lateral displacement (mm)",-1e6,1e6,
            parameters.value("lateral_offset",4.0));
        auto* sweepRise=number("Sweep vertical displacement (mm)",-1e6,1e6,
            parameters.value("vertical_rise",5.0));
        auto* loftWidth0=number("Loft first width (mm)",0.00001,1e6,parameters.value("width0",10.0));
        auto* loftDepth0=number("Loft first depth (mm)",0.00001,1e6,parameters.value("depth0",10.0));
        auto* loftWidth1=number("Loft last width (mm)",0.00001,1e6,parameters.value("width1",8.0));
        auto* loftDepth1=number("Loft last depth (mm)",0.00001,1e6,parameters.value("depth1",8.0));
        auto* loftHeight=number("Loft height (mm)",0.00001,1e6,parameters.value("height",15.0));
        auto* loftOffsetX=number("Loft X offset (mm)",-1e6,1e6,parameters.value("offset_x",0.0));
        auto* loftOffsetY=number("Loft Y offset (mm)",-1e6,1e6,parameters.value("offset_y",0.0));
        const TopoDS_Shape referenceBase=part->hasInterleavedHistory?part->interleavedBase:part->shape;
        auto* referenceSource=new QComboBox(&editor);
        referenceSource->addItem("Frozen base",QStringLiteral("@base"));
        for(int i=0;i<upper;++i)
            referenceSource->addItem(QStringLiteral("%1: %2").arg(i+1).arg(steps[i].id.left(12)),steps[i].id);
        if(existing&&!original.referenceSourceId.isEmpty()){
            const int known=referenceSource->findData(original.referenceSourceId);
            if(known>=0)referenceSource->setCurrentIndex(known);
        }
        form->addRow("Reference geometry from",referenceSource);
        // Each nested pattern has an independent stable feature-ID selection.
        // Zero is the seed. Old v4 single-occurrence data is displayed on its
        // one pattern stage; accepting the edit upgrades it to a v5 map.
        auto* occurrenceTable=new QTableWidget(&editor);
        occurrenceTable->setColumnCount(2);
        occurrenceTable->setHorizontalHeaderLabels({"Pattern stage", "Instance (0=seed)"});
        occurrenceTable->horizontalHeader()->setStretchLastSection(true);
        occurrenceTable->verticalHeader()->hide();
        occurrenceTable->setMaximumHeight(152);
        occurrenceTable->setMinimumHeight(65);
        QMap<QString,QSpinBox*> occurrenceSpins;
        QStringList availablePatterns;
        for(int i=0;i<upper;++i){
            if(steps[i].kind!=QStringLiteral("Shape")||
               !cad::supportsPatternReferenceLineage(steps[i].feature))continue;
            availablePatterns.append(steps[i].id);
            const int row=occurrenceTable->rowCount();occurrenceTable->insertRow(row);
            occurrenceTable->setItem(row,0,new QTableWidgetItem(
                QStringLiteral("%1  %2").arg(i+1).arg(steps[i].id.left(10))));
            auto* instance=new QSpinBox(occurrenceTable);
            instance->setRange(0,std::min(23,int(steps[i].feature.parameters.value("count",24))-1));
            int saved=original.referencePatternOccurrences.value(steps[i].id,0);
            if(original.referencePatternOccurrences.isEmpty()&&
               original.referencePatternOccurrence!=0&&availablePatterns.size()==1)
                saved=original.referencePatternOccurrence;
            instance->setValue(saved);
            occurrenceTable->setCellWidget(row,1,instance);
            occurrenceSpins.insert(steps[i].id,instance);
        }
        form->addRow("Tracked pattern instances",occurrenceTable);
        auto* referenceEdge=new QComboBox(&editor);
        form->addRow("Frozen-base edge / intermediate edge",referenceEdge);
        auto* referenceFace=new QComboBox(&editor);
        form->addRow("Intermediate face (planar or curved)",referenceFace);
        auto* ancestryHint=new QLabel(
            "Choose a reference source and a downstream first parent. "
            "Kernel-recorded Booleans, selected edge finishing, cylinder cuts, "
            "and additive loft/sweep fusions can carry verified source descendants. "
            "For a loft- or sweep-generated face, choose that same stage as both "
            "reference source and parent: its witness will be rechecked at rebuild. "
            "A direct-stage witness does not establish ancestry through another operation. "
            "Every branch route must agree; removed or ambiguous geometry fails.",&editor);
        ancestryHint->setWordWrap(true);form->addRow(ancestryHint);
        auto* previewStatus=new QLabel(&editor);previewStatus->setWordWrap(true);
        form->addRow("Preview",previewStatus);
        const auto selectedReferenceShape=[&](TopoDS_Shape* geometry,QString* reason)->bool{
            const QString sourceId=referenceSource->currentData().toString();
            if(sourceId==QStringLiteral("@base")){*geometry=referenceBase;return true;}
            int sourceIndex=-1;
            for(int i=0;i<upper;++i)if(steps[i].id==sourceId){sourceIndex=i;break;}
            if(sourceIndex<0){if(reason)*reason="The reference stage does not precede this feature.";return false;}
            QMap<QString,TopoDS_Shape> preview;
            if(!document_.previewInterleavedFeatureHistory(target,steps.mid(0,sourceIndex+1),&preview,reason))return false;
            *geometry=preview.value(sourceId);
            return !geometry->IsNull();
        };
        const auto refreshReferences=[&]{
            referenceEdge->clear();referenceFace->clear();
            TopoDS_Shape geometry;QString reason;
            if(!selectedReferenceShape(&geometry,&reason)){
                previewStatus->setText("Cannot preview source: "+reason);return;
            }
            TopTools_IndexedMapOfShape edges;
            TopExp::MapShapes(geometry,TopAbs_EDGE,edges);
            for(int edge=1;edge<=edges.Extent();++edge){
                QMap<QString,double> hint;
                if(!cad::captureStableEdgeAnchor(geometry,edge,&hint,nullptr)||
                   !hint.contains("edge_anchor_version"))continue;
                GProp_GProps props;BRepGProp::LinearProperties(edges.FindKey(edge),props);
                const gp_Pnt center=props.CentreOfMass();
                referenceEdge->addItem(QStringLiteral("Edge %1 · (%2, %3, %4) mm · length %5 mm")
                    .arg(edge).arg(center.X(),0,'g',4).arg(center.Y(),0,'g',4)
                    .arg(center.Z(),0,'g',4).arg(props.Mass(),0,'g',4),edge);
            }
            int face=0;
            for(TopExp_Explorer scan(geometry,TopAbs_FACE);scan.More();scan.Next()){
                ++face;
                QMap<QString,double> hint;
                const bool planar=cad::capturePlanarFaceAnchor(geometry,face,&hint,nullptr)&&
                    hint.contains("face_anchor_witness_x");
                const bool curved=!planar&&operation->currentText()==QStringLiteral("Open-face shell")&&
                    cad::captureCurvedFaceAnchor(geometry,face,&hint,nullptr);
                if(!planar&&!curved)continue;
                const QString prefix=planar?QStringLiteral("Planar"):QStringLiteral("Curved");
                const QString center=planar?QStringLiteral("face_anchor_"):QStringLiteral("curved_witness_");
                referenceFace->addItem(QStringLiteral("%1 face %2 · (%3, %4, %5) mm")
                    .arg(prefix).arg(face).arg(hint.value(center+"x"),0,'g',4)
                    .arg(hint.value(center+"y"),0,'g',4)
                    .arg(hint.value(center+"z"),0,'g',4),face);
            }
            if(existing&&original.referenceSourceId==referenceSource->currentData().toString()){
                const int knownEdge=referenceEdge->findData(int(parameters.value("edge_index")));
                if(knownEdge>=0)referenceEdge->setCurrentIndex(knownEdge);
                const int knownFace=referenceFace->findData(int(parameters.value("face_index")));
                if(knownFace>=0)referenceFace->setCurrentIndex(knownFace);
            }
            previewStatus->setText(QStringLiteral("Preview valid: %1 witnessed edges, %2 selectable faces.")
                .arg(referenceEdge->count()).arg(referenceFace->count()));
        };
        connect(referenceSource,qOverload<int>(&QComboBox::currentIndexChanged),&editor,
            [&](int){refreshReferences();});
        refreshReferences();
        const auto update=[&]{
            const QString type=operation->currentText();
            const bool isSketch=type==QStringLiteral("SketchFuse")||type==QStringLiteral("SketchCut");
            const bool isBoolean=type.endsWith(QStringLiteral(" branches"));
            const bool isEdge=type==QStringLiteral("Fillet selected edge")||
                              type==QStringLiteral("Chamfer selected edge");
            const bool isFace=type==QStringLiteral("Face normal pocket")||
                              type==QStringLiteral("Face normal boss");
            const bool isShell=type==QStringLiteral("Open-face shell");
            parent->setEnabled(true);second->setEnabled(isBoolean);
            sketch->setEnabled(isSketch);depth->setEnabled(isSketch);offset->setEnabled(isSketch);
            referenceSource->setEnabled(isEdge||isFace||isShell);
            occurrenceTable->setEnabled(isEdge||isFace||isShell);
            referenceEdge->setEnabled(isEdge);edgeSize->setEnabled(isEdge);
            referenceFace->setEnabled(isFace||isShell);
            for(auto* field:{faceU,faceV,faceRadius,faceDepth})field->setEnabled(isFace);
            shellThickness->setEnabled(isShell);
            const bool isSweep=type==QStringLiteral("Sweep circle path");
            const bool isLoft=type==QStringLiteral("Loft rectangles");
            for(auto* field:{sweepRadius,sweepLength,sweepLateral,sweepRise})field->setEnabled(isSweep);
            for(auto* field:{loftWidth0,loftDepth0,loftWidth1,loftDepth1,
                             loftHeight,loftOffsetX,loftOffsetY})field->setEnabled(isLoft);
            ancestryHint->setVisible(isEdge||isFace||isShell);
            for(auto* input:{x,y,z})input->setEnabled(type==QStringLiteral("Translate"));
            angle->setEnabled(type==QStringLiteral("Rotate Z"));
            factor->setEnabled(type==QStringLiteral("Uniform scale"));
            const bool isPattern=type.startsWith(QStringLiteral("Linear pattern "))||
                type==QStringLiteral("Circular pattern Z");
            patternCount->setEnabled(isPattern);
            patternSpacing->setEnabled(type.startsWith(QStringLiteral("Linear pattern ")));
            patternSweep->setEnabled(type==QStringLiteral("Circular pattern Z"));
        };
        connect(operation,qOverload<int>(&QComboBox::currentIndexChanged),&editor,
            [&](int){refreshReferences();update();});
        update();
        auto* dialogButtons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&editor);
        shellLayout->addWidget(dialogButtons);
        connect(dialogButtons,&QDialogButtonBox::accepted,&editor,&QDialog::accept);
        connect(dialogButtons,&QDialogButtonBox::rejected,&editor,&QDialog::reject);
        if(editor.exec()!=QDialog::Accepted)return false;
        const QString type=operation->currentText();
        const bool isBoolean=type.endsWith(QStringLiteral(" branches"));
        const bool isSketch=type==QStringLiteral("SketchFuse")||type==QStringLiteral("SketchCut");
        cad::InterleavedHistoryStep next;
        next.id=existing?original.id:QUuid::createUuid().toString(QUuid::WithoutBraces);
        next.kind=isSketch?type:QStringLiteral("Shape");
        next.feature.id=next.id;next.feature.type=isSketch?QString():type;
        next.feature.suppressed=existing&&!isSketch&&!isBoolean&&original.feature.suppressed;
        if(!parent->currentData().toString().isEmpty())next.inputs.append(parent->currentData().toString());
        if(isBoolean){
            const QString other=second->currentData().toString();
            if(next.inputs.isEmpty()||other.isEmpty()||other==next.inputs.first()){
                QMessageBox::warning(&dialog,"Invalid branch inputs","Select two distinct earlier feature steps or the frozen base.");
                return false;
            }
            next.inputs.append(other);
        }
        if(isSketch){
            if(sketch->count()==0){
                QMessageBox::warning(&dialog,"Missing sketch source",
                    "Create a current constrained polygon sketch body before adding a live sketch stage.");return false;
            }
            next.sketchSourceId=sketch->currentData().toString();
            next.depthMm=depth->value();next.offsetMm=offset->value();
        }else if(type==QStringLiteral("Translate")){
            next.feature.parameters={{"x",x->value()},{"y",y->value()},{"z",z->value()}};
        }else if(type==QStringLiteral("Rotate Z")){
            next.feature.parameters={{"degrees",angle->value()}};
        }else if(type==QStringLiteral("Uniform scale")){
            next.feature.parameters={{"factor",factor->value()}};
        }else if(type.startsWith(QStringLiteral("Linear pattern "))){
            next.feature.parameters={{"count",double(patternCount->value())},
                {"spacing",patternSpacing->value()}};
        }else if(type==QStringLiteral("Circular pattern Z")){
            next.feature.parameters={{"count",double(patternCount->value())},
                {"sweep_degrees",patternSweep->value()}};
        }else if(type==QStringLiteral("Sweep circle path")){
            next.feature.parameters={{"radius",sweepRadius->value()},
                {"path_length",sweepLength->value()},
                {"lateral_offset",sweepLateral->value()},
                {"vertical_rise",sweepRise->value()}};
        }else if(type==QStringLiteral("Loft rectangles")){
            next.feature.parameters={{"width0",loftWidth0->value()},
                {"depth0",loftDepth0->value()},
                {"width1",loftWidth1->value()},
                {"depth1",loftDepth1->value()},
                {"height",loftHeight->value()},
                {"offset_x",loftOffsetX->value()},
                {"offset_y",loftOffsetY->value()}};
        }else if(type==QStringLiteral("Fillet selected edge")||
                 type==QStringLiteral("Chamfer selected edge")||
                 type==QStringLiteral("Face normal pocket")||
                 type==QStringLiteral("Face normal boss")||
                 type==QStringLiteral("Open-face shell")){
            if(next.inputs.size()!=1){
                QMessageBox::warning(&dialog,"Missing tracked parent",
                    "Select one explicit downstream parent for the referenced feature.");return false;
            }
            const QString parentId=next.inputs.first();
            const QString sourceId=referenceSource->currentData().toString();
            // The current stage may itself own the witnessed face or edge.
            // Such references are revalidated directly instead of inventing
            // an upstream Boolean/fillet/loft relationship.
            if(parentId==sourceId){
                for(auto it=occurrenceSpins.cbegin();it!=occurrenceSpins.cend();++it){
                    if(it.value()->value()!=0){
                        QMessageBox::warning(&dialog,"Invalid instance selection",
                            "Direct-stage references cannot include upstream pattern selections.");
                        return false;
                    }
                }
            }
            TopoDS_Shape referenceGeometry;QString reason;
            if(!selectedReferenceShape(&referenceGeometry,&reason)){
                QMessageBox::warning(&dialog,"Invalid reference preview",reason);return false;
            }
            QMap<QString,double> anchor;
            const bool isEdge=type==QStringLiteral("Fillet selected edge")||
                              type==QStringLiteral("Chamfer selected edge");
            if(isEdge){
                if(referenceEdge->currentIndex()<0||
                   !cad::captureStableEdgeAnchor(referenceGeometry,
                       referenceEdge->currentData().toInt(),&anchor,&reason)){
                    QMessageBox::warning(&dialog,"Unsupported edge reference",reason);return false;
                }
                next.feature.parameters.insert("edge_index",referenceEdge->currentData().toInt());
                next.feature.parameters.insert(type==QStringLiteral("Fillet selected edge")?
                    QStringLiteral("radius"):QStringLiteral("distance"),edgeSize->value());
            }else{
                if(referenceFace->currentIndex()<0){
                    QMessageBox::warning(&dialog,"Missing face reference","Select a face from the preview.");return false;
                }
                const int faceIndex=referenceFace->currentData().toInt();
                const bool planar=cad::capturePlanarFaceAnchor(referenceGeometry,
                    faceIndex,&anchor,&reason)&&anchor.contains("face_anchor_witness_x");
                const bool curved=!planar&&type==QStringLiteral("Open-face shell")&&
                    cad::captureCurvedFaceAnchor(referenceGeometry,faceIndex,&anchor,&reason);
                if(!planar&&!curved){
                    QMessageBox::warning(&dialog,"Unsupported face reference",reason);return false;
                }
                if(type==QStringLiteral("Open-face shell")){
                    next.feature.parameters={{"face_index",double(faceIndex)},
                        {"thickness",shellThickness->value()}};
                }else{
                    next.feature.parameters={{"face_index",double(faceIndex)},
                        {"u",faceU->value()},{"v",faceV->value()},
                        {"radius",faceRadius->value()},{"depth",faceDepth->value()}};
                }
            }
            next.referenceSourceId=sourceId;
            for(auto it=occurrenceSpins.cbegin();it!=occurrenceSpins.cend();++it){
                if(it.value()->value()>0)
                    next.referencePatternOccurrences.insert(it.key(),it.value()->value());
            }
            // Do not keep the v4 positional occurrence if the editable v5
            // selections have been explicitly captured.
            next.referencePatternOccurrence=0;
            for(auto it=anchor.cbegin();it!=anchor.cend();++it)
                next.feature.parameters.insert(it.key(),it.value());
        }
        if(existing)steps[index]=std::move(next);
        else steps.append(std::move(next));
        redraw(existing?index:steps.size()-1);
        return true;
    };
    connect(batchParameters,&QPushButton::clicked,&dialog,[&]{
        if(steps.isEmpty()){
            QMessageBox::warning(&dialog,"Batch feature parameters","Add a history step first.");return;
        }
        QDialog editor(&dialog);
        editor.setWindowTitle("Batch edit history driving parameters");
        editor.resize(900,570);
        auto* panel=new QVBoxLayout(&editor);
        auto* description=new QLabel("Edit several driving dimensions together. Feature IDs, graph inputs "
            "and persistent face/edge witnesses cannot be changed here. All proposed operations "
            "must regenerate successfully before any value is staged.",&editor);
        description->setWordWrap(true);panel->addWidget(description);
        auto* fields=new QTableWidget(0,4,&editor);
        fields->setHorizontalHeaderLabels({"Step","Feature","Driving parameter","Value"});
        fields->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        QVector<QPair<int,QString>> cellKeys;
        for(int step=0;step<steps.size();++step){
            const auto& feature=steps[step];
            if(feature.kind!=QStringLiteral("Shape"))continue;
            for(auto it=feature.feature.parameters.cbegin();it!=feature.feature.parameters.cend();++it){
                if(topologyKey(it.key()))continue;
                if(cellKeys.size()>=500){
                    QMessageBox::warning(&dialog,"Batch feature parameters",
                        "A batch may contain at most 500 driving fields.");return;
                }
                const int row=fields->rowCount();fields->insertRow(row);
                auto locked=[](const QString& value){
                    auto* item=new QTableWidgetItem(value);
                    item->setFlags(item->flags()&~Qt::ItemIsEditable);
                    return item;
                };
                fields->setItem(row,0,locked(QString::number(step+1)));
                fields->setItem(row,1,locked(feature.feature.type));
                fields->setItem(row,2,locked(it.key()));
                fields->setItem(row,3,new QTableWidgetItem(QString::number(it.value(),'g',17)));
                cellKeys.append(qMakePair(step,it.key()));
            }
        }
        if(cellKeys.isEmpty()){
            QMessageBox::information(&dialog,"Batch feature parameters",
                "The staged history has no numeric driving parameters that can be changed safely.");return;
        }
        panel->addWidget(fields);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&editor);
        panel->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::accepted,&editor,&QDialog::accept);
        connect(buttons,&QDialogButtonBox::rejected,&editor,&QDialog::reject);
        if(editor.exec()!=QDialog::Accepted)return;
        auto proposed=steps;
        int changes=0;
        for(int row=0;row<cellKeys.size();++row){
            const auto& [step,key]=cellKeys[row];
            const auto* item=fields->item(row,3);
            bool ok=false;
            const double value=item?item->text().trimmed().toDouble(&ok):0.0;
            if(!ok||!std::isfinite(value)||std::abs(value)>1e9){
                QMessageBox::warning(&dialog,"Invalid driving parameter",
                    QString("Step %1, '%2': enter a finite number within ±1e9.").arg(step+1).arg(key));
                return;
            }
            if(proposed[step].feature.parameters.value(key)!=value){
                proposed[step].feature.parameters[key]=value;++changes;
            }
        }
        if(changes==0){feedback->setText("No driving parameters changed; no revision staged.");return;}
        QMap<QString,TopoDS_Shape> geometry;
        QString issue;
        if(!document_.previewInterleavedFeatureHistory(target,proposed,&geometry,&issue)){
            QMessageBox::warning(&dialog,"Batch geometry rejected",
                issue+"\nThe saved and staged feature history are unchanged.");return;
        }
        const TopoDS_Shape shape=geometry.value(proposed.last().id);
        if(shape.IsNull()||!BRepCheck_Analyzer(shape).IsValid()){
            QMessageBox::warning(&dialog,"Invalid batch result",
                "The proposed feature history did not produce a valid final solid.");return;
        }
        // Verify EVERY intermediate B-rep, not only the final body. The
        // user can inspect how edits affect later stages before staging them.
        QStringList assessment{
            QString("Changed driving fields: %1; history stages: %2").arg(changes).arg(proposed.size()),
            "Read-only OCCT rebuild; no change to the current document.",
            "Stage | Type | proposed volume (mm^3) | validity"};
        for(int i=0;i<proposed.size();++i){
            const auto& stage=proposed[i];
            const TopoDS_Shape intermediate=geometry.value(stage.id);
            if(intermediate.IsNull()||!BRepCheck_Analyzer(intermediate).IsValid()){
                QMessageBox::warning(&dialog,"Invalid intermediate history",
                    QString("Step %1 [%2] has an invalid intermediate B-rep. No changes staged.")
                        .arg(i+1).arg(stage.id));return;
            }
            GProp_GProps volume;
            BRepGProp::VolumeProperties(intermediate,volume);
            if(!std::isfinite(volume.Mass())){
                QMessageBox::warning(&dialog,"Invalid intermediate volume",
                    QString("Step %1 has a nonfinite volume. No changes staged.").arg(i+1));return;
            }
            assessment<<QString("%1 | %2 | %3 | valid")
                .arg(i+1).arg(stage.kind).arg(volume.Mass(),0,'g',12);
        }
        assessment<<"Accept to stage all changes; Save remains the only commit operation.";
        if(QMessageBox::question(&dialog,"Review complete parametric history rebuild",
                assessment.join('\n'),QMessageBox::Yes|QMessageBox::No,
                QMessageBox::No)!=QMessageBox::Yes)return;
        steps=std::move(proposed);
        redraw(list->currentRow());
        feedback->setText(QString("%1 driving values staged after full per-stage validation; Save commits one atomic rebuild.").arg(changes));
    });
    // Reordering and removal also change the implicit feature graph. Never
    // stage them before the kernel has validated every proposed output.
    const auto preflightHistoryStructure=[&](
        const QVector<cad::InterleavedHistoryStep>& proposed,const QString& operation)->bool{
        if(proposed.isEmpty()){
            QMessageBox::warning(&dialog,operation+" rejected",
                "A mixed-feature history must retain a constrained-sketch stage. "
                "Create a replacement before removing the last step.");return false;
        }
        QMap<QString,TopoDS_Shape> rebuilt;
        QString issue;
        if(!document_.previewInterleavedFeatureHistory(target,proposed,&rebuilt,&issue)){
            QMessageBox::warning(&dialog,operation+" rejected",
                issue+"\nNo staged or committed history was changed.");return false;
        }
        for(int i=0;i<proposed.size();++i){
            const TopoDS_Shape intermediate=rebuilt.value(proposed[i].id);
            if(intermediate.IsNull()||!BRepCheck_Analyzer(intermediate).IsValid()){
                QMessageBox::warning(&dialog,operation+" rejected",
                    QString("Stage %1 has no valid regenerated B-rep.").arg(i+1));return false;
            }
            GProp_GProps properties;BRepGProp::VolumeProperties(intermediate,properties);
            if(!std::isfinite(properties.Mass())){
                QMessageBox::warning(&dialog,operation+" rejected",
                    QString("Stage %1 has non-finite regenerated volume.").arg(i+1));return false;
            }
        }
        return true;
    };
    connect(add,&QPushButton::clicked,&dialog,[&]{openEditor(-1);});
    connect(editStep,&QPushButton::clicked,&dialog,[&]{
        if(list->currentRow()>=0)openEditor(list->currentRow());
    });
    connect(erase,&QPushButton::clicked,&dialog,[&]{
        const int at=list->currentRow();if(at<0)return;
        const QString removed=steps[at].id;
        QStringList dependents;
        for(int i=at+1;i<steps.size();++i){
            if(steps[i].inputs.contains(removed)||steps[i].referenceSourceId==removed)
                dependents.append(QStringLiteral("%1 (%2)").arg(steps[i].id.left(10),steps[i].kind));
        }
        if(!dependents.isEmpty()){
            QMessageBox::warning(&dialog,"Referenced feature cannot be removed",
                "Update or remove these dependent features first: "+dependents.join(", "));
            return;
        }
        if(at+1<steps.size()&&steps[at+1].inputs.isEmpty()&&
           QMessageBox::question(&dialog,"Sequential dependency will change",
               "The next step implicitly uses this result. Removing it changes downstream geometry. Continue?",
               QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        auto proposed=steps;
        proposed.removeAt(at);
        if(!preflightHistoryStructure(proposed,"Remove feature step"))return;
        QString dependencyReport,dependencyIssue;
        if(!document_.previewInterleavedHistoryWithDependents(target,proposed,
                &dependencyReport,&dependencyIssue)){
            QMessageBox::warning(&dialog,"Dependent-body history rejection",
                dependencyIssue+"\nNo edit was staged or committed.");return;
        }
        steps=std::move(proposed);
        redraw(std::min(at,int(steps.size())-1));
    });
    const auto reorder=[&](int at,int other){
        if(at<0||other<0||at>=steps.size()||other>=steps.size())return;
        auto proposed=steps;proposed.swapItemsAt(at,other);
        QSet<QString> earlier{QStringLiteral("@base")};
        for(const auto& step:proposed){
            for(const QString& required:step.inputs){
                if(!earlier.contains(required)){
                    QMessageBox::warning(&dialog,"Invalid feature order",
                        "This move would place a feature before one of its required inputs.");return;
                }
            }
            if(!step.referenceSourceId.isEmpty()&&!earlier.contains(step.referenceSourceId)){
                QMessageBox::warning(&dialog,"Invalid reference order",
                    "This move would place a referenced feature before its geometry source.");return;
            }
            earlier.insert(step.id);
        }
        if((steps[at].inputs.isEmpty()||steps[other].inputs.isEmpty())&&
           QMessageBox::question(&dialog,"Sequential feature order",
               "Reordering a step with an implicit previous-result input may change the geometry. Continue?",
               QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        if(!preflightHistoryStructure(proposed,"Reorder feature steps"))return;
        QString dependencyReport,dependencyIssue;
        if(!document_.previewInterleavedHistoryWithDependents(target,proposed,
                &dependencyReport,&dependencyIssue)){
            QMessageBox::warning(&dialog,"Dependent-body history rejection",
                dependencyIssue+"\nNo edit was staged or committed.");return;
        }
        steps=std::move(proposed);redraw(other);
    };
    connect(moveUp,&QPushButton::clicked,&dialog,[&]{reorder(list->currentRow(),list->currentRow()-1);});
    connect(moveDown,&QPushButton::clicked,&dialog,[&]{reorder(list->currentRow(),list->currentRow()+1);});
    connect(stagedAudit,&QPushButton::clicked,&dialog,[&]{
        if(steps.isEmpty()){
            QMessageBox::warning(&dialog,"Staged history preflight","Add at least one step before running the preview.");
            return;
        }
        QMap<QString,TopoDS_Shape> snapshots;
        QString issue;
        const bool complete=document_.previewInterleavedFeatureHistory(target,steps,&snapshots,&issue);
        QStringList lines;
        lines<<QStringLiteral("Read-only stage audit for %1 staged feature(s).").arg(steps.size());
        lines<<"The saved body, dependency graph, and undo history are NOT modified."<<QString();
        if(complete){
            lines<<"Rebuild: SUCCESS; inspecting every returned stage:";
            for(int i=0;i<steps.size();++i){
                const auto& step=steps[i];
                const auto shape=snapshots.value(step.id);
                if(shape.IsNull()){
                    lines<<QStringLiteral("%1. %2: MISSING OUTPUT").arg(i+1).arg(step.id);continue;
                }
                const bool valid=BRepCheck_Analyzer(shape).IsValid();
                GProp_GProps mass;BRepGProp::VolumeProperties(shape,mass);
                lines<<QStringLiteral("%1. %2 [%3]: %4, volume %5 mm^3")
                    .arg(i+1).arg(step.id,step.kind)
                    .arg(valid?QStringLiteral("valid B-rep"):QStringLiteral("INVALID B-rep"))
                    .arg(mass.Mass(),0,'g',12);
            }
        }else{
            lines<<"Rebuild: FAILED"<<issue;
            int firstSketch=-1;
            for(int i=0;i<steps.size();++i)if(steps[i].kind!=QStringLiteral("Shape")){
                firstSketch=i;break;
            }
            if(firstSketch>=0){
                for(int i=firstSketch;i<steps.size();++i){
                    QMap<QString,TopoDS_Shape> partial;
                    QString prefixIssue;
                    if(!document_.previewInterleavedFeatureHistory(target,steps.mid(0,i+1),&partial,&prefixIssue)){
                        lines<<QStringLiteral("First failing testable prefix ends at step %1 [%2]: %3")
                            .arg(i+1).arg(steps[i].id,prefixIssue);
                        break;
                    }
                }
            }else lines<<"At least one constrained-sketch stage is required for this history type.";
        }
        QDialog viewer(&dialog);viewer.setWindowTitle("Staged history geometry preflight");viewer.resize(790,480);
        auto* stack=new QVBoxLayout(&viewer);
        auto* report=new QPlainTextEdit(lines.join('\n'),&viewer);report->setReadOnly(true);
        stack->addWidget(report);
        auto* close=new QDialogButtonBox(QDialogButtonBox::Close,&viewer);
        connect(close,&QDialogButtonBox::rejected,&viewer,&QDialog::reject);
        stack->addWidget(close);viewer.exec();
    });
    connect(controls,&QDialogButtonBox::accepted,&dialog,[&]{
        QString issue;
        QString dependencyReport;
        if(!document_.previewInterleavedHistoryWithDependents(target,steps,
                &dependencyReport,&issue)){
            QMessageBox::warning(&dialog,"Full dependency rebuild rejected",issue);return;
        }
        if(!document_.setInterleavedFeatureHistory(target,steps,&issue)){
            feedback->setText("Rebuild failed: "+issue);
            QMessageBox::warning(&dialog,"Mixed history rejected",issue+"\nThe previous solid is unchanged.");
            return;
        }
        dialog.accept();
    });
    connect(controls,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    redraw(steps.isEmpty()?-1:0);
    if(dialog.exec()==QDialog::Accepted){
        refresh();statusBar()->showMessage("Mixed associative feature history rebuilt and saved as one undo step.",8000);
    }
}

void MainWindow::editGeneralFeatureTree(){
    QStringList ids=selectedIds();
    const auto pickedFaces=viewport_->selectedFaceReferences();
    const auto pickedEdges=viewport_->selectedEdgeReferences();
    if(ids.size()!=1&&pickedFaces.size()==1)ids={pickedFaces.first().partId};
    if(ids.size()!=1&&!pickedEdges.isEmpty()){
        const QString candidate=pickedEdges.first().partId;
        bool same=true;
        for(const auto& selected:pickedEdges)if(selected.partId!=candidate)same=false;
        if(same)ids={candidate};
    }
    if(ids.size()!=1){reportError("Feature graph","Select one body in the browser, one face, or one edge.");return;}
    const QString id=ids.first();
    const cad::Part* body=document_.find(id);
    if(!body)return;
    if(body->hasShapeFeatureTree&&!document_.shapeFeatureTreeCurrent(id)){
        const auto choice=QMessageBox::question(this,"Stale feature stack",
            "The body was modified outside its stored feature stack. Rebase onto its current geometry and discard the old stack? This is undoable.",
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No);
        if(choice!=QMessageBox::Yes)return;
        QString error;if(!document_.rebaseShapeFeatureTree(id,&error)){
            reportError("Rebase feature stack",error);return;
        }
        body=document_.find(id);
    }
    QVector<cad::ShapeFeatureNode> nodes=body->shapeFeatureNodes;
    // Viewport edge references use a face-exploration index. Feature anchors
    // use the unique TopExp::MapShapes index; translate the selection once.
    const auto selectedUniqueEdge=[&]()->int{
        if(pickedEdges.size()!=1||pickedEdges.first().partId!=id||
           pickedEdges.first().geometryToken!=body->geometryToken)return 0;
        const TopoDS_Shape edge=selectedSubshape(document_,id,body->geometryToken,
            pickedEdges.first().edgeIndex,TopAbs_EDGE);
        if(edge.IsNull())return 0;
        TopTools_IndexedMapOfShape unique;TopExp::MapShapes(body->shape,TopAbs_EDGE,unique);
        for(int i=1;i<=unique.Extent();++i)if(unique.FindKey(i).IsSame(edge))return i;
        return 0;
    };
    const int pickedUniqueEdgeIndex=selectedUniqueEdge();
    // Selected edges have face-exploration indices; never store those directly
    // in the persistent feature history. Convert every edge once, reject
    // duplicates and reject stale/mixed-body selections before staging.
    QVector<int> pickedUniqueEdgeIndices;
    bool validEdgeGroup=!pickedEdges.isEmpty()&&pickedEdges.size()<=32;
    if(validEdgeGroup){
        TopTools_IndexedMapOfShape unique;
        TopExp::MapShapes(body->shape,TopAbs_EDGE,unique);
        for(const auto& selected:pickedEdges){
            if(selected.partId!=id||selected.geometryToken!=body->geometryToken){
                validEdgeGroup=false;break;
            }
            const TopoDS_Shape edge=selectedSubshape(document_,id,body->geometryToken,
                selected.edgeIndex,TopAbs_EDGE);
            int index=0;
            if(!edge.IsNull())for(int i=1;i<=unique.Extent();++i)
                if(unique.FindKey(i).IsSame(edge)){index=i;break;}
            if(index<=0||pickedUniqueEdgeIndices.contains(index)){
                validEdgeGroup=false;break;
            }
            pickedUniqueEdgeIndices.append(index);
        }
    }
    if(!validEdgeGroup)pickedUniqueEdgeIndices.clear();
    QDialog dialog(this);dialog.setWindowTitle("General feature tree — "+body->name);
    dialog.resize(970,590);
    auto* layout=new QVBoxLayout(&dialog);
    auto* explanation=new QLabel(
        "Dependency graph: operations can take the previous result, the frozen body, or any earlier feature. "
        "Fuse/Subtract combines two branches. Feature IDs survive save and reopen; later-node references are rejected. "
        "Sketch edits replay all modifiers in one undo step. Select one face or edge before opening this dialog "
        "to capture a persistent geometry anchor on newly appended face/edge features. "
        "Unanchored operations use numeric topology indices; ambiguous matches are rejected. "
        "Group fillets/chamfers save 2-32 selected edge witnesses as one editable, atomic operation.",&dialog);
    explanation->setWordWrap(true);layout->addWidget(explanation);
    auto* featureFilter=new QLineEdit(&dialog);
    featureFilter->setPlaceholderText("Find existing features by name, parameter or ID...");
    featureFilter->setClearButtonEnabled(true);
    layout->addWidget(featureFilter);
    auto* list=new QListWidget(&dialog);list->setAlternatingRowColors(true);
    layout->addWidget(list,1);
    const auto applyFeatureFilter=[&]{
        const auto terms=featureFilter->text().simplified().split(' ',Qt::SkipEmptyParts);
        int firstVisible=-1;
        for(int index=0;index<list->count();++index){
            QListWidgetItem* item=list->item(index);
            bool match=true;
            for(const QString& term:terms)
                if(!item->text().contains(term,Qt::CaseInsensitive)){
                    match=false;break;
                }
            item->setHidden(!match);
            if(match&&firstVisible<0)firstVisible=index;
        }
        if(list->currentItem()&&list->currentItem()->isHidden())
            list->setCurrentRow(firstVisible);
        else if(!list->currentItem()&&firstVisible>=0)
            list->setCurrentRow(firstVisible);
    };
    connect(featureFilter,&QLineEdit::textChanged,&dialog,applyFeatureFilter);
    auto* tools=new QHBoxLayout;
    auto* add=new QPushButton("Add feature",&dialog);
    auto* edit=new QPushButton("Edit dimensions",&dialog);
    auto* sourceEdit=new QPushButton("Edit sources",&dialog);
    auto* suppress=new QPushButton("Suppress / restore",&dialog);
    auto* up=new QPushButton("Move up",&dialog);
    auto* down=new QPushButton("Move down",&dialog);
    auto* remove=new QPushButton("Remove",&dialog);
    for(auto* button:{add,edit,sourceEdit,suppress,up,down,remove})tools->addWidget(button);
    layout->addLayout(tools);
    auto* validate=new QPushButton("Validate staged geometry (dry run)",&dialog);
    validate->setToolTip("Build the proposed feature graph without changing the document.");
    layout->addWidget(validate);
    auto* feedback=new QLabel("All feature inputs are in millimetres except angles (degrees) and scale (unitless).",&dialog);
    feedback->setWordWrap(true);layout->addWidget(feedback);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);
    layout->addWidget(buttons);
    const auto refreshRows=[&](int index){
        list->clear();
        for(int i=0;i<nodes.size();++i){
            const auto& node=nodes[i];QStringList fields;
            for(auto it=node.parameters.cbegin();it!=node.parameters.cend();++it){
                const QString key=it.key();
                if(key.startsWith("face_anchor_")||
                   (key.startsWith("edge_")&&key!="edge_index"))continue;
                fields.append(QStringLiteral("%1=%2").arg(key).arg(it.value(),0,'g',8));
            }
            const QString sources=node.inputs.isEmpty()?QStringLiteral("previous"):
                node.inputs.join(" + ");
            const QString anchor=node.parameters.contains("face_anchor_x")?QStringLiteral(" [face anchor]"):
                node.parameters.contains("edge_x")?QStringLiteral(" [edge anchor]"):QString();
            list->addItem(QStringLiteral("%1  %2%3   [%4]   from %5   %6").arg(i+1).arg(node.type,anchor,
                node.suppressed?QStringLiteral("suppressed"):QStringLiteral("active"),sources,fields.join(", ")));
        }
        if(index>=0&&index<nodes.size())list->setCurrentRow(index);
        applyFeatureFilter();
        QString graphError;
        if(!cad::validateShapeFeatureNodes(nodes,&graphError)){
            feedback->setText(QStringLiteral("Dependency warning: %1 (changes not saved)").arg(graphError));
            feedback->setStyleSheet("color: #b65b37; font-weight: 600;");
        }else{
            const cad::Part* current=document_.find(id);
            QVector<int> positions;
            if(current&&current->hasShapeFeatureTree&&
               document_.shapeFeatureTreeCurrent(id)&&
               current->shapeFeatureCachedOutputs.size()==current->shapeFeatureNodes.size()){
                QVector<bool> available;
                for(const auto& shape:current->shapeFeatureCachedOutputs)
                    available.append(!shape.IsNull());
                QString reason;
                if(cad::planShapeFeatureReuse(current->shapeFeatureNodes,nodes,
                    available,&positions,&reason)){
                    int reusable=0;
                    for(int old:positions)if(old>=0)++reusable;
                    feedback->setText(QStringLiteral("%1 valid nodes. Planned rebuild: %2 reevaluated, %3 cached outputs reused. Save to commit.")
                        .arg(nodes.size()).arg(nodes.size()-reusable).arg(reusable));
                }else feedback->setText("Dependency planner: "+reason);
            }else{
                feedback->setText(QStringLiteral("%1 valid nodes. No reusable cache; Save builds the tree.").arg(nodes.size()));
            }
            feedback->setStyleSheet(QString());
        }
    };
    const auto names=[](const QString& type){
        if(type=="Translate")return QStringList{"x","y","z"};
        if(type=="Rotate arbitrary axis")return QStringList{"degrees","axis_x","axis_y","axis_z","pivot_x","pivot_y","pivot_z"};
        if(type.startsWith("Rotate "))return QStringList{"degrees"};
        if(type=="Uniform scale")return QStringList{"factor"};
        if(type=="Mirror arbitrary plane")return QStringList{"normal_x","normal_y","normal_z","origin_x","origin_y","origin_z"};
        if(type.startsWith("Mirror "))return QStringList{};
        if(type=="Linear pattern vector")return QStringList{"count","dx","dy","dz"};
        if(type.startsWith("Linear pattern "))return QStringList{"count","spacing"};
        if(type=="Circular pattern arbitrary axis")return QStringList{"count","sweep_degrees","axis_x","axis_y","axis_z","pivot_x","pivot_y","pivot_z"};
        if(type=="Circular pattern Z")return QStringList{"count","sweep_degrees"};
        if(type=="Open-face shell")return QStringList{"face_index","thickness"};
        if(type=="Draft face Z")return QStringList{"face_index","angle_degrees","neutral_z"};
        if(type=="Face rectangle pocket"||type=="Face rectangle boss")
            return QStringList{"face_index","u","v","width","height","depth"};
        if(type=="Fillet all edges")return QStringList{"radius"};
        if(type=="Chamfer all edges")return QStringList{"distance"};
        if(type=="Fillet selected edge")return QStringList{"edge_index","radius"};
        if(type=="Chamfer selected edge")return QStringList{"edge_index","distance"};
        if(type=="Cylinder cut arbitrary axis")
            return QStringList{"x","y","z","axis_x","axis_y","axis_z","radius","depth"};
        if(type=="Cylinder through all arbitrary axis")
            return QStringList{"x","y","z","axis_x","axis_y","axis_z","radius"};
        if(type=="Face hole grid through all")
            return QStringList{"face_index","u","v","radius","columns","rows","pitch_u","pitch_v","rotation_degrees"};
        if(type=="Face bolt circle through all")
            return QStringList{"face_index","u","v","radius","count","pcd","start_angle_degrees"};
        if(type=="Face slot through all")
            return QStringList{"face_index","u","v","radius","center_spacing","rotation_degrees"};
        if(type=="Face slot pocket")
            return QStringList{"face_index","u","v","radius","center_spacing","rotation_degrees","depth"};
        if(type=="Face through-all hole")
            return QStringList{"face_index","u","v","radius"};
        if(type=="Face counterbore through all")
            return QStringList{"face_index","u","v","radius","head_radius","recess_depth"};
        if(type=="Face countersink through all")
            return QStringList{"face_index","u","v","radius","head_radius","included_angle"};
        if(type=="Face normal pocket"||type=="Face normal boss")
            return QStringList{"face_index","u","v","radius","depth"};
        if(type=="Fuse branches"||type=="Subtract branches"||type=="Intersect branches")return QStringList{};
        if(type=="Revolve annulus Z")return QStringList{"inner_radius","outer_radius","height"};
        if(type=="Sweep circle path")return QStringList{"radius","path_length","lateral_offset","vertical_rise"};
        if(type=="Loft rectangles")return QStringList{"width0","depth0","width1","depth1","height","offset_x","offset_y"};
        return QStringList{"x","y","z","radius","depth"};
    };
    // Advanced sketch forms use the same persisted feature-node parameter map,
    // including typed 3D paths, multiple loft stations, and custom axes.
    const auto editAdvancedProfile=[&](cad::ShapeFeatureNode* node)->bool{
        const bool sweep=node->type=="Sketch polygon sweep 3D"||
            node->type=="Sketch polygon sweep spline 3D";
        const bool loft=node->type=="Sketch multi-section loft Z";
        const bool revolve=node->type=="Sketch polygon revolve axis";
        QDialog dialogInput(&dialog);
        dialogInput.setWindowTitle("Advanced sketch feature — "+node->type);
        dialogInput.resize(650,650);
        auto* layout=new QFormLayout(&dialogInput);
        auto* note=new QLabel(sweep?
            "Closed straight polygon in a local plane normal to the first 3D path tangent. The guide is defined in world millimetres. Select a spline operation for a smooth interpolated path. Self-intersecting geometry may be rejected by the solid kernel.":
            loft?"Two to eight XY section polygons, each with matching vertices and increasing Z elevations. Separate sections with --- and begin each with z=<elevation>.":
            "Closed radial/axial (r,h) polygon revolved around a custom axis. Radius must be positive; dimensions are in millimetres.",&dialogInput);
        note->setWordWrap(true);layout->addRow(note);
        auto* polygonText=new QPlainTextEdit(&dialogInput);
        auto parsePair=[&](const QString& text,QVector<QPointF>* result)->bool{
            result->clear();
            for(const QString& line:text.split('\n',Qt::SkipEmptyParts)){
                const QStringList fields=line.split(',');if(fields.size()!=2)return false;
                bool a=false,b=false;
                const double x=fields[0].trimmed().toDouble(&a),y=fields[1].trimmed().toDouble(&b);
                if(!a||!b||!std::isfinite(x)||!std::isfinite(y))return false;
                result->append(QPointF(x,y));
            }
            return result->size()>=3&&result->size()<=24;
        };
        const auto dumpPolygon=[&](const QString& prefix,int count)->QString{
            QStringList rows;
            for(int i=0;i<count;++i){const QString k=prefix+QString::number(i);
                rows<<QStringLiteral("%1, %2").arg(node->parameters.value(k+"_x"),0,'g',12)
                    .arg(node->parameters.value(k+"_y"),0,'g',12);
            }
            return rows.join('\n');
        };
        const int originalCount=static_cast<int>(node->parameters.value("profile_count",0));
        if(!loft){
            polygonText->setPlainText(originalCount>0?dumpPolygon("p",originalCount):
                revolve?"4, 4\n8, 4\n8, 16\n4, 16":"-2, -2\n2, -2\n2, 2\n-2, 2");
            layout->addRow(revolve?"Radial / axial outline":"Local 2D section",polygonText);
        }
        auto* pathText=new QPlainTextEdit(&dialogInput);
        auto* multiText=new QPlainTextEdit(&dialogInput);
        if(sweep){
            QStringList lines;
            const int count=static_cast<int>(node->parameters.value("path_count",0));
            for(int i=0;i<count;++i){const QString k="t"+QString::number(i);
                lines<<QStringLiteral("%1, %2, %3")
                    .arg(node->parameters.value(k+"_x"),0,'g',12)
                    .arg(node->parameters.value(k+"_y"),0,'g',12)
                    .arg(node->parameters.value(k+"_z"),0,'g',12);
            }
            pathText->setPlainText(lines.isEmpty()?(node->type=="Sketch polygon sweep spline 3D"?
                "10, 10, 8\n10, 10, 14\n10, 12, 19\n12, 15, 24":"10, 10, 8\n10, 10, 22"):lines.join('\n'));
            layout->addRow("Path X,Y,Z (one per line)",pathText);
        }
        if(loft){
            QStringList sections;
            const int count=static_cast<int>(node->parameters.value("section_count",0));
            const int pts=static_cast<int>(node->parameters.value("profile_count",0));
            for(int i=0;i<count;++i){const QString k="s"+QString::number(i);
                sections<<QStringLiteral("z=%1\n%2")
                    .arg(node->parameters.value(k+"_z"),0,'g',12)
                    .arg(dumpPolygon(k+"_p",pts));
            }
            multiText->setPlainText(sections.isEmpty()?
                "z=8\n5, 5\n12, 5\n12, 12\n5, 12\n---\nz=14\n3, 3\n14, 3\n14, 14\n3, 14\n---\nz=22\n5, 5\n12, 5\n12, 12\n5, 12":sections.join("\n---\n"));
            layout->addRow("Sections (z=... / x,y / ---)",multiText);
        }
        QMap<QString,QDoubleSpinBox*> axisInputs;
        if(revolve){
            const QStringList fields={"axis_x","axis_y","axis_z","origin_x","origin_y","origin_z","sweep_degrees"};
            for(const auto& key:fields){
                auto* spin=new QDoubleSpinBox(&dialogInput);spin->setDecimals(5);
                spin->setRange(-100000,100000);
                const double fallback=key=="axis_z"?1:key=="sweep_degrees"?360:0;
                spin->setValue(node->parameters.value(key,fallback));
                if(key=="sweep_degrees")spin->setRange(0.001,360);
                layout->addRow(key,spin);axisInputs.insert(key,spin);
            }
        }
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialogInput);
        layout->addRow(buttons);
        connect(buttons,&QDialogButtonBox::accepted,&dialogInput,&QDialog::accept);
        connect(buttons,&QDialogButtonBox::rejected,&dialogInput,&QDialog::reject);
        while(dialogInput.exec()==QDialog::Accepted){
            cad::ShapeFeatureNode attempt=*node;
            attempt.parameters.clear();
            QVector<QPointF> profile;
            if(!loft){
                if(!parsePair(polygonText->toPlainText(),&profile)){
                    QMessageBox::warning(&dialogInput,"Invalid sketch outline","Enter 3–24 vertices, exactly x,y per line.");continue;
                }
                attempt.parameters.insert("profile_count",profile.size());
                for(int i=0;i<profile.size();++i){const QString k="p"+QString::number(i);
                    attempt.parameters.insert(k+"_x",profile[i].x());
                    attempt.parameters.insert(k+"_y",profile[i].y());
                }
            }
            if(sweep){
                QVector<std::array<double,3>> path;
                bool ok=true;
                for(const QString& line:pathText->toPlainText().split('\n',Qt::SkipEmptyParts)){
                    const QStringList parts=line.split(',');if(parts.size()!=3){ok=false;break;}
                    bool a=false,b=false,c=false;
                    const double x=parts[0].trimmed().toDouble(&a);
                    const double y=parts[1].trimmed().toDouble(&b);
                    const double z=parts[2].trimmed().toDouble(&c);
                    if(!a||!b||!c||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)){
                        ok=false;break;
                    }
                    path.append(std::array<double,3>{x,y,z});
                }
                const int minimum=node->type=="Sketch polygon sweep spline 3D"?3:2;
                if(!ok||path.size()<minimum||path.size()>24){
                    QMessageBox::warning(&dialogInput,"Invalid sweep path",
                        QStringLiteral("Enter %1–24 positions, exactly x,y,z per line.").arg(minimum));continue;
                }
                attempt.parameters.insert("path_count",path.size());
                for(int i=0;i<path.size();++i){const QString k="t"+QString::number(i);
                    attempt.parameters.insert(k+"_x",path[i][0]);
                    attempt.parameters.insert(k+"_y",path[i][1]);
                    attempt.parameters.insert(k+"_z",path[i][2]);
                }
            }else if(loft){
                const QStringList blocks=multiText->toPlainText().split("---",Qt::SkipEmptyParts);
                bool ok=blocks.size()>=2&&blocks.size()<=8;
                int common=-1;
                if(ok)for(int i=0;i<blocks.size();++i){
                    const QStringList rows=blocks[i].trimmed().split('\n',Qt::SkipEmptyParts);
                    if(rows.size()<4||!rows[0].trimmed().startsWith("z=")){ok=false;break;}
                    bool zOk=false;
                    const double z=rows[0].trimmed().mid(2).toDouble(&zOk);
                    QVector<QPointF> points;
                    if(!zOk||!parsePair(rows.mid(1).join('\n'),&points)){
                        ok=false;break;
                    }
                    if(common<0)common=points.size();
                    if(common!=points.size()){ok=false;break;}
                    const QString section="s"+QString::number(i);
                    attempt.parameters.insert(section+"_z",z);
                    for(int j=0;j<points.size();++j){const QString key=section+"_p"+QString::number(j);
                        attempt.parameters.insert(key+"_x",points[j].x());
                        attempt.parameters.insert(key+"_y",points[j].y());
                    }
                }
                if(!ok){QMessageBox::warning(&dialogInput,"Invalid loft","Use 2–8 matching polygon sections, each starting with z=..., separated by ---.");continue;}
                attempt.parameters.insert("profile_count",common);
                attempt.parameters.insert("section_count",blocks.size());
            }else if(revolve){
                for(auto it=axisInputs.cbegin();it!=axisInputs.cend();++it)
                    attempt.parameters.insert(it.key(),it.value()->value());
            }
            QString issue;
            if(!cad::validateShapeFeatureNodes({attempt},&issue)){
                QMessageBox::warning(&dialogInput,"Invalid advanced sketch",issue);continue;
            }
            node->parameters=attempt.parameters;
            return true;
        }
        return false;
    };
    // Numeric polyline profiles are stored in the existing feature-parameter map.
    // No schema migration is necessary. The on-screen sketch canvas can author
    // straight-boundary profiles, while text editing allows exact vertex values.
    const auto editPolygon=[&](cad::ShapeFeatureNode* node)->bool{
        const bool loft=node->type=="Sketch polygons loft Z";
        const bool revolve=node->type=="Sketch polygon revolve Z";
        const bool attached=node->type=="Sketch polygon face boss"||node->type=="Sketch polygon face pocket";
        const bool pocket=node->type=="Sketch polygon pocket Z"||node->type=="Sketch polygon face pocket";
        const auto profileText=[&](const QString& prefix)->QString{
            const int n=static_cast<int>(node->parameters.value("profile_count",0));
            QStringList rows;
            for(int i=0;i<n&&i<24;++i){const QString k=prefix+QString::number(i);
                rows << QStringLiteral("%1, %2").arg(node->parameters.value(k+"_x"),0,'g',12)
                       .arg(node->parameters.value(k+"_y"),0,'g',12);
            }
            if(!rows.isEmpty())return rows.join('\n');
            if(revolve)return "4, 4\n8, 4\n8, 16\n4, 16";
            if(attached)return "-4, -4\n4, -4\n4, 4\n-4, 4";
            if(pocket)return "5, 5\n15, 5\n15, 15\n5, 15";
            return "5, 5\n12, 5\n12, 12\n5, 12";
        };
        QDialog input(&dialog);input.setWindowTitle("Editable sketch profile — "+node->type);
        input.resize(620,520);
        auto* form=new QFormLayout(&input);
        auto* help=new QLabel(revolve?
            "One radial/height (r,z) pair per line in mm. Revolve 360° around global Z; r must be positive.":
            attached?"Enter local (x,y) vertices in mm relative to the chosen planar face centroid. "
                     "A selected face can be anchored when appending this feature. Depth follows the face normal (boss) or enters the solid (pocket).":
            "Enter one (x,y) vertex per line in mm. Vertices follow the polygon boundary; closing edge is automatic.",&input);
        help->setWordWrap(true);form->addRow(help);
        auto* first=new QPlainTextEdit(&input);first->setPlainText(profileText("p"));
        form->addRow(loft?"Bottom sketch polygon":"Sketch polygon",first);
        QPlainTextEdit* second=nullptr;
        if(loft){
            second=new QPlainTextEdit(&input);
            if(node->parameters.contains("q0_x"))second->setPlainText(profileText("q"));
            else second->setPlainText("3, 3\n14, 3\n14, 14\n3, 14");
            form->addRow("Top sketch polygon (matching points)",second);
        }
        QDoubleSpinBox* base=nullptr;
        QDoubleSpinBox* height=nullptr;
        QDoubleSpinBox* faceIndex=nullptr;
        QDoubleSpinBox* offsetU=nullptr;
        QDoubleSpinBox* offsetV=nullptr;
        if(attached){
            faceIndex=new QDoubleSpinBox(&input);faceIndex->setRange(1,10000);
            faceIndex->setDecimals(0);
            const int pickedFace=(pickedFaces.size()==1&&pickedFaces.first().partId==id)?
                pickedFaces.first().faceIndex:1;
            faceIndex->setValue(node->parameters.value("face_index",pickedFace));
            form->addRow("Planar face index",faceIndex);
            offsetU=new QDoubleSpinBox(&input);offsetV=new QDoubleSpinBox(&input);
            for(auto* field:{offsetU,offsetV}){
                field->setRange(-100000,100000);field->setDecimals(4);field->setSuffix(" mm");
            }
            offsetU->setValue(node->parameters.value("u",0));
            offsetV->setValue(node->parameters.value("v",0));
            form->addRow("Local U shift",offsetU);form->addRow("Local V shift",offsetV);
            height=new QDoubleSpinBox(&input);height->setRange(0.0001,100000);
            height->setDecimals(4);height->setSuffix(" mm");
            height->setValue(node->parameters.value("depth",5));
            form->addRow("Extrusion depth",height);
        }
        if(!revolve&&!attached){
            base=new QDoubleSpinBox(&input);base->setRange(-100000,100000);
            base->setDecimals(4);base->setSuffix(" mm");
            base->setValue(node->parameters.value("base_z",pocket?1.0:8.0));
            form->addRow("Base Z",base);
            height=new QDoubleSpinBox(&input);height->setRange(0.0001,100000);
            height->setDecimals(4);height->setSuffix(" mm");
            height->setValue(node->parameters.value("height",loft?12.0:10.0));
            form->addRow("Height",height);
        }
        const auto drawProfile=[&](QPlainTextEdit* target){
            SketchCanvasDialog canvas(&input);
            if(canvas.exec()!=QDialog::Accepted)return;
            const auto sketch=canvas.sketch();
            const auto boundary=canvas.boundary();
            if(!sketch.arcs.isEmpty()||!sketch.splines.isEmpty()||!sketch.circles.isEmpty()){
                QMessageBox::warning(&input,"Polyline required","This feature accepts straight closed boundaries only. Remove arcs, splines and circular holes.");
                return;
            }
            QMap<QString,QPointF> coordinates;
            for(const auto& point:sketch.points)coordinates.insert(point.id,QPointF(point.x,point.y));
            QStringList rows;
            for(const QString& id:boundary){
                if(!coordinates.contains(id))return;
                const QPointF xy=coordinates.value(id);
                rows<<QStringLiteral("%1, %2").arg(xy.x(),0,'g',12).arg(xy.y(),0,'g',12);
            }
            if(rows.size()<3||rows.size()>24){
                QMessageBox::warning(&input,"Invalid outline","The closed outline must contain 3 to 24 vertices.");return;
            }
            target->setPlainText(rows.join('\n'));
        };
        auto* drawFirst=new QPushButton("Draw bottom/profile in sketch canvas...",&input);
        form->addRow(drawFirst);
        connect(drawFirst,&QPushButton::clicked,&input,[&]{drawProfile(first);});
        if(loft){auto* drawSecond=new QPushButton("Draw upper section in sketch canvas...",&input);
            form->addRow(drawSecond);connect(drawSecond,&QPushButton::clicked,&input,[&]{drawProfile(second);});}
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&input);
        form->addRow(buttons);
        connect(buttons,&QDialogButtonBox::accepted,&input,&QDialog::accept);
        connect(buttons,&QDialogButtonBox::rejected,&input,&QDialog::reject);
        const auto decode=[&](const QString& content,QVector<QPointF>* points)->bool{
            points->clear();
            for(const QString& line:content.split('\n',Qt::SkipEmptyParts)){
                const QStringList fields=line.split(',');if(fields.size()!=2)return false;
                bool xok=false,yok=false;
                const double x=fields[0].trimmed().toDouble(&xok);
                const double y=fields[1].trimmed().toDouble(&yok);
                if(!xok||!yok||!std::isfinite(x)||!std::isfinite(y))return false;
                points->append(QPointF(x,y));
            }
            return points->size()>=3&&points->size()<=24;
        };
        while(input.exec()==QDialog::Accepted){
            QVector<QPointF> a,b;
            if(!decode(first->toPlainText(),&a)||(loft&&(!decode(second->toPlainText(),&b)||a.size()!=b.size()))){
                QMessageBox::warning(&input,"Invalid polygon","Use 3–24 vertices, exactly x,y per line. Loft sections must have equal counts.");continue;
            }
            auto proposed=*node;
            proposed.parameters.clear();
            proposed.parameters.insert("profile_count",a.size());
            for(int i=0;i<a.size();++i){const QString prefix="p"+QString::number(i);
                proposed.parameters.insert(prefix+"_x",a[i].x());
                proposed.parameters.insert(prefix+"_y",a[i].y());
            }
            if(loft)for(int i=0;i<b.size();++i){const QString prefix="q"+QString::number(i);
                proposed.parameters.insert(prefix+"_x",b[i].x());
                proposed.parameters.insert(prefix+"_y",b[i].y());
            }
            if(attached){
                proposed.parameters.insert("face_index",faceIndex->value());
                proposed.parameters.insert("u",offsetU->value());
                proposed.parameters.insert("v",offsetV->value());
                proposed.parameters.insert("depth",height->value());
                // Keep the face descriptor only if this edit still refers to
                // the same face. Otherwise require an explicit new capture.
                if(node->parameters.value("face_index",-1)==faceIndex->value())
                    for(auto it=node->parameters.cbegin();it!=node->parameters.cend();++it)
                        if(it.key().startsWith("face_anchor_"))
                            proposed.parameters.insert(it.key(),it.value());
            }else if(!revolve){proposed.parameters.insert("base_z",base->value());
                proposed.parameters.insert("height",height->value());}
            QString issue;
            if(!cad::validateShapeFeatureNodes(QVector<cad::ShapeFeatureNode>{proposed},&issue)){
                QMessageBox::warning(&input,"Invalid polygon profile",issue);continue;
            }
            node->parameters=proposed.parameters;
            return true;
        }
        return false;
    };
    const auto editNumbers=[&](cad::ShapeFeatureNode* node)->bool{
        if(node->type=="Sketch polygon sweep 3D"||
           node->type=="Sketch polygon sweep spline 3D"||
           node->type=="Sketch multi-section loft Z"||
           node->type=="Sketch polygon revolve axis")return editAdvancedProfile(node);
        if(node->type.startsWith("Sketch polygon"))return editPolygon(node);
        if(node->type=="Fillet selected edges"||node->type=="Chamfer selected edges"){
            const bool fresh=!node->parameters.contains("edge_count");
            if(fresh&&pickedUniqueEdgeIndices.size()<2){
                QMessageBox::warning(&dialog,"Select a group of edges",
                    "Select 2-32 distinct edges from the same current body in the 3D view, "
                    "then choose the grouped fillet or chamfer operation.");return false;
            }
            QDialog editor(&dialog);
            editor.setWindowTitle(node->type);
            auto* form=new QFormLayout(&editor);
            auto* size=new QDoubleSpinBox(&editor);
            size->setRange(0.00001,100000.0);size->setDecimals(5);
            const QString key=node->type.startsWith("Fillet")?"radius":"distance";
            size->setValue(node->parameters.value(key,1.0));
            form->addRow(QStringLiteral("Default %1 (mm)").arg(key),size);
            auto* individual=new QLineEdit(&editor);
            QStringList currentSizes;
            const int countValue=fresh?pickedUniqueEdgeIndices.size():
                static_cast<int>(node->parameters.value("edge_count",0));
            for(int i=0;i<countValue;++i)
                currentSizes<<QString::number(node->parameters.value(
                    QStringLiteral("member_%1_size").arg(i),size->value()),'g',12);
            individual->setText(currentSizes.join(", "));
            individual->setPlaceholderText("One size per selected edge, in mm");
            form->addRow("Edge-specific sizes (mm)",individual);
            auto* count=new QLabel(fresh?
                QStringLiteral("%1 selected edge(s); group saved with individual witnesses").arg(pickedUniqueEdgeIndices.size()):
                QStringLiteral("%1 persisted edge witnesses (edit size only)").arg(node->parameters.value("edge_count")),&editor);
            count->setWordWrap(true);form->addRow(count);
            auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&editor);
            form->addRow(buttons);
            connect(buttons,&QDialogButtonBox::accepted,&editor,&QDialog::accept);
            connect(buttons,&QDialogButtonBox::rejected,&editor,&QDialog::reject);
            while(editor.exec()==QDialog::Accepted){
                cad::ShapeFeatureNode attempt=*node;
                if(fresh){
                    attempt.parameters.clear();
                    attempt.parameters.insert("edge_count",pickedUniqueEdgeIndices.size());
                    for(int i=0;i<pickedUniqueEdgeIndices.size();++i){
                        QMap<QString,double> witness;QString reason;
                        if(!cad::captureStableEdgeAnchor(body->shape,pickedUniqueEdgeIndices[i],&witness,&reason)||
                           !witness.contains("edge_anchor_version")){
                            QMessageBox::warning(&editor,"Unstable edge witness",
                                QStringLiteral("Edge %1 cannot be tracked persistently: %2")
                                  .arg(i+1).arg(reason));return false;
                        }
                        const QString prefix=QStringLiteral("member_%1_").arg(i);
                        attempt.parameters.insert(prefix+"edge_index",pickedUniqueEdgeIndices[i]);
                        for(auto it=witness.cbegin();it!=witness.cend();++it)
                            attempt.parameters.insert(prefix+it.key(),it.value());
                    }
                }
                attempt.parameters.insert(key,size->value());
                const QStringList entries=individual->text().split(',',Qt::KeepEmptyParts);
                const int expected=static_cast<int>(attempt.parameters.value("edge_count"));
                if(entries.size()!=expected){
                    QMessageBox::warning(&editor,"Incomplete edge sizes",
                        QStringLiteral("Provide exactly %1 comma-separated edge sizes in mm.").arg(expected));
                    continue;
                }
                bool sizesOkay=true;
                for(int i=0;i<expected;++i){
                    bool ok=false;const double value=entries[i].trimmed().toDouble(&ok);
                    if(!ok||!std::isfinite(value)||value<1e-5||value>1e6){sizesOkay=false;break;}
                    attempt.parameters.insert(QStringLiteral("member_%1_size").arg(i),value);
                }
                if(!sizesOkay){
                    QMessageBox::warning(&editor,"Invalid edge size",
                        "Every edge size must be finite and between 0.00001 and 1000000 mm.");continue;
                }
                QString reason;
                if(!cad::validateShapeFeatureNodes({attempt},&reason)){
                    QMessageBox::warning(&editor,"Invalid edge group",reason);continue;
                }
                node->parameters=attempt.parameters;return true;
            }
            return false;
        }
        if(names(node->type).isEmpty()){node->parameters.clear();return true;}
        QDialog input(&dialog);input.setWindowTitle("Feature dimensions: "+node->type);
        auto* form=new QFormLayout(&input);
        if(node->type=="Face through-all hole"||
           node->type=="Face counterbore through all"||
           node->type=="Face countersink through all"||
           node->type.startsWith("Face slot ")||
           node->type=="Face hole grid through all"||
           node->type=="Face bolt circle through all"){
            auto* help=new QLabel(
                "Select a planar stock face before adding this feature. "
                "u/v position the bore relative to the selected face center (mm). "
                "radius and head_radius are RADII (mm), not diameters. "
                "The inward drill direction and through-all depth update on rebuild. "
                "A counterbore uses recess_depth (mm); a countersink uses the "
                "included_angle in degrees. Valid geometry is required; not every "
                "head size and entry position can be machined. "
                "For obround slots, center_spacing is the distance between "
                "semicircle centers and rotation_degrees turns the slot in "
                "the selected face plane. Hole grids use centered rows/columns "
                "and pitches in mm; bolt circles use PCD (pitch circle DIAMETER) "
                "and start_angle_degrees. Every hole must fit the selected face.",&input);
            help->setWordWrap(true);form->addRow(help);
        }
        QMap<QString,QDoubleSpinBox*> fields;
        for(const QString& name:names(node->type)){
            auto* value=new QDoubleSpinBox(&input);
            const bool count=name=="count"||name=="columns"||name=="rows";
            const bool positive=name=="factor"||name=="radius"||name=="distance"||
                name=="depth"||name=="width"||name=="height"||name=="thickness"||
                name=="outer_radius"||name=="inner_radius"||name=="width0"||name=="depth0"||
                name=="width1"||name=="depth1"||name=="path_length"||
                name=="spacing"||name=="sweep_degrees"||name=="head_radius"||
                name=="recess_depth"||name=="included_angle"||
                name=="center_spacing"||name=="pitch_u"||name=="pitch_v"||name=="pcd";
            const bool topoIndex=name=="face_index"||name=="edge_index";
            value->setRange(topoIndex?1:(count?(name=="count"?2:1):(positive?0.0001:-1000000.0)),
                            topoIndex?10000:(count?(name=="count"?24:10):(positive?10000.0:1000000.0)));
            if(name=="sweep_degrees")value->setMaximum(360);
            if(name=="angle_degrees")value->setRange(-45,45);
            if(name=="included_angle")value->setRange(60,120);
            if(name=="rotation_degrees")value->setRange(-180,180);
            if(name=="start_angle_degrees")value->setRange(-360,360);
            value->setDecimals((topoIndex||count)?0:5);
            int faceDefault=1;
            if(pickedFaces.size()==1&&pickedFaces.first().partId==id)
                faceDefault=pickedFaces.first().faceIndex;
            value->setValue(node->parameters.value(name,
                name=="edge_index"?(pickedUniqueEdgeIndex>0?pickedUniqueEdgeIndex:1):
                name=="face_index"?faceDefault:
                name=="factor"?1.0:name=="radius"?2.0:
                name=="center_spacing"?12.0:name=="pitch_u"||name=="pitch_v"?10.0:
                name=="pcd"?14.0:name=="columns"||name=="rows"?2.0:name=="depth"?10.0:
                name=="head_radius"?4.0:name=="recess_depth"?3.0:
                name=="included_angle"?90.0:
                name=="thickness"?1.0:name=="count"?3.0:name=="spacing"?35.0:
                name=="width"||name=="height"||name=="width0"||name=="depth0"||
                name=="width1"||name=="depth1"?10.0:name=="path_length"?25.0:
                name=="outer_radius"?8.0:name=="inner_radius"?4.0:
                name=="sweep_degrees"?360.0:
                name=="axis_z"||name=="normal_z"?1.0:
                name=="dx"?30.0:
                name=="angle_degrees"?5.0:0.0));
            form->addRow(name,value);fields.insert(name,value);
        }
        auto* confirm=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&input);
        form->addRow(confirm);connect(confirm,&QDialogButtonBox::accepted,&input,&QDialog::accept);
        connect(confirm,&QDialogButtonBox::rejected,&input,&QDialog::reject);
        if(input.exec()!=QDialog::Accepted)return false;
        // Preserve captured stable face geometry only if the indexed face is unchanged.
        const auto prior=node->parameters;
        node->parameters.clear();
        for(auto it=fields.cbegin();it!=fields.cend();++it)node->parameters.insert(it.key(),it.value()->value());
        if(prior.value("face_index",-1)==node->parameters.value("face_index",-2))
            for(auto it=prior.cbegin();it!=prior.cend();++it)
                if(it.key().startsWith("face_anchor_"))node->parameters.insert(it.key(),it.value());
        if(prior.value("edge_index",-1)==node->parameters.value("edge_index",-2))
            for(auto it=prior.cbegin();it!=prior.cend();++it)
                if(it.key().startsWith("edge_")&&it.key()!="edge_index")
                    node->parameters.insert(it.key(),it.value());
        return true;
    };
    const auto editSources=[&](cad::ShapeFeatureNode* node,int position)->bool{
        const bool binary=node->type=="Fuse branches"||node->type=="Subtract branches"||node->type=="Intersect branches";
        QDialog popup(&dialog);popup.setWindowTitle("Feature dependency inputs");
        auto* form=new QFormLayout(&popup);
        auto* first=new QComboBox(&popup);
        if(!binary)first->addItem("Previous result (automatic)",QString());
        first->addItem("Frozen base body",QStringLiteral("@base"));
        for(int i=0;i<position&&i<nodes.size();++i)
            first->addItem(QStringLiteral("%1. %2  [%3]").arg(i+1).arg(nodes[i].type,nodes[i].id.left(8)),nodes[i].id);
        const QString current=node->inputs.isEmpty()?QString():node->inputs.first();
        int selection=first->findData(current);
        if(selection<0&&binary)selection=first->count()-1;
        first->setCurrentIndex(std::max(0,selection));form->addRow("Primary source",first);
        QComboBox* second=nullptr;
        if(binary){
            second=new QComboBox(&popup);
            second->addItem("Frozen base body",QStringLiteral("@base"));
            for(int i=0;i<position&&i<nodes.size();++i)
                second->addItem(QStringLiteral("%1. %2  [%3]").arg(i+1).arg(nodes[i].type,nodes[i].id.left(8)),nodes[i].id);
            if(node->inputs.size()==2){const int target=second->findData(node->inputs[1]);
                if(target>=0)second->setCurrentIndex(target);}
            form->addRow("Tool / second source",second);
        }
        auto* hint=new QLabel("Select only earlier feature outputs; a downstream feature cannot depend on itself or a later node.",&popup);
        hint->setWordWrap(true);form->addRow(hint);
        auto* controls=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&popup);
        form->addRow(controls);
        connect(controls,&QDialogButtonBox::accepted,&popup,&QDialog::accept);
        connect(controls,&QDialogButtonBox::rejected,&popup,&QDialog::reject);
        if(popup.exec()!=QDialog::Accepted)return false;
        const QString a=first->currentData().toString();
        const QString b=binary?second->currentData().toString():QString();
        if(binary&&a==b){QMessageBox::warning(&dialog,"Duplicate inputs","Choose two different branch sources.");return false;}
        node->inputs.clear();
        if(binary)node->inputs={a,b};
        else if(!a.isEmpty())node->inputs={a};
        return true;
    };
    connect(add,&QPushButton::clicked,&dialog,[&]{
        const QStringList choices={"Translate","Rotate X","Rotate Y","Rotate Z","Uniform scale",
            "Mirror XY","Mirror XZ","Mirror YZ","Mirror arbitrary plane","Rotate arbitrary axis",
            "Linear pattern X","Linear pattern Y","Linear pattern Z","Linear pattern vector",
            "Circular pattern Z","Circular pattern arbitrary axis",
            "Cylinder cut X","Cylinder cut Y","Cylinder cut Z","Cylinder cut arbitrary axis",
            "Cylinder through all arbitrary axis",
            "Face through-all hole","Face counterbore through all",
            "Face countersink through all","Face slot through all",
            "Face slot pocket","Face hole grid through all",
            "Face bolt circle through all",
            "Face normal pocket","Face normal boss","Face rectangle pocket","Face rectangle boss",
            "Open-face shell","Draft face Z",
            "Revolve annulus Z","Sweep circle path","Loft rectangles",
            "Sketch polygon face boss","Sketch polygon face pocket",
            "Sketch polygon extrude Z","Sketch polygon pocket Z",
            "Sketch polygon revolve Z","Sketch polygons loft Z",
            "Sketch polygon sweep 3D","Sketch polygon sweep spline 3D",
            "Sketch multi-section loft Z",
            "Sketch polygon revolve axis",
            "Fillet all edges","Chamfer all edges",
            "Fillet selected edge","Chamfer selected edge",
            "Fillet selected edges","Chamfer selected edges",
            "Fuse branches","Subtract branches","Intersect branches"};
        QDialog picker(&dialog);
        picker.setWindowTitle("Search geometry features");
        picker.resize(540,520);
        auto* pickerLayout=new QVBoxLayout(&picker);
        auto* query=new QLineEdit(&picker);
        query->setPlaceholderText("Search by operation, e.g. through all, fillet, pocket, loft...");
        pickerLayout->addWidget(query);
        auto* matches=new QListWidget(&picker);
        matches->setAlternatingRowColors(true);
        pickerLayout->addWidget(matches,1);
        auto* acceptButtons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&picker);
        pickerLayout->addWidget(acceptButtons);
        const auto updateChoices=[&]{
            const QString term=query->text().trimmed();
            matches->clear();
            for(const QString& choice:choices){
                // Common workshop names: an obround mounting/adjustment hole
                // is the same profile as a face slot. Keep saved feature IDs
                // unchanged; only expand the searchable vocabulary.
                const QString keywords=choice.startsWith("Face slot ")?
                    choice+" obround capsule adjustment mounting hole":
                    choice=="Face hole grid through all"?choice+" rectangular mounting array pattern plate":
                    choice=="Face bolt circle through all"?choice+" flange PCD pitch circle mounting ring":choice;
                if(term.isEmpty()||choice.contains(term,Qt::CaseInsensitive)||
                   keywords.contains(term,Qt::CaseInsensitive))
                    matches->addItem(choice);
            }
            if(matches->count())matches->setCurrentRow(0);
            acceptButtons->button(QDialogButtonBox::Ok)->setEnabled(matches->count()>0);
        };
        connect(query,&QLineEdit::textChanged,&picker,updateChoices);
        connect(matches,&QListWidget::itemDoubleClicked,&picker,[&](QListWidgetItem*){picker.accept();});
        connect(acceptButtons,&QDialogButtonBox::accepted,&picker,[&]{
            if(matches->currentItem())picker.accept();
        });
        connect(acceptButtons,&QDialogButtonBox::rejected,&picker,&QDialog::reject);
        updateChoices();query->setFocus();
        if(picker.exec()!=QDialog::Accepted||!matches->currentItem())return;
        const QString type=matches->currentItem()->text();
        cad::ShapeFeatureNode node;node.type=type;node.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        if(!editNumbers(&node))return;
        const int position=list->currentRow();
        // Witnesses were taken from the current visible final shape, so a
        // newly captured group MUST be appended after all existing stages.
        const int insert=(type=="Fillet selected edges"||type=="Chamfer selected edges")?
            nodes.size():(position<0?nodes.size():position+1);
        if(insert==nodes.size()&&pickedFaces.size()==1&&pickedFaces.first().partId==id&&
           (type=="Sketch polygon face boss"||type=="Sketch polygon face pocket"||
            type.startsWith("Face normal ")||type.startsWith("Face rectangle ")||
            type=="Face through-all hole"||type=="Face counterbore through all"||
            type=="Face countersink through all"||type.startsWith("Face slot ")||
            type=="Face hole grid through all"||type=="Face bolt circle through all"||
            type=="Open-face shell"||type=="Draft face Z")){
            QMap<QString,double> anchor;QString reason;
            const bool planar=cad::capturePlanarFaceAnchor(body->shape,
                pickedFaces.first().faceIndex,&anchor,&reason);
            const bool curved=!planar&&type==QStringLiteral("Open-face shell")&&
                cad::captureCurvedFaceAnchor(body->shape,pickedFaces.first().faceIndex,&anchor,&reason);
            if(planar||curved)
                for(auto it=anchor.cbegin();it!=anchor.cend();++it)node.parameters.insert(it.key(),it.value());
        }
        if(insert==nodes.size()&&pickedUniqueEdgeIndex>0&&
           (type=="Fillet selected edge"||type=="Chamfer selected edge")&&
           static_cast<int>(node.parameters.value("edge_index"))==pickedUniqueEdgeIndex){
            QMap<QString,double> anchor;QString reason;
            if(cad::captureStableEdgeAnchor(body->shape,pickedUniqueEdgeIndex,&anchor,&reason))
                for(auto it=anchor.cbegin();it!=anchor.cend();++it)
                    node.parameters.insert(it.key(),it.value());
        }
        if((type=="Fuse branches"||type=="Subtract branches"||type=="Intersect branches")&&!editSources(&node,insert))return;
        nodes.insert(insert,node);refreshRows(insert);
    });
    connect(edit,&QPushButton::clicked,&dialog,[&]{
        const int index=list->currentRow();if(index<0||index>=nodes.size())return;
        auto changed=nodes[index];if(!editNumbers(&changed))return;
        nodes[index]=changed;refreshRows(index);
    });
    connect(sourceEdit,&QPushButton::clicked,&dialog,[&]{
        const int index=list->currentRow();if(index<0||index>=nodes.size())return;
        auto changed=nodes[index];if(!editSources(&changed,index))return;
        nodes[index]=changed;refreshRows(index);
    });
    connect(suppress,&QPushButton::clicked,&dialog,[&]{
        const int index=list->currentRow();if(index<0||index>=nodes.size())return;
        nodes[index].suppressed=!nodes[index].suppressed;refreshRows(index);
    });
    connect(up,&QPushButton::clicked,&dialog,[&]{
        const int index=list->currentRow();if(index<1)return;
        nodes.swapItemsAt(index,index-1);refreshRows(index-1);
    });
    connect(down,&QPushButton::clicked,&dialog,[&]{
        const int index=list->currentRow();if(index<0||index>=nodes.size()-1)return;
        nodes.swapItemsAt(index,index+1);refreshRows(index+1);
    });
    connect(remove,&QPushButton::clicked,&dialog,[&]{
        const int index=list->currentRow();if(index<0||index>=nodes.size())return;
        nodes.removeAt(index);refreshRows(std::min(index,int(nodes.size())-1));
    });
    connect(validate,&QPushButton::clicked,&dialog,[&]{
        const cad::Part* current=document_.find(id);
        if(!current){feedback->setText("Body was removed.");return;}
        const TopoDS_Shape source=current->hasShapeFeatureTree?
            current->shapeFeatureBase:current->shape;
        TopoDS_Shape preview;QString error;
        if(cad::rebuildShapeFeatureTree(source,current->isSurfaceBody,nodes,&preview,&error)){
            feedback->setStyleSheet("color: #23865b; font-weight: 600;");
            feedback->setText(QStringLiteral("Dry run passed: %1 features regenerated. Document unchanged.").arg(nodes.size()));
        }else{
            feedback->setStyleSheet("color: #b65b37; font-weight: 600;");
            feedback->setText("Dry run failed: "+error);
        }
    });
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
        QString error;
        if(!document_.setShapeFeatureTree(id,nodes,&error)){
            feedback->setText("Rebuild failed: "+error);
            QMessageBox::warning(&dialog,"Feature rebuild rejected",error+"\nThe existing body has not changed.");
            return;
        }
        dialog.accept();
    });
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    refreshRows(nodes.isEmpty()?-1:0);
    if(dialog.exec()!=QDialog::Accepted)return;
    refresh();statusBar()->showMessage("Rebuilt the shape-independent feature stack; one undo step.",8000);
}

void MainWindow::createConstrainedPolygonFromVertices() {
    bool accepted=false;
    const QString entered=QInputDialog::getMultiLineText(this,"Constrained polygon from vertices",
        "Enter one X, Y coordinate pair per line, in boundary order (3–12 points, mm):",
        "0, 0\n40, 0\n50, 25\n10, 30",&accepted);
    if(!accepted)return;
    const QStringList rows=entered.split('\n',Qt::SkipEmptyParts);
    if(rows.size()<3||rows.size()>12){reportError("Constrained polygon","Enter 3 to 12 vertex rows.");return;}
    cad::SketchModel sketch;QStringList boundary;
    for(int i=0;i<rows.size();++i){
        const QStringList values=rows[i].split(',');
        bool validX=false,validY=false;
        const double x=values.size()==2?values[0].trimmed().toDouble(&validX):0.0;
        const double y=values.size()==2?values[1].trimmed().toDouble(&validY):0.0;
        if(!validX||!validY||!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>1e6||std::abs(y)>1e6){
            reportError("Constrained polygon",QStringLiteral("Vertex %1 needs two finite X, Y coordinates within ±1,000,000 mm.").arg(i+1));return;
        }
        const QString id=QStringLiteral("P%1").arg(i+1);
        boundary.append(id);sketch.points.append({id,x,y});
        cad::SketchConstraint anchor;anchor.type=cad::SketchConstraintType::FixedPoint;
        anchor.pointA=id;anchor.fixedX=x;anchor.fixedY=y;sketch.constraints.append(anchor);
    }
    for(int i=0;i<boundary.size();++i)
        sketch.segments.append({QStringLiteral("edge%1").arg(i+1),boundary[i],boundary[(i+1)%boundary.size()]});
    const double depth=QInputDialog::getDouble(this,"Constrained polygon extrusion","Extrusion depth (mm)",
        10.0,0.00001,1000000.0,3,&accepted);
    if(!accepted)return;
    QString error;
    if(!document_.addConstrainedPolygonExtrusion(sketch,boundary,depth,&error)){
        reportError("Constrained polygon",error);return;
    }
    refresh(true);
}
void MainWindow::editConstrainedPolygonExtrusion() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Edit constrained sketch","Select one constrained polygon extrusion.");return;}
    const cad::Part* part=document_.find(ids.first());
    if(!part||!part->hasConstrainedPolygonDefinition){
        reportError("Edit constrained sketch","The selected body has no editable constrained polygon sketch.");return;
    }
    if(part->geometryToken!=part->constrainedPolygonGeometryToken){
        reportError("Edit constrained sketch","The sketch is stale after a direct geometry edit.");return;
    }
    cad::SketchModel sketch=part->constrainedPolygonSketch;
    const QStringList boundary=part->constrainedPolygonBoundary;
    QDialog dialog(this);dialog.setWindowTitle("Edit constrained polygon sketch");dialog.setMinimumWidth(410);
    auto* form=new QFormLayout(&dialog);
    auto makeField=[&](const QString& label,double value,double minimum,double maximum){
        auto* box=new QDoubleSpinBox(&dialog);box->setRange(minimum,maximum);
        box->setDecimals(4);box->setValue(value);box->setSuffix(" mm");form->addRow(label,box);return box;
    };
    auto* depth=makeField("Extrusion depth",part->constrainedPolygonDepthMm,0.00001,1000000);
    QVector<std::function<void()>> apply;
    for(int i=0;i<sketch.constraints.size();++i){
        const auto& constraint=sketch.constraints[i];
        if(constraint.type==cad::SketchConstraintType::Distance||
           constraint.type==cad::SketchConstraintType::HorizontalDistance||
           constraint.type==cad::SketchConstraintType::VerticalDistance||
           constraint.type==cad::SketchConstraintType::SegmentLength||
           constraint.type==cad::SketchConstraintType::SignedPointLineDistance||
           constraint.type==cad::SketchConstraintType::SignedPointCircleDistance){
            const QString direction=constraint.type==cad::SketchConstraintType::HorizontalDistance?"horizontal ":
                constraint.type==cad::SketchConstraintType::VerticalDistance?"vertical ":"";
            const QString label=constraint.type==cad::SketchConstraintType::SegmentLength?
                QStringLiteral("Length of %1").arg(constraint.segmentA):
                constraint.type==cad::SketchConstraintType::SignedPointLineDistance?
                QStringLiteral("Signed offset %1 from %2").arg(constraint.pointA,constraint.segmentA):
                constraint.type==cad::SketchConstraintType::SignedPointCircleDistance?
                QStringLiteral("Signed offset %1 from circle %2").arg(constraint.pointA,constraint.circleA):
                QStringLiteral("%1Distance %2 to %3").arg(direction,constraint.pointA,constraint.pointB);
            const bool positive=constraint.type==cad::SketchConstraintType::Distance||
                                constraint.type==cad::SketchConstraintType::SegmentLength;
            auto* box=makeField(label,constraint.value,positive?0.00001:-1000000.0,1000000.0);
            apply.append([&sketch,i,box]{sketch.constraints[i].value=box->value();});
        }else if(constraint.type==cad::SketchConstraintType::Angle||
                 constraint.type==cad::SketchConstraintType::SegmentAngleFromX){
            const QString label=constraint.type==cad::SketchConstraintType::SegmentAngleFromX?
                QStringLiteral("Angle of %1 from X (degrees)").arg(constraint.segmentA):
                QStringLiteral("Angle %1 to %2 (degrees)").arg(constraint.segmentA,constraint.segmentB);
            auto* box=makeField(label,
                                constraint.value*180.0/std::acos(-1.0),-360,360);
            box->setSuffix("°");
            apply.append([&sketch,i,box]{sketch.constraints[i].value=box->value()*std::acos(-1.0)/180.0;});
        }else if(constraint.type==cad::SketchConstraintType::Radius){
            auto* box=makeField(QStringLiteral("Circle %1 radius").arg(constraint.circleA),constraint.value,0.00001,1000000);
            apply.append([&sketch,i,box]{sketch.constraints[i].value=box->value();});
        }else if(constraint.type==cad::SketchConstraintType::FixedPoint){
            auto* x=makeField(QStringLiteral("Anchor %1 X").arg(constraint.pointA),constraint.fixedX,-1000000,1000000);
            auto* y=makeField(QStringLiteral("Anchor %1 Y").arg(constraint.pointA),constraint.fixedY,-1000000,1000000);
            apply.append([&sketch,i,x,y]{sketch.constraints[i].fixedX=x->value();sketch.constraints[i].fixedY=y->value();});
        }
    }
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    for(const auto& update:apply)update();
    QString error;
    if(!document_.updateConstrainedPolygonExtrusion(ids.first(),sketch,boundary,depth->value(),&error)){
        reportError("Edit constrained sketch",error);return;
    }
    refresh();
}
void MainWindow::editConstrainedSketchRelations() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Sketch constraints","Select one constrained polygon extrusion.");return;}
    const cad::Part* part=document_.find(ids.first());
    if(!part||!part->hasConstrainedPolygonDefinition||
       part->geometryToken!=part->constrainedPolygonGeometryToken){
        reportError("Sketch constraints","Select a current constrained polygon sketch. A direct geometry edit makes its sketch history stale.");return;
    }
    cad::SketchModel sketch=part->constrainedPolygonSketch;
    QStringList pointIds,segmentIds,circleIds;
    for(const auto& point:sketch.points)pointIds.append(point.id);
    for(const auto& segment:sketch.segments)segmentIds.append(segment.id);
    for(const auto& circle:sketch.circles)circleIds.append(circle.id);
    bool accepted=false;
    const QString operation=QInputDialog::getItem(this,"Sketch constraints","Action:",
        {"Add constraint","Remove constraint","Unfix point"},0,false,&accepted);
    if(!accepted)return;
    if(operation=="Remove constraint"){
        if(sketch.constraints.isEmpty()){reportError("Sketch constraints","This sketch has no constraints to remove.");return;}
        const QStringList typeNames={"Coincident points","Horizontal segment","Vertical segment","Point distance",
            "Fixed point","Equal segment lengths","Parallel segments","Perpendicular segments","Segment angle",
            "Signed horizontal distance","Signed vertical distance","Point on line","Segment midpoint","Collinear segments",
            "Circle radius","Equal circle radii","Concentric circles","Point on circle","Line tangent to circle",
            "Points symmetric about line","Externally tangent circles","Internally tangent circles","Point on segment",
            "Segment length","Signed point-to-line distance","Segment angle from X","Signed point-to-circle distance"};
        QStringList labels;
        for(int i=0;i<sketch.constraints.size();++i){
            const auto& c=sketch.constraints[i];
            QStringList references{c.pointA,c.pointB,c.segmentA,c.segmentB,c.circleA,c.circleB};references.removeAll(QString());
            labels.append(QStringLiteral("%1. %2 — %3").arg(i+1).arg(typeNames.value(static_cast<int>(c.type),"Unknown"),
                references.join(", ")));
        }
        const QString chosen=QInputDialog::getItem(this,"Remove sketch constraint","Constraint:",labels,0,false,&accepted);
        if(!accepted)return;
        sketch.constraints.removeAt(labels.indexOf(chosen));
    }else if(operation=="Unfix point"){
        const QString point=QInputDialog::getItem(this,"Unfix sketch point","Point:",pointIds,0,false,&accepted);
        if(!accepted)return;
        int removed=0;
        for(int i=sketch.constraints.size()-1;i>=0;--i)
            if(sketch.constraints[i].type==cad::SketchConstraintType::FixedPoint&&sketch.constraints[i].pointA==point){
                sketch.constraints.removeAt(i);++removed;
            }
        if(!removed){reportError("Sketch constraints","The selected point is not fixed.");return;}
    }else{
        using Type=cad::SketchConstraintType;
        const QVector<QPair<QString,Type>> types={
            {"Coincident points",Type::Coincident},{"Horizontal segment",Type::Horizontal},
            {"Vertical segment",Type::Vertical},{"Point distance",Type::Distance},
            {"Signed horizontal distance",Type::HorizontalDistance},{"Signed vertical distance",Type::VerticalDistance},
            {"Fixed point",Type::FixedPoint},{"Equal segment lengths",Type::EqualLength},
            {"Parallel segments",Type::Parallel},{"Perpendicular segments",Type::Perpendicular},
            {"Angle between segments",Type::Angle},{"Point on line",Type::PointOnLine},
            {"Point at segment midpoint",Type::Midpoint},{"Collinear segments",Type::Collinear},
            {"Circle radius",Type::Radius},{"Equal circle radii",Type::EqualRadius},
            {"Concentric circles",Type::Concentric},{"Point on circle",Type::PointOnCircle},
            {"Line tangent to circle",Type::TangentLineCircle},
            {"Points symmetric about line",Type::SymmetricAboutLine},
            {"Externally tangent circles",Type::TangentCirclesExternal},
            {"Internally tangent circles",Type::TangentCirclesInternal},
            {"Point on segment",Type::PointOnSegment},
            {"Segment length",Type::SegmentLength},
            {"Signed point-to-line distance",Type::SignedPointLineDistance},
            {"Segment angle from X",Type::SegmentAngleFromX},
            {"Signed point-to-circle distance",Type::SignedPointCircleDistance}};
        QStringList labels;for(const auto& entry:types)labels.append(entry.first);
        const QString selected=QInputDialog::getItem(this,"Add sketch constraint","Constraint type:",labels,0,false,&accepted);
        if(!accepted)return;
        cad::SketchConstraint constraint;constraint.type=types[labels.indexOf(selected)].second;
        auto choose=[&](const QString& label,const QStringList& options){
            return QInputDialog::getItem(this,"Add sketch constraint",label,options,0,false,&accepted);
        };
        const bool usesPoints=constraint.type==Type::Coincident||constraint.type==Type::Distance||
            constraint.type==Type::HorizontalDistance||constraint.type==Type::VerticalDistance||
            constraint.type==Type::FixedPoint||constraint.type==Type::PointOnLine||constraint.type==Type::PointOnSegment||
            constraint.type==Type::Midpoint||
            constraint.type==Type::PointOnCircle||constraint.type==Type::SymmetricAboutLine||
            constraint.type==Type::SignedPointLineDistance||constraint.type==Type::SignedPointCircleDistance;
        const bool usesTwoPoints=constraint.type==Type::Coincident||constraint.type==Type::Distance||
            constraint.type==Type::HorizontalDistance||constraint.type==Type::VerticalDistance||
            constraint.type==Type::SymmetricAboutLine;
        const bool usesTwoSegments=constraint.type==Type::EqualLength||constraint.type==Type::Parallel||
            constraint.type==Type::Perpendicular||constraint.type==Type::Angle||constraint.type==Type::Collinear;
        const bool usesSegment=constraint.type==Type::Horizontal||constraint.type==Type::Vertical||
            constraint.type==Type::PointOnLine||constraint.type==Type::PointOnSegment||
            constraint.type==Type::Midpoint||constraint.type==Type::TangentLineCircle||
            constraint.type==Type::SymmetricAboutLine||constraint.type==Type::SegmentLength||
            constraint.type==Type::SignedPointLineDistance||constraint.type==Type::SegmentAngleFromX||usesTwoSegments;
        const bool usesCircle=constraint.type==Type::Radius||constraint.type==Type::EqualRadius||
            constraint.type==Type::Concentric||constraint.type==Type::PointOnCircle||constraint.type==Type::TangentLineCircle||
            constraint.type==Type::SignedPointCircleDistance||
            constraint.type==Type::TangentCirclesExternal||constraint.type==Type::TangentCirclesInternal;
        const bool usesTwoCircles=constraint.type==Type::EqualRadius||constraint.type==Type::Concentric||
            constraint.type==Type::TangentCirclesExternal||constraint.type==Type::TangentCirclesInternal;
        if(usesPoints){constraint.pointA=choose("First point:",pointIds);if(!accepted)return;}
        if(usesTwoPoints){constraint.pointB=choose("Second point:",pointIds);if(!accepted)return;}
        if(usesSegment){constraint.segmentA=choose("First segment:",segmentIds);if(!accepted)return;}
        if(usesTwoSegments){constraint.segmentB=choose("Second segment:",segmentIds);if(!accepted)return;}
        if(usesCircle){
            if(circleIds.isEmpty()){reportError("Sketch constraints","Add a sketch circle first.");return;}
            constraint.circleA=choose("First circle:",circleIds);if(!accepted)return;
        }
        if(usesTwoCircles){constraint.circleB=choose("Second circle:",circleIds);if(!accepted)return;}
        if(constraint.type==Type::Distance||constraint.type==Type::HorizontalDistance||
           constraint.type==Type::VerticalDistance||constraint.type==Type::Angle||constraint.type==Type::Radius||
           constraint.type==Type::SegmentLength||constraint.type==Type::SignedPointLineDistance||
           constraint.type==Type::SegmentAngleFromX||constraint.type==Type::SignedPointCircleDistance){
            const bool angle=constraint.type==Type::Angle||constraint.type==Type::SegmentAngleFromX;
            const bool positive=constraint.type==Type::Distance||constraint.type==Type::Radius||
                                constraint.type==Type::SegmentLength;
            const double entered=QInputDialog::getDouble(this,"Add sketch constraint",
                angle?"Signed angle (degrees):":"Distance (mm):",angle?90.0:10.0,
                angle?-360.0:(positive?0.00001:-1000000.0),angle?360.0:1000000.0,3,&accepted);
            if(!accepted)return;
            constraint.value=angle?entered*std::acos(-1.0)/180.0:entered;
        }
        if(constraint.type==Type::FixedPoint){
            const auto pointIt=std::find_if(sketch.points.cbegin(),sketch.points.cend(),
                [&](const cad::SketchPoint& p){return p.id==constraint.pointA;});
            const double initialX=pointIt==sketch.points.cend()?0.0:pointIt->x;
            const double initialY=pointIt==sketch.points.cend()?0.0:pointIt->y;
            constraint.fixedX=QInputDialog::getDouble(this,"Fix sketch point","X (mm):",initialX,-1000000,1000000,3,&accepted);
            if(!accepted)return;
            constraint.fixedY=QInputDialog::getDouble(this,"Fix sketch point","Y (mm):",initialY,-1000000,1000000,3,&accepted);
            if(!accepted)return;
        }
        sketch.constraints.append(constraint);
    }
    QString error;
    if(!document_.updateConstrainedPolygonExtrusion(ids.first(),sketch,part->constrainedPolygonBoundary,
                                                      part->constrainedPolygonDepthMm,&error)){
        reportError("Sketch constraints",error);return;
    }
    refresh();
}
void MainWindow::editConstrainedSketchCircles() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Sketch circles","Select one constrained polygon extrusion.");return;}
    const cad::Part* part=document_.find(ids.first());
    if(!part||!part->hasConstrainedPolygonDefinition||part->geometryToken!=part->constrainedPolygonGeometryToken){
        reportError("Sketch circles","Select a current constrained polygon sketch.");return;
    }
    cad::SketchModel sketch=part->constrainedPolygonSketch;
    bool accepted=false;
    const QString operation=QInputDialog::getItem(this,"Sketch circles","Action:",
        {"Add circular cutout","Add construction circle","Remove circle"},0,false,&accepted);
    if(!accepted)return;
    if(operation=="Remove circle"){
        if(sketch.circles.isEmpty()){reportError("Sketch circles","This sketch has no circles to remove.");return;}
        QStringList labels;for(const auto& circle:sketch.circles)labels.append(circle.id);
        const QString selected=QInputDialog::getItem(this,"Remove sketch circle","Circle:",labels,0,false,&accepted);
        if(!accepted)return;
        const int index=labels.indexOf(selected);if(index<0)return;
        const QString center=sketch.circles[index].centerPointId;
        sketch.circles.removeAt(index);
        for(int i=sketch.constraints.size()-1;i>=0;--i)
            if(sketch.constraints[i].circleA==selected||sketch.constraints[i].circleB==selected)sketch.constraints.removeAt(i);
        bool centerUsed=part->constrainedPolygonBoundary.contains(center);
        for(const auto& circle:sketch.circles)if(circle.centerPointId==center)centerUsed=true;
        for(const auto& segment:sketch.segments)if(segment.startPointId==center||segment.endPointId==center)centerUsed=true;
        for(const auto& constraint:sketch.constraints)
            if((constraint.pointA==center||constraint.pointB==center)&&constraint.type!=cad::SketchConstraintType::FixedPoint)centerUsed=true;
        if(!centerUsed){
            for(int i=sketch.constraints.size()-1;i>=0;--i)
                if(sketch.constraints[i].type==cad::SketchConstraintType::FixedPoint&&sketch.constraints[i].pointA==center)
                    sketch.constraints.removeAt(i);
            for(int i=sketch.points.size()-1;i>=0;--i)if(sketch.points[i].id==center)sketch.points.removeAt(i);
        }
    }else{
        if(sketch.circles.size()>=64||sketch.points.size()>=64){reportError("Sketch circles","The sketch entity limit has been reached.");return;}
        const double x=QInputDialog::getDouble(this,"Sketch circle","Center X (mm):",5,-1000000,1000000,3,&accepted);if(!accepted)return;
        const double y=QInputDialog::getDouble(this,"Sketch circle","Center Y (mm):",5,-1000000,1000000,3,&accepted);if(!accepted)return;
        const double radius=QInputDialog::getDouble(this,"Sketch circle","Radius (mm):",2,0.00001,1000000,3,&accepted);if(!accepted)return;
        int suffix=sketch.circles.size()+1;QString circleId,centerId;
        while(true){
            circleId=QStringLiteral("circle%1").arg(suffix);centerId=QStringLiteral("circle-center%1").arg(suffix);
            const bool circleUsed=std::any_of(sketch.circles.cbegin(),sketch.circles.cend(),[&](const cad::SketchCircle& c){return c.id==circleId;});
            const bool centerUsed=std::any_of(sketch.points.cbegin(),sketch.points.cend(),[&](const cad::SketchPoint& p){return p.id==centerId;});
            if(!circleUsed&&!centerUsed)break;++suffix;
        }
        sketch.points.append({centerId,x,y});
        sketch.circles.append({circleId,centerId,radius,operation=="Add circular cutout"});
        cad::SketchConstraint anchor;anchor.type=cad::SketchConstraintType::FixedPoint;anchor.pointA=centerId;
        anchor.fixedX=x;anchor.fixedY=y;sketch.constraints.append(anchor);
        cad::SketchConstraint size;size.type=cad::SketchConstraintType::Radius;size.circleA=circleId;size.value=radius;
        sketch.constraints.append(size);
    }
    QString error;
    if(!document_.updateConstrainedPolygonExtrusion(ids.first(),sketch,part->constrainedPolygonBoundary,
                                                      part->constrainedPolygonDepthMm,&error)){
        reportError("Sketch circles",error);return;
    }
    refresh();
}
void MainWindow::addMetricFastenerClearanceHole() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){
        reportError("ISO 273 clearance hole","Select exactly one solid body.");
        return;
    }
    const cad::Part* part=document_.find(ids.first());
    if(!part||part->isSurfaceBody||part->shape.IsNull()){
        reportError("ISO 273 clearance hole","Select a valid solid body, not a surface-only body.");
        return;
    }
    Bnd_Box bounds;
    BRepBndLib::AddOptimal(part->shape,bounds,false,false);
    if(bounds.IsVoid()){
        reportError("ISO 273 clearance hole","Selected solid has no finite bounding box.");
        return;
    }
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;
    bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    const auto selectedFaces=viewport_->selectedFaceReferences();
    const bool selectedPlanarFace=selectedFaces.size()==1&&
        selectedFaces.first().partId==part->id&&
        !selectedSubshape(document_,selectedFaces.first().partId,
            selectedFaces.first().geometryToken,selectedFaces.first().faceIndex,TopAbs_FACE).IsNull()&&
        BRepAdaptor_Surface(TopoDS::Face(selectedSubshape(document_,selectedFaces.first().partId,
            selectedFaces.first().geometryToken,selectedFaces.first().faceIndex,TopAbs_FACE))).GetType()==GeomAbs_Plane;
    gp_Pnt faceCenter;
    if(selectedPlanarFace){
        const TopoDS_Face face=TopoDS::Face(selectedSubshape(document_,selectedFaces.first().partId,
            selectedFaces.first().geometryToken,selectedFaces.first().faceIndex,TopAbs_FACE));
        GProp_GProps properties;BRepGProp::SurfaceProperties(face,properties);
        faceCenter=properties.CentreOfMass();
    }
    const bool canParametric=part->hasParametricDefinition&&
        document_.parametricDefinitionCurrent(part->id)&&
        (part->featureHistory==QStringList{
            "Sketch: XY rectangle (fully constrained)",
            "Extrude: Base solid (depth parameter)"}||
         part->featureHistory==QStringList{
            "Sketch: XY rectangle with editable origin (fully constrained)",
            "Extrude: Base solid (depth parameter)"});
    QDialog dialog(this);
    dialog.setWindowTitle("ISO 273 metric fastener clearance hole");
    auto* form=new QFormLayout(&dialog);
    auto* nominal=new QComboBox(&dialog);
    for(const auto& row:cad::metric_holes::kClearanceRows)
        nominal->addItem(QString::fromLatin1(row.designation.data(),static_cast<int>(row.designation.size())));
    nominal->setCurrentText("M6");
    auto* fit=new QComboBox(&dialog);
    fit->addItems({"Fine (close)","Medium (normal)","Coarse (free)"});
    fit->setCurrentIndex(1);
    auto* method=new QComboBox(&dialog);
    if(canParametric)method->addItem("Editable parametric hole (rectangle base only)",1);
    method->addItem("Direct through-hole (any valid solid; non-associative)",0);
    auto* drillAxis=new QComboBox(&dialog);
    drillAxis->addItem("World -Z (existing workflow)",0);
    if(selectedPlanarFace)
        drillAxis->addItem("Inward normal of selected planar face",1);
    if(selectedPlanarFace&&method->currentData().toInt()==0)drillAxis->setCurrentIndex(1);
    auto* x=new QDoubleSpinBox(&dialog);
    auto* y=new QDoubleSpinBox(&dialog);
    auto* entryZ=new QDoubleSpinBox(&dialog);
    for(auto* position:{x,y,entryZ}){
        position->setRange(-1000000.0,1000000.0);
        position->setDecimals(3);
        position->setSuffix(" mm");
    }
    x->setValue(selectedPlanarFace?faceCenter.X():(xmin+xmax)/2.0);
    y->setValue(selectedPlanarFace?faceCenter.Y():(ymin+ymax)/2.0);
    entryZ->setValue(selectedPlanarFace?faceCenter.Z():zmax);
    entryZ->setToolTip("Z coordinate at the upper entry side. Direct holes cut down along world -Z.");
    auto* diameter=new QLabel(&dialog);
    diameter->setObjectName("MetricClearanceDiameterPreview");
    diameter->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto updateDiameter=[nominal,fit,diameter]{
        double mm=0.0;
        const auto designation=nominal->currentText().toStdString();
        if(cad::metric_holes::lookup(designation,fit->currentIndex(),&mm))
            diameter->setText(QStringLiteral("Nominal hole diameter: %1 mm").arg(mm,0,'f',2));
        else diameter->setText("Unsupported fastener / fit combination");
    };
    connect(nominal,&QComboBox::currentTextChanged,&dialog,updateDiameter);
    connect(fit,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,updateDiameter);
    updateDiameter();
    form->addRow("Fastener designation",nominal);
    form->addRow("Clearance series",fit);
    form->addRow("Modeling method",method);
    form->addRow("Drilling axis",drillAxis);
    form->addRow("Hole center X",x);
    form->addRow("Hole center Y",y);
    form->addRow("Upper entry Z",entryZ);
    form->addRow("",diameter);
    auto* note=new QLabel(&dialog);
    note->setWordWrap(true);
    auto updateMethod=[method,entryZ,drillAxis,note]{
        const bool parametric=method->currentData().toInt()==1;
        entryZ->setEnabled(!parametric);
        drillAxis->setEnabled(!parametric);
        note->setText(parametric?
            "The hole remains editable as a named parameter in the current rectangle extrusion. "
            "It must remain strictly inside that sketch boundary.":
            "Cuts any valid solid using the selected drill axis. For face-normal drilling, "
            "enter a center on the selected planar face; the cutter follows its inward normal. "
            "The hole itself is a direct edit; linked descendants regenerate atomically, "
            "but its own dimensions are not yet associative. Check the preview.");
    };
    connect(method,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,updateMethod);
    updateMethod();
    form->addRow(note);
    auto* safety=new QLabel("ISO 273 values are nominal diameter references. Check fits, "
        "edge distances, tolerances and machining independently. No threads or fastener body are added.",&dialog);
    safety->setWordWrap(true);
    form->addRow(safety);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    double mm=0.0;
    const QString designation=nominal->currentText();
    if(!cad::metric_holes::lookup(designation.toStdString(),fit->currentIndex(),&mm)){
        reportError("ISO 273 clearance hole","Unsupported fastener size or clearance series.");return;
    }
    QString error;
    if(method->currentData().toInt()==1){
        if(!document_.addParametricThroughHole(ids.first(),mm,x->value(),y->value(),&error)){
            reportError("ISO 273 clearance hole",error);return;
        }
        refresh(true);
        statusBar()->showMessage(QStringLiteral("Added editable %1 clearance hole (%2 mm)")
            .arg(designation).arg(mm,0,'f',2),8500);
        return;
    }
    cad::HoleCommand command;
    const bool faceAxis=drillAxis->currentData().toInt()==1;
    const bool prepared=faceAxis?
        document_.prepareHoleOnPlanarFace(selectedFaces.first(),x->value(),y->value(),entryZ->value(),
                                          mm,1.0,true,&command,&error):
        document_.prepareHole(ids.first(),x->value(),y->value(),entryZ->value(),mm,
                              1.0,true,&command,&error);
    if(!prepared){
        reportError("ISO 273 clearance hole",error);return;
    }
    runPreviewAsync<cad::HolePreview>(this,statusBar(),"ISO 273 clearance hole",
        "Building shape-independent hole preview...",[command]{
            return cad::SceneDocument::buildHolePreview(command);
        },[this,command,designation,mm](const cad::HolePreview& preview){
            confirmPreview("Preview ISO 273 clearance hole",
                QStringLiteral("Inspect the hole cut in gold. Apply %1 hole (%2 mm)? "
                    "This is a direct non-associative edit.").arg(designation).arg(mm,0,'f',2),
                preview.shape,[this,command,preview]{
                    QString commitError;
                    if(!document_.commitHole(command,preview.shape,&commitError)){
                        reportError("ISO 273 clearance hole",commitError);
                    }else refresh(true);
                });
        });
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
void MainWindow::addParametricBlindHole() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Parametric blind hole","Select one current parametric rectangle extrusion.");return;}
    QDialog dialog(this);dialog.setWindowTitle("Add constrained circular sketch and blind hole");auto* form=new QFormLayout(&dialog);
    const auto dimension=[&](const QString& label,double value){auto* box=new QDoubleSpinBox(&dialog);box->setRange(0.00001,1000000);box->setDecimals(3);box->setSingleStep(1);box->setValue(value);box->setSuffix(" mm");form->addRow(label,box);return box;};
    const auto coordinate=[&](const QString& label,double value){auto* box=new QDoubleSpinBox(&dialog);box->setRange(-1000000,1000000);box->setDecimals(3);box->setSingleStep(1);box->setValue(value);box->setSuffix(" mm");form->addRow(label,box);return box;};
    auto* diameter=dimension("Hole diameter",8.0);auto* centerX=coordinate("Circle center X",20.0);
    auto* centerY=coordinate("Circle center Y",15.0);auto* holeDepth=dimension("Blind depth",8.0);
    auto* note=new QLabel("The circular profile is dimensioned and cut inward from the top face. Depth must be less than the base extrusion thickness.",&dialog);
    note->setWordWrap(true);form->addRow(note);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.addParametricBlindHole(ids.first(),diameter->value(),centerX->value(),centerY->value(),holeDepth->value(),&error)){
        reportError("Parametric blind hole",error);return;
    }
    refresh(true);
}
void MainWindow::addParametricLinearHolePattern() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Parametric hole pattern","Select one body with a current parametric through-hole or blind hole.");return;}
    QDialog dialog(this);dialog.setWindowTitle("Linear parametric hole pattern");auto* form=new QFormLayout(&dialog);
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
void MainWindow::addParametricAllEdgeChamfer() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Parametric chamfer","Select one current parametric solid.");return;}
    bool accepted=false;
    const double distance=QInputDialog::getDouble(this,"Add parametric all-edge chamfer",
        "Chamfer distance parameter (mm):",1.0,0.00001,1000000.0,4,&accepted);
    if(!accepted)return;
    QString error;
    if(!document_.addParametricAllEdgeChamfer(ids.first(),distance,&error)){
        reportError("Parametric chamfer",error);return;
    }
    refresh();statusBar()->showMessage("Added an editable all-edge chamfer to the parametric feature history.",8000);
}
void MainWindow::addParametricAllEdgeFillet() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Parametric fillet","Select one current parametric solid.");return;}
    bool accepted=false;
    const double radius=QInputDialog::getDouble(this,"Add parametric all-edge fillet",
        "Fillet radius parameter (mm):",1.0,0.00001,1000000.0,4,&accepted);
    if(!accepted)return;
    QString error;
    if(!document_.addParametricAllEdgeFillet(ids.first(),radius,&error)){
        reportError("Parametric fillet",error);return;
    }
    refresh();statusBar()->showMessage("Added an editable all-edge fillet to the parametric feature history.",8000);
}
void MainWindow::editParametricFeatureSuppressionBatch(){
    const QStringList ids=selectedIds();
    if(ids.size()<2||ids.size()>12){
        reportError("Coordinated feature history","Select 2-12 parametric source bodies.");return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle("Coordinated optional-feature history");dialog.resize(810,540);
    auto* layout=new QVBoxLayout(&dialog);
    auto* note=new QLabel("Set optional feature activity for several source bodies. All source solids "
        "and linked descendants rebuild together; invalid changes reject the complete transaction.",&dialog);
    note->setWordWrap(true);layout->addWidget(note);
    auto* table=new QTableWidget(0,3,&dialog);
    table->setHorizontalHeaderLabels({"Source body","Optional feature","Active"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    struct Toggle{QString id,featureId;QCheckBox* checkbox;};
    QVector<Toggle> toggles;
    QMap<QString,QStringList> desired;
    const auto option=[&](const QStringList& history,const QString& identifier){
        if(identifier=="through_hole")return history.contains("Cut: Parametric through-hole");
        if(identifier=="blind_hole")return history.contains("Cut: Parametric blind hole");
        if(identifier=="linear_hole_pattern"){
            for(const QString& item:history)if(item.startsWith("Pattern: Linear "))return true;
            return false;
        }
        if(identifier=="edge_chamfer")return history.contains("Chamfer: All edges");
        if(identifier=="edge_fillet")return history.contains("Fillet: All edges");
        return false;
    };
    const QVector<QPair<QString,QString>> features{
        {"through_hole","Through-hole"},{"blind_hole","Blind hole"},
        {"linear_hole_pattern","Linear hole pattern"},{"edge_chamfer","Edge chamfer"},
        {"edge_fillet","Edge fillet"}};
    for(const QString& id:ids){
        const auto* body=document_.find(id);
        if(!body||!document_.parametricDefinitionCurrent(id)){
            reportError("Coordinated feature history","Every selected body requires a current editable history.");return;
        }
        const QStringList history=document_.featureHistory(id);
        desired.insert(id,document_.suppressedParametricFeatures(id));
        bool any=false;
        for(const auto& feature:features){
            if(!option(history,feature.first))continue;
            const int row=table->rowCount();table->insertRow(row);
            const auto readOnly=[](const QString& value){
                auto* item=new QTableWidgetItem(value);
                item->setFlags(item->flags()&~Qt::ItemIsEditable);return item;
            };
            table->setItem(row,0,readOnly(body->name));
            table->setItem(row,1,readOnly(feature.second));
            auto* checkbox=new QCheckBox(table);
            checkbox->setChecked(!desired[id].contains(feature.first));
            table->setCellWidget(row,2,checkbox);
            toggles.append({id,feature.first,checkbox});any=true;
        }
        if(!any){
            reportError("Coordinated feature history",
                QString("%1 has no supported optional features.").arg(body->name));return;
        }
    }
    layout->addWidget(table);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    for(const Toggle& toggle:toggles){
        desired[toggle.id].removeAll(toggle.featureId);
        if(!toggle.checkbox->isChecked())desired[toggle.id].append(toggle.featureId);
    }
    QString report,error;
    if(!document_.previewParametricFeaturesSuppressedBatch(desired,&report,&error)){
        reportError("Coordinated feature-history preflight",error);return;
    }
    if(QMessageBox::question(this,"Commit coordinated feature history",
        report+"\n\nCommit all source and dependent geometry in one undoable edit?",
        QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
    if(!document_.setParametricFeaturesSuppressedBatch(desired,&error)){
        reportError("Commit coordinated feature history",error);return;
    }
    refresh(true);
    statusBar()->showMessage("Coordinated feature histories regenerated atomically.",9000);
}

void MainWindow::exploreParametricDesignAlternatives(){
    const QStringList ids=selectedIds();
    if(ids.isEmpty()||ids.size()>8){
        reportError("Parametric design alternatives","Select 1-8 editable parametric bodies.");return;
    }
    bool accepted=false;
    const int percent=QInputDialog::getInt(this,"Parametric alternative study",
        "Compare dimension changes of plus/minus (%)",10,1,35,1,&accepted);
    if(!accepted)return;
    QMap<QString,QMap<QString,double>> reduced,expanded;
    for(const QString& id:ids){
        const cad::Part* body=document_.find(id);
        if(!body){reportError("Parametric design alternatives","A selected body is missing.");return;}
        const auto dimensions=document_.namedParameters(id);
        const auto links=document_.parameterLinks(id);
        QStringList options;
        for(auto it=dimensions.cbegin();it!=dimensions.cend();++it){
            if(links.contains(it.key())||!std::isfinite(it.value())||it.value()<=0.0||
               it.key().contains("count",Qt::CaseInsensitive))continue;
            options.append(it.key());
        }
        if(options.isEmpty()){
            reportError("Parametric design alternatives",
                QString("Body %1 has no positive, directly editable continuous dimension.").arg(body->name));return;
        }
        const QString parameter=QInputDialog::getItem(this,"Explore "+body->name,
            "Dimension to compare",options,0,false,&accepted);
        if(!accepted)return;
        const double original=dimensions.value(parameter);
        reduced[id].insert(parameter,original*(1.0-percent/100.0));
        expanded[id].insert(parameter,original*(1.0+percent/100.0));
    }
    QVector<cad::ParametricDesignAlternative> candidates{
        {QStringLiteral("Reduced dimensions"),reduced},
        {QStringLiteral("Current baseline"),{}},
        {QStringLiteral("Expanded dimensions"),expanded}};
    QVector<cad::ParametricDesignAlternativeResult> result;
    QString issue;
    const QString evaluatedGeometryFingerprint=document_.designGeometryFingerprint();
    if(!document_.evaluateParametricDesignAlternatives(candidates,&result,&issue)){
        reportError("Parametric alternatives",issue);return;
    }
    QStringList choices;
    for(const auto& item:result){
        QString choice=item.label+": ";
        if(!item.valid)choice+="REJECTED: "+item.failure;
        else choice+=QString("scene volume %1 mm^3 (%2%); known material mass %3 kg; rebuilt bodies %4")
            .arg(item.totalSolidVolumeMm3,0,'g',12)
            .arg(item.volumeChangePercent,0,'f',3)
            .arg(item.knownMassKg,0,'g',10).arg(item.rebuiltBodies);
        if(item.valid)choice+=QString("; body topology count changes %1")
            .arg(item.topologyCountChangedBodies);
        choices.append(choice);
    }
    const QString chosen=QInputDialog::getItem(this,"Geometry-rebuilt design alternatives",
        "Select one evaluated design to commit (rejected alternatives are blocked):",
        choices,1,false,&accepted);
    if(!accepted)return;
    const int index=choices.indexOf(chosen);
    if(index<0||!result[index].valid){
        reportError("Parametric alternatives","This design alternative failed the geometry preflight.");return;
    }
    if(candidates[index].changes.isEmpty())return;
    QStringList impactLines;
    for(const auto& impact:result[index].bodyImpacts){
        const double delta=impact.beforeVolumeMm3>1e-9?
            100.0*(impact.afterVolumeMm3-impact.beforeVolumeMm3)/impact.beforeVolumeMm3:0.0;
        impactLines<<QString("%1: volume %2 -> %3 mm^3 (%4%); faces %5 -> %6; edges %7 -> %8%9")
            .arg(impact.bodyName).arg(impact.beforeVolumeMm3,0,'g',9)
            .arg(impact.afterVolumeMm3,0,'g',9).arg(delta,0,'f',2)
            .arg(impact.beforeFaces).arg(impact.afterFaces)
            .arg(impact.beforeEdges).arg(impact.afterEdges)
            .arg(impact.topologyCountChanged?" [TOPOLOGY COUNT CHANGED]":"");
    }
    if(QMessageBox::question(this,"Commit evaluated design",
        QString("Commit %1 across %2 parametric bodies with one undo checkpoint?\n\n"
                "Affected bodies and actual B-rep counts:\n%3\n\n"
                "Changed topology counts require downstream reference inspection. "
                "Equal counts do not prove stable face identity.\n"
                "All linked dependents will be rebuilt again at the commit boundary.")
            .arg(candidates[index].label).arg(ids.size())
            .arg(impactLines.isEmpty()?QStringLiteral("No changed bodies."):impactLines.join('\n')),
        QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
    if(!document_.applyParametricDesignAlternative(candidates[index],
            evaluatedGeometryFingerprint,&issue)){
        reportError("Commit parametric design alternative",issue);return;
    }
    refresh(true);
    statusBar()->showMessage("Selected geometry-rebuilt design alternative committed atomically.",9000);
}

void MainWindow::editNamedParameterBatch(){
    const QStringList ids=selectedIds();
    if(ids.size()<2||ids.size()>12){
        reportError("Coordinated parametric edit","Select 2 to 12 editable parametric bodies.");return;
    }
    QDialog dialog(this);dialog.setWindowTitle("Coordinated multi-body parametric change");
    dialog.resize(940,630);
    auto* layout=new QVBoxLayout(&dialog);
    auto* note=new QLabel("Change dimensions across the selected bodies together. The preview rebuilds "
        "all source solids and linked descendants. Commit creates one undo checkpoint. "
        "Changing a dimension explicitly replaces its saved formula, as in the single-body editor.",&dialog);
    note->setWordWrap(true);layout->addWidget(note);
    auto* table=new QTableWidget(0,4,&dialog);
    table->setHorizontalHeaderLabels({"Body","Named dimension","Current","Proposed"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    struct Input{QString partId;QString name;double initial;QDoubleSpinBox* control;};
    QVector<Input> inputs;
    for(const QString& id:ids){
        const cad::Part* part=document_.find(id);
        const auto params=document_.namedParameters(id);
        if(!part||params.isEmpty()){
            reportError("Coordinated parametric edit","Every selected body must have an editable parametric definition.");return;
        }
        const auto links=document_.parameterLinks(id);
        for(auto it=params.cbegin();it!=params.cend();++it){
            if(links.contains(it.key()))continue;
            const bool isCount=it.key()==QStringLiteral("hole_pattern_count");
            const bool isPosition=it.key()==QStringLiteral("origin_x")||
                it.key()==QStringLiteral("origin_y")||
                it.key()==QStringLiteral("hole_x")||it.key()==QStringLiteral("hole_y");
            const int row=table->rowCount();table->insertRow(row);
            auto item=[](const QString& value){auto* entry=new QTableWidgetItem(value);
                entry->setFlags(entry->flags()&~Qt::ItemIsEditable);return entry;};
            table->setItem(row,0,item(part->name));
            table->setItem(row,1,item(it.key()));
            table->setItem(row,2,item(QString::number(it.value(),'g',15)));
            auto* editor=new QDoubleSpinBox(table);editor->setDecimals(isCount?0:7);
            editor->setRange(isCount?2.0:isPosition?-1000000.0:0.00001,isCount?100.0:1000000.0);
            editor->setValue(it.value());table->setCellWidget(row,3,editor);
            inputs.append({id,it.key(),it.value(),editor});
        }
    }
    if(inputs.isEmpty()){reportError("Coordinated parametric edit","No freely editable dimensions found.");return;}
    layout->addWidget(table);
    // Optional features share the SAME final-state transaction as dimensions.
    // A feature that would be invalid at an intermediate dimension-only state
    // can now be suppressed without first committing that invalid geometry.
    auto* featureLabel=new QLabel("Optional features (change alongside dimensions)",&dialog);
    layout->addWidget(featureLabel);
    auto* featureTable=new QTableWidget(0,3,&dialog);
    featureTable->setHorizontalHeaderLabels({"Body","Feature","Active in final design"});
    featureTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    struct FeatureToggle{QString body,feature;QCheckBox* check;};
    QVector<FeatureToggle> toggles;
    QMap<QString,QStringList> proposedSuppression;
    const QVector<QPair<QString,QString>> supported{
        {"through_hole","Through hole"},{"blind_hole","Blind hole"},
        {"linear_hole_pattern","Linear hole pattern"},
        {"edge_chamfer","Edge chamfer"},{"edge_fillet","Edge fillet"}};
    for(const QString& id:ids){
        const auto history=document_.featureHistory(id);
        const auto suppressed=document_.suppressedParametricFeatures(id);
        proposedSuppression.insert(id,suppressed);
        for(const auto& option:supported){
            bool available=false;
            if(option.first=="through_hole")available=history.contains("Cut: Parametric through-hole");
            if(option.first=="blind_hole")available=history.contains("Cut: Parametric blind hole");
            if(option.first=="linear_hole_pattern")for(const QString& item:history)
                if(item.startsWith("Pattern: Linear "))available=true;
            if(option.first=="edge_chamfer")available=history.contains("Chamfer: All edges");
            if(option.first=="edge_fillet")available=history.contains("Fillet: All edges");
            if(!available)continue;
            const int row=featureTable->rowCount();featureTable->insertRow(row);
            auto* name=new QTableWidgetItem(document_.find(id)->name);
            auto* feature=new QTableWidgetItem(option.second);
            name->setFlags(name->flags()&~Qt::ItemIsEditable);
            feature->setFlags(feature->flags()&~Qt::ItemIsEditable);
            featureTable->setItem(row,0,name);featureTable->setItem(row,1,feature);
            auto* check=new QCheckBox(featureTable);
            check->setChecked(!suppressed.contains(option.first));
            featureTable->setCellWidget(row,2,check);
            toggles.append({id,option.first,check});
        }
    }
    if(!toggles.isEmpty()){featureTable->setMaximumHeight(190);layout->addWidget(featureTable);}
    else featureTable->hide();
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QMap<QString,QMap<QString,double>> edits;
    for(const auto& entry:inputs){
        const double value=entry.control->value();
        if(std::abs(value-entry.initial)>1e-9*std::max(1.0,std::abs(entry.initial)))
            edits[entry.partId].insert(entry.name,value);
    }
    bool suppressionChanged=false;
    for(const FeatureToggle& toggle:toggles){
        proposedSuppression[toggle.body].removeAll(toggle.feature);
        if(!toggle.check->isChecked())proposedSuppression[toggle.body].append(toggle.feature);
    }
    for(const QString& id:ids){
        QStringList proposed=proposedSuppression.value(id),current=document_.suppressedParametricFeatures(id);
        proposed.sort();current.sort();
        if(proposed!=current)suppressionChanged=true;
    }
    if(edits.isEmpty()&&!suppressionChanged){
        statusBar()->showMessage("No dimensions or optional features changed.",4000);return;
    }
    QString report,issue;
    if(!document_.previewCoordinatedParametricRevision(edits,proposedSuppression,&report,&issue)){
        reportError("Coordinated parametric geometry preflight",issue);return;
    }
    QDialog review(this);review.setWindowTitle("Review all parametric source and dependent solids");
    review.resize(780,500);auto* reviewLayout=new QVBoxLayout(&review);
    auto* details=new QPlainTextEdit(&review);details->setReadOnly(true);
    details->setPlainText(report);reviewLayout->addWidget(details);
    auto* approval=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&review);
    reviewLayout->addWidget(approval);
    connect(approval,&QDialogButtonBox::accepted,&review,&QDialog::accept);
    connect(approval,&QDialogButtonBox::rejected,&review,&QDialog::reject);
    if(review.exec()!=QDialog::Accepted)return;
    if(!document_.setCoordinatedParametricRevision(edits,proposedSuppression,&issue)){
        reportError("Commit coordinated parametric edit",issue);return;
    }
    refresh(true);
    statusBar()->showMessage("Multi-body dimensions, optional features and linked descendants regenerated atomically.",9000);
}

void MainWindow::editNamedParameter() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Edit named parameters","Select one parametric body.");return;}
    const QString id=ids.first();
    const auto parameters=document_.namedParameters(id);
    if(parameters.isEmpty()){
        reportError("Edit named parameters","The selected body has no editable parameter definition.");return;
    }
    const auto links=document_.parameterLinks(id);
    const auto savedFormulas=document_.namedParameterFormulas(id);
    QDialog dialog(this);
    dialog.setWindowTitle("Persistent dimension expressions · unit-checked preview");
    dialog.setMinimumWidth(560);
    auto* form=new QFormLayout(&dialog);
    QMap<QString,QLineEdit*> editors;
    std::map<std::string,cad::study::Quantity> baseline;
    std::map<std::string,cad::study::Dimension> dimensions;
    const auto quantityType=[](const QString& key){
        return key==QStringLiteral("hole_pattern_count")?cad::study::scalar:cad::study::length;
    };
    for(auto it=parameters.cbegin();it!=parameters.cend();++it){
        const bool count=it.key()==QStringLiteral("hole_pattern_count");
        const bool driven=links.contains(it.key());
        auto* field=new QLineEdit(&dialog);
        field->setText(savedFormulas.value(it.key(),
            QString::number(it.value(),'g',14)+(count?QString():QStringLiteral(" mm"))));
        field->setToolTip(driven?QStringLiteral("Driven by a parameter link. Edit the source or remove the link first."):
            QStringLiteral("Unit-aware formula, e.g. 35 mm, (width + height)/2, 2.5 cm, 1 in."));
        field->setReadOnly(driven);
        if(driven)field->setStyleSheet(QStringLiteral("color: #7d8190;"));
        editors.insert(it.key(),field);
        form->addRow(it.key()+(driven?QStringLiteral(" (linked)"):count?QStringLiteral(" (count)"):QStringLiteral(" (mm)")),field);
        const std::string name=it.key().toStdString();
        baseline.emplace(name,cad::study::Quantity{it.value(),quantityType(it.key())});
        dimensions.emplace(name,quantityType(it.key()));
    }
    auto* explanation=new QLabel(
        "Expressions accept mm, cm, m, in, ft and references to other parameters. "
        "Each result must have the correct dimension. Example: set width to 100 mm and height to width/2. "
        "Cycles, unknown names, unit mismatches and invalid final geometry are rejected. "
        "Formulas and evaluated dimensions are saved in the native scene. Editing a source dimension reevaluates its dependent formulas. "
        "Cycles, inconsistent units and invalid solids reject the whole edit.",&dialog);
    explanation->setWordWrap(true);
    form->addRow(explanation);
    auto* preview=new QLabel(&dialog);
    preview->setWordWrap(true);
    preview->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow("Preview:",preview);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    auto* previewButton=buttons->addButton(QStringLiteral("Validate expressions"),QDialogButtonBox::ActionRole);
    form->addRow(buttons);
    const auto evaluate=[&](QMap<QString,double>* changed,QString* problem)->bool{
        std::map<std::string,std::string> expressions;
        for(auto it=editors.cbegin();it!=editors.cend();++it){
            if(links.contains(it.key()))continue;
            expressions[it.key().toStdString()]=it.value()->text().trimmed().toStdString();
        }
        std::map<std::string,cad::study::Quantity> results;
        std::string error;
        if(!cad::study::evaluateParameterExpressions(baseline,expressions,dimensions,&results,&error)){
            if(problem)*problem=QString::fromStdString(error);
            return false;
        }
        changed->clear();
        for(auto it=editors.cbegin();it!=editors.cend();++it){
            if(links.contains(it.key()))continue;
            const double numeric=results.at(it.key().toStdString()).value;
            const bool count=it.key()==QStringLiteral("hole_pattern_count");
            if(count&&(numeric!=std::floor(numeric)||numeric<2||numeric>100)){
                if(problem)*problem=QStringLiteral("hole_pattern_count must be an integer from 2 to 100.");
                return false;
            }
            if(!qFuzzyCompare(numeric+1.0,parameters.value(it.key())+1.0))changed->insert(it.key(),numeric);
        }
        return true;
    };
    connect(previewButton,&QPushButton::clicked,&dialog,[&]{
        QMap<QString,double> changed;QString error;
        if(!evaluate(&changed,&error)){
            preview->setText(QStringLiteral("Invalid: ")+error);return;
        }
        QStringList details;
        for(auto it=changed.cbegin();it!=changed.cend();++it)
            details.append(QStringLiteral("%1 = %2%3").arg(it.key()).arg(it.value(),0,'g',12)
                .arg(it.key()==QStringLiteral("hole_pattern_count")?QString():QStringLiteral(" mm")));
        QMap<QString,QString> stagedFormulas;
        for(auto it=editors.cbegin();it!=editors.cend();++it)
            if(!links.contains(it.key()))stagedFormulas.insert(it.key(),it.value()->text().trimmed());
        QString solidReport;
        if(!document_.previewNamedParameterFormulas(id,stagedFormulas,&solidReport,&error)){
            preview->setText("Geometry preflight rejected: "+error);return;
        }
        preview->setText((details.isEmpty()?QStringLiteral("No dimensional changes."):details.join("; "))+
                         "\n"+solidReport);
    });
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QMap<QString,double> changed;QString error;
    if(!evaluate(&changed,&error)){
        reportError("Invalid dimensional expression",error);return;
    }
    QMap<QString,QString> formulas;
    for(auto it=editors.cbegin();it!=editors.cend();++it){
        if(!links.contains(it.key()))formulas.insert(it.key(),it.value()->text().trimmed());
    }
    if(!document_.setNamedParameterFormulas(id,formulas,&error)){
        reportError("Save persistent dimension formulas",error);return;
    }
    refresh();
    statusBar()->showMessage(QStringLiteral("Expressions and evaluated dimensions saved; dependent solids rebuilt atomically."),7000);
}
void MainWindow::exportFeatureInspectionNotes(){
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Feature inspection report","Select one feature-history body.");return;}
    const QString output=QFileDialog::getSaveFileName(this,"Export revision-stamped inspection review",
        QStringLiteral("feature-inspection.csv"),"CSV review sheets (*.csv)");
    if(output.isEmpty())return;
    QString error;
    if(!document_.exportFeatureInspectionCsv(ids.first(),output,&error)){
        reportError("Export feature inspection report",error);return;
    }
    statusBar()->showMessage("Feature inspection review exported, including stale/orphaned note statuses.",8000);
}

void MainWindow::editFeatureInspectionNotes(){
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Feature inspection notes","Select exactly one body.");return;}
    const QString id=ids.first();
    const cad::Part* part=document_.find(id);
    if(!part||part->isSurfaceBody){
        reportError("Feature inspection notes","Choose a solid with supported feature history.");return;
    }
    const QStringList features=document_.inspectionFeatureIds(id);
    const auto original=document_.featureInspectionNotes(id);
    if(features.isEmpty()&&original.isEmpty()){
        reportError("Feature inspection notes","No persistent shape-feature or mixed-history node is available."
            " Create an editable feature history before attaching an inspection note.");return;
    }
    QDialog dialog(this);dialog.setWindowTitle("Feature-linked engineering and inspection notes");
    dialog.resize(980,560);
    auto* layout=new QVBoxLayout(&dialog);
    auto* info=new QLabel("Notes are linked to stable feature IDs, not face indices. "
        "After any geometry revision, existing notes require explicit engineer review. "
        "This is an inspection worksheet, not a certified manufacturing approval.",&dialog);
    info->setWordWrap(true);layout->addWidget(info);
    auto* table=new QTableWidget(&dialog);
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({"Source feature ID","Category","Engineering / inspection requirement","Review status"});
    table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(3,QHeaderView::ResizeToContents);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(table,1);
    auto insertRow=[&](const cad::FeatureInspectionNote& note){
        const int row=table->rowCount();table->insertRow(row);
        auto* feature=new QComboBox(table);
        for(const QString& featureId:features)feature->addItem(featureId,featureId);
        if(feature->findData(note.featureId)<0&& !note.featureId.isEmpty())
            feature->addItem(QStringLiteral("MISSING: ")+note.featureId,note.featureId);
        feature->setCurrentIndex(feature->findData(note.featureId));
        if(feature->currentIndex()<0&&feature->count()>0)feature->setCurrentIndex(0);
        table->setCellWidget(row,0,feature);
        auto* category=new QComboBox(table);
        category->addItems({"DESIGN","INSPECTION","PROCESS"});
        category->setCurrentText(note.category.isEmpty()?QStringLiteral("INSPECTION"):note.category);
        table->setCellWidget(row,1,category);
        auto* noteItem=new QTableWidgetItem(note.text);
        noteItem->setData(Qt::UserRole,note.id);
        noteItem->setData(Qt::UserRole+1,note.reviewedGeometryToken);
        table->setItem(row,2,noteItem);
        const QString source=feature->currentData().toString();
        const QString status=!features.contains(source)?QStringLiteral("MISSING FEATURE"):
            note.reviewedGeometryToken==part->geometryToken?QStringLiteral("Current revision"):
                                                       QStringLiteral("REVIEW REQUIRED");
        auto* label=new QTableWidgetItem(status);
        label->setFlags(label->flags()&~Qt::ItemIsEditable);
        table->setItem(row,3,label);
    };
    for(const auto& note:original)insertRow(note);
    auto* actions=new QHBoxLayout;
    auto* add=new QPushButton("Add note",&dialog);
    auto* remove=new QPushButton("Remove selected note",&dialog);
    auto* markReviewed=new QPushButton("Review selected for current geometry",&dialog);
    actions->addWidget(add);actions->addWidget(remove);actions->addWidget(markReviewed);
    actions->addStretch();layout->addLayout(actions);
    connect(add,&QPushButton::clicked,&dialog,[&]{
        if(features.isEmpty()||table->rowCount()>=100){
            QMessageBox::warning(&dialog,"Feature notes","No available feature IDs or the 100-note limit was reached.");return;
        }
        cad::FeatureInspectionNote item;
        item.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        item.featureId=features.first();item.category="INSPECTION";
        item.reviewedGeometryToken=part->geometryToken;
        insertRow(item);
        table->selectRow(table->rowCount()-1);
    });
    connect(remove,&QPushButton::clicked,&dialog,[&]{
        const int row=table->currentRow();if(row>=0)table->removeRow(row);
    });
    connect(markReviewed,&QPushButton::clicked,&dialog,[&]{
        const int row=table->currentRow();if(row<0)return;
        auto* feature=qobject_cast<QComboBox*>(table->cellWidget(row,0));
        if(!feature||!features.contains(feature->currentData().toString())){
            QMessageBox::warning(&dialog,"Feature notes","Missing feature identity cannot be reviewed.");return;
        }
        if(QMessageBox::question(&dialog,"Confirm engineering review",
           "Have you inspected this feature against the CURRENT model geometry? "
           "This updates the revision acknowledgement only; it does not certify dimensional conformity.")
           !=QMessageBox::Yes)return;
        auto* note=table->item(row,2);if(!note)return;
        note->setData(Qt::UserRole+1,part->geometryToken);
        table->item(row,3)->setText("Current revision");
    });
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);
    layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{
        QVector<cad::FeatureInspectionNote> notes;
        for(int row=0;row<table->rowCount();++row){
            const auto* feature=qobject_cast<QComboBox*>(table->cellWidget(row,0));
            const auto* category=qobject_cast<QComboBox*>(table->cellWidget(row,1));
            const auto* text=table->item(row,2);
            if(!feature||!category||!text){
                QMessageBox::warning(&dialog,"Feature notes","An annotation row is incomplete.");return;
            }
            notes.append({text->data(Qt::UserRole).toString(),feature->currentData().toString(),
                          category->currentText(),text->text().trimmed(),
                          text->data(Qt::UserRole+1).toString()});
        }
        QString error;
        if(!document_.setFeatureInspectionNotes(id,notes,&error)){
            QMessageBox::warning(&dialog,"Feature notes",error);return;
        }
        dialog.accept();
    });
    if(dialog.exec()==QDialog::Accepted){
        refresh();statusBar()->showMessage("Feature-linked inspection notes saved. Export a revisioned review CSV for handoff.",8000);
    }
}

void MainWindow::showConfigurationStudy() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Design configurations","Select one parametric solid.");return;}
    const QString id=ids.first();
    const auto parameters=document_.namedParameters(id);
    if(parameters.isEmpty()){
        reportError("Design configurations","The selected body has no editable named dimensions.");return;
    }
    const auto links=document_.parameterLinks(id);
    std::map<std::string,cad::study::Quantity> baseline;
    std::map<std::string,cad::study::Dimension> dimensions;
    QStringList values;
    for(auto it=parameters.cbegin();it!=parameters.cend();++it){
        const bool count=it.key()==QStringLiteral("hole_pattern_count");
        const auto dimension=count?cad::study::scalar:cad::study::length;
        baseline.emplace(it.key().toStdString(),cad::study::Quantity{it.value(),dimension});
        dimensions.emplace(it.key().toStdString(),dimension);
        if(!links.contains(it.key()))values<<it.key()+QStringLiteral("=")+
            QString::number(it.value(),'g',12)+(count?QString():QStringLiteral(" mm"));
    }
    QDialog dialog(this);
    dialog.setWindowTitle("CAD configuration study · explicit dimensional expressions");
    dialog.resize(900,610);
    auto* layout=new QVBoxLayout(&dialog);
    auto* intro=new QLabel("One configuration per line: Name: width=100 mm; height=width/2; depth=25 mm. "
        "Validate and save up to 32 named presets inside this CAD project; any saved preset can then "
        "be applied as one undoable model rebuild. .cadcfg sidecars remain optional. "
        "Arbitrary cross-part variant switching is not supported.",&dialog);
    intro->setWordWrap(true);layout->addWidget(intro);
    auto* editor=new QPlainTextEdit(&dialog);
    const auto savedConfigurations=document_.designConfigurations(id);
    QStringList initialLines;
    for(const auto& saved:savedConfigurations){
        QStringList assignments;
        for(auto it=saved.expressions.cbegin();it!=saved.expressions.cend();++it)
            assignments<<it.key()+QStringLiteral("=")+it.value();
        initialLines<<saved.name+QStringLiteral(": ")+assignments.join(QStringLiteral("; "));
    }
    editor->setPlainText(initialLines.isEmpty()?QStringLiteral("Baseline: ")+values.join(QStringLiteral("; ")):
                         initialLines.join(QStringLiteral("\n")));
    layout->addWidget(editor,2);
    auto* selection=new QComboBox(&dialog);
    layout->addWidget(selection);
    auto* feedback=new QPlainTextEdit(&dialog);feedback->setReadOnly(true);
    feedback->setPlaceholderText("Validate variants to see computed dimensions and errors.");
    layout->addWidget(feedback,1);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);
    auto* store=buttons->addButton(QStringLiteral("Save presets in project"),QDialogButtonBox::ActionRole);
    auto* validate=buttons->addButton(QStringLiteral("Validate variants"),QDialogButtonBox::ActionRole);
    auto* apply=buttons->addButton(QStringLiteral("Apply selected variant"),QDialogButtonBox::ActionRole);
    auto* compare=buttons->addButton(QStringLiteral("Compare rebuilt BOM and mass..."),QDialogButtonBox::ActionRole);
    auto* load=buttons->addButton(QStringLiteral("Load .cadcfg..."),QDialogButtonBox::ActionRole);
    auto* save=buttons->addButton(QStringLiteral("Save .cadcfg..."),QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);
    // Both the preview and the project save use one parser; this prevents a
    // syntax variant being shown as valid but stored with different semantics.
    auto parseSaved=[&](QString* issue)->QVector<cad::SavedDesignConfiguration>{
        QVector<cad::SavedDesignConfiguration> configurations;
        QSet<QString> names;
        for(const QString& raw:editor->toPlainText().split('\n')){
            const QString line=raw.trimmed();if(line.isEmpty()||line.startsWith('#'))continue;
            if(configurations.size()>=32){*issue="Maximum 32 configurations.";return {};}
            const qsizetype colon=line.indexOf(':');
            if(colon<=0){*issue="Expected Name: parameter=expression; ...";return {};}
            cad::SavedDesignConfiguration item;item.name=line.left(colon).trimmed();
            if(item.name.isEmpty()||item.name.size()>48||names.contains(item.name.toCaseFolded())){
                *issue="Configuration names must be unique and at most 48 characters.";return {};
            }
            names.insert(item.name.toCaseFolded());
            for(const QString& rawPair:line.mid(colon+1).split(';',Qt::SkipEmptyParts)){
                const QString pair=rawPair.trimmed();
                const qsizetype equals=pair.indexOf('=');
                if(equals<=0){*issue="Invalid assignment: "+pair;return {};}
                const QString name=pair.left(equals).trimmed();
                const QString expression=pair.mid(equals+1).trimmed();
                if(!parameters.contains(name)||links.contains(name)||expression.isEmpty()||
                   item.expressions.contains(name)||expression.size()>256){
                    *issue="Unknown, linked, duplicate or invalid dimension expression: "+name;
                    return {};
                }
                item.expressions.insert(name,expression);
            }
            if(item.expressions.isEmpty()||item.expressions.size()>16){
                *issue="Each configuration must set 1 to 16 dimensions.";return {};
            }
            configurations.append(std::move(item));
        }
        if(configurations.isEmpty())*issue="No configurations were provided.";
        return configurations;
    };
    auto evaluate=[&]()->std::vector<cad::study::ConfigurationResult>{
        QString issue;
        const auto definitions=parseSaved(&issue);
        if(!issue.isEmpty())return {{"",false,{},issue.toStdString()}};
        std::vector<cad::study::Configuration> candidates;
        for(const auto& definition:definitions){
            cad::study::Configuration candidate;candidate.name=definition.name.toStdString();
            for(auto it=definition.expressions.cbegin();it!=definition.expressions.cend();++it)
                candidate.parameters.emplace(it.key().toStdString(),it.value().toStdString());
            candidates.push_back(std::move(candidate));
        }
        return cad::study::evaluateConfigurations(baseline,dimensions,candidates);
    };
    std::vector<cad::study::ConfigurationResult> current;
    connect(validate,&QPushButton::clicked,&dialog,[&]{
        current=evaluate();selection->clear();QStringList lines;
        for(std::size_t i=0;i<current.size();++i){
            const auto& variant=current[i];
            selection->addItem(QString::fromStdString(variant.name),static_cast<int>(i));
            lines<<QStringLiteral("%1: %2").arg(QString::fromStdString(variant.name),
                variant.valid?QStringLiteral("valid"):QStringLiteral("INVALID - ")+QString::fromStdString(variant.error));
            if(variant.valid){
                QStringList dimensionsText;
                for(const auto& [name,value]:variant.values)
                    dimensionsText<<QStringLiteral("%1=%2%3").arg(QString::fromStdString(name))
                         .arg(value.value,0,'g',12).arg(value.unit==cad::study::scalar?QString():QStringLiteral(" mm"));
                lines<<QStringLiteral("  ")+dimensionsText.join(QStringLiteral("; "));
            }
        }
        feedback->setPlainText(lines.join('\n'));
    });
    connect(store,&QPushButton::clicked,&dialog,[&]{
        QString issue;
        const auto definitions=parseSaved(&issue);
        if(!issue.isEmpty()){
            QMessageBox::warning(&dialog,"Save design configurations",issue);return;
        }
        QString error;
        if(!document_.setDesignConfigurations(id,definitions,&error)){
            QMessageBox::warning(&dialog,"Save design configurations",error);return;
        }
        refresh();
        feedback->appendPlainText(QStringLiteral("Saved %1 named configuration(s) inside the CAD project. ")
            .arg(definitions.size())+QStringLiteral("Save the .cadscene file to retain them across sessions."));
    });
    connect(apply,&QPushButton::clicked,&dialog,[&]{
        // Always re-evaluate to avoid applying a stale preview after editing.
        current=evaluate();
        const int index=selection->currentIndex();
        if(index<0||index>=static_cast<int>(current.size())||!current[static_cast<std::size_t>(index)].valid||
           QString::fromStdString(current[static_cast<std::size_t>(index)].name)!=selection->currentText()){
            QMessageBox::warning(&dialog,"Configuration study","Validate and select a valid configuration first.");return;
        }
        const auto& chosen=current[static_cast<std::size_t>(index)];
        QMap<QString,double> changes;
        for(const auto& [name,quantity]:chosen.values){
            const QString key=QString::fromStdString(name);
            if(links.contains(key))continue;
            const bool count=key==QStringLiteral("hole_pattern_count");
            if(count&&(quantity.value<2||quantity.value>100||std::floor(quantity.value)!=quantity.value)){
                QMessageBox::warning(&dialog,"Configuration study","Pattern count must be an integer from 2 to 100.");return;
            }
            if(!qFuzzyCompare(parameters.value(key)+1,quantity.value+1))changes.insert(key,quantity.value);
        }
        QString issue;
        const auto edited=parseSaved(&issue);
        if(!issue.isEmpty()){
            QMessageBox::warning(&dialog,"Invalid configuration",issue);return;
        }
        const QString selectedName=QString::fromStdString(chosen.name);
        const auto saved=document_.designConfigurations(id);
        const auto selected=std::find_if(edited.cbegin(),edited.cend(),
            [&](const cad::SavedDesignConfiguration& value){return value.name==selectedName;});
        const auto persistent=std::find_if(saved.cbegin(),saved.cend(),
            [&](const cad::SavedDesignConfiguration& value){return value.name==selectedName;});
        const bool stored=selected!=edited.cend()&&persistent!=saved.cend()&&
                          selected->expressions==persistent->expressions;
        if(!stored&&changes.isEmpty()){
            feedback->appendPlainText("Selected ad-hoc variant already matches the current model.");return;
        }
        QString error;
        if(!(stored?document_.applyDesignConfiguration(id,selectedName,&error):
                    document_.setNamedParameters(id,changes,&error))){
            QMessageBox::warning(&dialog,"Invalid configuration",error);return;
        }
        refresh();
        QMessageBox::information(&dialog,"Configuration applied",
            stored?"Stored configuration applied in one undoable rebuild.":
                   "Ad-hoc configuration applied. Save presets in the project to retain them.");
        dialog.accept();
    });
    connect(compare,&QPushButton::clicked,&dialog,[&]{
        // Compare only committed presets. Unsaved editor text is not considered
        // a project revision and cannot be presented as a verified CAD variant.
        QVector<cad::ConfigurationBomComparison> rows;
        QString issue;
        if(!document_.compareDesignConfigurations(id,&rows,&issue)){
            QMessageBox::warning(&dialog,"Compare saved configurations",issue);return;
        }
        QString report=QStringLiteral("Read-only comparison of %1 freshly rebuilt CAD states.\n")
            .arg(rows.size());
        report+=QStringLiteral("No change is made to the current project or its undo history.\n")
                +QStringLiteral("Unknown material density means assembly mass is incomplete.\n\n");
        for(const auto& row:rows){
            report+=QStringLiteral("%1%2\n  Target: %3 mm^3 | assembly: %4 mm^3\n"
                                   "  Known mass: %5 kg%6 | BOM: %7 lines / %8 pieces\n\n")
                .arg(row.name,row.current?QStringLiteral(" [CURRENT]"):QString())
                .arg(row.targetVolumeMm3,0,'g',12)
                .arg(row.totalAssemblyVolumeMm3,0,'g',12)
                .arg(row.knownAssemblyMassKg,0,'g',12)
                .arg(row.allMassesKnown?QString():QStringLiteral(" [INCOMPLETE DENSITY]"))
                .arg(row.bomLines).arg(row.componentQuantity);
        }
        QDialog result(&dialog);result.setWindowTitle("Rebuilt variant / BOM comparison");
        result.resize(700,480);
        auto* outputLayout=new QVBoxLayout(&result);
        auto* description=new QLabel("Model-recomputed report; not a manufacturing approval.",&result);
        outputLayout->addWidget(description);
        auto* view=new QPlainTextEdit(&result);view->setReadOnly(true);view->setPlainText(report);
        outputLayout->addWidget(view);
        auto* actions=new QDialogButtonBox(QDialogButtonBox::Close,&result);
        auto* exportCsv=actions->addButton("Export revision-linked CSV...",QDialogButtonBox::ActionRole);
        outputLayout->addWidget(actions);
        connect(actions,&QDialogButtonBox::rejected,&result,&QDialog::reject);
        connect(exportCsv,&QPushButton::clicked,&result,[&]{
            const QString path=QFileDialog::getSaveFileName(&result,"Save configuration/BOM comparison",
                "configuration-bom-comparison.csv","CSV files (*.csv)");
            if(path.isEmpty())return;
            QString saveError;
            if(!document_.exportConfigurationComparisonCsv(id,path,&saveError))
                QMessageBox::warning(&result,"Export configuration comparison",saveError);
        });
        result.exec();
    });
    connect(load,&QPushButton::clicked,&dialog,[&]{
        const QString fileName=QFileDialog::getOpenFileName(&dialog,"Load CAD configurations",{},"CAD configurations (*.cadcfg);;Text files (*.txt)");
        if(fileName.isEmpty())return;
        QFile file(fileName);
        if(!file.open(QIODevice::ReadOnly)||file.size()>65536){
            QMessageBox::warning(&dialog,"Configuration study","Could not open configuration file or it exceeds 64 KB.");return;
        }
        editor->setPlainText(QString::fromUtf8(file.readAll()));
        selection->clear();feedback->clear();current.clear();
    });
    connect(save,&QPushButton::clicked,&dialog,[&]{
        const QString fileName=QFileDialog::getSaveFileName(&dialog,"Save CAD configurations","design-variants.cadcfg","CAD configurations (*.cadcfg)");
        if(fileName.isEmpty())return;
        QSaveFile file(fileName);
        const QByteArray bytes=editor->toPlainText().toUtf8();
        if(bytes.size()>65536||!file.open(QIODevice::WriteOnly|QIODevice::Text)||
           file.write(bytes)!=bytes.size()||!file.commit())
            QMessageBox::warning(&dialog,"Configuration study","Could not save the configuration sidecar.");
    });
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    dialog.exec();
}

void MainWindow::editParameterLinks() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Edit parameter links","Select one parametric body.");return;}
    const QString id=ids.first();
    const QMap<QString,double> parameters=document_.namedParameters(id);
    if(parameters.isEmpty()){reportError("Edit parameter links","The selected body has no editable parameter definition.");return;}
    QStringList dimensions;
    for(auto it=parameters.cbegin();it!=parameters.cend();++it)
        if(it.key()=="width"||it.key()=="height"||it.key()=="depth"||it.key()=="hole_diameter"||it.key()=="hole_depth"||it.key()=="hole_pattern_spacing")
            dimensions.append(it.key());
    if(dimensions.size()<2){reportError("Edit parameter links","This feature history does not have enough linkable dimensions.");return;}
    const auto links=document_.parameterLinks(id);
    QDialog dialog(this);dialog.setWindowTitle("Edit named parameter links");auto* form=new QFormLayout(&dialog);
    auto* target=new QComboBox(&dialog);auto* source=new QComboBox(&dialog);
    target->addItems(dimensions);source->addItems(dimensions);
    auto* scale=new QDoubleSpinBox(&dialog);scale->setRange(0.000001,1000000.0);scale->setDecimals(6);scale->setSingleStep(0.1);scale->setValue(1.0);
    auto* offset=new QDoubleSpinBox(&dialog);offset->setRange(-1000000.0,1000000.0);offset->setDecimals(4);offset->setSingleStep(1.0);offset->setValue(0.0);offset->setSuffix(" mm");
    auto* current=new QLabel(&dialog);current->setWordWrap(true);
    form->addRow("Driven target dimension:",target);form->addRow("Source dimension:",source);
    form->addRow("Scale:",scale);form->addRow("Offset:",offset);form->addRow("Current links:",current);
    auto* explanation=new QLabel("Links rebuild the model as one undoable edit. Cycles, invalid dimensions, and edits that violate sketch containment are rejected without changing the current solid.",&dialog);
    explanation->setWordWrap(true);form->addRow(explanation);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    buttons->button(QDialogButtonBox::Ok)->setText("Set / replace link");
    auto* remove=buttons->addButton("Remove selected link",QDialogButtonBox::ActionRole);
    form->addRow(buttons);
    auto refreshLinkEditor=[&]{
        const QString driven=target->currentText();
        const auto link=links.constFind(driven);
        if(link==links.cend())current->setText("No link for this target.");
        else current->setText(QStringLiteral("%1 = %2 × %3 %4 %5 mm").arg(driven,link->source)
            .arg(link->scale,0,'g',8).arg(link->offset<0.0?QStringLiteral("−"):QStringLiteral("+")).arg(std::abs(link->offset),0,'g',8));
        remove->setEnabled(links.contains(driven));
        const QString defaultSource=dimensions.first()==driven?dimensions.value(1):dimensions.first();
        const int sourceIndex=source->findText(link==links.cend()?defaultSource:link->source);
        if(sourceIndex>=0)source->setCurrentIndex(sourceIndex);
        scale->setValue(link==links.cend()?1.0:link->scale);
        offset->setValue(link==links.cend()?0.0:link->offset);
    };
    connect(target,&QComboBox::currentTextChanged,&dialog,[&]{refreshLinkEditor();});
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(remove,&QPushButton::clicked,&dialog,[&]{
        QString error;
        if(!document_.removeParameterLink(id,target->currentText(),&error))reportError("Remove parameter link",error);
        else{refresh();dialog.accept();}
    });
    refreshLinkEditor();
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.setParameterLink(id,target->currentText(),source->currentText(),scale->value(),offset->value(),&error)){
        reportError("Set parameter link",error);return;
    }
    refresh();
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
    const bool editable=document_.parametricDefinitionCurrent(ids.first());
    if(!editable||features.isEmpty()){
        QMessageBox::information(this,"Parametric sketch and feature history",lines.join('\n'));return;
    }
    const QStringList suppressed=document_.suppressedParametricFeatures(ids.first());
    QStringList choices;
    QStringList featureIds;
    const auto addFeatureChoice=[&](const QString& label,const QString& featureId){
        choices.append(QStringLiteral("%1 [%2]").arg(label,
            suppressed.contains(featureId)?QStringLiteral("suppressed"):QStringLiteral("active")));
        featureIds.append(featureId);
    };
    if(features.contains(QStringLiteral("Cut: Parametric through-hole")))
        addFeatureChoice(QStringLiteral("Through-hole cut"),QStringLiteral("through_hole"));
    if(features.contains(QStringLiteral("Cut: Parametric blind hole")))
        addFeatureChoice(QStringLiteral("Blind-hole cut"),QStringLiteral("blind_hole"));
    if(features.contains(QStringLiteral("Pattern: Linear through-hole array along X"))||
       features.contains(QStringLiteral("Pattern: Linear through-hole array along Y"))||
       features.contains(QStringLiteral("Pattern: Linear blind-hole array along X"))||
       features.contains(QStringLiteral("Pattern: Linear blind-hole array along Y")))
        addFeatureChoice(QStringLiteral("Linear hole pattern"),QStringLiteral("linear_hole_pattern"));
    if(features.contains(QStringLiteral("Chamfer: All edges")))
        addFeatureChoice(QStringLiteral("All-edge chamfer"),QStringLiteral("edge_chamfer"));
    if(features.contains(QStringLiteral("Fillet: All edges")))
        addFeatureChoice(QStringLiteral("All-edge fillet"),QStringLiteral("edge_fillet"));
    if(choices.isEmpty()){
        QMessageBox::information(this,"Parametric sketch and feature history",lines.join('\n'));return;
    }
    QDialog dialog(this);dialog.setWindowTitle("Edit feature history");dialog.resize(610,480);
    auto* layout=new QVBoxLayout(&dialog);
    auto* summary=new QPlainTextEdit(lines.join('\n'),&dialog);summary->setReadOnly(true);
    layout->addWidget(summary);
    layout->addWidget(new QLabel("Active optional operations (clear a check box to suppress):",&dialog));
    QVector<QCheckBox*> toggles;toggles.reserve(featureIds.size());
    for(int i=0;i<featureIds.size();++i){
        auto* toggle=new QCheckBox(choices[i].section(" [",0,0),&dialog);
        toggle->setChecked(!suppressed.contains(featureIds[i]));
        layout->addWidget(toggle);toggles.append(toggle);
    }
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QStringList proposed;
    for(int i=0;i<featureIds.size();++i)if(!toggles[i]->isChecked())proposed.append(featureIds[i]);
    QString error;
    if(!document_.setParametricFeaturesSuppressed(ids.first(),proposed,&error)){
        reportError("Edit feature history",error);return;
    }
    refresh();statusBar()->showMessage("Feature history rebuilt from the selected active operations.",8000);
}

void MainWindow::createLoftedSurfaceBody() {
    QDialog dialog(this);dialog.setWindowTitle("Lofted surface body");dialog.setMinimumWidth(420);
    auto* form=new QFormLayout(&dialog);
    auto makeLength=[&](double value){
        auto* field=new QDoubleSpinBox(&dialog);field->setRange(0.001,1000000.0);field->setDecimals(3);
        field->setSingleStep(1.0);field->setSuffix(" mm");field->setValue(value);return field;
    };
    auto* bottomWidth=makeLength(40);auto* bottomHeight=makeLength(30);
    auto* middleWidth=makeLength(32);auto* middleHeight=makeLength(24);auto* middleZ=makeLength(40);
    auto* topWidth=makeLength(24);auto* topHeight=makeLength(18);auto* topZ=makeLength(80);
    form->addRow("Bottom section width:",bottomWidth);form->addRow("Bottom section height:",bottomHeight);
    form->addRow("Middle section width:",middleWidth);form->addRow("Middle section height:",middleHeight);
    form->addRow("Middle section Z:",middleZ);form->addRow("Top section width:",topWidth);
    form->addRow("Top section height:",topHeight);form->addRow("Top section Z:",topZ);
    auto* explanation=new QLabel("Creates an open surface shell through three centered rectangular sections. It remains a surface body until stitched or thickened.",&dialog);
    explanation->setWordWrap(true);form->addRow(explanation);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    if(middleZ->value()>=topZ->value()){
        reportError("Lofted surface","The top section Z position must be above the middle section.");return;
    }
    auto rectangleWire=[](double width,double height,double z){
        const double x=width*0.5,y=height*0.5;
        BRepBuilderAPI_MakePolygon polygon;
        polygon.Add(gp_Pnt(-x,-y,z));polygon.Add(gp_Pnt(x,-y,z));
        polygon.Add(gp_Pnt(x,y,z));polygon.Add(gp_Pnt(-x,y,z));polygon.Close();
        return polygon.IsDone()?polygon.Wire():TopoDS_Wire{};
    };
    const std::vector<TopoDS_Wire> sections{
        rectangleWire(bottomWidth->value(),bottomHeight->value(),0.0),
        rectangleWire(middleWidth->value(),middleHeight->value(),middleZ->value()),
        rectangleWire(topWidth->value(),topHeight->value(),topZ->value())};
    TopoDS_Shell shell;QString error;
    if(!cad::makeLoftedSurfaceShell(sections,&shell,&error)){
        reportError("Lofted surface",error);return;
    }
    if(!document_.addSurfaceBody("Lofted Surface",shell,&error)){
        reportError("Lofted surface",error);return;
    }
    refresh(true);statusBar()->showMessage("Created a lofted surface body from three sections.",8000);
}

void MainWindow::loftSelectedFaceBoundaries() {
    const auto selected=viewport_->selectedFaceReferences();
    if(selected.size()<2||selected.size()>3){
        reportError("Freeform surface loft",
            "Switch to Select > Faces, then select two or three source faces. "
            "The selected faces may have different polygon edge counts.");
        return;
    }
    std::vector<TopoDS_Wire> sections;
    sections.reserve(static_cast<std::size_t>(selected.size()));
    for(const auto& ref:selected){
        const TopoDS_Shape picked=selectedSubshape(document_,ref.partId,
            ref.geometryToken,ref.faceIndex,TopAbs_FACE);
        if(picked.IsNull()){
            reportError("Freeform surface loft","A source face has changed. Select the faces again.");
            return;
        }
        const TopoDS_Face face=TopoDS::Face(picked);
        int wireCount=0;
        for(TopExp_Explorer wires(face,TopAbs_WIRE);wires.More();wires.Next())++wireCount;
        if(wireCount!=1){
            reportError("Freeform surface loft","Each source face must have one outer boundary. "
                "Faces containing holes or internal trimming loops are not supported by this loft command.");
            return;
        }
        const TopoDS_Wire outline=BRepTools::OuterWire(face);
        if(outline.IsNull()){
            reportError("Freeform surface loft","Could not identify the selected face's outer wire.");
            return;
        }
        sections.push_back(outline);
    }
    TopoDS_Shell shell;
    QString error;
    if(!cad::makeLoftedSurfaceShell(sections,&shell,&error)){
        reportError("Freeform surface loft",error);return;
    }
    if(!document_.addSurfaceBody("Face Boundary Loft",shell,&error)){
        reportError("Freeform surface loft",error);return;
    }
    refresh(true);
    statusBar()->showMessage("Created an open lofted surface from selected face boundaries. "
        "The source faces are not associatively linked.",9500);
}

void MainWindow::extrudeSelectedEdgeSurface() {
    const auto edges=viewport_->selectedEdgeReferences();
    if(edges.size()!=1){QMessageBox::information(this,"Extruded surface","Switch to Select → Edges and select exactly one boundary edge.");return;}
    const auto values=valuesDialog(this,"Extrude selected edge as a surface",
        {"X translation (mm)","Y translation (mm)","Z translation (mm)"},{0,0,25},-1e6,1e6);
    if(!values)return;
    const auto edge=selectedSubshape(document_,edges.first().partId,edges.first().geometryToken,edges.first().edgeIndex,TopAbs_EDGE);
    if(edge.IsNull()){reportError("Extruded surface","The selected edge reference is stale. Select it again.");return;}
    TopoDS_Face face;QString error;
    if(!cad::makeExtrudedSurface(TopoDS::Edge(edge),gp_Vec((*values)[0],(*values)[1],(*values)[2]),&face,&error)||
       !document_.addSurfaceBody("Extruded Surface",face,&error)){
        reportError("Extruded surface",error);return;
    }
    refresh(true);statusBar()->showMessage("Created a persisted surface body from the selected edge.",8000);
}

void MainWindow::revolveSelectedEdgeSurface() {
    const auto edges=viewport_->selectedEdgeReferences();
    if(edges.size()!=1){QMessageBox::information(this,"Revolved surface","Switch to Select → Edges and select exactly one profile edge.");return;}
    const auto values=valuesDialog(this,"Revolve selected edge about world Z",
        {"Axis X (mm)","Axis Y (mm)","Axis Z (mm)","Revolve angle (degrees)"},{0,0,0,90},-1e6,1e6);
    if(!values)return;
    if((*values)[3]<0.001||(*values)[3]>360.0){reportError("Revolved surface","Angle must be between 0.001 and 360 degrees.");return;}
    const auto edge=selectedSubshape(document_,edges.first().partId,edges.first().geometryToken,edges.first().edgeIndex,TopAbs_EDGE);
    if(edge.IsNull()){reportError("Revolved surface","The selected edge reference is stale. Select it again.");return;}
    TopoDS_Face face;QString error;
    const gp_Ax1 axis(gp_Pnt((*values)[0],(*values)[1],(*values)[2]),gp_Dir(0,0,1));
    if(!cad::makeRevolvedSurface(TopoDS::Edge(edge),axis,(*values)[3]*std::numbers::pi/180.0,&face,&error)||
       !document_.addSurfaceBody("Revolved Surface",face,&error)){
        reportError("Revolved surface",error);return;
    }
    refresh(true);statusBar()->showMessage("Created a persisted revolved surface body.",8000);
}

void MainWindow::offsetSelectedSurface() {
    const auto faces=viewport_->selectedFaceReferences();
    if(faces.size()!=1){QMessageBox::information(this,"Offset surface","Switch to Select → Faces and select exactly one surface face.");return;}
    const auto values=valuesDialog(this,"Offset selected surface face",{"Offset distance (mm)","Kernel tolerance (mm)"},{2,0.01},-1e6,1e6);
    if(!values)return;
    const auto face=selectedSubshape(document_,faces.first().partId,faces.first().geometryToken,faces.first().faceIndex,TopAbs_FACE);
    if(face.IsNull()){reportError("Offset surface","The selected face reference is stale. Select it again.");return;}
    TopoDS_Shape offset;QString error;
    if(!cad::makeOffsetSurface(TopoDS::Face(face),(*values)[0],(*values)[1],&offset,&error)||
       !document_.addSurfaceBody("Offset Surface",offset,&error)){
        reportError("Offset surface",error);return;
    }
    refresh(true);statusBar()->showMessage("Created an offset surface body.",8000);
}

void MainWindow::extendSelectedSurface() {
    const auto faces=viewport_->selectedFaceReferences();
    if(faces.size()!=1){QMessageBox::information(this,"Extend surface","Switch to Select → Faces and select exactly one surface face.");return;}
    QDialog dialog(this);dialog.setWindowTitle("Extend selected surface face");auto* form=new QFormLayout(&dialog);
    auto* distance=new QDoubleSpinBox(&dialog);distance->setRange(0.000001,1000000);distance->setDecimals(3);distance->setValue(5);distance->setSuffix(" mm");
    auto* uMin=new QCheckBox("U minimum",&dialog);auto* uMax=new QCheckBox("U maximum",&dialog);
    auto* vMin=new QCheckBox("V minimum",&dialog);auto* vMax=new QCheckBox("V maximum",&dialog);uMax->setChecked(true);
    form->addRow("Extension distance:",distance);form->addRow(uMin);form->addRow(uMax);form->addRow(vMin);form->addRow(vMax);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    const auto face=selectedSubshape(document_,faces.first().partId,faces.first().geometryToken,faces.first().faceIndex,TopAbs_FACE);
    if(face.IsNull()){reportError("Extend surface","The selected face reference is stale. Select it again.");return;}
    TopoDS_Face extended;QString error;
    if(!cad::extendSurfaceFace(TopoDS::Face(face),distance->value(),uMin->isChecked(),uMax->isChecked(),vMin->isChecked(),vMax->isChecked(),&extended,&error)||
       !document_.addSurfaceBody("Extended Surface",extended,&error)){
        reportError("Extend surface",error);return;
    }
    refresh(true);statusBar()->showMessage("Created an extended surface body.",8000);
}

void MainWindow::thickenSelectedSurface() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){QMessageBox::information(this,"Thicken surface","Select exactly one surface body in the browser.");return;}
    bool accepted=false;
    const double thickness=QInputDialog::getDouble(this,"Thicken surface body",
        "Signed wall thickness (mm):",2.0,-1e6,1e6,4,&accepted);
    if(!accepted)return;
    QString error;
    if(!document_.thickenSurfaceBody(ids.first(),thickness,&error)){
        reportError("Thicken surface body",error);return;
    }
    refresh(true);statusBar()->showMessage("Created a solid from the selected surface body; the source surface is preserved.",9000);
}

void MainWindow::stitchSelectedSurfaces() {
    const QStringList ids=selectedIds();
    if(ids.isEmpty()){QMessageBox::information(this,"Stitch surfaces","Select one or more surface bodies in the browser.");return;}
    bool accepted=false;
    const double tolerance=QInputDialog::getDouble(this,"Stitch surface bodies",
        "Sewing tolerance (mm):",0.01,1e-7,1.0,6,&accepted);
    if(!accepted)return;
    QString error;
    if(!document_.stitchSurfaceBodies(ids,tolerance,&error)){
        reportError("Stitch surface bodies",error);return;
    }
    refresh(true);statusBar()->showMessage("Created a solid from the selected closed surfaces; source surfaces are preserved.",9000);
}

void MainWindow::sewSelectedSurfaces() {
    const QStringList ids=selectedIds();
    if(ids.size()<2){QMessageBox::information(this,"Sew surfaces","Select at least two adjacent surface bodies in the browser.");return;}
    bool accepted=false;
    const double tolerance=QInputDialog::getDouble(this,"Sew surfaces into a shell",
        "Maximum edge sewing tolerance (mm):",0.01,1e-7,1.0,6,&accepted);
    if(!accepted)return;
    QString error;
    if(!document_.sewSurfaceBodies(ids,tolerance,&error)){
        reportError("Sew surfaces",error);return;
    }
    refresh(true);statusBar()->showMessage("Created one surface shell; all source surface bodies are preserved.",9000);
}

void MainWindow::trimSelectedSurface() {
    const QStringList ids=selectedIds();
    if(ids.size()!=2){QMessageBox::information(this,"Trim surface","Select one surface body and one solid body in the browser.");return;}
    const cad::Part* first=document_.find(ids[0]);
    const cad::Part* second=document_.find(ids[1]);
    const cad::Part* surface=first&&first->isSurfaceBody?first:second&&second->isSurfaceBody?second:nullptr;
    const cad::Part* solid=first&&first->isSurfaceBody?second:second&&second->isSurfaceBody?first:nullptr;
    if(!surface||!solid||solid->isSurfaceBody){
        QMessageBox::information(this,"Trim surface","The selection must contain exactly one persisted surface body and one solid body.");return;
    }
    QMessageBox choice(QMessageBox::Question,"Trim surface",
        "Choose which side of the solid boundary to keep. The source surface and tool solid will be preserved.",
        QMessageBox::NoButton,this);
    QAbstractButton* inside=choice.addButton("Keep inside solid",QMessageBox::AcceptRole);
    QAbstractButton* outside=choice.addButton("Keep outside solid",QMessageBox::DestructiveRole);
    choice.addButton(QMessageBox::Cancel);
    choice.exec();
    const bool keepInside=choice.clickedButton()==inside;
    if(!keepInside&&choice.clickedButton()!=outside)return;
    QString error;
    if(!document_.trimSurfaceBody(surface->id,solid->id,keepInside,&error)){
        reportError("Trim surface",error);return;
    }
    refresh(true);statusBar()->showMessage("Trimmed surface body created; source surface and tool solid are preserved.",9000);
}

void MainWindow::splitSelectedSurface() {
    const QStringList ids=selectedIds();
    if(ids.size()!=2){QMessageBox::information(this,"Split surface","Select one persisted face or shell surface body and one solid body in the browser.");return;}
    const cad::Part* first=document_.find(ids[0]);
    const cad::Part* second=document_.find(ids[1]);
    const cad::Part* surface=first&&first->isSurfaceBody?first:second&&second->isSurfaceBody?second:nullptr;
    const cad::Part* solid=first&&first->isSurfaceBody?second:second&&second->isSurfaceBody?first:nullptr;
    if(!surface||!solid||solid->isSurfaceBody){
        QMessageBox::information(this,"Split surface","The selection must contain exactly one surface body and one solid body.");return;
    }
    QString error;
    if(!document_.splitSurfaceBodyBySolid(surface->id,solid->id,&error)){
        reportError("Split surface",error);return;
    }
    refresh(true);statusBar()->showMessage("Surface fragments created; the source surface and splitting solid are preserved.",9000);
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
void MainWindow::orientSelectedComponentOccurrence() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Orient component","Select one free component occurrence.");return;}
    const cad::Part* part=document_.find(ids.first());
    if(!part||part->componentDefinitionId.isEmpty()){
        reportError("Orient component","Make the selected body a component first.");return;
    }
    QDialog dialog(this);dialog.setWindowTitle("Orient free component occurrence");auto* form=new QFormLayout(&dialog);
    auto angle=[&](const QString& label,double value){auto* box=new QDoubleSpinBox(&dialog);
        box->setRange(-36000,36000);box->setDecimals(3);box->setValue(value);box->setSuffix(" degrees");
        form->addRow(label,box);return box;};
    auto* roll=angle("Roll about local X",part->placementRollDegrees);
    auto* pitch=angle("Pitch about local Y",part->placementPitchDegrees);
    auto* yaw=angle("Yaw about world Z",part->placementYawDegrees);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.setFreeOccurrenceOrientation(ids.first(),roll->value(),pitch->value(),yaw->value(),&error)){
        reportError("Orient component",error);return;
    }
    refresh();
}
void MainWindow::synchronizeSelectedComponentDefinition() {
    const QStringList ids=selectedIds();if(ids.size()!=1){reportError("Synchronize component definition","Select one reusable component occurrence as the definition source.");return;}
    QString error;if(!document_.synchronizeComponentOccurrences(ids.first(),&error)){reportError("Synchronize component definition",error);return;}refresh();
}
void MainWindow::linearPatternSelectedComponentOccurrence() {
    if(!requireBodyMode("Linear component pattern"))return;
    const QStringList ids=selectedIds();if(ids.size()!=1){reportError("Linear component pattern","Select one free component occurrence.");return;}
    bool accepted=false;
    const int count=QInputDialog::getInt(this,"Linear component pattern","Total occurrences (including the selected source):",3,2,100,1,&accepted);
    if(!accepted)return;
    const auto spacing=valuesDialog(this,"Linear component pattern spacing",{"X spacing (mm)","Y spacing (mm)","Z spacing (mm)"},{25,0,0},-1e6,1e6);
    if(!spacing)return;
    QString error;
    if(!document_.linearPatternComponentOccurrences(ids.first(),count,(*spacing)[0],(*spacing)[1],(*spacing)[2],&error)){
        reportError("Linear component pattern",error);return;
    }
    refresh(true);statusBar()->showMessage(QString("Created %1 component occurrences in one undoable linear pattern.").arg(count-1),6000);
}
void MainWindow::circularPatternSelectedComponentOccurrence() {
    if(!requireBodyMode("Circular component pattern"))return;
    const QStringList ids=selectedIds();if(ids.size()!=1){reportError("Circular component pattern","Select one free component occurrence.");return;}
    const cad::Part* source=document_.find(ids.first());if(!source)return;
    bool accepted=false;
    const int count=QInputDialog::getInt(this,"Circular component pattern","Total occurrences (including the selected source):",4,2,100,1,&accepted);
    if(!accepted)return;
    const auto center=valuesDialog(this,"Circular component pattern center",{"Center X (mm)","Center Y (mm)","Center Z (mm)"},
        {0.0,0.0,source->placementZ},-1e6,1e6);
    if(!center)return;
    const double step=QInputDialog::getDouble(this,"Circular component pattern","Angle step about world Z (degrees):",90.0,-360.0,360.0,3,&accepted);
    if(!accepted)return;
    QString error;
    if(!document_.circularPatternComponentOccurrences(ids.first(),count,(*center)[0],(*center)[1],(*center)[2],step,&error)){
        reportError("Circular component pattern",error);return;
    }
    refresh(true);statusBar()->showMessage(QString("Created %1 component occurrences in one undoable circular pattern.").arg(count-1),6000);
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
    for(const QString& entry:text.split(',',Qt::KeepEmptyParts)){
        const QStringList pair=entry.trimmed().split(':');bool timeOkay=false,valueOkay=false;
        if(pair.size()!=2||pair[0].trimmed().isEmpty()||pair[1].trimmed().isEmpty()){
            reportError("Edit joint motion track","Use nonempty entries in time:value format, for example 0:0, 1:45.");return;
        }
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
void MainWindow::scanSelectedMotionInterference() {
    const QStringList ids=selectedIds();
    if(ids.size()!=2){reportError("Motion interference","Select exactly two bodies in the model browser.");return;}
    QDialog dialog(this);dialog.setWindowTitle("Scan motion-track interference");auto* form=new QFormLayout(&dialog);
    auto* start=new QDoubleSpinBox(&dialog);start->setRange(0,3599.999);start->setDecimals(3);start->setSuffix(" s");
    auto* end=new QDoubleSpinBox(&dialog);end->setRange(0.001,3600);end->setDecimals(3);end->setSuffix(" s");end->setValue(4);
    auto* count=new QSpinBox(&dialog);count->setRange(2,51);count->setValue(17);
    auto* adaptive=new QCheckBox("Adaptively refine near-clearance and long intervals",&dialog);
    adaptive->setChecked(true);
    count->setRange(2,256);count->setValue(128);
    auto* threshold=new QDoubleSpinBox(&dialog);threshold->setRange(0,1000000);
    threshold->setDecimals(3);threshold->setValue(1.0);threshold->setSuffix(" mm");
    auto* coarse=new QDoubleSpinBox(&dialog);coarse->setRange(0.001,3600);
    coarse->setDecimals(4);coarse->setValue(0.25);coarse->setSuffix(" s");
    auto* fine=new QDoubleSpinBox(&dialog);fine->setRange(0.0001,3600);
    fine->setDecimals(4);fine->setValue(0.0156);fine->setSuffix(" s");
    form->addRow("Start time:",start);form->addRow("End time:",end);
    form->addRow(adaptive);form->addRow("Sample budget:",count);
    form->addRow("Refine below clearance:",threshold);
    form->addRow("Normal maximum step:",coarse);
    form->addRow("Near-clearance minimum step:",fine);
    auto* speedBoundEnabled=new QCheckBox("Use an independently justified speed bound",&dialog);
    auto* speedBound=new QDoubleSpinBox(&dialog);
    speedBound->setRange(0,1000000000);speedBound->setDecimals(3);
    speedBound->setValue(0.0);speedBound->setSuffix(" mm/s");
    speedBound->setEnabled(false);
    speedBound->setToolTip("A conservative bound on the sum of the largest surface-point speeds of both solids. "
                           "Must be established independently from motion kinematics; sampled gaps do not prove it.");
    form->addRow(speedBoundEnabled);form->addRow("Maximum relative surface speed:",speedBound);
    connect(speedBoundEnabled,&QCheckBox::toggled,speedBound,&QDoubleSpinBox::setEnabled);
    connect(adaptive,&QCheckBox::toggled,&dialog,[=](bool checked){
        count->setMaximum(checked?256:51);
        threshold->setEnabled(checked);coarse->setEnabled(checked);fine->setEnabled(checked);
        speedBoundEnabled->setEnabled(checked);speedBound->setEnabled(checked&&speedBoundEnabled->isChecked());
    });
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QVector<cad::MotionInterferenceSample> samples;QString error;
    QVector<cad::MotionClearanceInterval> intervalAudit;
    bool unresolved=false, boundSatisfied=false;
    double conservativeLowerBoundMm=0.0;
    const bool scanned=adaptive->isChecked()?
        document_.scanAssemblyMotionAdaptive(ids[0],ids[1],start->value(),end->value(),
            count->value(),threshold->value(),coarse->value(),fine->value(),
            &samples,&unresolved,&error,
            speedBoundEnabled->isChecked()?speedBound->value():-1.0,
            &boundSatisfied,&conservativeLowerBoundMm,&intervalAudit):
        document_.scanAssemblyMotionInterference(ids[0],ids[1],start->value(),end->value(),
            count->value(),&samples,&error);
    if(!scanned){reportError("Motion interference",error);return;}
    int collisions=0;double maxOverlap=0.0,firstCollision=0.0,lastCollision=0.0;
    double minimumClearance=std::numeric_limits<double>::infinity(),closestTime=0.0;
    for(const auto& sample:samples)if(sample.clearanceMm<minimumClearance){
        minimumClearance=sample.clearanceMm;closestTime=sample.timeSeconds;
    }
    for(const auto& sample:samples)if(sample.interfering){
        if(collisions==0)firstCollision=sample.timeSeconds;
        lastCollision=sample.timeSeconds;maxOverlap=std::max(maxOverlap,sample.overlapVolumeMm3);++collisions;
    }
    const QString message=collisions==0?
        QString("No overlap found in %1 sampled positions.\nMinimum sampled clearance: %2 mm at %3 s.\n\nSome intervals may not have been evaluated.")
            .arg(samples.size()).arg(minimumClearance,0,'f',3).arg(closestTime,0,'f',3):
        QString("Overlap found at %1 of %2 sampled positions.\nFirst sampled overlap: %3 s\nLast sampled overlap: %4 s\nMaximum sampled shared volume: %5 mm³\nMinimum sampled clearance: %6 mm\n\nSome intervals may not have been evaluated.")
            .arg(collisions).arg(samples.size()).arg(firstCollision,0,'f',3).arg(lastCollision,0,'f',3).arg(maxOverlap,0,'f',3)
            .arg(minimumClearance,0,'f',3);
    const QString refinement=adaptive->isChecked()?
        (unresolved?QStringLiteral("\nRefinement budget reached: unresolved intervals remain."):
                    QStringLiteral("\nAll requested interval refinements were evaluated.")):
        QStringLiteral("\nFixed-interval mode used.");
    const QString speedEvidence=adaptive->isChecked()&&speedBoundEnabled->isChecked()?
        (boundSatisfied?
            QStringLiteral("\nConditional conservative minimum clearance: at least %1 mm over the entire time window, IF the separately justified surface-speed bound is correct. This exceeds the requested safety margin.")
                .arg(conservativeLowerBoundMm,0,'g',8):
            QStringLiteral("\nConditional interval clearance proof NOT established (lower bound %1 mm): some intervals may approach the safety margin. This does not prove collision freedom.")
                .arg(conservativeLowerBoundMm,0,'g',8)):
        QStringLiteral("\nNo independently established relative surface-speed bound supplied: all intervals remain uncertified.");
    int unprovenIntervals=0,observedOverlapIntervals=0;
    QStringList firstUnproven;
    for(const auto& interval:intervalAudit){
        if(interval.sampledOverlap)++observedOverlapIntervals;
        if(!interval.provenAboveMargin){
            ++unprovenIntervals;
            if(firstUnproven.size()<3)firstUnproven.append(
                QStringLiteral("%1–%2 s").arg(interval.startSeconds,0,'g',8)
                                               .arg(interval.endSeconds,0,'g',8));
        }
    }
    const QString auditSummary=adaptive->isChecked()?
        QStringLiteral("\nInterval audit: %1 of %2 time intervals remain unproven; "
                       "%3 have sampled overlap.%4")
            .arg(unprovenIntervals).arg(intervalAudit.size()).arg(observedOverlapIntervals)
            .arg(firstUnproven.isEmpty()?QString():
                QStringLiteral("\nFirst unproven ranges: ")+firstUnproven.join(QStringLiteral(", "))):
        QString();
    const QString warning=QStringLiteral("\n\nThis is a SAMPLE-BASED scan, not certified continuous collision clearance.\n")+
        QStringLiteral("Tiny collisions can still occur between checks. Do not use this as a safety approval.");
    QMessageBox report(this);report.setIcon(QMessageBox::Information);
    report.setWindowTitle("Motion interference scan");report.setText(message+refinement+speedEvidence+auditSummary+warning);
    auto* exportCsv=report.addButton("Export samples CSV...",QMessageBox::ActionRole);
    auto* exportAudit=adaptive->isChecked()?
        report.addButton("Export interval audit CSV...",QMessageBox::ActionRole):nullptr;
    report.addButton(QMessageBox::Close);
    report.exec();
    if(report.clickedButton()!=exportCsv&&report.clickedButton()!=exportAudit)return;
    const QString folder=document_.path().isEmpty()?QDir::homePath():QFileInfo(document_.path()).absolutePath();
    if(exportAudit&&report.clickedButton()==exportAudit){
        const QString suggested=QDir(folder).filePath(QStringLiteral("assembly_motion_interval_audit.csv"));
        const QString path=QFileDialog::getSaveFileName(this,"Export clearance interval audit",suggested,"CSV files (*.csv)");
        if(!path.isEmpty()&&!document_.exportMotionClearanceIntervalsCsv(ids[0],ids[1],intervalAudit,path,&error))
            reportError("Export clearance interval audit",error);
        return;
    }
    const QString suggested=QDir(folder).filePath(QStringLiteral("assembly_motion_samples.csv"));
    const QString path=QFileDialog::getSaveFileName(this,"Export motion samples",suggested,"CSV files (*.csv)");
    if(path.isEmpty())return;
    if(!document_.exportMotionSamplesCsv(ids[0],ids[1],samples,path,&error))
        reportError("Export motion samples",error);
}
void MainWindow::scanAllAssemblyMotionInterference() {
    QDialog settings(this);settings.setWindowTitle("Scan all assembly motion contacts");
    auto* form=new QFormLayout(&settings);
    auto* start=new QDoubleSpinBox(&settings);start->setRange(0,3599.999);start->setDecimals(3);start->setSuffix(" s");
    auto* end=new QDoubleSpinBox(&settings);end->setRange(0.001,3600);end->setDecimals(3);end->setSuffix(" s");end->setValue(4);
    auto* count=new QSpinBox(&settings);count->setRange(2,51);count->setValue(17);
    form->addRow("Start time:",start);form->addRow("End time:",end);form->addRow("Sample count:",count);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&settings);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&settings,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&settings,&QDialog::reject);
    if(settings.exec()!=QDialog::Accepted)return;
    QVector<cad::MotionInterferenceContact> contacts;QString error;
    if(!document_.scanAssemblyMotionAllPairs(start->value(),end->value(),count->value(),&contacts,&error)){
        reportError("All-pairs motion interference",error);return;
    }
    struct PairSummary {QString firstId,secondId;int count=0;double firstTime=0,lastTime=0,maximumVolume=0;};
    QMap<QString,PairSummary> pairs;
    for(const auto& contact:contacts){
        const QString key=contact.firstBodyId+QLatin1Char('|')+contact.secondBodyId;
        PairSummary& pair=pairs[key];
        if(pair.count==0){pair.firstId=contact.firstBodyId;pair.secondId=contact.secondBodyId;
            pair.firstTime=contact.timeSeconds;}
        ++pair.count;pair.lastTime=contact.timeSeconds;
        pair.maximumVolume=std::max(pair.maximumVolume,contact.overlapVolumeMm3);
    }
    QStringList lines;
    lines.append(QStringLiteral("Requested %1 uniform positions and included saved keyframe times; found %2 overlapping body pairs.")
        .arg(count->value()).arg(pairs.size()));
    if(pairs.isEmpty())lines.append(QStringLiteral("No overlap was found at the sampled positions."));
    for(const PairSummary& pair:pairs){
        const cad::Part* first=document_.find(pair.firstId);
        const cad::Part* second=document_.find(pair.secondId);
        lines.append(QStringLiteral("\n%1 ↔ %2\n  Overlap samples: %3\n  First/last: %4 / %5 s\n  Maximum shared volume: %6 mm³")
            .arg(first?first->name:pair.firstId,second?second->name:pair.secondId)
            .arg(pair.count).arg(pair.firstTime,0,'f',3).arg(pair.lastTime,0,'f',3)
            .arg(pair.maximumVolume,0,'f',3));
    }
    lines.append(QStringLiteral("\nThis is a discrete scan; collisions between samples may be missed."));
    QDialog report(this);report.setWindowTitle("Assembly motion contacts");report.resize(680,460);
    auto* layout=new QVBoxLayout(&report);
    auto* text=new QPlainTextEdit(&report);text->setReadOnly(true);text->setPlainText(lines.join('\n'));
    layout->addWidget(text);
    auto* close=new QDialogButtonBox(QDialogButtonBox::Close,&report);
    auto* exportCsv=close->addButton("Export contacts CSV...",QDialogButtonBox::ActionRole);
    layout->addWidget(close);
    connect(close,&QDialogButtonBox::rejected,&report,&QDialog::reject);
    connect(exportCsv,&QPushButton::clicked,&report,[this,&report,contacts]{
        const QString folder=document_.path().isEmpty()?QDir::homePath():QFileInfo(document_.path()).absolutePath();
        const QString suggested=QDir(folder).filePath(QStringLiteral("assembly_motion_contacts.csv"));
        const QString path=QFileDialog::getSaveFileName(&report,"Export motion contacts",suggested,"CSV files (*.csv)");
        if(path.isEmpty())return;
        QString error;
        if(!document_.exportMotionContactsCsv(contacts,path,&error))reportError("Export motion contacts",error);
    });
    report.exec();
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
void MainWindow::batchRenameSelectedBodies() {
    if(!requireBodyMode("Batch rename"))return;
    const QStringList selected=selectedIds();
    if(selected.size()<2){reportError("Batch rename","Select two or more bodies.");return;}
    // Build the preview from immutable identities: renaming never changes shape,
    // BOM metadata, assembly placements, selection IDs or feature references.
    QVector<QPair<QString,QString>> originals;
    for(const auto& part:document_.parts())
        if(selected.contains(part.id))originals.append({part.id,part.name});
    if(originals.size()!=selected.size()){
        reportError("Batch rename","Selection changed; one or more bodies no longer exist.");return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle("Batch rename bodies · one undo step");
    dialog.resize(780,540);
    auto* layout=new QVBoxLayout(&dialog);
    auto* help=new QLabel("Preview names before applying. All final names must be unique across the project. "
        "The model is unchanged until you apply the complete batch.",&dialog);
    help->setWordWrap(true);layout->addWidget(help);
    auto* form=new QFormLayout;
    auto* findText=new QLineEdit(&dialog);
    auto* replacement=new QLineEdit(&dialog);
    auto* prefix=new QLineEdit(&dialog);
    auto* suffix=new QLineEdit(&dialog);
    findText->setPlaceholderText("Optional text to replace in selected names");
    form->addRow("Find:",findText);form->addRow("Replace with:",replacement);
    form->addRow("Prefix:",prefix);form->addRow("Suffix:",suffix);
    auto* number=new QCheckBox("Add numbered suffix (001, 002, ...)",&dialog);
    form->addRow(number);
    auto* start=new QSpinBox(&dialog);start->setRange(1,999999);start->setValue(1);
    start->setEnabled(false);form->addRow("Start at:",start);
    layout->addLayout(form);
    auto* table=new QTableWidget(&dialog);
    table->setObjectName("BatchRenamePreview");
    table->setColumnCount(3);table->setRowCount(originals.size());
    table->setHorizontalHeaderLabels({"Current body name","Proposed name","Status"});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setColumnWidth(0,220);table->setColumnWidth(1,270);
    layout->addWidget(table,1);
    auto* status=new QLabel(&dialog);status->setWordWrap(true);layout->addWidget(status);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Cancel|QDialogButtonBox::Apply,&dialog);
    layout->addWidget(buttons);
    auto* apply=buttons->button(QDialogButtonBox::Apply);
    QMap<QString,QString> proposed;
    auto recalculate=[&] {
        proposed.clear();QSet<QString> planned;
        for(const auto& part:document_.parts())if(!selected.contains(part.id))
            planned.insert(part.name.toCaseFolded());
        bool okay=true;int changed=0;
        for(int i=0;i<originals.size();++i){
            const auto& entry=originals[i];
            QString name=entry.second;
            if(!findText->text().isEmpty())name.replace(findText->text(),replacement->text(),Qt::CaseInsensitive);
            name=(prefix->text()+name+suffix->text()).trimmed();
            if(number->isChecked())name+=QStringLiteral("-%1").arg(start->value()+i,3,10,QChar('0'));
            const bool valid=!name.isEmpty()&&name.size()<=120&&
                !name.contains('\n')&&!name.contains('\r')&&!planned.contains(name.toCaseFolded());
            if(valid)planned.insert(name.toCaseFolded());
            else okay=false;
            changed+=name!=entry.second;
            proposed.insert(entry.first,name);
            table->setItem(i,0,new QTableWidgetItem(entry.second));
            table->setItem(i,1,new QTableWidgetItem(name));
            auto* state=new QTableWidgetItem(valid?"OK":"Collision / invalid");
            if(!valid)state->setForeground(QColor(188,58,47));
            table->setItem(i,2,state);
        }
        status->setText(okay?QStringLiteral("%1 bodies · %2 name changes · one undo checkpoint")
                           .arg(originals.size()).arg(changed):
            QStringLiteral("Resolve duplicate, blank or overlong names before applying."));
        apply->setEnabled(okay&&changed>0);
    };
    connect(findText,&QLineEdit::textChanged,&dialog,recalculate);
    connect(replacement,&QLineEdit::textChanged,&dialog,recalculate);
    connect(prefix,&QLineEdit::textChanged,&dialog,recalculate);
    connect(suffix,&QLineEdit::textChanged,&dialog,recalculate);
    connect(number,&QCheckBox::toggled,&dialog,[&](bool enabled){start->setEnabled(enabled);recalculate();});
    connect(start,qOverload<int>(&QSpinBox::valueChanged),&dialog,[&](int){recalculate();});
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(apply,&QPushButton::clicked,&dialog,[&] {
        QString issue;
        if(!document_.renameBatch(proposed,&issue)){
            QMessageBox::warning(&dialog,"Batch rename refused",issue);recalculate();return;
        }
        dialog.accept();
        refresh();
        statusBar()->showMessage(QStringLiteral("Renamed %1 bodies atomically. Undo reverses the whole batch.")
                                   .arg(proposed.size()),6000);
    });
    recalculate();dialog.exec();
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
    struct MaterialChoice { QString name; double densityKgPerM3; QString label; };
    QVector<MaterialChoice> library={
        {"Unassigned",0.0,"Unassigned"},
        {"Aluminum 6061",2700.0,"Aluminum 6061 (2700 kg/m³)"},
        {"Carbon steel",7850.0,"Carbon steel (7850 kg/m³)"},
        {"Stainless steel 304",8000.0,"Stainless steel 304 (8000 kg/m³)"},
        {"Brass",8500.0,"Brass (8500 kg/m³)"},
        {"Copper",8960.0,"Copper (8960 kg/m³)"},
        {"ABS plastic",1040.0,"ABS plastic (1040 kg/m³)"},
        {"Nylon 6",1150.0,"Nylon 6 (1150 kg/m³)"}
    };
    for(const cad::Part& body:document_.parts()){
        if(body.materialName.isEmpty())continue;
        const bool known=std::any_of(library.cbegin(),library.cend(),[&body](const MaterialChoice& choice){
            return choice.name==body.materialName&&choice.densityKgPerM3==body.materialDensityKgPerM3;
        });
        if(!known){
            const QString density=QString::number(body.materialDensityKgPerM3,'g',8);
            library.append({body.materialName,body.materialDensityKgPerM3,
                QStringLiteral("%1 (%2 kg/m³)").arg(body.materialName,density)});
        }
    }
    library.append({"Custom...",-1.0,"Custom..."});
    QStringList labels;int current=0;
    for(int i=0;i<library.size();++i){
        labels.append(library[i].label);
        if(library[i].name==part->materialName&&library[i].densityKgPerM3==part->materialDensityKgPerM3)current=i;
    }
    bool accepted=false;
    const QString selected=QInputDialog::getItem(this,"Assign material","Material:",labels,current,false,&accepted);
    if(!accepted)return;
    QString materialName;
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
        const auto preset=std::find_if(library.cbegin(),library.cend(),[&selected](const MaterialChoice& entry){return entry.label==selected;});
        if(preset==library.cend()||preset->densityKgPerM3<0.0)return;
        materialName=preset->name;
        density=preset->densityKgPerM3;
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
                node->setData(0,Qt::UserRole,prefix);
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
    // Relocates an entire group atomically, not just a selected BOM row.
    // Existing body shape, geometry token, mate definition and placement are retained.
    auto* groupControls=new QHBoxLayout();
    auto* relocateGroup=new QPushButton("Rename / move selected assembly group...",&dialog);
    relocateGroup->setToolTip("Select an assembly group in the structure tab. All nested groups and their occurrences move together.");
    groupControls->addWidget(relocateGroup);
    auto* auditGroup=new QPushButton("Audit selected group clearance...",&dialog);
    auditGroup->setToolTip("Read-only exact pairwise solid clearance and interference for 2-32 bodies in this group and all descendants.");
    groupControls->addWidget(auditGroup);
    groupControls->addStretch(1);
    connect(auditGroup,&QPushButton::clicked,&dialog,[&] {
        tabs->setCurrentWidget(assemblyTree);
        const auto* selected=assemblyTree->currentItem();
        const QString groupPath=selected?selected->data(0,Qt::UserRole).toString():QString();
        if(groupPath.isEmpty()) {
            reportError("Assembly clearance","Select an assembly group, not a part.");return;
        }
        bool accepted=false;
        const double clearance=QInputDialog::getDouble(&dialog,"Group fit and collision audit",
            "Required minimum clearance (mm):",1.0,0.0,1000000.0,3,&accepted);
        if(!accepted)return;
        QVector<cad::AssemblyFitPair> pairs;QString issue;
        if(!cad::inspectAssemblyGroupFit(document_,groupPath,clearance,&pairs,&issue)) {
            reportError("Assembly clearance",issue);return;
        }
        QDialog inspection(&dialog);
        inspection.setWindowTitle(QStringLiteral("Clearance audit: %1").arg(groupPath));
        inspection.resize(850,560);
        auto* inspectionLayout=new QVBoxLayout(&inspection);
        auto* display=new QPlainTextEdit(&inspection);
        display->setReadOnly(true);
        display->setPlainText(cad::assemblyFitAuditText(pairs,clearance));
        inspectionLayout->addWidget(display);
        auto* done=new QDialogButtonBox(QDialogButtonBox::Close,&inspection);
        inspectionLayout->addWidget(done);
        connect(done,&QDialogButtonBox::rejected,&inspection,&QDialog::accept);
        inspection.exec();
    });
    layout->addLayout(groupControls);
    bool hierarchyChanged=false;
    connect(relocateGroup,&QPushButton::clicked,&dialog,[&] {
        tabs->setCurrentWidget(assemblyTree);
        const auto* selected=assemblyTree->currentItem();
        const QString oldPath=selected?selected->data(0,Qt::UserRole).toString():QString();
        if(oldPath.isEmpty()) {
            reportError("Assembly hierarchy","Select a group rather than an individual part, then choose Rename / move.");
            return;
        }
        bool accepted=false;
        const QString newPath=QInputDialog::getText(&dialog,"Rename or move assembly group",
            QStringLiteral("New full group path for '%1' (e.g. Machine/Drive/NewGroup):").arg(oldPath),
            QLineEdit::Normal,oldPath,&accepted);
        if(!accepted || newPath==oldPath)return;
        QString issue;
        if(!document_.moveAssemblySubtree(oldPath,newPath,&issue)) {
            reportError("Assembly hierarchy",issue);
            return;
        }
        hierarchyChanged=true;
        dialog.accept();
    });
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    dialog.exec();
    if(hierarchyChanged)refresh();
}
void MainWindow::showDocumentAudit() {
    // Read-only inspection; no geometry, linked drawing token, or undo history changes.
    const cad::DocumentAuditReport report=cad::runDocumentAudit(document_);
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Project health audit — %1 errors, %2 warnings")
        .arg(report.errorCount).arg(report.warningCount));
    dialog.resize(900,600);
    auto* layout=new QVBoxLayout(&dialog);
    auto* intro=new QLabel(QStringLiteral("%1 bodies  |  %2 errors  |  %3 warnings  |  %4 information findings. "
        "Double-check every issue before exporting manufacturing deliverables. This is not engineering certification.")
        .arg(report.bodyCount).arg(report.errorCount).arg(report.warningCount).arg(report.informationCount),&dialog);
    intro->setWordWrap(true);layout->addWidget(intro);
    auto* filter=new QLineEdit(&dialog);
    filter->setPlaceholderText("Filter by subsystem, issue, body ID or suggested action...");
    filter->setClearButtonEnabled(true);layout->addWidget(filter);
    auto* severityFilter=new QComboBox(&dialog);
    severityFilter->setObjectName("AuditSeverityFilter");
    severityFilter->addItems({"All findings","Errors only","Errors and warnings"});
    layout->addWidget(severityFilter);
    auto* table=new QTableWidget(&dialog);
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels({"Severity","Area","Issue","Body ID","Description and next step"});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->setWordWrap(true);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setColumnWidth(0,86);table->setColumnWidth(1,108);
    table->setColumnWidth(2,200);table->setColumnWidth(3,130);
    layout->addWidget(table,1);
    auto refreshRows=[&] {
        table->setRowCount(0);
        const QString query=filter->text().trimmed();
        for(const cad::AuditFinding& finding:report.findings) {
            const QString severity=finding.severity==cad::AuditSeverity::Error?"ERROR":
                (finding.severity==cad::AuditSeverity::Warning?"WARNING":"INFO");
            if(severityFilter->currentIndex()==1&&severity!="ERROR")continue;
            if(severityFilter->currentIndex()==2&&severity=="INFO")continue;
            const QString detail=finding.message+(finding.suggestedAction.isEmpty()?QString():
                QStringLiteral("\nNext: ")+finding.suggestedAction);
            const QString combined=QStringList{severity,finding.area,finding.code,finding.bodyId,detail}.join(' ');
            if(!query.isEmpty()&&!combined.contains(query,Qt::CaseInsensitive))continue;
            const int row=table->rowCount();table->insertRow(row);
            const QStringList columns={severity,finding.area,finding.code,finding.bodyId,detail};
            for(int col=0;col<columns.size();++col)table->setItem(row,col,new QTableWidgetItem(columns[col]));
        }
        table->resizeRowsToContents();
    };
    connect(filter,&QLineEdit::textChanged,&dialog,refreshRows);
    connect(severityFilter,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,
        [&](int){refreshRows();});
    refreshRows();
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);
    auto* saveCsv=buttons->addButton("Export findings CSV...",QDialogButtonBox::ActionRole);
    auto* saveText=buttons->addButton("Export report TXT...",QDialogButtonBox::ActionRole);
    connect(saveCsv,&QPushButton::clicked,&dialog,[&] {
        const QString path=QFileDialog::getSaveFileName(&dialog,"Export project audit CSV",{},"CSV files (*.csv)");
        if(path.isEmpty())return;
        QSaveFile file(path);
        const QByteArray content=report.toCsv();
        if(!file.open(QIODevice::WriteOnly)||file.write(content)!=content.size()||!file.commit())
            QMessageBox::warning(&dialog,"Export project audit","Could not write the CSV report.");
    });
    connect(saveText,&QPushButton::clicked,&dialog,[&] {
        const QString path=QFileDialog::getSaveFileName(&dialog,"Export project audit text",{},"Text files (*.txt)");
        if(path.isEmpty())return;
        QSaveFile file(path);
        const QByteArray content=report.toPlainText().toUtf8();
        if(!file.open(QIODevice::WriteOnly)||file.write(content)!=content.size()||!file.commit())
            QMessageBox::warning(&dialog,"Export project audit","Could not write the text report.");
    });
    auto* jump=buttons->addButton("Select affected body",QDialogButtonBox::ActionRole);
    jump->setObjectName("AuditSelectAffectedBody");
    auto jumpToBody=[&] {
        const int row=table->currentRow();
        if(row<0||!table->item(row,3))return;
        const QString id=table->item(row,3)->text();
        if(!document_.find(id)){
            QMessageBox::information(&dialog,"No linked body", "This finding has no existing model body to select.");
            return;
        }
        {
            const QSignalBlocker blocker(tree_);
            tree_->clearSelection();
            for(int i=0;i<tree_->topLevelItemCount();++i){
                auto* candidate=tree_->topLevelItem(i);
                if(candidate->data(0,Qt::UserRole).toString()==id){
                    candidate->setSelected(true);
                    if(candidate->isHidden()&&browserFilter_)browserFilter_->clear();
                    tree_->scrollToItem(candidate);break;
                }
            }
        }
        viewport_->selectIds({id});
        updateInspector({id});updateContextActions();updateFeatureTimeline();
        statusBar()->showMessage("Selected the body referenced by the audit finding.",6000);
        dialog.accept();
    };
    connect(jump,&QPushButton::clicked,&dialog,jumpToBody);
    connect(table,&QTableWidget::cellDoubleClicked,&dialog,[&](int,int){jumpToBody();});
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    layout->addWidget(buttons);
    filter->setFocus();
    dialog.exec();
}

void MainWindow::openCommandSearch() {
    openCommandSearchWithQuery(QString());
}

void MainWindow::openCommandSearchWithQuery(const QString& initialQuery) {
    // One canonical command catalog: the live QAction objects in real menus.
    // Search never bypasses action availability and never edits the document.
    struct Entry { QString label; QString location; QString key; QAction* action=nullptr; };
    QVector<Entry> entries;
    QSet<QAction*> seen;
    std::function<void(QMenu*,QString)> traverse=[&](QMenu* menu,QString path) {
        if(!menu)return;
        for(QAction* action:menu->actions()) {
            if(action->isSeparator())continue;
            if(QMenu* child=action->menu()) {
                QString name=action->text();name.remove('&');
                traverse(child,path+" / "+name);
            } else if(!seen.contains(action)&&action->isVisible()&&!action->text().isEmpty()) {
                seen.insert(action);
                QString label=action->text();label.remove('&');
                entries.append({label,path,path+" | "+label,action});
            }
        }
    };
    for(QAction* action:menuBar()->actions()) {
        if(QMenu* menu=action->menu()) {
            QString name=action->text();name.remove('&');
            traverse(menu,name);
        }
    }
    QSettings settings("CADWorkbench","CADWorkbench");
    QStringList recent=settings.value("commandSearch/recent").toStringList();
    QStringList favorite=settings.value("commandSearch/favorites").toStringList();
    QDialog dialog(this);
    dialog.setObjectName("CadCommandPalette");
    dialog.setWindowTitle("Find CAD tools and components (Ctrl+K)");
    dialog.resize(800,575);
    auto* layout=new QVBoxLayout(&dialog);
    auto* search=new QLineEdit(&dialog);
    search->setObjectName("CadCommandPaletteSearch");
    search->setPlaceholderText("Try: bearing, fastener, M6 clearance, chamfer, sheet bend, BOM...");
    search->setClearButtonEnabled(true);
    layout->addWidget(search);
    auto* guidance=new QLabel("Search across all modeling commands and component families. "
        "Use common engineering terms or multiple keywords.",&dialog);
    guidance->setWordWrap(true);
    layout->addWidget(guidance);
    auto* list=new QListWidget(&dialog);
    list->setObjectName("CadCommandPaletteResults");
    list->setAlternatingRowColors(true);
    layout->addWidget(list,1);
    auto* count=new QLabel(&dialog);
    layout->addWidget(count);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);
    auto* star=buttons->addButton("Favorite / Unfavorite",QDialogButtonBox::ActionRole);
    auto* run=buttons->addButton("Run selected",QDialogButtonBox::AcceptRole);
    layout->addWidget(buttons);
    auto current=[&]() -> const Entry* {
        auto* item=list->currentItem();
        if(!item)return nullptr;
        const int index=item->data(Qt::UserRole).toInt();
        if(index<0||index>=entries.size())return nullptr;
        return &entries[index];
    };
    auto update=[&] {
        const QString query=search->text().trimmed();
        struct Match { int index; int rank; };
        QVector<Match> matches;
        for(int i=0;i<entries.size();++i) {
            const Entry& entry=entries[i];
            if(!entry.action||!entry.action->isVisible()||!entry.action->isEnabled())continue;
            const QString shortcut=entry.action->shortcut().toString(QKeySequence::NativeText);
            int rank=cad::commandsearch::score(entry.label.toStdString(),entry.location.toStdString(),
                                               query.toStdString());
            if(!rank&&!query.isEmpty())rank=cad::commandsearch::score(
                entry.label.toStdString(),entry.location.toStdString()+" / "+shortcut.toStdString(),
                query.toStdString());
            if(rank==0)continue;
            if(favorite.contains(entry.key))rank+=query.isEmpty()?600:35;
            const int recentIndex=recent.indexOf(entry.key);
            if(recentIndex>=0)rank+=query.isEmpty()?(200-recentIndex):10;
            matches.append({i,rank});
        }
        std::sort(matches.begin(),matches.end(),[&](const Match& a,const Match& b){
            if(a.rank!=b.rank)return a.rank>b.rank;
            return entries[a.index].label.compare(entries[b.index].label,Qt::CaseInsensitive)<0;
        });
        list->clear();
        for(int j=0;j<matches.size()&&j<100;++j) {
            const Entry& e=entries[matches[j].index];
            const QString shortcut=e.action->shortcut().toString(QKeySequence::NativeText);
            const QString label=(favorite.contains(e.key)?"★  ":QString())+e.label+
                "    ·    "+e.location+(shortcut.isEmpty()?QString():"    ["+shortcut+"]");
            auto* item=new QListWidgetItem(label,list);
            item->setData(Qt::UserRole,matches[j].index);
            item->setToolTip(e.location+" / "+e.label);
        }
        count->setText(QStringLiteral("%1 matching command(s)%2")
            .arg(matches.size()).arg(matches.size()>100?QStringLiteral(" · top 100 shown"):QString()));
        if(list->count())list->setCurrentRow(0);
        run->setEnabled(list->count()>0);
        star->setEnabled(list->count()>0);
    };
    auto execute=[&] {
        const Entry* e=current();
        if(!e||!e->action||!e->action->isEnabled()||!e->action->isVisible())return;
        QAction* action=e->action;
        recent.removeAll(e->key);
        recent.prepend(e->key);
        while(recent.size()>15)recent.removeLast();
        settings.setValue("commandSearch/recent",recent);
        dialog.accept();
        // The search dialog must be closed before commands open another dialog.
        QTimer::singleShot(0,this,[action] {if(action&&action->isEnabled())action->trigger();});
    };
    connect(search,&QLineEdit::textChanged,&dialog,update);
    connect(search,&QLineEdit::returnPressed,&dialog,execute);
    connect(list,&QListWidget::itemDoubleClicked,&dialog,[&](QListWidgetItem*){execute();});
    connect(list,&QListWidget::currentItemChanged,&dialog,[&](QListWidgetItem*,QListWidgetItem*){
        run->setEnabled(current()!=nullptr);
        star->setEnabled(current()!=nullptr);
    });
    connect(run,&QPushButton::clicked,&dialog,execute);
    connect(star,&QPushButton::clicked,&dialog,[&] {
        const Entry* e=current();if(!e)return;
        if(favorite.contains(e->key))favorite.removeAll(e->key);
        else favorite.append(e->key);
        settings.setValue("commandSearch/favorites",favorite);
        update();
    });
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    search->setText(initialQuery);
    update();search->setFocus();search->selectAll();dialog.exec();
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
    addPolicyCheck("Section view","section_view",requiredConventions);
    addPolicyCheck("Detail view","detail_view",requiredConventions);
    addPolicyCheck("Linked parts list","linked_parts_list",requiredConventions);
    addPolicyCheck("BOM balloons","bom_balloons",requiredConventions);
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

void MainWindow::runLinkedBoolean(cad::BooleanOp op){
    if(!requireBodyMode("Linked Boolean"))return;
    const auto ids=selectedIds();
    if(ids.size()!=2){reportError("Linked Boolean","Select two solid bodies (Ctrl+click). The sources will be preserved.");return;}
    QString a=ids[0],b=ids[1];
    if(op==cad::BooleanOp::Cut){
        QStringList choices={document_.find(a)->name,document_.find(b)->name};
        bool accepted=false;
        const auto selected=QInputDialog::getItem(this,"Associative subtraction","Target (retain this body minus the other):",choices,0,false,&accepted);
        if(!accepted)return;
        if(selected==choices[1])std::swap(a,b);
    }
    QString error;
    if(!document_.addLinkedBooleanBody(op,a,b,&error)){reportError("Linked Boolean",error);return;}
    refresh(true);statusBar()->showMessage("Linked result created. Source bodies are preserved.",7000);
}

void MainWindow::editLinkedBoolean(){
    if(!requireBodyMode("Edit linked Boolean"))return;
    const auto ids=selectedIds();
    if(ids.size()!=1){reportError("Edit linked Boolean","Select exactly one linked Boolean result.");return;}
    const cad::Part* result=document_.find(ids.first());
    if(!result||!result->hasLinkedBoolean){
        reportError("Edit linked Boolean","Select an associative Boolean result, not a source body.");return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle("Edit associative Boolean result");
    QVBoxLayout layout(&dialog);
    auto* form=new QFormLayout;
    auto* operation=new QComboBox(&dialog);
    operation->addItems({"Union (Fuse)","Subtract (Cut)","Intersection (Common)"});
    operation->setCurrentIndex(result->linkedOperation=="Fuse"?0:result->linkedOperation=="Cut"?1:2);
    auto* primary=new QComboBox(&dialog);
    auto* tool=new QComboBox(&dialog);
    for(const cad::Part& part:document_.parts()){
        if(part.id==result->id||part.isSurfaceBody)continue;
        const QString label=part.name+QStringLiteral(" [")+part.id.left(8)+QStringLiteral("]");
        primary->addItem(label,part.id);
        tool->addItem(label,part.id);
    }
    primary->setCurrentIndex(primary->findData(result->linkedPrimaryId));
    tool->setCurrentIndex(tool->findData(result->linkedToolId));
    form->addRow("Operation",operation);
    form->addRow("Primary body (A)",primary);
    form->addRow("Tool body (B)",tool);
    layout.addLayout(form);
    auto* help=new QLabel("Rebuilds all dependent linked bodies. Cycles, empty results, and invalid geometry are rejected without changing the document.",&dialog);
    help->setWordWrap(true);
    layout.addWidget(help);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    layout.addWidget(buttons);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    const auto op=operation->currentIndex()==0?cad::BooleanOp::Fuse:
                  operation->currentIndex()==1?cad::BooleanOp::Cut:cad::BooleanOp::Common;
    if(!document_.updateLinkedBooleanBody(result->id,op,primary->currentData().toString(),
                                          tool->currentData().toString(),&error)){
        reportError("Edit linked Boolean",error);return;
    }
    refresh(true);
    statusBar()->showMessage("Associative result and its linked descendants updated.",7000);
}

void MainWindow::editLinkedBooleanBatch(){
    if(!requireBodyMode("Batch associative Boolean edit"))return;
    QVector<const cad::Part*> results;
    for(const cad::Part& part:document_.parts())if(part.hasLinkedBoolean)results.append(&part);
    if(results.isEmpty()){
        QMessageBox::information(this,"Batch associative Boolean edit","No linked Boolean results exist in this document.");
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle("Edit linked results as one transaction");
    dialog.resize(1100,560);
    QVBoxLayout layout(&dialog);
    auto* instructions=new QLabel("Edit the operation or sources in one or more rows. The complete dependency graph is validated before any geometry changes; all successful edits form one undo step.",&dialog);
    instructions->setWordWrap(true);
    layout.addWidget(instructions);
    auto* table=new QTableWidget(results.size(),4,&dialog);
    table->setHorizontalHeaderLabels({"Result (read-only)","Operation","Primary body","Tool body"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for(int row=0;row<results.size();++row){
        const cad::Part& result=*results[row];
        auto* name=new QTableWidgetItem(result.name+QStringLiteral(" [")+result.id.left(8)+QStringLiteral("]"));
        name->setData(Qt::UserRole,result.id);
        table->setItem(row,0,name);
        auto* operation=new QComboBox(table);
        operation->addItem("Union (Fuse)","Fuse");
        operation->addItem("Subtract (Cut)","Cut");
        operation->addItem("Intersection (Common)","Common");
        operation->setCurrentIndex(qMax(0,operation->findData(result.linkedOperation)));
        table->setCellWidget(row,1,operation);
        auto* primary=new QComboBox(table);
        auto* tool=new QComboBox(table);
        for(const cad::Part& part:document_.parts()){
            if(part.id==result.id||part.isSurfaceBody)continue;
            const QString title=part.name+QStringLiteral(" [")+part.id.left(8)+QStringLiteral("]");
            primary->addItem(title,part.id);
            tool->addItem(title,part.id);
        }
        primary->setCurrentIndex(primary->findData(result.linkedPrimaryId));
        tool->setCurrentIndex(tool->findData(result.linkedToolId));
        table->setCellWidget(row,2,primary);
        table->setCellWidget(row,3,tool);
    }
    layout.addWidget(table);
    auto edits=[&](){
        QVector<cad::LinkedBooleanEdit> result;
        for(int row=0;row<table->rowCount();++row){
            const auto* op=qobject_cast<QComboBox*>(table->cellWidget(row,1));
            const auto* a=qobject_cast<QComboBox*>(table->cellWidget(row,2));
            const auto* b=qobject_cast<QComboBox*>(table->cellWidget(row,3));
            cad::LinkedBooleanEdit edit;
            edit.resultId=table->item(row,0)->data(Qt::UserRole).toString();
            edit.operation=op->currentIndex()==0?cad::BooleanOp::Fuse:
                           op->currentIndex()==1?cad::BooleanOp::Cut:cad::BooleanOp::Common;
            edit.primaryId=a->currentData().toString();
            edit.toolId=b->currentData().toString();
            result.append(std::move(edit));
        }
        return result;
    };
    auto* preview=new QPushButton("Validate graph and preview affected bodies",&dialog);
    layout.addWidget(preview);
    QObject::connect(preview,&QPushButton::clicked,&dialog,[&](){
        QStringList impacted;
        QString error;
        if(!document_.previewLinkedBooleanBatch(edits(),&impacted,&error)){
            QMessageBox::warning(&dialog,"Invalid dependency graph",error);return;
        }
        QStringList labels;
        for(const QString& id:impacted){
            const cad::Part* part=document_.find(id);
            labels<<QStringLiteral("%1 [%2]").arg(part?part->name:QStringLiteral("Unknown")).arg(id.left(8));
        }
        QMessageBox::information(&dialog,"Batch dependency impact",
            impacted.isEmpty()?QStringLiteral("No changes to the dependency graph."):
            QStringLiteral("%1 result(s) would be recomputed or checked in this order:\n\n%2")
                .arg(impacted.size()).arg(labels.join('\n')));
    });
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    layout.addWidget(buttons);
    if(dialog.exec()!=QDialog::Accepted)return;
    QString error;
    if(!document_.updateLinkedBooleanBatch(edits(),&error)){
        reportError("Batch associative Boolean edit",error);return;
    }
    refresh(true);
    statusBar()->showMessage("Linked Boolean batch committed atomically as one undo step.",7000);
}

void MainWindow::inspectLinkedBooleanGraph(){
    if(!requireBodyMode("Associative dependencies"))return;
    QStringList lines;
    int total=0,stale=0;
    for(const cad::Part& part:document_.parts()){
        if(!part.hasLinkedBoolean)continue;
        ++total;
        const cad::Part* primary=document_.find(part.linkedPrimaryId);
        const cad::Part* tool=document_.find(part.linkedToolId);
        const bool current=document_.linkedBooleanCurrent(part.id);
        if(!current)++stale;
        lines<<QStringLiteral("%1 [%2]  %3  (%4)")
               .arg(part.name).arg(part.id.left(8)).arg(part.linkedOperation)
               .arg(current?QStringLiteral("current"):QStringLiteral("STALE"));
        lines<<QStringLiteral("  A: %1 [%2]")
               .arg(primary?primary->name:QStringLiteral("MISSING")).arg(part.linkedPrimaryId.left(8));
        lines<<QStringLiteral("  B: %1 [%2]")
               .arg(tool?tool->name:QStringLiteral("MISSING")).arg(part.linkedToolId.left(8));
        lines<<QString();
    }
    QDialog dialog(this);
    dialog.setWindowTitle("Associative body dependency report");
    dialog.resize(650,520);
    QVBoxLayout layout(&dialog);
    layout.addWidget(new QLabel(QStringLiteral("Linked results: %1   Stale: %2").arg(total).arg(stale),&dialog));
    auto* report=new QPlainTextEdit(&dialog);
    report->setReadOnly(true);
    report->setPlainText(lines.isEmpty()?QStringLiteral("No associative Boolean results in this scene."):lines.join('\n'));
    layout.addWidget(report);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    layout.addWidget(buttons);
    dialog.exec();
}

void MainWindow::previewLinkedBodyImpact(){
    if(!requireBodyMode("Associative rebuild preview"))return;
    const auto ids=selectedIds();
    if(ids.size()!=1){reportError("Associative rebuild preview","Select one source body or linked result.");return;}
    QStringList affected,explanations;
    QString error;
    if(!document_.linkedBodyImpact(ids.first(),&affected,&error,&explanations)){
        reportError("Associative rebuild preview",error);return;
    }
    QStringList names;
    for(int i=0;i<affected.size();++i){
        const QString& id=affected[i];
        const cad::Part* p=document_.find(id);
        if(p)names<<QStringLiteral("%1 [%2]%3\n    dependency: %4")
            .arg(p->name).arg(id.left(8))
            .arg(id==ids.first()?QStringLiteral(" (selected source)"):QStringLiteral(" (dependent)"))
            .arg(i<explanations.size()?explanations[i]:QStringLiteral("unknown"));
    }
    QMessageBox::information(this,"Associative rebuild impact",
        QStringLiteral("Editing the selected body may require rebuilding %1 linked result(s).\n\n%2")
        .arg((affected.size()>0?affected.size()-1:0)).arg(names.join('\n')));
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
    else {rememberRecentProject(path);refresh(true);}
}
void MainWindow::createDocumentRevision() {
    if(document_.path().isEmpty()||document_.dirty())if(!saveFile(document_.path().isEmpty()))return;
    bool accepted=false;
    const QString label=QInputDialog::getText(this,"Create document revision","Revision name:",QLineEdit::Normal,{},&accepted).trimmed();
    if(!accepted)return;
    const QString note=QInputDialog::getText(this,"Create document revision","Optional change note:",QLineEdit::Normal,{},&accepted);
    if(!accepted)return;
    QString revisionId,error;
    if(!cad::DocumentRevisionHistory::create(document_.path(),label,note,&revisionId,&error)){
        reportError("Create document revision",error);return;
    }
    statusBar()->showMessage(QString("Saved revision '%1' (%2). History stays beside the scene file.").arg(label,revisionId.left(8)),8000);
}
void MainWindow::compareDocumentRevisions() {
    if(document_.path().isEmpty()){reportError("Compare document revisions","Save the scene before creating or comparing named revisions.");return;}
    if(document_.dirty()&&!saveFile())return;
    QVector<cad::DocumentRevisionSummary> revisions;QString error;
    if(!cad::DocumentRevisionHistory::list(document_.path(),&revisions,&error)){
        reportError("Compare document revisions",error);return;
    }
    if(revisions.isEmpty()){reportError("Compare document revisions","Create a named revision before comparing saved scenes.");return;}
    QStringList labels;for(const auto& revision:revisions)
        labels.append(QStringLiteral("%1%2 — %3 [%4]").arg(revision.label,
            revision.tags.isEmpty()?QString():QStringLiteral(" {tags: %1}").arg(revision.tags.join(", ")),
            revision.createdUtc,revision.id.left(8)));
    bool accepted=false;
    const QString olderLabel=QInputDialog::getItem(this,"Compare document revisions","Earlier revision:",labels,0,false,&accepted);
    if(!accepted)return;
    const int olderIndex=labels.indexOf(olderLabel);if(olderIndex<0)return;
    QStringList newerLabels;QVector<int> newerIndexes;
    for(int i=0;i<labels.size();++i)if(i!=olderIndex){newerLabels.append(labels[i]);newerIndexes.append(i);}
    newerLabels.append(QStringLiteral("Current saved scene"));newerIndexes.append(-1);
    const QString newerLabel=QInputDialog::getItem(this,"Compare document revisions","Revision to compare against:",newerLabels,0,false,&accepted);
    if(!accepted)return;
    const int newerChoice=newerLabels.indexOf(newerLabel);if(newerChoice<0)return;
    cad::DocumentRevisionDiff diff;
    const int newerIndex=newerIndexes[newerChoice];
    const bool compared=newerIndex<0
        ? cad::DocumentRevisionHistory::compareWithSavedScene(document_.path(),revisions[olderIndex].id,&diff,&error)
        : cad::DocumentRevisionHistory::compare(document_.path(),revisions[olderIndex].id,
                                                revisions[newerIndex].id,&diff,&error);
    if(!compared){
        reportError("Compare document revisions",error);return;
    }
    QStringList lines{QStringLiteral("From: %1").arg(revisions[olderIndex].label),
                      QStringLiteral("To: %1").arg(newerIndex<0?QStringLiteral("Current saved scene"):revisions[newerIndex].label)};
    if(diff.drawingReadinessPolicyChanged)lines.append(QStringLiteral("Project drawing-readiness settings changed."));
    for(const auto& change:diff.bodyChanges){
        QStringList fields;
        if(change.added)fields.append(QStringLiteral("added"));
        if(change.removed)fields.append(QStringLiteral("removed"));
        if(change.geometryChanged)fields.append(QStringLiteral("geometry changed"));
        if(change.metadataChanged)fields.append(QStringLiteral("part metadata changed"));
        if(change.featureHistoryChanged)fields.append(QStringLiteral("saved parametric feature history changed"));
        lines.append(QStringLiteral("• %1 (%2): %3").arg(change.bodyName,change.bodyId.left(8),fields.join(QStringLiteral(", "))));
    }
    const auto displayJsonValue=[](const QJsonValue& value){
        if(value.isUndefined())return QStringLiteral("<missing>");
        if(value.isObject())return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
        if(value.isArray())return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
        if(value.isNull())return QStringLiteral("null");
        return value.toVariant().toString();
    };
    const int displayFieldLimit=200;
    const qsizetype displayCount=std::min<qsizetype>(displayFieldLimit,diff.fieldChanges.size());
    for(qsizetype i=0;i<displayCount;++i){
        const auto& change=diff.fieldChanges[i];
        lines.append(QStringLiteral("%1: %2 → %3").arg(change.path,displayJsonValue(change.olderValue),displayJsonValue(change.newerValue)));
    }
    if(diff.fieldChanges.size()>displayFieldLimit)
        lines.append(QStringLiteral("Showing the first %1 of %2 detailed changes.").arg(displayFieldLimit).arg(diff.fieldChanges.size()));
    if(diff.fieldChangesTruncated)lines.append(QStringLiteral("Detailed field changes were truncated at the safety limit."));
    if(diff.bodyChanges.isEmpty()&&diff.fieldChanges.isEmpty())
        lines.append(QStringLiteral("No stored body or project changes."));
    const QString reportText=lines.join('\n');
    QDialog report(this);report.setWindowTitle("Document revision comparison");report.resize(760,520);
    auto* layout=new QVBoxLayout(&report);
    auto* text=new QPlainTextEdit(&report);text->setReadOnly(true);text->setPlainText(reportText);
    layout->addWidget(text);
    auto* actions=new QDialogButtonBox(QDialogButtonBox::Close,&report);
    auto* saveReport=actions->addButton("Save report...",QDialogButtonBox::ActionRole);
    layout->addWidget(actions);
    connect(actions,&QDialogButtonBox::rejected,&report,&QDialog::reject);
    connect(saveReport,&QPushButton::clicked,&report,[this,&report,reportText]{
        const QString defaultName=QFileInfo(document_.path()).completeBaseName()+QStringLiteral("_revision_comparison.txt");
        const QString path=QFileDialog::getSaveFileName(&report,"Save revision comparison report",
            QDir(QFileInfo(document_.path()).absolutePath()).filePath(defaultName),"Text report (*.txt)");
        if(path.isEmpty())return;
        QSaveFile file(path);
        const QByteArray bytes=reportText.toUtf8();
        if(!file.open(QIODevice::WriteOnly)){
            reportError("Save revision comparison report",file.errorString());return;
        }
        if(file.write(bytes)!=bytes.size()){
            file.cancelWriting();reportError("Save revision comparison report","Could not write the complete report.");return;
        }
        if(!file.commit())reportError("Save revision comparison report",file.errorString());
    });
    report.exec();
}
void MainWindow::restoreDocumentRevision() {
    if(document_.path().isEmpty()){reportError("Restore document revision","Save the scene before creating or restoring named revisions.");return;}
    QVector<cad::DocumentRevisionSummary> revisions;QString error;
    if(!cad::DocumentRevisionHistory::list(document_.path(),&revisions,&error)){
        reportError("Restore document revision",error);return;
    }
    if(revisions.isEmpty()){reportError("Restore document revision","No named revisions are saved beside this scene.");return;}
    QStringList labels;for(const auto& revision:revisions)
        labels.append(QStringLiteral("%1%2 — %3 [%4]").arg(revision.label,
            revision.tags.isEmpty()?QString():QStringLiteral(" {tags: %1}").arg(revision.tags.join(", ")),
            revision.createdUtc,revision.id.left(8)));
    bool accepted=false;const QString label=QInputDialog::getItem(this,"Restore document revision","Revision to restore:",labels,0,false,&accepted);
    if(!accepted)return;const int index=labels.indexOf(label);if(index<0)return;
    QString defaultPath=QFileInfo(document_.path()).dir().filePath(QFileInfo(document_.path()).completeBaseName()+QStringLiteral("-restored.cadscene"));
    const QString target=QFileDialog::getSaveFileName(this,"Save restored revision as",defaultPath,"CADWorkbench (*.cadscene)");
    if(target.isEmpty())return;
    QString finalTarget=target;if(!finalTarget.endsWith(QStringLiteral(".cadscene"),Qt::CaseInsensitive))finalTarget+=QStringLiteral(".cadscene");
    if(!cad::DocumentRevisionHistory::restore(document_.path(),revisions[index].id,finalTarget,&error)){
        reportError("Restore document revision",error);return;
    }
    statusBar()->showMessage(QString("Restored '%1' as %2. Your open scene was left unchanged.")
        .arg(revisions[index].label,QFileInfo(finalTarget).fileName()),10000);
}
void MainWindow::manageDocumentRevisionHistory() {
    if(document_.path().isEmpty()){
        reportError("Manage document revisions","Save the scene before managing its revision history.");return;
    }
    QString error;QVector<cad::DocumentRevisionSummary> revisions;
    if(!cad::DocumentRevisionHistory::list(document_.path(),&revisions,&error)){
        reportError("Manage document revisions",error);return;
    }
    if(revisions.isEmpty()){
        reportError("Manage document revisions","No named revisions are saved beside this scene.");return;
    }
    const QStringList choices{QStringLiteral("Edit revision tags"),QStringLiteral("Prune older untagged revisions")};
    bool accepted=false;const QString choice=QInputDialog::getItem(this,"Manage document revisions","Choose an action:",choices,0,false,&accepted);
    if(!accepted)return;
    if(choice==choices[0]){
        QStringList labels;for(const auto& revision:revisions)
            labels.append(QStringLiteral("%1%2 — %3 [%4]").arg(revision.label,
                revision.tags.isEmpty()?QString():QStringLiteral(" {tags: %1}").arg(revision.tags.join(", ")),
                revision.createdUtc,revision.id.left(8)));
        const QString selected=QInputDialog::getItem(this,"Tag a document revision","Revision:",labels,0,false,&accepted);
        if(!accepted)return;const int index=labels.indexOf(selected);if(index<0)return;
        const QString entered=QInputDialog::getText(this,"Edit revision tags","Comma-separated tags (clear to remove all):",
            QLineEdit::Normal,revisions[index].tags.join(", "),&accepted);
        if(!accepted)return;
        const QStringList tags=entered.split(',',Qt::SkipEmptyParts);
        if(!cad::DocumentRevisionHistory::setTags(document_.path(),revisions[index].id,tags,&error)){
            reportError("Tag document revision",error);return;
        }
        statusBar()->showMessage(QString("Updated tags for revision '%1'.").arg(revisions[index].label),8000);
        return;
    }
    const int keepCount=QInputDialog::getInt(this,"Prune revision history","Keep this many newest revisions, in addition to every tagged revision:",
                                              std::min(20,static_cast<int>(revisions.size())),1,1000,1,&accepted);
    if(!accepted)return;
    if(QMessageBox::question(this,"Prune revision history",
        QStringLiteral("Remove older untagged checkpoints and keep the newest %1 plus all tagged revisions?").arg(keepCount),
        QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
    int removed=0;
    if(!cad::DocumentRevisionHistory::prune(document_.path(),keepCount,&removed,&error)){
        reportError("Prune revision history",error);return;
    }
    statusBar()->showMessage(QString("Removed %1 older untagged revision(s).").arg(removed),8000);
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
    const QString path=QFileDialog::getOpenFileName(this,"Import OBJ mesh",{},"OBJ meshes (*.obj)");
    if(path.isEmpty())return;
    QSettings settings;bool accepted=false;
    const double tolerance=QInputDialog::getDouble(this,"OBJ mesh repair","Vertex weld tolerance (mm):",
        settings.value(QStringLiteral("meshImport/weldToleranceMm"),0.01).toDouble(),1e-6,1.0,6,&accepted);
    if(!accepted)return;
    settings.setValue(QStringLiteral("meshImport/weldToleranceMm"),tolerance);
    const QStringList repairChoices{QStringLiteral("Reject open boundaries (solid only)"),
                                   QStringLiteral("Fill simple convex planar holes (up to 64 edges)"),
                                   QStringLiteral("Keep cleaned mesh as faceted surface body")};
    const QString repairChoice=QInputDialog::getItem(this,"OBJ mesh repair","Open-boundary handling:",repairChoices,0,false,&accepted);
    if(!accepted)return;
    QString error;cad::MeshImportSummary summary;
    if(!document_.importOBJ(path,tolerance,&error,&summary,repairChoice==repairChoices[1],repairChoice==repairChoices[2])){reportError("Import OBJ mesh",error);return;}
    refresh(true);statusBar()->showMessage(meshImportStatus("OBJ",QFileInfo(path).fileName(),summary),10000);
}
void MainWindow::importSTLMeshFile() {
    const QString path=QFileDialog::getOpenFileName(this,"Import STL mesh",{},"STL meshes (*.stl)");
    if(path.isEmpty())return;
    QSettings settings;bool accepted=false;
    const double tolerance=QInputDialog::getDouble(this,"STL mesh repair","Vertex weld tolerance (mm):",
        settings.value(QStringLiteral("meshImport/weldToleranceMm"),0.01).toDouble(),1e-6,1.0,6,&accepted);
    if(!accepted)return;
    settings.setValue(QStringLiteral("meshImport/weldToleranceMm"),tolerance);
    const QStringList repairChoices{QStringLiteral("Reject open boundaries (solid only)"),
                                   QStringLiteral("Fill simple convex planar holes (up to 64 edges)"),
                                   QStringLiteral("Keep cleaned mesh as faceted surface body")};
    const QString repairChoice=QInputDialog::getItem(this,"STL mesh repair","Open-boundary handling:",repairChoices,0,false,&accepted);
    if(!accepted)return;
    QString error;cad::MeshImportSummary summary;
    if(!document_.importSTL(path,tolerance,&error,&summary,repairChoice==repairChoices[1],repairChoice==repairChoices[2])){reportError("Import STL mesh",error);return;}
    refresh(true);statusBar()->showMessage(meshImportStatus("STL",QFileInfo(path).fileName(),summary),10000);
}

void MainWindow::exportBodyScheduleCsv() {
    const QString path=QFileDialog::getSaveFileName(this,"Export body schedule",{},"CSV files (*.csv)");
    if(path.isEmpty())return;
    QString error;
    if(!document_.exportBodyScheduleCsv(path,&error)){reportError("Export body schedule",error);return;}
    statusBar()->showMessage(QString("Body schedule exported: %1").arg(QFileInfo(path).fileName()),8000);
}
void MainWindow::exportHoleTableCsv(){
    if(!requireBodyMode("Axial hole inspection"))return;
    const QStringList ids=selectedIds();
    if(ids.size()!=1){
        reportError("Axial hole table","Select exactly one solid part to inspect for cylindrical Z-axis bores.");return;
    }
    const cad::Part* part=document_.find(ids.first());
    if(!part||part->isSurfaceBody){
        reportError("Axial hole table","Select one solid body, not a surface or drawing sheet.");return;
    }
    cad::AxialHoleAudit audit;QString error;
    if(!cad::inspectAxialHoles(*part,&audit,&error)){
        reportError("Axial hole inspection",error);return;
    }
    if(audit.holes.isEmpty()){
        reportError("Axial hole inspection",QStringLiteral(
            "No complete, Z-axis internal cylindrical bores were confirmed. %1 cylinder faces "
            "were excluded. This tool deliberately skips angled, partial or ambiguous openings.")
            .arg(audit.excludedCylinders));return;
    }
    const QString path=QFileDialog::getSaveFileName(this,"Export model-linked axial hole audit CSV",{},
                                                    "Comma-separated values (*.csv)");
    if(path.isEmpty())return;
    if(!cad::exportAxialHoleTableCsv(*part,path,&error)){
        reportError("Export axial hole table",error);return;
    }
    statusBar()->showMessage(QStringLiteral("Exported %1 candidate axial holes from the current B-rep; "
        "%2 unsupported faces excluded. Verify before manufacturing.")
        .arg(audit.holes.size()).arg(audit.excludedCylinders),11000);
}

void MainWindow::exportDrillingWorksheetCsv(){
    if(!requireBodyMode("Drilling feasibility worksheet"))return;
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Drilling worksheet","Select exactly one solid body.");return;}
    const cad::Part* part=document_.find(ids.first());
    if(!part||part->isSurfaceBody){reportError("Drilling worksheet","Select a valid solid body.");return;}
    cad::AxialHoleAudit audit;QString error;
    if(!cad::inspectAxialHoles(*part,&audit,&error)){
        reportError("Drilling worksheet",error);return;
    }
    if(audit.holes.isEmpty()){
        reportError("Drilling worksheet","No complete axial bores were confirmed on the selected body.");return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle("Review drilling feasibility (not executable CAM)");
    auto* layout=new QVBoxLayout(&dialog);
    auto* warning=new QLabel(QStringLiteral(
        "%1 candidate holes, %2 unsupported cylindrical faces.\n"
        "Only complete top-entry Z-axis holes are considered. This is a geometric feasibility "
        "worksheet, NOT G-code, approved tooling or collision-checked machining.")
        .arg(audit.holes.size()).arg(audit.excludedCylinders));
    warning->setWordWrap(true);layout->addWidget(warning);
    auto* form=new QFormLayout;
    auto* tools=new QLineEdit(QStringLiteral("5:30,8:30"));
    tools->setPlaceholderText("diameter:mm length pairs, e.g. 5:30,8:30");
    tools->setToolTip("Enter comma-separated drill diameter:usable cutting length pairs in mm.");
    form->addRow("Drill catalog (diameter:length, mm)",tools);
    auto* clearance=new QDoubleSpinBox;
    clearance->setRange(0.2,10000);clearance->setDecimals(3);clearance->setValue(10);
    clearance->setSuffix(" mm");form->addRow("Preview clearance above stock",clearance);
    auto* diameterTolerance=new QDoubleSpinBox;
    diameterTolerance->setRange(0,2);diameterTolerance->setDecimals(3);
    diameterTolerance->setSingleStep(0.01);diameterTolerance->setValue(0.05);
    diameterTolerance->setSuffix(" mm");form->addRow("Drill diameter matching tolerance",diameterTolerance);
    auto* fixture=new QCheckBox("I separately reviewed fixture/exit clearance for through holes");
    form->addRow(fixture);layout->addLayout(form);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    layout->addWidget(buttons);
    if(dialog.exec()!=QDialog::Accepted)return;
    cad::DrillingReviewSettings settings;
    settings.toolCatalog=tools->text();
    settings.clearanceAboveStockMm=clearance->value();
    settings.diameterToleranceMm=diameterTolerance->value();
    settings.fixtureExitReviewed=fixture->isChecked();
    // Validate all inputs and fresh geometry before asking for an output file.
    QByteArray preview;
    if(!cad::buildDrillingWorksheetCsv(*part,settings,&preview,&error)){
        reportError("Drilling worksheet",error);return;
    }
    const QString path=QFileDialog::getSaveFileName(this,
        "Export revision-linked drilling feasibility worksheet",{},"CSV files (*.csv)");
    if(path.isEmpty())return;
    // Regenerate from the same current model rather than exporting stale UI preview.
    if(!cad::exportDrillingWorksheetCsv(*part,settings,path,&error)){
        reportError("Drilling worksheet export",error);return;
    }
    statusBar()->showMessage(QStringLiteral(
        "Exported drilling feasibility review for %1 holes. No machining operation was approved.")
        .arg(audit.holes.size()),10000);
}

void MainWindow::exportManufacturingHandoff() {
    if(document_.path().isEmpty()||document_.dirty()){
        reportError("Manufacturing handoff", "Save the current project first. The handoff must match a saved design revision.");
        return;
    }
    const auto confirmation=QMessageBox::question(this,"Unissued engineering handoff",
        "Create one traceable package containing the saved project, STEP assembly, BOM, "
        "occurrence list, model audit, procurement exceptions, and inspection requirements?\n\n"
        "This is NOT an approved fabrication release. The audit must have no errors; "
        "any warnings and missing procurement information remain explicitly listed.",
        QMessageBox::Yes|QMessageBox::No,QMessageBox::No);
    if(confirmation!=QMessageBox::Yes)return;
    const QString suggested=QFileInfo(document_.path()).absolutePath();
    const QString parent=QFileDialog::getExistingDirectory(this,
        "Choose an existing parent folder for the new handoff directory",suggested);
    if(parent.isEmpty())return;
    cad::ManufacturingHandoffResult result;
    QString issue;
    if(!cad::createManufacturingHandoff(document_,parent,&result,&issue)){
        reportError("Manufacturing handoff",issue);return;
    }
    QMessageBox::information(this,"Unissued handoff created",
        QStringLiteral("A traceable handoff folder was created:\n%1\n\n"
            "%2 BOM lines, %3 audit warnings, %4 procurement exceptions, "
            "%5 inspection requirements.\n"
            "%6 observed Z-axis holes; %7 bodies need separate machining review.\n\n"
            "NOT RELEASED FOR MANUFACTURE. Hole measurements are not CAM instructions. "
            "Review all findings and drawing revisions first.")
        .arg(result.folder).arg(result.bomLines).arg(result.warningCount)
        .arg(result.procurementExceptions).arg(result.inspectionRequirements)
        .arg(result.axialHolesReviewed).arg(result.machiningBodiesRequiringReview));
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
    if(part->hasDrawingSettings){
        const auto sourceStatus=document_.drawingSheetSourceStatus(ids.first());
        if(sourceStatus.state==cad::DrawingSourceState::Stale||sourceStatus.state==cad::DrawingSourceState::MissingBodies){
            const QString message=sourceStatus.state==cad::DrawingSourceState::MissingBodies
                ? QStringLiteral("A saved drawing source body is missing. Restore it before refreshing the source link.")
                : QStringLiteral("The saved drawing source geometry or BOM/material data is stale. Moving views keeps its saved revision until you explicitly refresh it.");
            auto* stale=new QLabel(message,&dialog);
            stale->setWordWrap(true);stale->setStyleSheet("color:#a65b00");form->addRow(stale);
        }
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
    QString error;
    if(!document_.setDrawingSheetSettings(ids.first(),settings,&error)){reportError("Save drawing layout",error);return;}
    refresh();statusBar()->showMessage("Projected-view placements saved; the existing source revision is unchanged.",8000);
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
    settings.generalNotes=noteTexts;settings.noteAnnotations=annotations;
    QString error;
    if(!document_.setDrawingSheetSettings(ids.first(),settings,&error)){reportError("Save drawing notes",error);return;}
    refresh();statusBar()->showMessage("Drawing notes saved with stable IDs; the existing source revision is unchanged.",8000);
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
    settings.dimensionAnnotations=annotations;
    QString error;
    if(!document_.setDrawingSheetSettings(ids.first(),settings,&error)){reportError("Save drawing dimensions",error);return;}
    refresh();statusBar()->showMessage("Overall dimensions saved with stable IDs; the existing source revision is unchanged.",8000);
}
void MainWindow::addSelectedEdgeDrawingDimension() {
    if(viewport_->selectionMode()!=CadViewport::SelectionMode::Edge){
        reportError("Drawing engineering dimension","Choose Edges (inspection), then select one circle/edge or two circular edges.");return;
    }
    const QVector<cad::EdgeReference> edges=viewport_->selectedEdgeReferences();
    if((edges.size()!=1&&edges.size()!=2)||
       (edges.size()==2&&edges[0].partId!=edges[1].partId)){
        reportError("Drawing engineering dimension","Select one edge for length/diameter, or two circles on the same body for center distance.");return;
    }
    const cad::EdgeReference& edge=edges.first();
    const cad::Part* part=document_.find(edge.partId);
    if(!part){reportError("Drawing engineering dimension","The source body no longer exists.");return;}
    QString error;double length=0.0;
    if(!document_.measureEdgeLength(edge,&length,&error)){
        reportError("Drawing engineering dimension",error);return;
    }
    cad::DrawingSheetSettings settings=part->hasDrawingSettings?part->drawingSettings:cad::DrawingSheetSettings{};
    if(settings.edgeDimensionAnnotations.size()>=20){
        reportError("Drawing engineering dimension","The drawing already contains 20 linked dimensions.");return;
    }
    QDialog dialog(this);dialog.setWindowTitle("Add model-linked manufacturing dimension");
    auto* form=new QFormLayout(&dialog);
    form->addRow(new QLabel(edges.size()==2?
        QStringLiteral("Center distance between two selected circles. Both centers must lie in the same plane."):
        QStringLiteral("Selected edge currently measures %1 mm. Values are remeasured at drawing export.").arg(length,0,'f',3),&dialog));
    auto* kind=new QComboBox(&dialog);
    if(edges.size()==2)kind->addItem("Center-to-center distance","CENTER_DISTANCE");
    else {
        kind->addItem("Edge length","LENGTH");
        kind->addItem("Full circular edge diameter","DIAMETER");
        kind->addItem("Hole center X from model origin","CENTER_X");
        kind->addItem("Hole center Y from model origin","CENTER_Y");
        kind->addItem("Hole center Z from model origin","CENTER_Z");
    }
    form->addRow("Dimension type:",kind);
    auto* view=new QComboBox(&dialog);view->addItems({"TOP","FRONT","RIGHT"});
    form->addRow("Normal drawing view:",view);
    auto* offset=new QDoubleSpinBox(&dialog);offset->setRange(5,60);offset->setDecimals(2);
    offset->setSuffix(" mm");offset->setValue(9);form->addRow("Leader offset:",offset);
    auto* upper=new QDoubleSpinBox(&dialog);upper->setRange(0,1000);upper->setDecimals(3);
    upper->setSuffix(" mm");form->addRow("Upper tolerance (+):",upper);
    auto* lower=new QDoubleSpinBox(&dialog);lower->setRange(0,1000);lower->setDecimals(3);
    lower->setSuffix(" mm");form->addRow("Lower tolerance (-):",lower);
    form->addRow(new QLabel("Hole-center X/Y/Z positions are signed ordinates measured from the model origin, not GD&T datums. Tolerances are user-entered, not certified.",&dialog));
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);
    form->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    // Viewport occurrences and TopExp's stable unique map have different
    // indices. Only save kernel-backed geometric witnesses.
    TopTools_IndexedMapOfShape uniqueEdges;TopExp::MapShapes(part->shape,TopAbs_EDGE,uniqueEdges);
    auto capture=[&](const cad::EdgeReference& item,int* uniqueIndex,QMap<QString,double>* anchor){
        const TopoDS_Shape picked=selectedSubshape(document_,item.partId,item.geometryToken,
                                                  item.edgeIndex,TopAbs_EDGE);
        *uniqueIndex=0;
        for(int i=1;i<=uniqueEdges.Extent();++i)
            if(uniqueEdges.FindKey(i).IsSame(picked)){*uniqueIndex=i;break;}
        return *uniqueIndex>0&&cad::captureStableEdgeAnchor(part->shape,*uniqueIndex,anchor,&error)&&
               anchor->contains("edge_anchor_version");
    };
    int firstIndex=0,secondIndex=0;QMap<QString,double> firstWitness,secondWitness;
    if(!capture(edge,&firstIndex,&firstWitness)||
       (edges.size()==2&&!capture(edges[1],&secondIndex,&secondWitness))){
        reportError("Drawing engineering dimension",error.isEmpty()?QStringLiteral("The chosen edge cannot be witnessed."):error);return;
    }
    if(edges.size()==2&&firstIndex==secondIndex){
        reportError("Drawing engineering dimension","Select two distinct full circular edges.");return;
    }
    cad::DrawingEdgeDimensionAnnotation annotation{{},edge.partId,edge.geometryToken,
        edge.edgeIndex,view->currentText(),offset->value(),firstWitness};
    annotation.kind=kind->currentData().toString();
    if((annotation.kind=="CENTER_X"&&annotation.view=="RIGHT")||
       (annotation.kind=="CENTER_Y"&&annotation.view=="FRONT")||
       (annotation.kind=="CENTER_Z"&&annotation.view=="TOP")){
        reportError("Drawing hole location","Choose a drawing view that displays the requested world-axis coordinate.");return;
    }
    // Reject unsupported geometry at configuration time, not after a scene
    // has already been saved with an unusable manufacturing callout.
    if(annotation.kind!="LENGTH"){
        const BRepAdaptor_Curve first(TopoDS::Edge(uniqueEdges.FindKey(firstIndex)));
        const auto completeCircle=[](const BRepAdaptor_Curve& curve){
            return curve.GetType()==GeomAbs_Circle&&
                std::abs(curve.LastParameter()-curve.FirstParameter()-2*std::numbers::pi)<1e-4;
        };
        const gp_Dir viewNormal=view->currentText()=="TOP"?gp_Dir(0,0,1):
            (view->currentText()=="RIGHT"?gp_Dir(1,0,0):gp_Dir(0,-1,0));
        if(!completeCircle(first)||
           std::abs(gp_Vec(first.Circle().Axis().Direction()).Dot(gp_Vec(viewNormal)))<0.99999){
            reportError("Drawing manufacturing dimension",
                        "Select a full circular edge and a view normal to its circle plane.");return;
        }
        if(annotation.kind=="CENTER_DISTANCE"){
            const BRepAdaptor_Curve other(TopoDS::Edge(uniqueEdges.FindKey(secondIndex)));
            if(!completeCircle(other)||
               std::abs(gp_Vec(first.Circle().Axis().Direction()).Dot(
                   gp_Vec(other.Circle().Axis().Direction())))<0.99999||
               std::abs(gp_Vec(first.Circle().Location(),other.Circle().Location()).Dot(
                   gp_Vec(first.Circle().Axis().Direction())))>1e-4){
                reportError("Drawing manufacturing dimension",
                            "Both selected circles must be complete, coplanar, and have parallel axes.");return;
            }
        }
    }
    annotation.secondEdgeIndex=secondIndex;
    annotation.secondEdgeAnchor=secondWitness;
    annotation.upperToleranceMm=upper->value();
    annotation.lowerToleranceMm=lower->value();
    settings.edgeDimensionAnnotations.append(annotation);
    if(!document_.setDrawingSheetSettings(edge.partId,settings,&error)){
        reportError("Save drawing manufacturing dimension",error);return;
    }
    refresh();statusBar()->showMessage("Witnessed manufacturing dimension and tolerance saved to drawing sheet.",8000);
}
void MainWindow::editSelectedEdgeDrawingDimension() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){
        reportError("Edit linked drawing dimension","Select exactly one drawing-sheet owner body.");return;
    }
    const cad::Part* master=document_.find(ids.first());
    if(!master||!master->hasDrawingSettings||master->drawingSettings.edgeDimensionAnnotations.isEmpty()){
        reportError("Edit linked drawing dimension","No saved linked manufacturing dimensions exist on this sheet.");return;
    }
    const auto originalSettings=master->drawingSettings;
    QStringList entries;
    for(const auto& dimension:originalSettings.edgeDimensionAnnotations)
        entries.append(QStringLiteral("%1 | %2 | %3 | %4").arg(dimension.id,dimension.kind,
                        dimension.sourceBodyId,dimension.view));
    bool accepted=false;
    const QString selected=QInputDialog::getItem(this,"Edit linked manufacturing dimension",
        "Select the stored annotation:",entries,0,false,&accepted);
    if(!accepted)return;
    const int index=entries.indexOf(selected);
    if(index<0)return;
    const auto original=originalSettings.edgeDimensionAnnotations[index];
    const cad::Part* source=document_.find(original.sourceBodyId);
    if(!source){reportError("Edit linked drawing dimension","The source body is missing.");return;}
    QDialog dialog(this);
    dialog.setWindowTitle("Edit linked dimension, paper placement and manual tolerances");
    auto* form=new QFormLayout(&dialog);
    auto* note=new QLabel(QStringLiteral(
        "Dimension: %1\nSource: %2\nThe source edge, measurement kind, and annotation ID remain unchanged. "
        "To change the selected geometry, remove and reselect the dimension. "
        "This edit does not acknowledge a changed drawing source revision.")
        .arg(original.kind,source->name),&dialog);
    note->setWordWrap(true);form->addRow(note);
    if(original.sourceGeometryToken!=source->geometryToken){
        auto* warning=new QLabel("Source geometry has changed. The saved edge witness must resolve uniquely on the current body before this edit can be committed.",&dialog);
        warning->setWordWrap(true);form->addRow(warning);
    }
    auto* view=new QComboBox(&dialog);view->addItems({"TOP","FRONT","RIGHT"});
    view->setCurrentText(original.view);form->addRow("Drawing view:",view);
    auto* offset=new QDoubleSpinBox(&dialog);offset->setRange(5.0,60.0);
    offset->setDecimals(3);offset->setValue(original.offsetMm);offset->setSuffix(" mm");
    form->addRow("Leader offset:",offset);
    auto* upper=new QDoubleSpinBox(&dialog);upper->setRange(0.0,1000.0);
    upper->setDecimals(4);upper->setValue(original.upperToleranceMm);upper->setSuffix(" mm");
    form->addRow("Upper tolerance (+):",upper);
    auto* lower=new QDoubleSpinBox(&dialog);lower->setRange(0.0,1000.0);
    lower->setDecimals(4);lower->setValue(original.lowerToleranceMm);lower->setSuffix(" mm");
    form->addRow("Lower tolerance (-):",lower);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);
    form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    auto edited=original;
    edited.view=view->currentText();edited.offsetMm=offset->value();
    edited.upperToleranceMm=upper->value();edited.lowerToleranceMm=lower->value();
    if(edited==original){statusBar()->showMessage("No drawing dimension changes; revision unchanged.",6000);return;}
    QString error;
    if(!cad::validateDrawingEdgeDimensionPreview(*source,edited,&error)){
        reportError("Invalid linked drawing dimension",error);return;
    }
    auto updated=originalSettings;
    updated.edgeDimensionAnnotations[index]=edited;
    if(!document_.setDrawingSheetSettings(ids.first(),updated,&error)){
        reportError("Save linked drawing dimension",error);return;
    }
    refresh();statusBar()->showMessage("Linked dimension updated in place; identity and source revision preserved.",8000);
}

void MainWindow::removeSelectedEdgeDrawingDimension() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Remove drawing edge dimension","Select exactly one drawing source body in the model tree.");return;}
    const cad::Part* part=document_.find(ids.first());
    if(!part||!part->hasDrawingSettings||part->drawingSettings.edgeDimensionAnnotations.isEmpty()){
        reportError("Remove drawing edge dimension","This body has no saved selected-edge dimensions.");return;
    }
    QStringList labels;
    for(const auto& dimension:part->drawingSettings.edgeDimensionAnnotations)
        labels.append(QString("%1 | %2 edge %3 | %4").arg(dimension.id,dimension.sourceBodyId).arg(dimension.edgeIndex).arg(dimension.view));
    bool accepted=false;
    const QString choice=QInputDialog::getItem(this,"Remove drawing edge dimension","Saved dimension:",labels,0,false,&accepted);
    if(!accepted)return;
    const int index=labels.indexOf(choice);if(index<0)return;
    cad::DrawingSheetSettings settings=part->drawingSettings;
    settings.edgeDimensionAnnotations.removeAt(index);
    QString error;
    if(!document_.setDrawingSheetSettings(ids.first(),settings,&error)){reportError("Remove drawing edge dimension",error);return;}
    refresh();statusBar()->showMessage("Selected-edge dimension removed from drawing sheet.",8000);
}
void MainWindow::editDrawingGdtFrames(){
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Geometric tolerance frames","Select exactly one drawing-sheet master body.");return;}
    const cad::Part* part=document_.find(ids.first());
    if(!part){reportError("Geometric tolerance frames","Selected body is absent.");return;}
    cad::DrawingSheetSettings settings=part->hasDrawingSettings?part->drawingSettings:cad::DrawingSheetSettings{};
    if(settings.sourceBodyIds.isEmpty())settings.sourceBodyIds.append(part->id);
    const int existingCount=settings.gdtFrames.size();
    QStringList actions={"Add a manual geometric tolerance frame"};
    for(const auto& frame:settings.gdtFrames)
        actions.append(QStringLiteral("Edit %1 (%2)").arg(frame.id,frame.characteristic));
    for(const auto& frame:settings.gdtFrames)
        actions.append(QStringLiteral("Remove %1 (%2)").arg(frame.id,frame.characteristic));
    bool accepted=false;
    const QString action=QInputDialog::getItem(this,"Geometric tolerance annotations",
        "Choose an annotation action:",actions,0,false,&accepted);
    if(!accepted)return;
    const int actionIndex=actions.indexOf(action);
    if(actionIndex<0)return;
    if(actionIndex>existingCount){
        const int removeIndex=actionIndex-existingCount-1;
        if(removeIndex<0||removeIndex>=existingCount)return;
        settings.gdtFrames.removeAt(removeIndex);
        QString error;
        if(!document_.setDrawingSheetSettings(part->id,settings,&error)){
            reportError("Remove geometric tolerance frame",error);return;
        }
        refresh();statusBar()->showMessage("Manual geometric tolerance frame removed.",6000);return;
    }
    const bool editing=actionIndex>0;
    const int editIndex=actionIndex-1;
    if(!editing&&existingCount>=8){reportError("Geometric tolerance frames","Maximum of eight per drawing.");return;}
    const cad::DrawingGdtFrameAnnotation previous=editing?settings.gdtFrames[editIndex]:
        cad::DrawingGdtFrameAnnotation{};
    QDialog dialog(this);dialog.setWindowTitle(editing?
        "Edit manual geometric tolerance frame":"Author manual geometric tolerance frame");
    auto* layout=new QVBoxLayout(&dialog);
    auto* info=new QLabel("This is a user-authored geometric-tolerance annotation, not a compliant ISO 1101 / ASME Y14.5 checker. "
        "It is tied to a source BODY revision. Editing and saving an older frame explicitly re-authors it against current geometry; regenerate the drawing before issue.",&dialog);
    info->setWordWrap(true);layout->addWidget(info);
    auto* form=new QFormLayout;
    auto* source=new QComboBox(&dialog);
    for(const QString& bodyId:settings.sourceBodyIds){
        const cad::Part* b=document_.find(bodyId);
        if(b)source->addItem(b->name+QStringLiteral(" (")+b->id.left(8)+QStringLiteral(")"),bodyId);
    }
    if(source->count()==0){reportError("Geometric tolerance frames","Source bodies are missing.");return;}
    if(editing){
        const int selected=source->findData(previous.sourceBodyId);
        if(selected>=0)source->setCurrentIndex(selected);
    }
    form->addRow("Source body",source);
    auto* kind=new QComboBox(&dialog);
    kind->addItems({"POSITION","FLATNESS","PARALLELISM","PERPENDICULARITY","PROFILE","CIRCULARITY"});
    if(editing)kind->setCurrentText(previous.characteristic);
    form->addRow("Characteristic",kind);
    auto* tolerance=new QDoubleSpinBox(&dialog);
    tolerance->setDecimals(4);tolerance->setRange(0.0001,1000);
    tolerance->setValue(editing?previous.toleranceMm:0.1);
    tolerance->setSuffix(" mm");form->addRow("Specified tolerance",tolerance);
    auto* primary=new QLineEdit(&dialog);primary->setMaxLength(1);
    auto* secondary=new QLineEdit(&dialog);secondary->setMaxLength(1);
    auto* tertiary=new QLineEdit(&dialog);tertiary->setMaxLength(1);
    if(editing){
        primary->setText(previous.datumA);secondary->setText(previous.datumB);
        tertiary->setText(previous.datumC);
    }
    form->addRow("Primary datum (A-Z, optional)",primary);
    form->addRow("Secondary datum (A-Z, optional)",secondary);
    form->addRow("Tertiary datum (A-Z, optional)",tertiary);
    const bool a3=settings.sheetSize==QStringLiteral("A3");
    auto* x=new QDoubleSpinBox(&dialog);x->setRange(10,a3?345:222);x->setValue(12);x->setSuffix(" mm");
    auto* y=new QDoubleSpinBox(&dialog);y->setRange(20,a3?285:198);y->setValue(155);y->setSuffix(" mm");
    if(editing){x->setValue(previous.xMm);y->setValue(previous.yMm);}
    form->addRow("Paper X",x);form->addRow("Paper Y",y);
    layout->addLayout(form);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    layout->addWidget(buttons);
    if(dialog.exec()!=QDialog::Accepted)return;
    const cad::Part* body=document_.find(source->currentData().toString());
    if(!body){reportError("Geometric tolerance frames","Selected source body no longer exists.");return;}
    if(editing&&previous.sourceGeometryToken!=body->geometryToken){
        const auto decision=QMessageBox::question(this,"Re-author tolerance after geometry change",
            "The geometry revision linked to this frame has changed. Saving now re-authors this manual tolerance frame against the current body revision. "
            "This does not establish GD&T compliance or regenerate the drawing. Confirm only after reviewing the updated geometry.",
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No);
        if(decision!=QMessageBox::Yes)return;
    }
    cad::DrawingGdtFrameAnnotation frame=editing?previous:cad::DrawingGdtFrameAnnotation{};
    if(!editing)frame.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    frame.sourceBodyId=body->id;frame.sourceGeometryToken=body->geometryToken;
    frame.characteristic=kind->currentText();frame.toleranceMm=tolerance->value();
    frame.datumA=primary->text().trimmed().toUpper();
    frame.datumB=secondary->text().trimmed().toUpper();
    frame.datumC=tertiary->text().trimmed().toUpper();
    frame.xMm=x->value();frame.yMm=y->value();
    if(editing)settings.gdtFrames[editIndex]=frame;
    else settings.gdtFrames.append(frame);
    QString error;
    if(!document_.setDrawingSheetSettings(part->id,settings,&error)){
        reportError("Save geometric tolerance frame",error);return;
    }
    refresh();statusBar()->showMessage(editing?
        "Edited tolerance frame saved with stable identity and current source-body revision.":
        "Manual geometric tolerance frame saved with source-body revision.",8000);
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
    QString error;
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
    // Retain source validation, but do not acknowledge changed geometry
    // simply because BOM balloon positions were edited.
    settings.balloonAnnotations=balloons;settings.sourceBodyIds.removeDuplicates();std::sort(settings.sourceBodyIds.begin(),settings.sourceBodyIds.end());
    QVector<cad::Part> sources;for(const QString& sourceId:settings.sourceBodyIds){const cad::Part* source=document_.find(sourceId);if(source)sources.append(*source);}
    cad::Part composite;QString error;if(!cad::buildDrawingAssemblyPart(sources,&composite,&error)){reportError("Save drawing BOM balloons",error);return;}
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
                QString("Current: the saved drawing links to %1 existing source body/bodies.\n\nA new export will use current geometry and parts-list data.").arg(status.sourceBodyIds.size()));return;
        case cad::DrawingSourceState::Stale:
            QMessageBox::warning(this,"Saved drawing source",
                "Stale: source geometry or BOM/material data changed since the saved sheet revision. Export the drawing again to refresh both revisions.");return;
        case cad::DrawingSourceState::MissingBodies:
            QMessageBox::warning(this,"Saved drawing source",
                QString("Missing source body/bodies: %1\n\nSelect the intended existing source bodies and export again to replace the saved source set.").arg(status.missingSourceBodyIds.join(", ")));return;
    }
}
void MainWindow::refreshSavedDrawingSource() {
    // A revision token is evidence that the corresponding drawing was issued.
    // Do NOT acknowledge a source revision without first writing the updated
    // drawing. The old "refresh link only" command could incorrectly mark a
    // stale sheet as current while the published SVG was still out of date.
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Regenerate saved drawing","Select exactly one body that owns a saved drawing sheet.");return;}
    const QString masterId=ids.first();
    const cad::Part* master=document_.find(masterId);
    if(!master||!master->hasDrawingSettings){
        reportError("Regenerate saved drawing","This body has no saved sheet settings. Export a drawing first.");return;
    }
    cad::DrawingSheetSettings updated=master->drawingSettings;
    QStringList sourceIds=updated.sourceBodyIds;
    if(sourceIds.isEmpty())sourceIds.append(masterId);
    if(sourceIds.size()>50){reportError("Regenerate saved drawing","The saved sheet has too many source bodies.");return;}
    sourceIds.removeDuplicates();
    if(sourceIds.size()!=updated.sourceBodyIds.size()&&!updated.sourceBodyIds.isEmpty()){
        reportError("Regenerate saved drawing","The saved sheet has duplicate source body IDs.");return;
    }
    std::sort(sourceIds.begin(),sourceIds.end());
    QVector<cad::Part> sources;sources.reserve(sourceIds.size());
    for(const QString& sourceId:sourceIds){
        const cad::Part* source=document_.find(sourceId);
        if(!source){
            reportError("Regenerate saved drawing",QString("Missing saved drawing source: %1. Restore the body before regenerating.").arg(sourceId));
            return;
        }
        sources.append(*source);
    }
    QString error;
    cad::Part composite;
    if(!cad::buildDrawingAssemblyPart(sources,&composite,&error)){
        reportError("Regenerate saved drawing",error);return;
    }
    updated.sourceBodyIds=sourceIds;
    updated.sourceGeometryToken=composite.geometryToken;
    updated.sourceDataToken=document_.drawingSourceDataToken(sourceIds,&error);
    if(!error.isEmpty()){reportError("Regenerate saved drawing",error);return;}
    cad::DrawingSheetSettings prepared;
    if(!document_.prepareDrawingSheetSettings(masterId,updated,&prepared,&error)){
        reportError("Regenerate saved drawing",error);return;
    }
    const auto projection=prepared.projection==QStringLiteral("First-angle")
        ?cad::OrthographicProjection::FirstAngle:cad::OrthographicProjection::ThirdAngle;
    const auto size=prepared.sheetSize==QStringLiteral("A3")
        ?cad::DrawingSheetSize::A3Landscape:cad::DrawingSheetSize::A4Landscape;
    const auto auxiliary=prepared.auxiliaryView==QStringLiteral("Section A-A")
        ?cad::DrawingAuxiliaryView::SectionA:cad::DrawingAuxiliaryView::Isometric;
    const cad::DrawingTitleBlock title{prepared.drawingNumber,prepared.drawnBy,
        prepared.checkedBy,prepared.issueDate,prepared.revision};
    const auto bom=document_.billOfMaterials(sourceIds,&error);
    if(!error.isEmpty()){reportError("Regenerate saved drawing",error);return;}
    // A drawing revision must first be visually inspected against the CURRENT
    // model. Rendering is read-only, and cancelling does not acknowledge the
    // updated geometry/source-data tokens or modify the previously issued SVG.
    QByteArray stagedSvg;
    if(!cad::buildOrthographicDrawingSvg(composite,&stagedSvg,&error,nullptr,nullptr,
        nullptr,projection,size,auxiliary,prepared.sectionDepthFraction,
        prepared.generalNotes,title,prepared.viewCenters,prepared.viewIds,
        prepared.sheetId,prepared.noteAnnotations,prepared.dimensionAnnotations,
        prepared.partsListAnnotation,bom,sourceIds,prepared.balloonAnnotations,
        prepared.detailAnnotations,prepared.sectionAnnotations,
        prepared.edgeDimensionAnnotations,sources,prepared.gdtFrames)){
        reportError("Regenerate saved drawing preview",error);return;
    }
    QSvgRenderer drawingRenderer(stagedSvg);
    if(!drawingRenderer.isValid()){
        reportError("Regenerate saved drawing preview","The regenerated SVG could not be rendered for review.");return;
    }
    const QSize imageSize=size==cad::DrawingSheetSize::A3Landscape
        ?QSize(1260,891):QSize(1188,840);
    QImage drawingImage(imageSize,QImage::Format_ARGB32_Premultiplied);
    drawingImage.fill(Qt::white);
    {
        QPainter painter(&drawingImage);
        drawingRenderer.render(&painter);
        painter.end();
    }
    QDialog review(this);
    review.setWindowTitle("Review regenerated drawing before updating source revision");
    review.resize(980,740);
    auto* reviewLayout=new QVBoxLayout(&review);
    auto* description=new QLabel(
        "Read-only preview of the CURRENT model and saved annotations. "
        "Check views, dimensions, datums and source references. "
        "Only a successful export will acknowledge the new revision.",&review);
    description->setWordWrap(true);reviewLayout->addWidget(description);
    auto* imageLabel=new QLabel(&review);
    imageLabel->setPixmap(QPixmap::fromImage(drawingImage));
    imageLabel->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    auto* imageScroll=new QScrollArea(&review);
    imageScroll->setWidget(imageLabel);imageScroll->setWidgetResizable(false);
    reviewLayout->addWidget(imageScroll);
    auto* reviewButtons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&review);
    reviewButtons->button(QDialogButtonBox::Ok)->setText("Export reviewed revision...");
    reviewLayout->addWidget(reviewButtons);
    connect(reviewButtons,&QDialogButtonBox::accepted,&review,&QDialog::accept);
    connect(reviewButtons,&QDialogButtonBox::rejected,&review,&QDialog::reject);
    if(review.exec()!=QDialog::Accepted)return;
    const QString path=QFileDialog::getSaveFileName(this,"Regenerate saved drawing from current model",{},"SVG drawings (*.svg)");
    if(path.isEmpty())return;
    // QSaveFile in the export function makes file replacement atomic. Keep
    // stored source revisions untouched if HLR, validation or writing fails.
    if(!cad::exportOrthographicDrawingSvg(composite,path,&error,projection,size,
        auxiliary,prepared.sectionDepthFraction,prepared.generalNotes,title,
        prepared.viewCenters,prepared.viewIds,prepared.sheetId,
        prepared.noteAnnotations,prepared.dimensionAnnotations,
        prepared.partsListAnnotation,bom,sourceIds,prepared.balloonAnnotations,
        prepared.detailAnnotations,prepared.sectionAnnotations,
        prepared.edgeDimensionAnnotations,sources,prepared.gdtFrames)){
        reportError("Regenerate saved drawing",error);return;
    }
    if(!document_.setDrawingSheetSettings(masterId,prepared,&error)){
        reportError("Save regenerated drawing revision",error);return;
    }
    refresh();
    statusBar()->showMessage(QString("Saved drawing regenerated with current source revision: %1")
        .arg(QFileInfo(path).fileName()),8000);
}
void MainWindow::exportDrawingSvg() { exportDrawing(false); }
void MainWindow::exportDrawingPdf() { exportDrawing(true); }
void MainWindow::exportDrawingDxf() { exportDrawing(false,true); }
void MainWindow::exportDrawing(bool asPdf,bool asDxf) {
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
    settings.sourceDataToken=document_.drawingSourceDataToken(sourceBodyIds,&error);
    if(!error.isEmpty()){reportError("Prepare drawing source",error);return;}
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
    const QString path=QFileDialog::getSaveFileName(this,asPdf?"Export orthographic drawing PDF":
        (asDxf?"Export orthographic drawing DXF":"Export orthographic drawing SVG"),{},
        asPdf?"PDF drawing sheets (*.pdf)":(asDxf?"DXF drawings (*.dxf)":"SVG drawings (*.svg)"));
    if(path.isEmpty())return;
    const QVector<cad::BillOfMaterialsRow> bomRows=document_.billOfMaterials(sourceBodyIds,&error);
    if(!error.isEmpty()){reportError("Export orthographic drawing",error);return;}
    // Prepare stable IDs and validate the sheet without mutating the document.
    // A failed render/write must never undo a prior, unrelated modeling action.
    cad::DrawingSheetSettings prepared;
    if(!document_.prepareDrawingSheetSettings(masterId,settings,&prepared,&error)){
        reportError("Prepare drawing setup",error);return;
    }
    bool exported=false;
    if(asPdf||asDxf){
        QByteArray svg;
        if(cad::buildOrthographicDrawingSvg(drawingPart,&svg,&error,nullptr,nullptr,nullptr,projection,sheetSize,auxiliaryView,
                sectionDepthFraction,generalNotes,titleBlock,prepared.viewCenters,prepared.viewIds,
                prepared.sheetId,prepared.noteAnnotations,prepared.dimensionAnnotations,
                prepared.partsListAnnotation,bomRows,sourceBodyIds,prepared.balloonAnnotations,
                prepared.detailAnnotations,prepared.sectionAnnotations,prepared.edgeDimensionAnnotations,sourceBodies,prepared.gdtFrames))
            exported=asPdf?cad::exportOrthographicDrawingPdf(svg,path,sheetSize,&error):
                cad::exportOrthographicDrawingDxf(svg,path,&error);
    }else{
        exported=cad::exportOrthographicDrawingSvg(drawingPart,path,&error,projection,sheetSize,auxiliaryView,sectionDepthFraction,generalNotes,titleBlock,
                prepared.viewCenters,prepared.viewIds,prepared.sheetId,
                prepared.noteAnnotations,prepared.dimensionAnnotations,
                prepared.partsListAnnotation,bomRows,sourceBodyIds,prepared.balloonAnnotations,
                prepared.detailAnnotations,prepared.sectionAnnotations,prepared.edgeDimensionAnnotations,sourceBodies,prepared.gdtFrames);
    }
    if(!exported){
        reportError("Export orthographic drawing",error);return;
    }
    // Only a successful snapshot export acknowledges the current geometry revision.
    // Reusing prepared IDs guarantees that the file and saved sheet refer to the same views.
    if(!document_.setDrawingSheetSettings(masterId,prepared,&error)){
        reportError("Save exported drawing setup",error);return;
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
void MainWindow::createFoldedSheetMetal() {
    QDialog dialog(this);dialog.setWindowTitle("Folded sheet-metal body");
    auto* form=new QFormLayout(&dialog);
    auto* flangeInput=new QLineEdit("40,30",&dialog);
    auto* angleInput=new QLineEdit("90",&dialog);
    auto* radiusInput=new QLineEdit("1.5",&dialog);
    auto* width=new QDoubleSpinBox(&dialog);width->setRange(0.00001,1000000);width->setDecimals(3);width->setValue(25);width->setSuffix(" mm");
    auto* thickness=new QDoubleSpinBox(&dialog);thickness->setRange(0.00001,1000000);thickness->setDecimals(3);thickness->setValue(1.5);thickness->setSuffix(" mm");
    form->addRow("Straight neutral-surface flange lengths, in order (mm):",flangeInput);
    form->addRow("Signed fold angles, in order (degrees):",angleInput);
    form->addRow("Inside radii per bend, in order (mm):",radiusInput);
    form->addRow("Sheet width (mm):",width);form->addRow("Thickness (mm):",thickness);
    auto* note=new QLabel("Positive and negative angles fold in opposite directions. Flange values are straight neutral-surface segment lengths; the circular neutral bend arcs are added between them. Each bend can use its own positive inside radius. The result is a faceted solid; verify developed lengths against material and tooling data.",&dialog);
    note->setWordWrap(true);form->addRow(note);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QVector<double> flanges,angles,radii;QString error;
    if(!cad::parseSheetMetalNumberList(flangeInput->text(),&flanges,&error)||
       !cad::parseSheetMetalNumberList(angleInput->text(),&angles,&error)||flanges.size()!=angles.size()+1){
        if(error.isEmpty())error="Enter exactly one more straight flange length than signed fold angles.";
        reportError("Folded sheet-metal body",error);return;
    }
    if(!cad::parseSheetMetalNumberList(radiusInput->text(),&radii,&error)||radii.size()!=angles.size()){
        if(error.isEmpty())error="Enter exactly one positive inside radius for each fold angle.";
        reportError("Folded sheet-metal body",error);return;
    }
    QVector<cad::FoldedSheetBend> bends;bends.reserve(angles.size());
    for(int i=0;i<angles.size();++i)bends.append({angles[i],radii[i]});
    if(!document_.addFoldedSheetMetal(flanges,width->value(),thickness->value(),bends,&error)){
        reportError("Folded sheet-metal body",error);return;
    }
    refresh(true);statusBar()->showMessage("Folded sheet-metal body added to the scene.",6000);
}
void MainWindow::createLinkedFoldedFlatBlank(){
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Linked sheet blank","Select exactly one saved folded body.");return;}
    QString error;
    if(!document_.addLinkedFoldedFlatBlank(ids.first(),&error)){
        reportError("Linked sheet blank",error);return;
    }
    refresh(true);
    statusBar()->showMessage("Associative neutral flat blank created; edits update it atomically.",8000);
}

void MainWindow::editLinkedFoldedFlatDevelopment(){
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Edit linked development","Select exactly one associated flat blank.");return;}
    const cad::Part* blank=document_.find(ids.first());
    if(!blank||blank->linkedFoldedFlatSourceId.isEmpty()){
        reportError("Edit linked development","The selected body is not an associated folded flat blank.");return;
    }
    const cad::Part* source=document_.find(blank->linkedFoldedFlatSourceId);
    if(!source||!source->hasFoldedSheetMetalDefinition||
       source->geometryToken!=blank->linkedFoldedFlatSourceToken){
        reportError("Edit linked development","The folded source is missing or out of date.");return;
    }
    QDialog dialog(this);dialog.setWindowTitle("Edit linked flat development");
    auto* form=new QFormLayout(&dialog);
    auto* factors=new QLineEdit(&dialog);
    QVector<double> initial=blank->linkedFoldedFlatKFactors.isEmpty()?
        QVector<double>(source->foldedSheetBends.size(),0.33):blank->linkedFoldedFlatKFactors;
    QStringList factorText;
    for(double v:initial)factorText<<QString::number(v,'g',12);
    factors->setText(factorText.join(", "));
    form->addRow("K-factor for each bend (0–1)",factors);
    auto* calibrate=new QPushButton("Calculate K-factors from measured shop coupons...",&dialog);
    calibrate->setToolTip("Uses one-bend coupon blank lengths and tangent-to-tangent straight lengths; never invents extrapolated material values.");
    form->addRow(calibrate);
    connect(calibrate,&QPushButton::clicked,&dialog,[this,source,factors,&dialog](){
        QDialog measure(&dialog);measure.setWindowTitle("Shop bend-coupon calibration");
        auto* layout=new QVBoxLayout(&measure);
        auto* info=new QLabel("One measured single-bend coupon per line, comma-separated:\n"
          "thickness mm, inside radius mm, absolute angle degrees, total straight tangent lengths mm, measured flat blank mm.\n"
          "For each target bend, matching coupons must be within 15 degrees. R/t extrapolation is rejected. "
          "Only the K-factor editor is prefilled; no CAD modification occurs until you confirm the outer edit. "
          "Use material and tooling from the same verified shop process.",&measure);
        info->setWordWrap(true);layout->addWidget(info);
        auto* rows=new QPlainTextEdit(&measure);
        rows->setPlaceholderText("1.5, 1.5, 90, 80, 82.35\n1.5, 3.0, 90, 80, 84.80");
        rows->setMinimumSize(520,130);layout->addWidget(rows);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&measure);
        layout->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::accepted,&measure,&QDialog::accept);
        connect(buttons,&QDialogButtonBox::rejected,&measure,&QDialog::reject);
        if(measure.exec()!=QDialog::Accepted)return;
        std::vector<cad::sheet::MeasuredBendCoupon> coupons;
        const auto lines=rows->toPlainText().split('\n');
        for(const QString& line:lines){
            if(line.trimmed().isEmpty())continue;
            const auto fields=line.split(',',Qt::KeepEmptyParts);
            if(fields.size()!=5){reportError("Bend calibration","Each coupon must have exactly five numeric columns.");return;}
            double values[5];
            for(int i=0;i<5;++i){
                bool ok=false;values[i]=fields.at(i).trimmed().toDouble(&ok);
                if(!ok||!std::isfinite(values[i])){
                    reportError("Bend calibration","Every coupon entry must be a finite number.");return;
                }
            }
            coupons.push_back({values[0],values[1],values[2],values[3],values[4]});
        }
        std::vector<cad::sheet::BendCouponFactor> measured;
        std::vector<cad::sheet::BendCalibrationTarget> targets;
        for(const auto& bend:source->foldedSheetBends)
            targets.push_back({source->foldedSheetThicknessMm,bend.insideRadiusMm,std::abs(bend.angleDegrees)});
        std::vector<cad::sheet::CalibratedBendFactor> plan;
        std::string reason;
        if(!cad::sheet::inferBendCouponFactors(coupons,&measured,&reason)||
           !cad::sheet::interpolateCouponKFactors(measured,targets,&plan,&reason)){
            reportError("Bend calibration",QString::fromStdString(reason));return;
        }
        QStringList values;
        for(const auto& bend:plan)values<<QString::number(bend.kFactor,'g',12);
        factors->setText(values.join(", "));
        QMessageBox::information(&dialog,"Shop calibration proposal",
           QStringLiteral("%1 measured coupons were used to prefill %2 bend K-factors.\n"
             "This does not qualify material, thickness, grain direction, tools, bend order, or springback. "
             "Review the values and confirm the main edit to apply them.")
            .arg(coupons.size()).arg(plan.size()));
    });
    auto* reliefWidth=new QDoubleSpinBox(&dialog);
    reliefWidth->setRange(0.0,100000.0);reliefWidth->setDecimals(6);
    reliefWidth->setValue(blank->linkedFoldedFlatRelief.widthMm);
    form->addRow("Relief width (mm, 0=none)",reliefWidth);
    auto* reliefDepth=new QDoubleSpinBox(&dialog);
    reliefDepth->setRange(0.0,100000.0);reliefDepth->setDecimals(6);
    reliefDepth->setValue(blank->linkedFoldedFlatRelief.depthMm);
    form->addRow("Relief depth (mm, 0=none)",reliefDepth);
    form->addRow(new QLabel("Rectangular end reliefs are cut at each bend station; this is not a validated forming toolpath.",&dialog));
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QVector<double> requested;QString issue;
    if(!cad::parseSheetMetalNumberList(factors->text(),&requested,&issue)||
       requested.size()!=source->foldedSheetBends.size()){
        reportError("Edit linked development","Enter exactly one valid K-factor per saved bend. "+issue);return;
    }
    const cad::SheetMetalRelief relief{reliefWidth->value(),reliefDepth->value()};
    if(!document_.editLinkedFoldedFlatDevelopment(blank->id,requested,relief,&issue)){
        reportError("Edit linked development",issue);return;
    }
    refresh(true);
    statusBar()->showMessage("Linked developed blank rebuilt with bend reliefs.",8000);
}

void MainWindow::editFoldedSheetMetal() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Edit folded sheet-metal body","Select exactly one folded sheet-metal body.");return;}
    QVector<double> flanges;double sheetWidth=0.0,thicknessValue=0.0;QVector<cad::FoldedSheetBend> bends;bool current=false;
    if(!document_.foldedSheetMetalDefinition(ids.first(),&flanges,&sheetWidth,&thicknessValue,&bends,&current)){
        reportError("Edit folded sheet-metal body","The selected body has no editable folded sheet-metal definition.");return;
    }
    if(!current){reportError("Edit folded sheet-metal body","This definition is stale because the body was changed by another modeling operation. Recreate the folded body to edit its bend inputs.");return;}
    QStringList flangeText,angleText,radiusText;
    for(double value:flanges)flangeText.append(QString::number(value,'g',12));
    for(const auto& bend:bends){angleText.append(QString::number(bend.angleDegrees,'g',12));radiusText.append(QString::number(bend.insideRadiusMm,'g',12));}
    QDialog dialog(this);dialog.setWindowTitle("Edit folded sheet-metal body");auto* form=new QFormLayout(&dialog);
    auto* flangeInput=new QLineEdit(flangeText.join(','),&dialog);auto* angleInput=new QLineEdit(angleText.join(','),&dialog);
    auto* radiusInput=new QLineEdit(radiusText.join(','),&dialog);
    auto* width=new QDoubleSpinBox(&dialog);width->setRange(0.00001,1000000);width->setDecimals(3);width->setValue(sheetWidth);width->setSuffix(" mm");
    auto* thickness=new QDoubleSpinBox(&dialog);thickness->setRange(0.00001,1000000);thickness->setDecimals(3);thickness->setValue(thicknessValue);thickness->setSuffix(" mm");
    form->addRow("Straight neutral-surface flange lengths, in order (mm):",flangeInput);
    form->addRow("Signed fold angles, in order (degrees):",angleInput);form->addRow("Inside radii per bend, in order (mm):",radiusInput);
    form->addRow("Sheet width (mm):",width);form->addRow("Thickness (mm):",thickness);
    auto* note=new QLabel("Editing rebuilds the selected solid as one undoable operation. Direct modeling changes make this definition read-only to avoid overwriting newer geometry.",&dialog);
    note->setWordWrap(true);form->addRow(note);auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QVector<double> angles,radii;QString error;
    if(!cad::parseSheetMetalNumberList(flangeInput->text(),&flanges,&error)||
       !cad::parseSheetMetalNumberList(angleInput->text(),&angles,&error)||flanges.size()!=angles.size()+1){
        if(error.isEmpty())error="Enter exactly one more straight flange length than fold angles.";
        reportError("Edit folded sheet-metal body",error);return;
    }
    if(!cad::parseSheetMetalNumberList(radiusInput->text(),&radii,&error)||radii.size()!=angles.size()){
        if(error.isEmpty())error="Enter exactly one positive inside radius for each fold angle.";
        reportError("Edit folded sheet-metal body",error);return;
    }
    const int oldBendCount=bends.size();
    bends.clear();for(int i=0;i<angles.size();++i)bends.append({angles[i],radii[i]});
    QMap<QString,QVector<double>> newDevelopments;
    if(oldBendCount!=bends.size()){
        // Adding/removing a flange changes the number of physical bend
        // stations. A saved per-bend K-factor list cannot be silently reused
        // at the wrong positions. Obtain explicit confirmation for every
        // linked flat blank and stage all edits with the folded source.
        QVector<const cad::Part*> linked;
        for(const cad::Part& part:document_.parts())
            if(part.linkedFoldedFlatSourceId==ids.first())linked.append(&part);
        if(!linked.isEmpty()){
            QDialog development(this);
            development.setWindowTitle("Review linked flat developments after flange-count change");
            auto* reviewForm=new QFormLayout(&development);
            auto* explanation=new QLabel("The new flange count changes the number of bend stations. "
                "Confirm K-factors for each associated flat blank. New bends are suggested as K=0.33; "
                "existing factors are retained by their current bend order. "
                "All folded and flat bodies are rebuilt atomically.",&development);
            explanation->setWordWrap(true);reviewForm->addRow(explanation);
            QMap<QString,QLineEdit*> fields;
            for(const cad::Part* blank:linked){
                QVector<double> factors=blank->linkedFoldedFlatKFactors;
                if(factors.isEmpty())factors=QVector<double>(oldBendCount,0.33);
                factors.resize(bends.size());
                for(int k=oldBendCount;k<bends.size();++k)factors[k]=0.33;
                QStringList numbers;
                for(double factor:factors)numbers<<QString::number(factor,'g',12);
                auto* field=new QLineEdit(numbers.join(", "),&development);
                reviewForm->addRow(blank->name+" (K per new bend)",field);
                fields.insert(blank->id,field);
            }
            auto* confirm=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&development);
            reviewForm->addRow(confirm);
            connect(confirm,&QDialogButtonBox::accepted,&development,&QDialog::accept);
            connect(confirm,&QDialogButtonBox::rejected,&development,&QDialog::reject);
            if(development.exec()!=QDialog::Accepted)return;
            for(auto it=fields.cbegin();it!=fields.cend();++it){
                QVector<double> parsed;QString reason;
                if(!cad::parseSheetMetalNumberList(it.value()->text(),&parsed,&reason)||
                   parsed.size()!=bends.size()){
                    reportError("Linked blank bend factors","Provide exactly one K-factor for every proposed bend. "+reason);return;
                }
                newDevelopments.insert(it.key(),parsed);
            }
        }
    }
    double previewVolume=0,previewFlatLength=0;
    if(!document_.previewFoldedSheetMetalEdit(ids.first(),flanges,width->value(),
            thickness->value(),bends,&previewVolume,&previewFlatLength,&error)){
        reportError("Folded-sheet edit preflight",error);return;
    }
    const QString summary=QStringLiteral("The proposed geometry passed the OCCT solid check.\n\n"
        "Bends: %1\nMeasured solid volume: %2 mm³\n"
        "Approximate neutral flat length (K=0.33): %3 mm\n\n"
        "Commit this folded-body edit?\n\n"
        "This is not a manufacturing clearance, springback, or tooling certification.")
        .arg(bends.size()).arg(previewVolume,0,'g',12).arg(previewFlatLength,0,'g',12);
    if(QMessageBox::question(this,"Review folded-body change",summary,
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
    if(!document_.editFoldedSheetMetalWithDevelopments(ids.first(),flanges,width->value(),
            thickness->value(),bends,newDevelopments,&error)){
        reportError("Edit folded sheet-metal body",error);return;
    }
    refresh(true);statusBar()->showMessage("Folded sheet-metal body rebuilt.",6000);
}
bool MainWindow::saveFile(bool as) {
    QString path = document_.path();
    if (as || path.isEmpty()) path = QFileDialog::getSaveFileName(this,"Save project",path,"CADWorkbench (*.cadscene)");
    if (path.isEmpty()) return false;
    if (!path.endsWith(".cadscene",Qt::CaseInsensitive)) path += ".cadscene";
    QString error;
    if (!document_.saveAs(path,&error)) { reportError("Save",error); return false; }
    if(QSettings().value(QStringLiteral("history/automaticCheckpoint"),false).toBool()){
        QString revisionId,historyError;
        const QString label=QStringLiteral("Auto %1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        if(!cad::DocumentRevisionHistory::createIfChanged(path,label,QStringLiteral("Created automatically after saving the document."),
                                                           &revisionId,&historyError))
            QMessageBox::warning(this,"Automatic revision checkpoint",
                QStringLiteral("The scene was saved, but its automatic revision checkpoint could not be created.\n\n%1")
                    .arg(historyError));
    }
    rememberRecentProject(path);
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
    auto* minRadiusRatio=new QDoubleSpinBox(&dialog);minRadiusRatio->setRange(0.0,1000.0);minRadiusRatio->setDecimals(3);minRadiusRatio->setValue(0.0);
    auto* minStraight=new QDoubleSpinBox(&dialog);minStraight->setRange(0.0,1000000.0);minStraight->setDecimals(3);minStraight->setValue(0.0);minStraight->setSuffix(" mm");
    form->addRow("Shop minimum inside radius / thickness (0 disables):",minRadiusRatio);
    form->addRow("Shop minimum straight flange (0 disables):",minStraight);
    auto* explanation=new QLabel("Enter 2–50 bend stations and one more flange length than bends. Optional rectangular end reliefs are cut into the developed blank at each bend line. Folded solids and springback are not modeled.",&dialog);
    explanation->setWordWrap(true);form->addRow(explanation);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QVector<double> flangeValues,angleValues,radiusValues,kValues;
    if(!cad::parseSheetMetalNumberList(flanges->text(),&flangeValues)||
       !cad::parseSheetMetalNumberList(angles->text(),&angleValues)||
       !cad::parseSheetMetalNumberList(radii->text(),&radiusValues)||
       !cad::parseSheetMetalNumberList(kFactors->text(),&kValues)||
       angleValues.size()!=radiusValues.size()||angleValues.size()!=kValues.size()){
        reportError("Multi-bend flat pattern","Enter numeric comma-separated values, with matching angle, radius, and K-factor counts.");return;
    }
    QVector<cad::SheetMetalBend> bendValues;bendValues.reserve(angleValues.size());
    for(int i=0;i<angleValues.size();++i)bendValues.append({angleValues[i],radiusValues[i],kValues[i]});
    QString error;
    const cad::SheetMetalShopLimits shopLimits{minRadiusRatio->value(),minStraight->value()};
    if(!cad::validateMultiBendShopLimits(flangeValues,width->value(),thickness->value(),bendValues,shopLimits,&error)){
        reportError("Multi-bend shop-rule check",error);return;
    }
    const QString path=QFileDialog::getSaveFileName(this,"Export multi-bend flat pattern",{},"DXF files (*.dxf)");if(path.isEmpty())return;
    cad::MultiBendFlatPattern pattern;
    const cad::SheetMetalRelief relief{reliefWidth->value(),reliefDepth->value()};
    if(!cad::exportMultiBendFlatPatternDxf(flangeValues,width->value(),thickness->value(),bendValues,relief,path,&pattern,&error)){
        reportError("Export multi-bend flat pattern",error);return;
    }
    statusBar()->showMessage(QString("DXF exported: flat length %1 mm, %2 bend lines").arg(pattern.flatLengthMm,0,'f',3).arg(pattern.bendLinePositionsMm.size()),8000);
}
void MainWindow::auditSelectedFoldedSheetBendSequence(){
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Bend sequence audit","Select exactly one folded sheet-metal body.");return;}
    QVector<double> flanges;double width=0.0,thickness=0.0;
    QVector<cad::FoldedSheetBend> bends;bool current=false;
    if(!document_.foldedSheetMetalDefinition(ids.first(),&flanges,&width,&thickness,&bends,&current)||!current){
        reportError("Bend sequence audit","Select a folded body with a current, saved bend definition.");return;
    }
    QStringList defaults;for(int i=0;i<bends.size();++i)defaults<<"0.33";
    bool accepted=false;
    const QString text=QInputDialog::getText(this,"Folded-sheet bend sequence audit",
        "K-factor per bend (0-1, comma separated):",QLineEdit::Normal,defaults.join(','),&accepted);
    if(!accepted)return;
    QVector<double> k;QString error;
    if(!cad::parseSheetMetalNumberList(text,&k,&error)||k.size()!=bends.size()){
        reportError("Bend sequence audit",error.isEmpty()?QStringLiteral("Provide one K-factor per bend."):error);return;
    }
    std::vector<double> neutral;neutral.reserve(static_cast<std::size_t>(flanges.size()));
    for(double value:flanges)neutral.push_back(value);
    std::vector<cad::sheet::BendInput> inputs;inputs.reserve(static_cast<std::size_t>(bends.size()));
    for(int i=0;i<bends.size();++i)
        inputs.push_back({bends[i].angleDegrees,bends[i].insideRadiusMm,k[i]});
    cad::sheet::BendSequenceReport audit;std::string issue;
    if(!cad::sheet::inspectBendSequence(neutral,thickness,inputs,&audit,&issue)){
        reportError("Bend sequence audit",QString::fromStdString(issue));return;
    }
    cad::MultiBendFlatPattern reference;
    if(!cad::calculateFoldedSheetFlatPattern(flanges,width,thickness,bends,k,&reference,&error)){
        reportError("Bend sequence audit",error);return;
    }
    const double tolerance=std::max(1e-8,reference.flatLengthMm*1e-10);
    if(reference.bendLinePositionsMm.size()!=static_cast<qsizetype>(audit.stations.size())||
       std::abs(reference.flatLengthMm-audit.developedLengthMm)>tolerance){
        reportError("Bend sequence audit","Independent neutral-length audit disagrees with the DXF kernel.");return;
    }
    for(int i=0;i<reference.bendLinePositionsMm.size();++i)
        if(std::abs(reference.bendLinePositionsMm[i]-audit.stations[static_cast<std::size_t>(i)].stationMm)>tolerance){
            reportError("Bend sequence audit",QStringLiteral("Bend station %1 disagrees with the saved flat-pattern kernel.").arg(i+1));return;
        }
    QStringList lines;
    lines<<"FOLDED-SHEET BEND SEQUENCE REVIEW (READ ONLY)"
         <<QStringLiteral("Thickness: %1 mm, width: %2 mm").arg(thickness,0,'g',12).arg(width,0,'g',12)
         <<QStringLiteral("Developed neutral length: %1 mm").arg(audit.developedLengthMm,0,'g',12)
         <<QStringLiteral("Shortest straight flange: %1 mm").arg(audit.minimumStraightFlangeMm,0,'g',12)
         <<QStringLiteral("Reversals of bend direction: %1").arg(static_cast<int>(audit.reverseDirectionCount))
         <<QStringLiteral("Largest absolute cumulative turn: %1 degrees").arg(audit.maximumAbsoluteCumulativeDirectionDegrees,0,'g',12)
         <<"Bend | Sign | Angle deg | Cumulative deg | Allowance mm | Flat station mm | Tangent start mm | Tangent end mm | R/t";
    QByteArray csv="index,direction,angle_degrees,cumulative_angle_degrees,allowance_mm,station_mm,tangent_start_mm,tangent_end_mm,radius_over_thickness\n";
    const auto field=[](double v){return QString::number(v,'g',17);};
    for(const auto& entry:audit.stations){
        const QString direction=entry.signedDegrees>0?"UP":"DOWN";
        lines<<QStringLiteral("%1 | %2 | %3 | %4 | %5 | %6 | %7 | %8 | %9")
            .arg(static_cast<int>(entry.number)).arg(direction)
            .arg(entry.signedDegrees,0,'g',10)
            .arg(entry.cumulativeDirectionDegrees,0,'g',10)
            .arg(entry.neutralAllowanceMm,0,'g',10)
            .arg(entry.stationMm,0,'g',12)
            .arg(entry.flatTangentStartMm,0,'g',12)
            .arg(entry.flatTangentEndMm,0,'g',12)
            .arg(entry.insideRadiusToThickness,0,'g',8);
        csv+=(QString::number(entry.number)+","+direction+","+
              field(entry.signedDegrees)+","+field(entry.cumulativeDirectionDegrees)+","+
              field(entry.neutralAllowanceMm)+","+field(entry.stationMm)+","+
              field(entry.flatTangentStartMm)+","+field(entry.flatTangentEndMm)+","+
              field(entry.insideRadiusToThickness)+"\n").toUtf8();
    }
    lines<<QString()<<"Straight neutral flange intervals on the developed blank (mm):";
    for(const auto& flange:audit.flatFlanges)
        lines<<QStringLiteral("  Flange %1: %2 to %3 (straight %4)")
            .arg(static_cast<int>(flange.number))
            .arg(flange.startMm,0,'g',12).arg(flange.endMm,0,'g',12)
            .arg(flange.straightLengthMm,0,'g',12);
    lines<<QString()<<"The report agrees with the existing DXF flat-length/bend-station calculation."
         <<"It does NOT establish tooling access, springback, minimum flange feasibility or collisions."
         <<"A reverse-direction count is descriptive, not a validated manufacturing bend order.";
    QDialog dialog(this);dialog.setWindowTitle("Folded-sheet bend sequence audit");dialog.resize(870,510);
    auto* layout=new QVBoxLayout(&dialog);
    auto* report=new QPlainTextEdit(lines.join('\n'),&dialog);report->setReadOnly(true);
    layout->addWidget(report);
    auto* row=new QHBoxLayout;
    auto* exportCsv=new QPushButton("Export sequence CSV...",&dialog);
    row->addWidget(exportCsv);row->addStretch();
    auto* close=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);row->addWidget(close);
    connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(exportCsv,&QPushButton::clicked,&dialog,[&]{
        const QString path=QFileDialog::getSaveFileName(&dialog,"Save bend sequence audit",{},"CSV files (*.csv)");
        if(path.isEmpty())return;
        QSaveFile file(path);
        if(!file.open(QIODevice::WriteOnly)||file.write(csv)!=csv.size()||!file.commit()){
            file.cancelWriting();QMessageBox::warning(&dialog,"Export failed","Could not save complete CSV report.");return;
        }
        statusBar()->showMessage("Folded bend sequence report exported.",6000);
    });
    layout->addLayout(row);dialog.exec();
}

void MainWindow::checkSelectedFoldedSheetShopLimits() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Folded-sheet shop check","Select exactly one folded sheet-metal body.");return;}
    QVector<double> flanges;double width=0.0,thickness=0.0;QVector<cad::FoldedSheetBend> bends;bool current=false;
    if(!document_.foldedSheetMetalDefinition(ids.first(),&flanges,&width,&thickness,&bends,&current)||!current){
        reportError("Folded-sheet shop check","Select a folded body whose saved bend definition is current.");return;
    }
    QDialog dialog(this);dialog.setWindowTitle("Folded-sheet shop limits");
    auto* form=new QFormLayout(&dialog);
    auto* ratio=new QDoubleSpinBox(&dialog);ratio->setRange(0,1000);ratio->setDecimals(3);ratio->setSingleStep(0.1);
    auto* straight=new QDoubleSpinBox(&dialog);straight->setRange(0,1000000);straight->setDecimals(3);
    straight->setSuffix(" mm");
    form->addRow("Minimum inside radius / thickness (0 disables):",ratio);
    form->addRow("Minimum straight flange (0 disables):",straight);
    auto* bendStation=new QDoubleSpinBox(&dialog);
    bendStation->setRange(0,1000000);bendStation->setDecimals(3);
    bendStation->setSuffix(" mm");
    bendStation->setToolTip("Shop-selected minimum center-to-center distance between adjacent flat-pattern bend lines; 0 disables.");
    auto* kFactor=new QDoubleSpinBox(&dialog);
    kFactor->setRange(0,1);kFactor->setDecimals(4);kFactor->setSingleStep(0.01);
    kFactor->setValue(0.33);
    kFactor->setToolTip("Neutral-axis K-factor used only for the spacing preflight, not written to the model.");
    form->addRow("Minimum adjacent bend-line spacing (0 disables):",bendStation);
    form->addRow("K-factor for bend-line spacing:",kFactor);
    auto* reliefWidth=new QDoubleSpinBox(&dialog);reliefWidth->setRange(0,1000000);
    reliefWidth->setDecimals(3);reliefWidth->setSuffix(" mm");
    auto* reliefDepth=new QDoubleSpinBox(&dialog);reliefDepth->setRange(0,1000000);
    reliefDepth->setDecimals(3);reliefDepth->setSuffix(" mm");
    auto* reliefWidthRatio=new QDoubleSpinBox(&dialog);
    reliefWidthRatio->setRange(0,100);reliefWidthRatio->setDecimals(3);
    reliefWidthRatio->setValue(1.0);
    auto* reliefRadiusAllowance=new QDoubleSpinBox(&dialog);
    reliefRadiusAllowance->setRange(0,1000000);reliefRadiusAllowance->setDecimals(3);
    reliefRadiusAllowance->setSuffix(" mm");
    form->addRow("Proposed end-relief width (0 disables):",reliefWidth);
    form->addRow("Proposed end-relief depth (0 disables):",reliefDepth);
    form->addRow("Shop min width / sheet thickness:",reliefWidthRatio);
    form->addRow("Shop min depth beyond inside radius:",reliefRadiusAllowance);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);form->addRow(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    if(bendStation->value()>0&&bends.size()<2){
        reportError("Folded-sheet shop check","Adjacent bend-line spacing requires at least two bends.");return;
    }
    if((reliefWidth->value()==0.0)!=(reliefDepth->value()==0.0)){
        reportError("Folded-sheet shop check","Specify both proposed relief width and depth, or neither.");return;
    }
    if(ratio->value()==0.0&&straight->value()==0.0&&bendStation->value()==0.0&&reliefWidth->value()==0.0){
        reportError("Folded-sheet shop check","Enter at least one nonzero shop limit.");return;
    }
    QString error;
    if(!cad::validateFoldedSheetShopLimits(flanges,width,thickness,bends,
                                           {ratio->value(),straight->value()},&error)){
        reportError("Folded-sheet shop check",error);return;
    }
    QString spacingReport;
    if(bendStation->value()>0){
        std::vector<double> neutral(flanges.cbegin(),flanges.cend());
        std::vector<cad::sheet::BendInput> inputs;
        for(const auto& bend:bends)
            inputs.push_back({bend.angleDegrees,bend.insideRadiusMm,kFactor->value()});
        cad::sheet::BendSequenceReport development;
        std::string issue;
        if(!cad::sheet::inspectBendSequence(neutral,thickness,inputs,&development,&issue)){
            reportError("Folded-sheet shop check",QString::fromStdString(issue));return;
        }
        cad::sheet::BendStationSpacingCheck spacing;
        if(!cad::sheet::checkBendStationSpacing(development,bendStation->value(),&spacing,&issue)){
            reportError("Folded-sheet shop check",QString::fromStdString(issue));return;
        }
        if(!spacing.tooClosePairs.empty()){
            QStringList pairs;
            for(auto first:spacing.tooClosePairs)
                pairs<<QStringLiteral("%1–%2").arg(static_cast<int>(first)).arg(static_cast<int>(first+1));
            reportError("Folded-sheet shop check",
                QStringLiteral("Adjacent bend lines %1 are closer than the shop-selected %2 mm minimum (actual minimum %3 mm).")
                .arg(pairs.join(", ")).arg(spacing.requiredMinimumMm,0,'g',12)
                .arg(spacing.actualMinimumMm,0,'g',12));return;
        }
        spacingReport=QStringLiteral("\nBend-line spacing: minimum %1 mm, required %2 mm (K=%3).")
            .arg(spacing.actualMinimumMm,0,'g',12)
            .arg(spacing.requiredMinimumMm,0,'g',12).arg(kFactor->value(),0,'g',6);
    }
    QString reliefReport;
    if(reliefWidth->value()>0.0){
        std::vector<cad::sheet::BendInput> inputs;
        for(const auto& bend:bends)
            inputs.push_back({bend.angleDegrees,bend.insideRadiusMm,kFactor->value()});
        std::vector<cad::sheet::ReliefDecision> findings;
        std::string issue;
        if(!cad::sheet::reviewEndReliefs(inputs,thickness,width,reliefWidth->value(),
                reliefDepth->value(),{reliefWidthRatio->value(),reliefRadiusAllowance->value()},
                &findings,&issue)){
            reportError("Folded-sheet relief preflight",QString::fromStdString(issue));return;
        }
        QStringList failures;
        for(const auto& finding:findings){
            if(!finding.widthPass||!finding.depthPass)
                failures<<QStringLiteral("bend %1 (min width %2 mm, min depth %3 mm)")
                    .arg(static_cast<int>(finding.bendNumber))
                    .arg(finding.minimumWidthMm,0,'g',10)
                    .arg(finding.minimumDepthMm,0,'g',10);
        }
        if(!failures.isEmpty()){
            reportError("Folded-sheet relief preflight",
                "Proposed relief fails user-supplied shop policy at "+failures.join(", "));return;
        }
        reliefReport=QStringLiteral("\nProposed end relief meets the entered dimensional policy for %1 bends.")
            .arg(static_cast<int>(findings.size()));
    }
    QMessageBox::information(this,"Folded-sheet shop check",
        QStringLiteral("The folded body meets the selected project shop limits.")+
        spacingReport+reliefReport+QStringLiteral("\nThis is not a material, tooling, bend-order or collision certification. No relief cut was made."));
}
void MainWindow::exportSelectedAssociativeFlatPattern(){
    const auto ids=selectedIds();
    if(ids.size()!=1){
        reportError("Export linked development","Select one saved associative flat blank.");return;
    }
    const cad::Part* flat=document_.find(ids.first());
    if(!flat||flat->linkedFoldedFlatSourceId.isEmpty()){
        reportError("Export linked development","The selection is not a linked flat-blank body.");return;
    }
    const cad::Part* folded=document_.find(flat->linkedFoldedFlatSourceId);
    if(!folded||!folded->hasFoldedSheetMetalDefinition||
       folded->geometryToken!=folded->foldedSheetGeometryToken||
       flat->linkedFoldedFlatSourceToken!=folded->geometryToken){
        reportError("Export linked development",
            "The source is missing or stale. Rebuild the folded body and associated blank before exporting.");
        return;
    }
    // All driving inputs are taken from the committed source/blank pair.
    // Export never prompts for replacement bend/K-factor/relief values.
    QByteArray svg,dxf,bendCsv;
    QString reason;
    if(!document_.buildLinkedFlatDevelopmentSet(flat->id,&svg,&dxf,&bendCsv,&reason)){
        reportError("Export linked development",reason);return;
    }
    // The same model-derived source generates each output; no replacement
    // dimensions or independent preview math are accepted in this workflow.
    cad::MultiBendFlatPattern svgPattern;
    const QVector<double> factors=flat->linkedFoldedFlatKFactors.isEmpty()?
        QVector<double>(folded->foldedSheetBends.size(),0.33):flat->linkedFoldedFlatKFactors;
    // The source-neutral length is used only for the informational label.
    // SVG/DXF/CSV bytes were already generated and cross-checked together.
    if(!cad::calculateFoldedSheetFlatPattern(folded->foldedSheetFlangesMm,
           folded->foldedSheetWidthMm,folded->foldedSheetThicknessMm,
           folded->foldedSheetBends,factors,&svgPattern,&reason)){
        reportError("Export linked development",reason);return;
    }
    QSvgRenderer renderer(svg);
    if(!renderer.isValid()){
        reportError("Export linked development","Unable to render the linked development SVG.");return;
    }
    QImage image(1050,460,QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    {
        QPainter painter(&image);
        renderer.render(&painter,QRectF(10,10,1030,440));
    }
    QDialog review(this);review.setWindowTitle("Saved associative flat development review");
    auto* layout=new QVBoxLayout(&review);
    auto* explanation=new QLabel(QString("%1 bends; neutral length %2 mm. "
        "The displayed SVG and DXF use the saved source, K-factors and relief settings. "
        "This does not certify forming feasibility or tool clearance.")
        .arg(folded->foldedSheetBends.size())
        .arg(svgPattern.flatLengthMm,0,'g',12),&review);
    explanation->setWordWrap(true);layout->addWidget(explanation);
    auto* picture=new QLabel(&review);picture->setPixmap(QPixmap::fromImage(image));
    layout->addWidget(picture);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&review);
    layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&review,&QDialog::accept);
    connect(buttons,&QDialogButtonBox::rejected,&review,&QDialog::reject);
    if(review.exec()!=QDialog::Accepted)return;
    const QStringList formats={"SVG drawing","DXF flat pattern","CSV bend schedule"};
    bool accepted=false;
    const QString format=QInputDialog::getItem(this,"Export saved linked blank",
        "Export format",formats,1,false,&accepted);
    if(!accepted)return;
    const bool saveSvg=format==formats[0],saveDxf=format==formats[1];
    const QString filter=saveSvg?"SVG drawings (*.svg)":
                         saveDxf?"DXF flat patterns (*.dxf)":"CSV bend schedules (*.csv)";
    QString path=QFileDialog::getSaveFileName(this,"Export saved associative flat blank",{},filter);
    if(path.isEmpty())return;
    const QString suffix=saveSvg?".svg":saveDxf?".dxf":".csv";
    if(!path.endsWith(suffix,Qt::CaseInsensitive))path+=suffix;
    const QByteArray& data=saveSvg?svg:saveDxf?dxf:bendCsv;
    QSaveFile output(path);
    if(!output.open(QIODevice::WriteOnly)||output.write(data)!=data.size()||!output.commit()){
        output.cancelWriting();
        reportError("Export linked development","Could not atomically save the complete flat-pattern file.");
        return;
    }
    statusBar()->showMessage("Saved associative flat pattern exported without changing the CAD scene.",7500);
}

void MainWindow::exportSelectedAssociativeFlatPackage(){
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Manufacturing package","Select exactly one saved associative flat blank.");return;}
    const cad::Part* blank=document_.find(ids.first());
    if(!blank||blank->linkedFoldedFlatSourceId.isEmpty()){
        reportError("Manufacturing package","The selected body is not a linked flat blank.");return;
    }
    const QString parent=QFileDialog::getExistingDirectory(this,
        "Choose an existing parent folder for a NEW manufacturing package");
    if(parent.isEmpty())return;
    QString base=blank->name;
    for(qsizetype i=0;i<base.size();++i){
        const QChar ch=base.at(i);
        if(!ch.isLetterOrNumber()&&ch!=QLatin1Char('_')&&ch!=QLatin1Char('-'))
            base[i]=QLatin1Char('_');
    }
    if(base.isEmpty())base="flat-development";
    base=base.left(45)+"_manufacturing";
    QString path=QDir(parent).filePath(base);
    for(int i=2;QDir(path).exists()&&i<10000;++i)
        path=QDir(parent).filePath(base+QString("_%1").arg(i));
    QString issue;
    if(!document_.exportLinkedFlatManufacturingPackage(ids.first(),path,&issue)){
        reportError("Manufacturing package",issue);return;
    }
    QMessageBox::information(this,"Associative package exported",
        QString("Complete package created:\n%1\n\nFiles: flat-pattern.svg, "
            "flat-pattern.dxf, bend-schedule.csv and manifest.json. "
            "All exports use the current saved geometry and bend rules. "
            "The package does not certify manufacturing tooling or springback.").arg(path));
    statusBar()->showMessage("Saved linked flat manufacturing package with verified source hashes.",9000);
}

void MainWindow::exportSelectedAssociativeFlatJobBatch(){
    const QStringList ids=selectedIds();
    if(ids.size()<2||ids.size()>24){
        reportError("Sheet-metal manufacturing job","Select 2-24 associative flat blanks.");return;
    }
    for(const QString& id:ids){
        const cad::Part* part=document_.find(id);
        if(!part||part->linkedFoldedFlatSourceId.isEmpty()){
            reportError("Sheet-metal manufacturing job","All selections must be associative flat-blank bodies.");return;
        }
    }
    const QString parent=QFileDialog::getExistingDirectory(this,"Choose manufacturing job parent folder");
    if(parent.isEmpty())return;
    bool ok=false;
    const QString name=QInputDialog::getText(this,"New multi-part manufacturing job",
        "New job directory name",QLineEdit::Normal,"sheet_metal_job",&ok).trimmed();
    if(!ok||name.isEmpty())return;
    if(name=="."||name==".."||name.contains('/')||name.contains('\\')||name.contains(':')){
        reportError("Sheet-metal manufacturing job","Use a simple new directory name without path characters.");return;
    }
    const QString destination=QDir(parent).filePath(name);
    cad::StockLayoutSettings stock;
    stock.enabled=QMessageBox::question(this,"Stock-sheet layout planning",
        "Also create a stock-sheet placement CSV/SVG?\n"
        "Linked blanks with rectangular reliefs use their actual neutral contour; other shapes use their envelopes. "
        "This is not certified CNC nesting, kerf or grain planning.",
        QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes;
    if(stock.enabled){
        // One physical sheet cannot simultaneously have different gauges or
        // incompatible grades. The material grade may still be unassigned,
        // in which case the manufacturing manifest explicitly calls that out.
        stock.requireCompatibleGaugeAndMaterial=true;
        stock.stockWidthMm=QInputDialog::getDouble(this,"Stock width","Usable stock width (mm)",
            1500,1,100000,2,&ok);if(!ok)return;
        stock.stockHeightMm=QInputDialog::getDouble(this,"Stock height","Usable stock height (mm)",
            3000,1,100000,2,&ok);if(!ok)return;
        stock.gapMm=QInputDialog::getDouble(this,"Stock spacing",
            "Edge and inter-blank spacing (mm)",3,0,1000,2,&ok);if(!ok)return;
        stock.allowRotation=QMessageBox::question(this,"Blank orientation",
            "Permit 90 degree rotation of blank envelopes?\n"
            "Check material grain rules independently before cutting.",
            QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes;
    }
    QString issue;
    if(!document_.exportLinkedFlatManufacturingBatch(ids,destination,&issue,stock)){
        reportError("Sheet-metal manufacturing job",issue);return;
    }
    QMessageBox::information(this,"Manufacturing job exported",
        QString("Published %1 associative flat-development packages under:\n%2\n\n"
            "Each part has DXF, SVG, bend schedule and source-hash manifest. "
            "The job index records the entire batch. "
            "Optional stock-layout.svg/CSV use relief-aware contour planning where applicable. "
            "This does not certify manufacturability.")
            .arg(ids.size()).arg(destination));
    statusBar()->showMessage("Multi-part sheet-metal manufacturing job published atomically.",9000);
}

void MainWindow::exportSelectedFoldedSheetFlatPatternDxf() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Export folded-sheet flat pattern","Select exactly one folded sheet-metal body.");return;}
    QVector<double> flanges;double width=0.0,thickness=0.0;QVector<cad::FoldedSheetBend> bends;bool current=false;
    if(!document_.foldedSheetMetalDefinition(ids.first(),&flanges,&width,&thickness,&bends,&current)){
        reportError("Export folded-sheet flat pattern","The selected body has no folded sheet-metal definition.");return;
    }
    if(!current){reportError("Export folded-sheet flat pattern","The saved bend inputs are stale because the body was changed by another modeling operation.");return;}
    QStringList defaults;for(int i=0;i<bends.size();++i)defaults.append("0.33");
    bool accepted=false;const QString kText=QInputDialog::getText(this,"Folded-sheet DXF",
        "K-factor for each bend, in order (0–1, comma separated):",QLineEdit::Normal,defaults.join(','),&accepted);
    if(!accepted)return;
    QVector<double> kFactors;QString error;
    if(!cad::parseSheetMetalNumberList(kText,&kFactors,&error)||kFactors.size()!=bends.size()){
        reportError("Export folded-sheet flat pattern",error.isEmpty()?QStringLiteral("Enter one K-factor for each saved bend."):error);return;
    }
    const QString reliefText=QInputDialog::getText(this,"Folded-sheet bend relief",
        "End relief width and depth in mm (0,0 disables relief):",QLineEdit::Normal,"0,0",&accepted);
    if(!accepted)return;
    QVector<double> reliefValues;
    if(!cad::parseSheetMetalNumberList(reliefText,&reliefValues,&error)||reliefValues.size()!=2){
        reportError("Export folded-sheet flat pattern",error.isEmpty()?QStringLiteral("Enter relief width and depth as two comma-separated numbers."):error);return;
    }
    const cad::SheetMetalRelief relief{reliefValues[0],reliefValues[1]};
    QByteArray flatSvg;
    cad::MultiBendFlatPattern visualPattern;
    if(!cad::buildFoldedSheetFlatPatternSvg(flanges,width,thickness,bends,kFactors,relief,
            &flatSvg,&visualPattern,&error)){
        reportError("Folded-sheet flat pattern preview",error);return;
    }
    QSvgRenderer flatRenderer(flatSvg);
    if(!flatRenderer.isValid()){
        reportError("Folded-sheet flat pattern preview","Could not render the validated SVG preview.");return;
    }
    QImage flatImage(1024,480,QImage::Format_ARGB32);
    flatImage.fill(Qt::white);
    {
        QPainter painter(&flatImage);
        flatRenderer.render(&painter,QRectF(12,12,1000,456));
    }
    QDialog review(this);review.setWindowTitle("Review folded-sheet DXF before export");
    auto* reviewLayout=new QVBoxLayout(&review);
    auto* previewImage=new QLabel(&review);
    previewImage->setPixmap(QPixmap::fromImage(flatImage));
    reviewLayout->addWidget(previewImage);
    auto* summary=new QLabel(QString("Flat blank: %1 mm × %2 mm; %3 bends; per-bend K factors: %4\n"
        "Brown dashed lines indicate bend stations. Relief notches are shown at both ends. "
        "This is not a manufacturing or springback certification.")
        .arg(visualPattern.flatLengthMm,0,'f',4).arg(width,0,'f',4)
        .arg(bends.size()).arg(kText),&review);
    summary->setWordWrap(true);reviewLayout->addWidget(summary);
    auto* reviewButtons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&review);
    reviewButtons->button(QDialogButtonBox::Ok)->setText("Export DXF...");
    reviewLayout->addWidget(reviewButtons);
    connect(reviewButtons,&QDialogButtonBox::accepted,&review,&QDialog::accept);
    connect(reviewButtons,&QDialogButtonBox::rejected,&review,&QDialog::reject);
    if(review.exec()!=QDialog::Accepted)return;
    const QString path=QFileDialog::getSaveFileName(this,"Export folded-sheet flat pattern",{},"DXF files (*.dxf)");if(path.isEmpty())return;
    cad::MultiBendFlatPattern pattern;
    if(!cad::exportFoldedSheetFlatPatternDxf(flanges,width,thickness,bends,kFactors,relief,path,&pattern,&error)){
        reportError("Export folded-sheet flat pattern",error);return;
    }
    statusBar()->showMessage(QString("Folded-body DXF exported: %1 mm flat length, %2 bend lines; per-bend K values %3")
        .arg(pattern.flatLengthMm,0,'f',3).arg(pattern.bendLinePositionsMm.size()).arg(kText),10000);
}
void MainWindow::exportSelectedFoldedSheetBendTableCsv() {
    const QStringList ids=selectedIds();
    if(ids.size()!=1){reportError("Export folded-sheet bend table","Select exactly one folded sheet-metal body.");return;}
    QVector<double> flanges;double width=0.0,thickness=0.0;QVector<cad::FoldedSheetBend> bends;bool current=false;
    if(!document_.foldedSheetMetalDefinition(ids.first(),&flanges,&width,&thickness,&bends,&current)||!current){
        reportError("Export folded-sheet bend table","Select a folded body whose saved bend definition is current.");return;
    }
    QStringList defaults;for(int i=0;i<bends.size();++i)defaults.append("0.33");
    bool accepted=false;const QString kText=QInputDialog::getText(this,"Folded-sheet bend table",
        "K-factor for each bend, in order (0–1, comma separated):",QLineEdit::Normal,defaults.join(','),&accepted);
    if(!accepted)return;
    QVector<double> kFactors;QString error;
    if(!cad::parseSheetMetalNumberList(kText,&kFactors,&error)||kFactors.size()!=bends.size()){
        reportError("Export folded-sheet bend table",error.isEmpty()?QStringLiteral("Enter one K-factor for each saved bend."):error);return;
    }
    const QString path=QFileDialog::getSaveFileName(this,"Export folded-sheet bend table",{},"CSV files (*.csv)");
    if(path.isEmpty())return;
    if(!cad::exportFoldedSheetBendTableCsv(flanges,width,thickness,bends,kFactors,path,&error)){
        reportError("Export folded-sheet bend table",error);return;
    }
    statusBar()->showMessage(QString("Bend table exported for %1 folded bends.").arg(bends.size()),8000);
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
void MainWindow::closeEvent(QCloseEvent* event) {
    if(maybeSave()){saveWorkspaceLayout();event->accept();}
    else event->ignore();
}



// Usability workspace commands. Persist only UI state, never geometry or saved
// CAD metadata, and never treat a QSettings layout as an engineering revision.
void MainWindow::restoreWorkspaceLayout() {
    QSettings settings("CADWorkbench","CADWorkbench");
    const QByteArray geometry=settings.value("ui/workspace/geometry").toByteArray();
    const QByteArray state=settings.value("ui/workspace/docks").toByteArray();
    if(!geometry.isEmpty())restoreGeometry(geometry);
    if(!state.isEmpty()&&!restoreState(state))resetWorkspaceLayout();
    if(workspaceSelector_){
        const int last=settings.value("ui/workspace/active",0).toInt();
        if(last>=0&&last<workspaceSelector_->count())workspaceSelector_->setCurrentIndex(last);
    }
}
void MainWindow::saveWorkspaceLayout() {
    QSettings settings("CADWorkbench","CADWorkbench");
    settings.setValue("ui/workspace/geometry",saveGeometry());
    settings.setValue("ui/workspace/docks",saveState());
    if(workspaceSelector_)settings.setValue("ui/workspace/active",workspaceSelector_->currentIndex());
}
void MainWindow::resetWorkspaceLayout() {
    QSettings settings("CADWorkbench","CADWorkbench");
    settings.remove("ui/workspace");
    if(browserDock_){browserDock_->setFloating(false);browserDock_->show();
        addDockWidget(Qt::LeftDockWidgetArea,browserDock_);}
    if(assemblyDock_){assemblyDock_->setFloating(false);assemblyDock_->show();
        addDockWidget(Qt::LeftDockWidgetArea,assemblyDock_);
        if(browserDock_){tabifyDockWidget(browserDock_,assemblyDock_);browserDock_->raise();}}
    if(propertiesDock_){propertiesDock_->setFloating(false);propertiesDock_->show();
        addDockWidget(Qt::RightDockWidgetArea,propertiesDock_);}
    if(timelineDock_){timelineDock_->setFloating(false);timelineDock_->show();
        addDockWidget(Qt::BottomDockWidgetArea,timelineDock_);}
    if(browserDock_&&propertiesDock_)
        resizeDocks({browserDock_,propertiesDock_},{310,340},Qt::Horizontal);
    if(workspaceSelector_)workspaceSelector_->setCurrentIndex(0);
    statusBar()->showMessage("Design workspace layout reset. No model data changed.",5000);
}

void MainWindow::isolateSelectedBodies() {
    const QStringList ids=selectedIds();
    QString issue;
    if(!document_.isolateBodies(ids,&issue)){reportError("Isolate bodies",issue);return;}
    refresh();
    statusBar()->showMessage("Only selected bodies visible. Undo restores all previous visibility states.",6000);
}
void MainWindow::hideSelectedBodies() {
    const QStringList ids=selectedIds();
    if(ids.isEmpty()){reportError("Hide selected","Select at least one body.");return;}
    QMap<QString,bool> requested;
    for(const QString& id:ids)requested.insert(id,false);
    QString issue;
    if(!document_.setVisibilityBatch(requested,&issue)){reportError("Hide selected",issue);return;}
    refresh();
    statusBar()->showMessage("Selected bodies hidden as one undoable operation.",6000);
}
void MainWindow::showAllBodies() {
    QString issue;
    if(!document_.showAllBodies(&issue)){reportError("Show all",issue);return;}
    refresh();
    statusBar()->showMessage("All bodies visible. Undo restores the previous visibility states.",6000);
}

void MainWindow::updateContextActions() {
    const QStringList ids=selectedIds();
    syncAssemblyNavigatorSelection(ids);
    const bool hasSelection=!ids.isEmpty();
    const bool one=ids.size()==1&&document_.find(ids.first())!=nullptr;
    const bool bodyMode=viewport_&&viewport_->selectionMode()==CadViewport::SelectionMode::Body;
    for(QAction* action:singleBodyActions_)
        if(action)action->setEnabled(one&&bodyMode);
    for(QAction* action:multipleBodyActions_)
        if(action)action->setEnabled(hasSelection&&bodyMode);
    if(batchRenameAction_)batchRenameAction_->setEnabled(ids.size()>=2&&bodyMode);
    if(isolateAction_)isolateAction_->setEnabled(hasSelection&&bodyMode);
    if(hideSelectedAction_)hideSelectedAction_->setEnabled(hasSelection&&bodyMode);
    if(showAllAction_)showAllAction_->setEnabled(!document_.parts().isEmpty());
    // Face/edge mode is intentional and does not require selecting body mode.
    // A fillet/chamfer is usable only if an actual edge was picked.
    const bool edges=viewport_&&!viewport_->selectedEdgeReferences().isEmpty();
    for(QAction* action:singleBodyActions_){
        if(action&&(action->text().contains("Fillet selected edges")||
                    action->text().contains("Chamfer selected edges")))
            action->setEnabled(edges);
    }
    if(contextSummary_){
        const QString mode=viewport_->selectionMode()==CadViewport::SelectionMode::Body?"Body":
            viewport_->selectionMode()==CadViewport::SelectionMode::Face?"Face":
            viewport_->selectionMode()==CadViewport::SelectionMode::Edge?"Edge":"Vertex";
        const QString hint=ids.isEmpty()?"Pick a body in the canvas or design browser":
            ids.size()==1?QStringLiteral("Selected: %1").arg(document_.find(ids.first())?
                document_.find(ids.first())->name:QStringLiteral("Unresolved")):
            QStringLiteral("%1 bodies selected").arg(ids.size());
        contextSummary_->setText(QStringLiteral("%1  |  %2 mode").arg(hint,mode));
    }
}

void MainWindow::updateFeatureTimeline() {
    if(!featureTimeline_)return;
    const QSignalBlocker blocker(featureTimeline_);
    featureTimeline_->clear();
    const QStringList ids=selectedIds();
    if(ids.size()!=1){
        auto* item=new QListWidgetItem(ids.isEmpty()?"Select a body to view its history":
            "Select one body to show its design timeline",featureTimeline_);
        item->setFlags(item->flags()&~Qt::ItemIsSelectable);
        return;
    }
    const cad::Part* part=document_.find(ids.first());
    if(!part)return;
    const QString id=part->id;
    auto* base=new QListWidgetItem(QStringLiteral("BASE · %1").arg(part->name),featureTimeline_);
    base->setData(Qt::UserRole,id);
    base->setToolTip("Original B-rep stock. Double-click to edit this body's feature history.");
    for(int i=0;i<part->shapeFeatureNodes.size();++i){
        const auto& node=part->shapeFeatureNodes[i];
        const QString title=QStringLiteral("%1 · %2%3").arg(i+1).arg(node.type,
            node.suppressed?QStringLiteral(" [off]"):QString());
        auto* item=new QListWidgetItem(title,featureTimeline_);
        item->setData(Qt::UserRole,id);
        item->setData(Qt::UserRole+1,node.id);
        QStringList details;
        for(auto it=node.parameters.cbegin();it!=node.parameters.cend();++it)
            details<<QStringLiteral("%1 = %2").arg(it.key()).arg(it.value(),0,'g',8);
        item->setToolTip(QStringLiteral("Feature ID: %1\n%2\nDouble-click to open the editor.")
                         .arg(node.id,details.join("\n")));
        if(node.suppressed)item->setForeground(QColor(132,145,151));
    }
    if(part->shapeFeatureNodes.isEmpty()){
        auto* tip=new QListWidgetItem("+  Edit feature history to add machining features",featureTimeline_);
        tip->setData(Qt::UserRole,id);
    }
    if(part->hasShapeFeatureTree&&!document_.shapeFeatureTreeCurrent(id)){
        auto* warning=new QListWidgetItem("⚠ STALE: rebase before regeneration",featureTimeline_);
        warning->setToolTip("Saved feature output does not match current B-rep; investigate before rebuilding.");
        warning->setForeground(QColor(174,81,37));
    }
}


void MainWindow::rememberRecentProject(const QString& path) {
    if(path.isEmpty())return;
    const QString absolute=QFileInfo(path).absoluteFilePath();
    QSettings settings("CADWorkbench","CADWorkbench");
    QStringList recent=settings.value("ui/recentProjects").toStringList();
    recent.removeAll(absolute);
    recent.prepend(absolute);
    while(recent.size()>10)recent.removeLast();
    settings.setValue("ui/recentProjects",recent);
    rebuildRecentProjectsMenu();
}
void MainWindow::rebuildRecentProjectsMenu() {
    if(!recentProjectsMenu_)return;
    recentProjectsMenu_->clear();
    const QStringList recent=QSettings("CADWorkbench","CADWorkbench")
        .value("ui/recentProjects").toStringList();
    if(recent.isEmpty()){
        QAction* empty=recentProjectsMenu_->addAction("No recent projects");
        empty->setEnabled(false);return;
    }
    for(const QString& filename:recent){
        if(filename.trimmed().isEmpty())continue;
        const QFileInfo info(filename);
        QAction* entry=recentProjectsMenu_->addAction(info.fileName());
        entry->setToolTip(info.absoluteFilePath());
        entry->setStatusTip(info.absoluteFilePath());
        entry->setEnabled(info.exists()&&info.isFile());
        connect(entry,&QAction::triggered,this,[this,filename]{openRecentProject(filename);});
    }
    recentProjectsMenu_->addSeparator();
    recentProjectsMenu_->addAction("Clear recent list",this,[this]{
        QSettings("CADWorkbench","CADWorkbench").remove("ui/recentProjects");
        rebuildRecentProjectsMenu();
    });
}
void MainWindow::openRecentProject(const QString& path) {
    if(!maybeSave())return;
    QString issue;
    if(!document_.load(path,&issue)){
        reportError("Open recent project",issue);
        rebuildRecentProjectsMenu();return;
    }
    rememberRecentProject(path);
    refresh(true);
}

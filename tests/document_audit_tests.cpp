#include "core/DocumentAudit.h"
#include "core/DrawingRevisionImpact.h"
#include "core/ShapeFeatureTree.h" // real OCCT witnessed drawing references
#include "core/ManufacturingHandoff.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <stdexcept>
#include <iostream>

namespace {
int checks=0;
void require(bool condition,const char* reason) {
    ++checks;
    if(!condition)throw std::runtime_error(reason);
}
bool has(const cad::DocumentAuditReport& report,const QString& code) {
    for(const auto& finding:report.findings)if(finding.code==code)return true;
    return false;
}
}

int main(int argc,char** argv) {
    QCoreApplication application(argc,argv);
    try {
        // Sprint 16: one read-only, bounded engineering preflight must join
        // actual OCCT geometry, strict drawing regeneration and refold audit.
        // It must not silently issue drawings or mutate the model and must
        // report a revision-stale sheet as a blocker instead of hiding it.
        {
            cad::SceneDocument project;
            QString reason;
            QByteArray output("PREVIOUS REPORT");
            require(!project.buildIntegratedEngineeringReadinessCsv(&output,&reason)&&
                    output=="PREVIOUS REPORT", "Empty readiness source overwrote report");
            require(project.addPrimitive(cad::Primitive::Box,{30,20,8},&reason),
                    "Readiness fixture: cannot create an ordinary solid");
            const QString plate=project.parts().last().id;
            cad::DrawingSheetSettings sheet;
            sheet.drawingNumber="S16-PLATE";
            require(project.setDrawingSheetSettings(plate,sheet,&reason),
                    "Readiness fixture: cannot persist linked drawing");
            require(project.addFoldedSheetMetal({40,30},25,1,{{90,2}},&reason),
                    "Readiness fixture: cannot create folded solid");
            const QString folded=project.parts().last().id;
            const QString originalToken=project.find(plate)->geometryToken;
            require(project.buildIntegratedEngineeringReadinessCsv(&output,&reason)&&
                    output.contains("GEOMETRY")&&output.contains("DRAWING")&&
                    output.contains("SHEET_METAL")&&output.contains("NOT_ISSUED")&&
                    output.contains("Stored/refolded OCCT volume")&&
                    project.find(plate)->geometryToken==originalToken,
                    "Integrated readiness failed cross-domain native preflight");
            const QByteArray goodReport=output;
            require(!project.buildIntegratedEngineeringReadinessCsv(nullptr,&reason)&&
                    output==goodReport, "Null readiness result was not rejected atomically");
            QTemporaryDir destination;
            require(destination.isValid(),"Cannot create temporary readiness destination");
            const auto csvPath=destination.filePath("unissued-readiness.csv");
            require(project.exportIntegratedEngineeringReadinessCsv(csvPath,&reason)&&
                    QFile::exists(csvPath),"Cannot atomically publish engineering readiness CSV");
            require(project.translate(plate,1,0,0,&reason),
                    "Readiness fixture: could not revise drawing source");
            require(project.buildIntegratedEngineeringReadinessCsv(&output,&reason)&&
                    output.contains("BLOCK")&&output.contains("drawing source or annotation is stale"),
                    "Readiness audit failed to block a stale drawing source");
            require(project.find(folded)!=nullptr&&
                    output.contains("engineering_approval=NOT_ISSUED"),
                    "Readiness report silently issued geometry or lost a saved part");
        }
        // Sprint 12: a reference that was current in the previous drawing
        // revision must NOT remain current just because its own token is unchanged.
        {
            using namespace cad::drawing_audit;
            DrawingRevisionImpact evidence;
            std::string diagnostic;
            const std::vector<Source> oldSources={{"plate","geom-v1","meta-v1"}};
            const std::vector<Source> editedSources={{"plate","geom-v2","meta-v1"}};
            const std::vector<Annotation> prior={{"D1","plate","geom-v1","",true,false}};
            require(compareDrawingRevisions(oldSources,prior,editedSources,prior,&evidence,&diagnostic),
                    "Drawing revision comparison failed to process source geometry change");
            require(evidence.changedBodyIds.size()==1&&evidence.currentIssues.size()==1&&
                    evidence.annotations[0].currentReferenceStale&&!evidence.releaseApproved,
                    "Unchanged dimension token was silently approved across geometry revision");
            auto reviewed=prior;
            reviewed[0].capturedGeometryToken="geom-v2";
            require(compareDrawingRevisions(oldSources,prior,editedSources,reviewed,&evidence,&diagnostic)&&
                    evidence.modified==1&&evidence.currentIssues.empty()&&!evidence.releaseApproved,
                    "Explicit revised dimension must remain unissued until human review");
        }
        // Sprint 14: multi-sheet edge witness recovery must be atomic, not a
        // series of independently committed drawing edits. This also checks
        // stale legacy indices and independently reviewed GD&T are not approved.
        {
            cad::SceneDocument batch;
            QString issue;
            require(batch.addPrimitive(cad::Primitive::Box,{30,20,10},&issue)&&
                    batch.addPrimitive(cad::Primitive::Box,{26,16,8},&issue),
                    "Batch drawing: could not create independent source parts");
            const QString first=batch.parts()[0].id;
            const QString second=batch.parts()[1].id;
            for(const QString& owner:{first,second}){
                const cad::Part* body=batch.find(owner);
                QMap<QString,double> witness;
                require(cad::captureStableEdgeAnchor(body->shape,1,&witness,&issue),
                        "Batch drawing: could not capture original edge witness");
                cad::DrawingEdgeDimensionAnnotation dim;
                dim.id=owner==first?"batch-first":"batch-second";
                dim.sourceBodyId=owner;dim.sourceGeometryToken=body->geometryToken;
                dim.edgeIndex=1;dim.kind="LENGTH";dim.view="TOP";dim.edgeAnchor=witness;
                cad::DrawingSheetSettings settings;
                settings.edgeDimensionAnnotations.append(dim);
                if(owner==second){
                    auto legacy=dim;legacy.id="batch-legacy";legacy.edgeAnchor.clear();
                    settings.edgeDimensionAnnotations.append(legacy);
                    settings.gdtFrames.append({"batch-gdt",owner,body->geometryToken,
                                             "POSITION",0.1,"A","B","C",20,20});
                }
                require(batch.setDrawingSheetSettings(owner,settings,&issue),
                        "Batch drawing: could not persist source-linked drawing");
            }
            require(batch.translate(first,2,0,0,&issue)&&batch.translate(second,3,0,0,&issue),
                    "Batch drawing: source revisions failed");
            require(batch.refreshDrawingSheetSource(first,&issue)&&
                    batch.refreshDrawingSheetSource(second,&issue),
                    "Batch drawing: aggregate refresh failed");
            QByteArray recoveryCsv;
            require(batch.buildDrawingRecoveryCsv(&recoveryCsv,&issue)&&
                    recoveryCsv.contains("batch-first")&&recoveryCsv.contains("batch-legacy")&&
                    recoveryCsv.contains("REVIEW_REQUIRED")&&
                    recoveryCsv.contains("GD_AND_T"),
                    "Batch drawing: recovery register omitted stale edge and datum evidence");
            const QString oldFirst=batch.find(first)->drawingSettings.edgeDimensionAnnotations[0].sourceGeometryToken;
            const QString oldSecond=batch.find(second)->drawingSettings.edgeDimensionAnnotations[0].sourceGeometryToken;
            QMap<QString,QStringList> unsafe{{first,{"batch-first"}},
                                            {second,{"batch-second","batch-legacy"}}};
            QString preview="UNCHANGED_PREVIEW";
            require(!batch.previewDrawingEdgeReviewBatch(unsafe,&preview,&issue)&&
                    preview=="UNCHANGED_PREVIEW"&&
                    batch.find(first)->drawingSettings.edgeDimensionAnnotations[0].sourceGeometryToken==oldFirst,
                    "Batch drawing: legacy index was accepted or failed preview was replaced");
            require(!batch.reviewDrawingEdgeReviewBatch(unsafe,nullptr,&issue)&&
                    batch.find(second)->drawingSettings.edgeDimensionAnnotations[0].sourceGeometryToken==oldSecond,
                    "Batch drawing: invalid second sheet partially modified first sheet");
            QMap<QString,QStringList> good{{first,{"batch-first"}},{second,{"batch-second"}}};
            require(batch.previewDrawingEdgeReviewBatch(good,&preview,&issue)&&
                    preview.contains("requested_annotations=2")&&
                    batch.find(first)->drawingSettings.edgeDimensionAnnotations[0].sourceGeometryToken==oldFirst,
                    "Batch drawing: safe preview changed live sheet revision");
            QString result;
            require(batch.reviewDrawingEdgeReviewBatch(good,&result,&issue)&&
                    result.contains("changed_sheets=2")&&
                    batch.find(first)->drawingSettings.edgeDimensionAnnotations[0].sourceGeometryToken==
                        batch.find(first)->geometryToken&&
                    batch.find(second)->drawingSettings.edgeDimensionAnnotations[0].sourceGeometryToken==
                        batch.find(second)->geometryToken,
                    "Batch drawing: two-sheet geometry review did not commit together");
            const auto reviewed=batch.drawingSheetSourceStatus(second);
            require(reviewed.staleAnnotationIds.contains("batch-legacy")&&
                    reviewed.staleAnnotationIds.contains("batch-gdt")&&
                    !reviewed.staleAnnotationIds.contains("batch-second"),
                    "Batch drawing: selected edge review silently approved legacy/GD&T records");
            require(batch.undo()&&
                    batch.find(first)->drawingSettings.edgeDimensionAnnotations[0].sourceGeometryToken==oldFirst&&
                    batch.find(second)->drawingSettings.edgeDimensionAnnotations[0].sourceGeometryToken==oldSecond,
                    "Batch drawing: one undo must restore both sheets");
            require(batch.redo()&&
                    batch.find(first)->drawingSettings.edgeDimensionAnnotations[0].sourceGeometryToken==
                        batch.find(first)->geometryToken&&
                    batch.find(second)->drawingSettings.edgeDimensionAnnotations[0].sourceGeometryToken==
                        batch.find(second)->geometryToken,
                    "Batch drawing: redo must re-apply both reviews");
            QTemporaryDir saved;
            require(saved.isValid()&&batch.saveAs(saved.filePath("batch-review.cadscene"),&issue),
                    "Batch drawing: could not save reviewed project");
            cad::SceneDocument reopened;
            require(reopened.load(saved.filePath("batch-review.cadscene"),&issue)&&
                    reopened.drawingSheetSourceStatus(second).staleAnnotationIds.contains("batch-legacy")&&
                    !reopened.drawingSheetSourceStatus(second).staleAnnotationIds.contains("batch-second"),
                    "Batch drawing: save/reopen lost reviewed and unreviewed identities");
            // Explicit repeated review is a no-op and creates no undo entry.
            require(!reopened.canUndo()&&
                    reopened.reviewDrawingEdgeReviewBatch(good,nullptr,&issue)&&
                    !reopened.canUndo(),
                    "Batch drawing: unchanged geometry review consumed undo history");
            require(reopened.exportDrawingRecoveryCsv(saved.filePath("recovery.csv"),&issue)&&
                    QFile::exists(saved.filePath("recovery.csv")),
                    "Batch drawing: recovery report could not export atomically");
        }
        cad::SceneDocument document;
        const auto empty=cad::runDocumentAudit(document);
        require(empty.bodyCount==0&&has(empty,"DOC_EMPTY")&&empty.errorCount>0,
                "Empty document was not flagged");
        QString error;
        require(document.addPrimitive(cad::Primitive::Box,{30,20,10},&error),"Could not create test box");
        const QString id=document.parts().first().id;
        const QString firstToken=document.find(id)->geometryToken;
        const auto initial=cad::runDocumentAudit(document);
        require(initial.bodyCount==1&&!has(initial,"GEOMETRY_INVALID")&&
                has(initial,"PART_NUMBER_MISSING")&&initial.errorCount==0,
                "Basic project preflight did not distinguish geometry errors from metadata warnings");
        require(document.find(id)->geometryToken==firstToken&&document.parts().size()==1,
                "Read-only preflight modified the scene");
        cad::DrawingSheetSettings sheet;
        sheet.drawingNumber="QA-001";
        require(document.setDrawingSheetSettings(id,sheet,&error),"Could not create linked sheet");
        const auto current=cad::runDocumentAudit(document);
        require(!has(current,"DRAWING_SOURCE_STALE"),"Current drawing source was incorrectly marked stale");
        require(document.translate(id,5,0,0,&error),"Could not change drawing source geometry");
        const auto stale=cad::runDocumentAudit(document);
        require(has(stale,"DRAWING_SOURCE_STALE")&&stale.warningCount>current.warningCount,
                "Preflight missed a saved drawing with stale source geometry");
        require(document.drawingSheetSourceStatus(id).state==cad::DrawingSourceState::Stale,
                "Preflight acknowledged a stale drawing without a user action");
        require(document.refreshDrawingSheetSource(id,&error),"Could not acknowledge changed drawing source");
        const auto refreshed=cad::runDocumentAudit(document);
        require(!has(refreshed,"DRAWING_SOURCE_STALE"),"Refreshed drawing incorrectly remained stale");
        // A blanket sheet refresh is never approval for an individual
        // GD&T or dimension reference. The annotation must be reviewed.
        cad::DrawingSheetSettings captured=document.find(id)->drawingSettings;
        captured.gdtFrames.append({"audit-gdt-revision",id,document.find(id)->geometryToken,
                                   "POSITION",0.15,"A","B","C",18,24});
        require(document.setDrawingSheetSettings(id,captured,&error),
                "Could not save a revision-linked annotation audit fixture");
        require(document.translate(id,1,0,0,&error),
                "Could not edit the topology of the annotation source");
        require(document.refreshDrawingSheetSource(id,&error),
                "Aggregate source revision refresh failed");
        const auto stillStale=document.drawingSheetSourceStatus(id);
        require(stillStale.state==cad::DrawingSourceState::Stale&&
                stillStale.staleAnnotationIds.contains("audit-gdt-revision")&&
                has(cad::runDocumentAudit(document),"DRAWING_ANNOTATION_REVISION_STALE"),
                "Whole-sheet refresh incorrectly approved a stale GD&T source");
        require(document.undo()&&
                document.drawingSheetSourceStatus(id).state==cad::DrawingSourceState::Stale,
                "Undo unexpectedly approved stale individual drawing reference");
        // Sprint 8: full native source-revision recovery across undo, redo,
        // save, reopen, sheet refresh, and an explicit per-annotation review.
        // Run this in its own document so the historic audit fixture and
        // its 130-CTest release inventory stay unchanged.
        {
            cad::SceneDocument recovery;
            require(recovery.addPrimitive(cad::Primitive::Box,{30,20,10},&error),
                    "Drawing recovery: failed to create the source box");
            const QString bodyId=recovery.parts().first().id;
            const QString originalToken=recovery.find(bodyId)->geometryToken;
            QMap<QString,double> witness;
            require(cad::captureStableEdgeAnchor(recovery.find(bodyId)->shape,1,&witness,&error),
                    "Drawing recovery: failed to capture a kernel edge witness");
            cad::DrawingSheetSettings settings;
            cad::DrawingEdgeDimensionAnnotation dimension;
            dimension.id="recovery-length";
            dimension.sourceBodyId=bodyId;
            dimension.sourceGeometryToken=originalToken;
            dimension.edgeIndex=1;
            dimension.view="TOP";
            dimension.kind="LENGTH";
            dimension.edgeAnchor=witness;
            settings.edgeDimensionAnnotations.append(dimension);
            settings.gdtFrames.append({"recovery-gdt",bodyId,originalToken,
                                       "POSITION",0.15,"A","B","C",18,24});
            require(recovery.setDrawingSheetSettings(bodyId,settings,&error),
                    "Drawing recovery: failed to create source-linked annotations");
            const QString sheetId=recovery.find(bodyId)->drawingSettings.sheetId;
            const auto status=[&](){return recovery.drawingSheetSourceStatus(bodyId);};
            require(status().state==cad::DrawingSourceState::Current&&
                    status().staleAnnotationIds.isEmpty(),
                    "Drawing recovery: initial annotations are not current");
            require(recovery.translate(bodyId,4,0,0,&error),
                    "Drawing recovery: source translation failed");
            const QString modifiedToken=recovery.find(bodyId)->geometryToken;
            require(modifiedToken!=originalToken&&
                    status().staleAnnotationIds.contains("recovery-length")&&
                    status().staleAnnotationIds.contains("recovery-gdt"),
                    "Drawing recovery: edited geometry did not stale both annotations");
            require(recovery.undo()&&recovery.find(bodyId)->geometryToken==originalToken&&
                    status().state==cad::DrawingSourceState::Current&&
                    status().staleAnnotationIds.isEmpty(),
                    "Drawing recovery: undo did not restore captured geometry revision");
            require(recovery.redo()&&recovery.find(bodyId)->geometryToken==modifiedToken&&
                    status().state==cad::DrawingSourceState::Stale&&
                    status().staleAnnotationIds.size()==2,
                    "Drawing recovery: redo silently approved stale annotations");
            require(recovery.refreshDrawingSheetSource(bodyId,&error)&&
                    status().state==cad::DrawingSourceState::Stale&&
                    status().staleAnnotationIds.size()==2,
                    "Drawing recovery: sheet refresh incorrectly approved annotations");
            QTemporaryDir revisionFolder;
            require(revisionFolder.isValid(),"Drawing recovery: temporary project folder failed");
            const QString stalePath=revisionFolder.filePath("stale-after-redo.cadscene");
            require(recovery.saveAs(stalePath,&error),
                    "Drawing recovery: saving stale reviewed project failed");
            cad::SceneDocument reopened;
            require(reopened.load(stalePath,&error)&&reopened.find(bodyId),
                    "Drawing recovery: could not reopen the saved project");
            const auto reopenedStale=reopened.drawingSheetSourceStatus(bodyId);
            require(reopenedStale.state==cad::DrawingSourceState::Stale&&
                    reopenedStale.staleAnnotationIds.contains("recovery-length")&&
                    reopenedStale.staleAnnotationIds.contains("recovery-gdt")&&
                    reopened.find(bodyId)->drawingSettings.sheetId==sheetId&&
                    reopened.find(bodyId)->drawingSettings.edgeDimensionAnnotations[0].edgeAnchor==witness,
                    "Drawing recovery: save/reopen acknowledged stale refs or lost witness identity");
            // Reopen must NOT resurrect the previous process's in-memory undo.
            require(!reopened.canUndo(),
                    "Drawing recovery: saved project unexpectedly inherited old undo history");
            // Token-only spoofing with a displaced physical witness may not
            // turn a stale dimension into a current dimension.
            auto forged=reopened.find(bodyId)->drawingSettings;
            forged.edgeDimensionAnnotations[0].sourceGeometryToken=modifiedToken;
            forged.edgeDimensionAnnotations[0].edgeAnchor["edge_witness_x"]+=500.0;
            require(!reopened.setDrawingSheetSettings(bodyId,forged,&error)&&
                    reopened.drawingSheetSourceStatus(bodyId).staleAnnotationIds.size()==2,
                    "Drawing recovery: accepted a forged edge ancestry token");
            require(reopened.reviewDrawingEdgeDimension(bodyId,"recovery-length",&error)&&
                    reopened.drawingSheetSourceStatus(bodyId).staleAnnotationIds.contains("recovery-gdt")&&
                    !reopened.drawingSheetSourceStatus(bodyId).staleAnnotationIds.contains("recovery-length"),
                    "Drawing recovery: geometry-checked edge review failed to reattach uniquely");
            auto reviewed=reopened.find(bodyId)->drawingSettings;
            reviewed.gdtFrames[0].sourceGeometryToken=modifiedToken;
            require(reopened.setDrawingSheetSettings(bodyId,reviewed,&error)&&
                    reopened.drawingSheetSourceStatus(bodyId).state==cad::DrawingSourceState::Current,
                    "Drawing recovery: explicitly reviewed GD&T revision was not persisted");
            require(reopened.undo()&&
                    reopened.drawingSheetSourceStatus(bodyId).staleAnnotationIds.size()==1&&
                    reopened.drawingSheetSourceStatus(bodyId).staleAnnotationIds.contains("recovery-gdt"),
                    "Drawing recovery: undo of GD&T review did not restore stale state");
            require(reopened.redo()&&
                    reopened.drawingSheetSourceStatus(bodyId).state==cad::DrawingSourceState::Current,
                    "Drawing recovery: redo of review did not restore current state");
            const QString approvedPath=revisionFolder.filePath("reviewed.cadscene");
            require(reopened.saveAs(approvedPath,&error),
                    "Drawing recovery: could not save reviewed revision");
            cad::SceneDocument verified;
            require(verified.load(approvedPath,&error)&&verified.find(bodyId)&&
                    verified.drawingSheetSourceStatus(bodyId).state==cad::DrawingSourceState::Current&&
                    verified.find(bodyId)->drawingSettings.sheetId==sheetId&&
                    verified.find(bodyId)->drawingSettings.edgeDimensionAnnotations[0].edgeAnchor==witness,
                    "Drawing recovery: reviewed drawing changed across second save/reopen");
            require(!verified.canUndo()&&
                    verified.reviewDrawingEdgeDimension(bodyId,"recovery-length",&error)&&
                    !verified.canUndo()&&
                    verified.drawingSheetSourceStatus(bodyId).state==cad::DrawingSourceState::Current,
                    "Drawing recovery: validating an unchanged reference consumed undo or changed the sheet");
        }
        require(document.addParametricRectangularExtrusion(30,20,6,&error),
                "Could not create parametric test extrusion");
        const QString featureId=document.parts().last().id;
        require(document.parametricDefinitionCurrent(featureId),"New parametric definition was not current");
        require(document.translate(featureId,2,0,0,&error),"Could not change parametric geometry");
        const auto changed=cad::runDocumentAudit(document);
        require(has(changed,"PARAMETRIC_STALE"),"Preflight missed a stale parametric definition");
        require(document.undo()&&document.parametricDefinitionCurrent(featureId),
                "Could not undo geometry operation back to a current parametric definition");
        const auto undone=cad::runDocumentAudit(document);
        require(!has(undone,"PARAMETRIC_STALE"),"Audit didn't reflect undo of the parametric geometry edit");
        require(changed.toPlainText().contains("PARAMETRIC_STALE")&&
                changed.toCsv().contains("PARAMETRIC_STALE"),"Audit exports omitted a model finding");
        cad::SceneDocument folded;
        require(folded.addFoldedSheetMetal({40.0,30.0},25.0,1.0,{{90.0,2.0}},&error),
                "Could not create folded-sheet preflight fixture");
        const QString foldedId=folded.parts().first().id;
        require(!has(cad::runDocumentAudit(folded),"FOLDED_DEFINITION_STALE"),
                "Newly created folded-sheet definition was incorrectly stale");
        require(folded.translate(foldedId,1,0,0,&error)&&
                has(cad::runDocumentAudit(folded),"FOLDED_DEFINITION_STALE"),
                "Audit missed a directly modified folded body");
        cad::SceneDocument assembly;
        require(assembly.addPrimitive(cad::Primitive::Box,{10,10,10},&error),
                "Could not create assembly preflight fixture");
        const QString parent=assembly.parts().first().id;
        require(assembly.makeComponent(parent,"Audit base",&error)&&
                assembly.insertComponentOccurrence(parent,20,0,0,&error),
                "Could not insert assembly preflight component occurrence");
        const QString child=assembly.parts().last().id;
        require(assembly.addRigidJoint(parent,child,&error),
                "Could not create assembly preflight rigid joint");
        require(!has(cad::runDocumentAudit(assembly),"JOINT_PARENT_INVALID"),
                "Audit incorrectly rejected a valid rigid-joint parent");
        // RC5: an inspection note on a stable feature ID is a release gate
        // until the corresponding design revision is explicitly reviewed.
        cad::SceneDocument annotated;
        require(annotated.addPrimitive(cad::Primitive::Box,{30,20,10},&error),
                "RC5 feature-audit base failed");
        const QString annotatedId=annotated.parts().first().id;
        cad::ShapeFeatureNode move;
        move.id="audit-move";move.type="Translate";
        move.parameters={{"x",2},{"y",0},{"z",0}};
        require(annotated.setShapeFeatureTree(annotatedId,{move},&error),
                "RC5 feature-audit graph failed");
        cad::FeatureInspectionNote note{"QA-check","audit-move","INSPECTION",
            "Verify external mounting clearance",annotated.find(annotatedId)->geometryToken};
        require(annotated.setFeatureInspectionNotes(annotatedId,{note},&error),
                "RC5 feature-audit note failed");
        require(!has(cad::runDocumentAudit(annotated),"FEATURE_NOTE_REVIEW_REQUIRED"),
                "RC5 current inspection was incorrectly declared stale");
        move.parameters["x"]=4;
        require(annotated.setShapeFeatureTree(annotatedId,{move},&error),
                "RC5 feature audit could not edit source geometry");
        const auto requireReview=cad::runDocumentAudit(annotated);
        require(has(requireReview,"FEATURE_NOTE_REVIEW_REQUIRED")&&
                !requireReview.passed(),
                "RC5 stale inspection failed to block document preflight");
        note.reviewedGeometryToken=annotated.find(annotatedId)->geometryToken;
        require(annotated.setFeatureInspectionNotes(annotatedId,{note},&error),
                "RC5 re-review failed");
        require(!has(cad::runDocumentAudit(annotated),"FEATURE_NOTE_REVIEW_REQUIRED"),
                "RC5 re-reviewed note remained stale in project audit");
        require(annotated.setShapeFeatureTree(annotatedId,{},&error),
                "RC5 could not remove audited feature");
        require(has(cad::runDocumentAudit(annotated),"FEATURE_NOTE_SOURCE_MISSING"),
                "RC5 orphaned inspection note was not reported");
        // Sprint 15: independently render two saved drawing sheets from the
        // current B-reps and atomically publish a new, unissued drawing set.
        {
            cad::SceneDocument series;
            QString issue;
            require(series.addPrimitive(cad::Primitive::Box,{20,15,10},&issue)&&
                    series.addPrimitive(cad::Primitive::Box,{25,18,12},&issue),
                    "multi-sheet export: fixture parts missing");
            const QString first=series.parts()[0].id,second=series.parts()[1].id;
            cad::DrawingSheetSettings sheet;
            sheet.drawingNumber="DRAW-A";
            require(series.setDrawingSheetSettings(first,sheet,&issue),
                    "multi-sheet export: first saved drawing missing");
            sheet.drawingNumber="DRAW-B";
            require(series.setDrawingSheetSettings(second,sheet,&issue),
                    "multi-sheet export: second saved drawing missing");
            QMap<QString,QByteArray> drawings;
            QMap<QString,cad::DrawingSheetSettings> prepared;
            QString batchReport;
            require(series.buildSavedDrawingSvgBatch({first,second},&drawings,&prepared,
                    &batchReport,&issue)&&drawings.size()==2&&prepared.size()==2&&
                    drawings[first].contains("<svg")&&drawings[second].contains("<svg"),
                    "two saved drawing sheets did not regenerate from live geometry");
            const auto oldOutput=drawings;
            const auto firstSavedToken=series.find(first)->drawingSettings.sourceGeometryToken;
            require(!series.buildSavedDrawingSvgBatch({first,first},&drawings,&prepared,
                    &batchReport,&issue)&&drawings==oldOutput,
                    "duplicate drawing sheet failed to reject atomically");
            require(series.translate(first,5,0,0,&issue),
                    "multi-sheet export: could not revise source");
            require(series.buildSavedDrawingSvgBatch({first,second},&drawings,&prepared,
                    &batchReport,&issue)&&
                    series.find(first)->drawingSettings.sourceGeometryToken==firstSavedToken,
                    "regeneration silently acknowledged a changed drawing source");
            QTemporaryDir exports;
            require(exports.isValid(),"multi-sheet export: no temporary directory");
            const QString target=exports.filePath("unissued-drawings");
            require(series.exportSavedDrawingSvgBatch({first,second},target,&batchReport,&issue)&&
                    QFile::exists(target+"/sheet-001.svg")&&
                    QFile::exists(target+"/sheet-002.svg")&&
                    QFile::exists(target+"/DRAWING_BATCH_INDEX.csv")&&
                    series.find(first)->drawingSettings.sourceGeometryToken==firstSavedToken,
                    "unissued drawing batch did not publish atomically without approving revision");
            require(!series.exportSavedDrawingSvgBatch({first,second},target,nullptr,&issue),
                    "drawing batch overwrote an existing released directory");
        }
        cad::DocumentAuditReport escape;
        escape.findings.append({cad::AuditSeverity::Warning,"Drawing","QUOTING","id,1",
                                "title contains \"double quotes\"","check, repair"});
        const QByteArray csv=escape.toCsv();
        require(csv.contains("\"id,1\"")&&csv.contains("\"title contains \"\"double quotes\"\"\""),
                "Audit CSV did not quote identifiers and double quotes correctly");
        cad::DocumentAuditReport formula;
        formula.findings.append({cad::AuditSeverity::Warning,"Drawing","CELL_FORMULA","=SUM(1,1)",
                                "+HYPERLINK(\"https://example.com\")","review"});
        const QByteArray safeCsv=formula.toCsv();
        require(safeCsv.contains("\"'=SUM(1,1)\"")&&
                safeCsv.contains("\"'+HYPERLINK"),
                "Audit CSV did not neutralize spreadsheet formula cells");
        // v1.70: the full native Qt/OCCT export now includes a revision-linked
        // coverage row for every body, even when no axial holes exist. Existing
        // native CTest target remains registered (130 total) and NOT skipped.
        QTemporaryDir manufacturingFolder;
        require(manufacturingFolder.isValid(),"Could not create manufacturing audit directory");
        cad::SceneDocument manufacture;
        require(manufacture.addPrimitive(cad::Primitive::Box,{20,20,10},&error),
                "Could not create machining handoff fixture");
        require(manufacture.saveAs(manufacturingFolder.filePath("machining.cadworkbench"),&error),
                "Could not save the machining handoff source");
        cad::ManufacturingHandoffResult handoff;
        require(cad::createManufacturingHandoff(manufacture,manufacturingFolder.path(),&handoff,&error),
                "Native manufacturing handoff export failed");
        require(handoff.bodyCount==1&&handoff.axialHolesReviewed==0&&
                handoff.machiningBodiesRequiringReview==0,
                "Axial review improperly classified a box as having drill holes");
        QFile coverage(handoff.folder+"/machining_coverage.csv");
        require(coverage.open(QIODevice::ReadOnly)&&
                coverage.readAll().contains("INSPECTED_NO_AXIAL_HOLES"),
                "Native handoff did not record inspected zero-hole body");
        QFile holes(handoff.folder+"/axial_hole_locations.csv");
        require(holes.open(QIODevice::ReadOnly)&&
                holes.readAll().contains("body_id,geometry_token,hole_number"),
                "Native handoff omitted geometry-linked hole table");
        QFile inspectionTemplate(handoff.folder+"/hole_position_inspection_TEMPLATE.csv");
        require(inspectionTemplate.open(QIODevice::ReadOnly),
                "Native manufacturing handoff did not include a measurement template");
        const QByteArray unmeasured=inspectionTemplate.readAll();
        require(unmeasured.contains("nominal_x_mm,nominal_y_mm,nominal_diameter_mm")&&
                unmeasured.contains("inspection_status")&&
                !unmeasured.contains("INSPECTION_PASS")&&
                !unmeasured.contains("ACTUAL_MEASURED"),
                "Native handoff template incorrectly claims inspected evidence");
        QFile manifest(handoff.folder+"/handoff_manifest.json");
        require(manifest.open(QIODevice::ReadOnly),"Native handoff manifest missing");
        const auto manifestJson=QJsonDocument::fromJson(manifest.readAll()).object();
        require(manifestJson.value("schema_version").toInt()==2&&
                manifestJson.value("machining_holes").toInt(-1)==0&&
                manifestJson.value("machining_review_required").toInt(-1)==0,
                "Native export handoff manifest omitted machining review evidence");
        std::cout << "PASS: " << checks << " document-audit assertions\n";
        return 0;
    } catch(const std::exception& exception) {
        std::cerr << "FAIL at check " << checks << ": " << exception.what() << '\n';
        return 1;
    }
}

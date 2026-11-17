#include "DocumentAudit.h"

#include <BRepCheck_Analyzer.hxx>
#include <QSet>

#include <cmath>

namespace cad {
namespace {
QString severityName(AuditSeverity severity) {
    switch(severity) {
        case AuditSeverity::Error: return QStringLiteral("ERROR");
        case AuditSeverity::Warning: return QStringLiteral("WARNING");
        case AuditSeverity::Information: return QStringLiteral("INFO");
    }
    return QStringLiteral("INFO");
}
QString csvCell(QString value) {
    // Quoting fields is not enough for spreadsheets: cells beginning with
    // formula characters can execute when opened by Excel or similar tools.
    const QString trimmed=value.trimmed();
    if(!trimmed.isEmpty()&&QStringLiteral("=+-@").contains(trimmed.front()))
        value.prepend(QChar(0x27));
    value.replace('"',QStringLiteral("\"\""));
    return QStringLiteral("\"")+value+QStringLiteral("\"");
}
void add(DocumentAuditReport& report,AuditSeverity severity,const QString& area,
         const QString& code,const QString& bodyId,const QString& message,const QString& action) {
    report.findings.append({severity,area,code,bodyId,message,action});
    if(severity==AuditSeverity::Error)++report.errorCount;
    else if(severity==AuditSeverity::Warning)++report.warningCount;
    else ++report.informationCount;
}
}

DocumentAuditReport runDocumentAudit(const SceneDocument& document) {
    DocumentAuditReport report;
    report.bodyCount=static_cast<int>(document.parts().size());
    // The built-in BOM/data check already validates solids, part numbers,
    // metadata, and supplier fields; do not repeat its expensive diagnostics.
    const StandardsCheckReport standard=document.checkStandards(StandardsProfile::BomData);
    for(const StandardsFinding& finding:standard.findings) {
        // Surface-only bodies are valid intermediate CAD geometry and are not
        // required to be volumetric solids. Avoid a false preflight error.
        if(finding.code==QStringLiteral("GEOMETRY_INVALID")) {
            const Part* part=document.find(finding.bodyId);
            if(part&&part->isSurfaceBody) {
                if(part->shape.IsNull()||!BRepCheck_Analyzer(part->shape).IsValid()) {
                    add(report,AuditSeverity::Error,"Surface","SURFACE_INVALID",part->id,
                        QStringLiteral("Surface body '%1' has invalid geometry.").arg(part->name),
                        "Repair or rebuild this surface before downstream operations.");
                }
                continue;
            }
        }
        const AuditSeverity severity=finding.severity==FindingSeverity::Error?AuditSeverity::Error:
            (finding.severity==FindingSeverity::Warning?AuditSeverity::Warning:AuditSeverity::Information);
        add(report,severity,"Model data",finding.code,finding.bodyId,finding.message,
            finding.severity==FindingSeverity::Info?QString():
                QStringLiteral("Inspect this body and correct its geometry or part metadata."));
    }
    for(const Part& part:document.parts()) {
        if(part.hasLinkedSketchExtrusion&&!document.linkedSketchExtrusionCurrent(part.id))
            add(report,AuditSeverity::Warning,"Sketch dependency","LINKED_SKETCH_STALE",part.id,
                QStringLiteral("'%1' has a missing or stale external sketch reference.").arg(part.name),
                "Restore the constrained source sketch or undo the unsupported edit before rebuilding.");
        if(part.hasShapeFeatureTree&&!document.shapeFeatureTreeCurrent(part.id))
            add(report,AuditSeverity::Warning,"Feature tree","FEATURE_TREE_STALE",part.id,
                QStringLiteral("'%1' was modified outside its generic feature stack.").arg(part.name),
                "Open General feature tree and explicitly rebase its source, or undo the direct edit.");
        if(part.hasParametricDefinition&&!document.parametricDefinitionCurrent(part.id))
            add(report,AuditSeverity::Warning,"Parametric","PARAMETRIC_STALE",part.id,
                QStringLiteral("'%1' has a feature history that no longer matches its geometry.").arg(part.name),
                "Review or rebuild the original feature history before editing its dimensions.");
        if(part.hasConstrainedPolygonDefinition&&part.constrainedPolygonGeometryToken!=part.geometryToken)
            add(report,AuditSeverity::Warning,"Sketch","SKETCH_STALE",part.id,
                QStringLiteral("'%1' has a constrained sketch that no longer matches its extrusion.").arg(part.name),
                "Review the original constrained sketch and rebuild instead of assuming the dimensions are current.");
        if(part.hasFoldedSheetMetalDefinition&&part.foldedSheetGeometryToken!=part.geometryToken)
            add(report,AuditSeverity::Warning,"Sheet metal","FOLDED_DEFINITION_STALE",part.id,
                QStringLiteral("'%1' has folded-sheet inputs that no longer match its geometry.").arg(part.name),
                "Rebuild a folded body from the bend definition before exporting a flat pattern.");
        if(!part.assemblyJointType.isEmpty()) {
            const QString parent=part.assemblyJointParentBodyId;
            if(parent.isEmpty()||parent==part.id||!document.find(parent))
                add(report,AuditSeverity::Error,"Assembly","JOINT_PARENT_INVALID",part.id,
                    QStringLiteral("Joint on '%1' references an absent, empty, or self parent.").arg(part.name),
                    "Reassign the parent joint body or remove the invalid joint.");
            const QString type=part.assemblyJointType.toCaseFolded();
            if(type==QStringLiteral("revolute")&&
                (!std::isfinite(part.jointAngleDegrees)||part.jointMinimumDegrees>part.jointMaximumDegrees||
                 part.jointAngleDegrees<part.jointMinimumDegrees-1e-7||
                 part.jointAngleDegrees>part.jointMaximumDegrees+1e-7))
                add(report,AuditSeverity::Error,"Assembly","REVOLUTE_LIMIT_INVALID",part.id,
                    QStringLiteral("Revolute joint on '%1' is outside its saved angle limits.").arg(part.name),
                    "Correct the joint angle and allowed limits.");
            if(type==QStringLiteral("prismatic")&&
                (!std::isfinite(part.jointTranslationMm)||part.jointMinimumMm>part.jointMaximumMm||
                 part.jointTranslationMm<part.jointMinimumMm-1e-7||
                 part.jointTranslationMm>part.jointMaximumMm+1e-7))
                add(report,AuditSeverity::Error,"Assembly","PRISMATIC_LIMIT_INVALID",part.id,
                    QStringLiteral("Prismatic joint on '%1' is outside its saved translation limits.").arg(part.name),
                    "Correct the joint displacement and allowed limits.");
        }
        // A revision-stamped feature inspection note is part of design review.
        // Geometry edits must not silently count an old inspection as current.
        // Missing features are stronger errors than stale geometry because the
        // original engineering intention can no longer be located.
        if(!part.featureInspectionNotes.isEmpty()){
            const QStringList featureIds=document.inspectionFeatureIds(part.id);
            for(const FeatureInspectionNote& note:part.featureInspectionNotes){
                if(!featureIds.contains(note.featureId))
                    add(report,AuditSeverity::Error,"Feature inspection","FEATURE_NOTE_SOURCE_MISSING",part.id,
                        QStringLiteral("Feature-linked note '%1' no longer has a source feature '%2'.")
                            .arg(note.id,note.featureId),
                        "Reattach or explicitly disposition the orphaned inspection requirement.");
                else if(note.reviewedGeometryToken!=part.geometryToken)
                    add(report,AuditSeverity::Error,"Feature inspection","FEATURE_NOTE_REVIEW_REQUIRED",part.id,
                        QStringLiteral("Feature-linked note '%1' refers to a previous geometry revision.")
                            .arg(note.id),
                        "Review the revised feature and acknowledge its current geometry in the note editor.");
            }
        }
        if(!part.hasDrawingSettings)continue;
        const DrawingSheetSourceStatus status=document.drawingSheetSourceStatus(part.id);
        if(status.state==DrawingSourceState::Stale)
            add(report,AuditSeverity::Warning,"Drawing","DRAWING_SOURCE_STALE",part.id,
                QStringLiteral("Drawing sheet on '%1' has changed source geometry or metadata.").arg(part.name),
                "Review the source changes, refresh the saved drawing link, and re-export the sheet.");
        else if(status.state==DrawingSourceState::MissingBodies)
            add(report,AuditSeverity::Error,"Drawing","DRAWING_SOURCE_MISSING",part.id,
                QStringLiteral("Drawing sheet on '%1' refers to missing bodies: %2.")
                    .arg(part.name,status.missingSourceBodyIds.join(", ")),
                "Restore or reselect the source bodies before exporting the drawing.");
        for(const QString& staleAnnotationId:status.staleAnnotationIds)
            add(report,AuditSeverity::Error,"Drawing","DRAWING_ANNOTATION_REVISION_STALE",part.id,
                QStringLiteral("Drawing annotation '%1' still references a previous body geometry revision.")
                    .arg(staleAnnotationId),
                "Re-resolve its actual face/edge witness, review the changed dimension or GD&T frame, and re-author the individual captured source revision.");
        for(const DrawingEdgeDimensionAnnotation& edge:part.drawingSettings.edgeDimensionAnnotations) {
            const Part* source=document.find(edge.sourceBodyId);
            if(!source)
                add(report,AuditSeverity::Error,"Drawing","DIMENSION_SOURCE_MISSING",part.id,
                    QStringLiteral("Drawing edge dimension '%1' refers to a missing body.").arg(edge.id),
                    "Remove or reattach this edge dimension.");
            else if(edge.edgeIndex<1||edge.sourceGeometryToken!=source->geometryToken)
                add(report,AuditSeverity::Warning,"Drawing","DIMENSION_EDGE_STALE",part.id,
                    QStringLiteral("Drawing edge dimension '%1' needs a current source edge selection.").arg(edge.id),
                    "Review the edge reference and recreate this dimension if needed.");
        }
        for(const DrawingBalloonAnnotation& balloon:part.drawingSettings.balloonAnnotations)
            if(!document.find(balloon.sourceBodyId))
                add(report,AuditSeverity::Error,"Drawing","BALLOON_SOURCE_MISSING",part.id,
                    QStringLiteral("BOM balloon '%1' refers to a missing source body.").arg(balloon.id),
                    "Reconnect or remove the outdated balloon.");
    }
    return report;
}

QString DocumentAuditReport::toPlainText() const {
    QStringList lines;
    lines<<QStringLiteral("CADWorkbench document preflight")
         <<QStringLiteral("Bodies: %1 | Errors: %2 | Warnings: %3 | Information: %4")
                .arg(bodyCount).arg(errorCount).arg(warningCount).arg(informationCount)
         <<QStringLiteral("Scope: saved model state and project-defined data checks. Not engineering certification.")
         <<QString();
    for(const AuditFinding& finding:findings) {
        lines<<QStringLiteral("[%1] %2 / %3%4: %5").arg(severityName(finding.severity),
            finding.area,finding.code,finding.bodyId.isEmpty()?QString():" ("+finding.bodyId+")",
            finding.message);
        if(!finding.suggestedAction.isEmpty())lines<<QStringLiteral("    Next: %1").arg(finding.suggestedAction);
    }
    return lines.join('\n')+'\n';
}

QByteArray DocumentAuditReport::toCsv() const {
    QByteArray bytes="Severity,Area,Code,Body ID,Message,Suggested action\r\n";
    for(const AuditFinding& finding:findings) {
        const QStringList columns={severityName(finding.severity),finding.area,finding.code,
            finding.bodyId,finding.message,finding.suggestedAction};
        QStringList escaped;
        for(const QString& column:columns)escaped.append(csvCell(column));
        bytes.append(escaped.join(',').toUtf8());
        bytes.append("\r\n");
    }
    return bytes;
}

} // namespace cad

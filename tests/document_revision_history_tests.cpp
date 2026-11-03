#include "../src/core/DocumentRevisionHistory.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
int checks=0;
void require(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
QJsonObject body(const QString& id,const QString& name,const QString& token,const QString& brep,const QString& revision,const QString& history){
    QJsonObject item;item.insert("id",id);item.insert("name",name);item.insert("geometry_token",token);
    item.insert("brep",brep);item.insert("revision",revision);item.insert("visible",true);
    item.insert("parametric_definition",QJsonObject{{"feature_history",QJsonArray{history}}});
    item.insert("named_parameters",QJsonObject{{"width",revision=="A"?10.0:12.0}});
    item.insert("assembly_joint_type",revision=="A"?"Fixed":"Revolute");
    item.insert("drawing_settings",QJsonObject{{"note_annotations",QJsonArray{
        QJsonObject{{"id","note-1"},{"text",revision=="A"?"Initial note":"Updated note"}}}}});
    return item;
}
QByteArray scene(const QString& firstToken,const QString& firstRevision,bool includeSecond,const QString& firstBrep="brep-1"){
    QJsonArray parts;parts.append(body("body-1","Bracket",firstToken,firstBrep,firstRevision,firstRevision=="A"?"Extrude 10":"Extrude 12"));
    if(includeSecond)parts.append(body("body-2","Pin","pin-token","brep-2","A","Imported solid"));
    QJsonObject root;root.insert("schema","cadworkbench.scene.v19");root.insert("units","mm");root.insert("parts",parts);
    root.insert("drawing_readiness_policy",QJsonObject{{"required_title_block_fields",
        firstRevision=="A"?QJsonArray{"drawing_number"}:QJsonArray{"drawing_number","revision"}}});
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}
bool writeFile(const QString& path,const QByteArray& bytes){QFile file(path);return file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size();}
QByteArray readFile(const QString& path){QFile file(path);return file.open(QIODevice::ReadOnly)?file.readAll():QByteArray{};}
}

int main(int argc,char** argv){
    QCoreApplication application(argc,argv);
    try{
        QTemporaryDir temporary;require(temporary.isValid(),"Could not create revision-history test directory");
        const QString scenePath=QDir(temporary.path()).filePath("assembly.cadscene");
        const QByteArray firstScene=scene("token-a","A",true);
        require(writeFile(scenePath,firstScene),"Could not write first native scene fixture");
        QString firstId,error;
        require(cad::DocumentRevisionHistory::create(scenePath,"Released A","Initial released baseline",&firstId,&error),
                "Could not create the first named scene revision");
        require(!firstId.isEmpty(),"Revision creation did not return an ID");
        QString automaticId;
        require(cad::DocumentRevisionHistory::createIfChanged(scenePath,"Autosave","Unchanged scene bytes",&automaticId,&error)&&
                automaticId==firstId,
                "Automatic checkpointing created a duplicate revision for unchanged scene bytes");
        require(writeFile(scenePath,scene("token-b","B",false)),"Could not write edited native scene fixture");
        QString secondId;
        require(cad::DocumentRevisionHistory::create(scenePath,"Released B","Changed bracket geometry and revision",&secondId,&error),
                "Could not create the second named scene revision");
        QVector<cad::DocumentRevisionSummary> revisions;
        require(cad::DocumentRevisionHistory::list(scenePath,&revisions,&error)&&revisions.size()==2&&
                revisions.first().label=="Released A"&&revisions.last().bodyCount==1,
                "Named document revisions were not listed with basic metadata");
        cad::DocumentRevisionDiff diff;
        require(cad::DocumentRevisionHistory::compare(scenePath,firstId,secondId,&diff,&error)&&diff.bodyChanges.size()==2,
                "Revision comparison did not report one body edit and one body removal");
        const auto edited=std::find_if(diff.bodyChanges.cbegin(),diff.bodyChanges.cend(),[](const auto& change){return change.bodyId=="body-1";});
        const auto removed=std::find_if(diff.bodyChanges.cbegin(),diff.bodyChanges.cend(),[](const auto& change){return change.bodyId=="body-2";});
        require(edited!=diff.bodyChanges.cend()&&edited->geometryChanged&&edited->metadataChanged&&edited->featureHistoryChanged,
                "Revision diff missed geometry, part-revision, or feature-history changes");
        require(removed!=diff.bodyChanges.cend()&&removed->removed&&!removed->added,
                "Revision diff missed a removed assembly body");
        auto hasFieldChange=[&](const QString& path){return std::any_of(diff.fieldChanges.cbegin(),diff.fieldChanges.cend(),
            [&](const auto& change){return change.path==path;});};
        require(hasFieldChange("parts/body-1/named_parameters/width")&&
                hasFieldChange("parts/body-1/assembly_joint_type")&&
                hasFieldChange("parts/body-1/drawing_settings/note_annotations/0/text")&&
                hasFieldChange("parts/body-1/parametric_definition/feature_history/0")&&
                hasFieldChange("parts/body-1/geometry_fingerprint"),
                "Revision diff did not identify geometry, parameter, assembly, drawing-note, and feature-history fields");
        require(hasFieldChange("scene/drawing_readiness_policy/required_title_block_fields/1")&&
                diff.drawingReadinessPolicyChanged,
                "Revision diff did not identify a project-level drawing policy field change");
        const auto widthChange=std::find_if(diff.fieldChanges.cbegin(),diff.fieldChanges.cend(),
            [](const auto& change){return change.path=="parts/body-1/named_parameters/width";});
        require(widthChange!=diff.fieldChanges.cend()&&widthChange->olderValue.toDouble()==10.0&&
                widthChange->newerValue.toDouble()==12.0&&!widthChange->added&&!widthChange->removed,
                "Revision field change did not retain old and new parameter values");
        QString tagError;
        require(cad::DocumentRevisionHistory::setTags(scenePath,firstId,{"baseline","release"},&tagError),
                "Could not add descriptive tags to a saved revision");
        QVector<cad::DocumentRevisionSummary> taggedList;
        require(cad::DocumentRevisionHistory::list(scenePath,&taggedList,&error)&&taggedList.first().tags==QStringList{"baseline","release"},
                "Revision tags were not persisted in the local history manifest");
        require(!cad::DocumentRevisionHistory::setTags(scenePath,firstId,{"duplicate","duplicate"},&tagError),
                "Duplicate revision tags were accepted");
        require(cad::DocumentRevisionHistory::list(scenePath,&taggedList,&error)&&taggedList.first().tags==QStringList{"baseline","release"},
                "Rejected duplicate tags changed the prior revision tags");
        require(writeFile(scenePath,scene("token-b","B",false,"tampered-brep")),
                "Could not write repeated-token geometry fixture");
        QString thirdId;
        require(cad::DocumentRevisionHistory::create(scenePath,"Released C","B-rep changed without token refresh",&thirdId,&error),
                "Could not create a revision with a reused geometry token");
        cad::DocumentRevisionDiff reusedTokenDiff;
        require(cad::DocumentRevisionHistory::compare(scenePath,secondId,thirdId,&reusedTokenDiff,&error)&&
                reusedTokenDiff.bodyChanges.size()==1&&reusedTokenDiff.bodyChanges.first().geometryChanged,
                "B-rep digest did not reveal geometry changed under a reused geometry token");
        const QString restoredPath=QDir(temporary.path()).filePath("restored.cadscene");
        require(cad::DocumentRevisionHistory::restore(scenePath,firstId,restoredPath,&error)&&readFile(restoredPath)==firstScene,
                "Revision restore did not reproduce the exact previous native scene bytes");
        require(!cad::DocumentRevisionHistory::compare(scenePath,firstId,firstId,&diff,&error),
                "Comparing a revision with itself was accepted");
        const QString storedSnapshot=scenePath+QStringLiteral(".history/snapshots/")+firstId+QStringLiteral(".cadscene");
        QFile damaged(storedSnapshot);require(damaged.open(QIODevice::Append)&&damaged.write("tamper",6)==6&&damaged.flush(),
                "Could not prepare a corrupted snapshot fixture");damaged.close();
        QVector<cad::DocumentRevisionSummary> preservedList{{QStringLiteral("sentinel")}};
        require(!cad::DocumentRevisionHistory::list(scenePath,&preservedList,&error)&&error.contains("SHA-256")&&
                preservedList.size()==1&&preservedList.first().id==QStringLiteral("sentinel"),
                "Listing accepted a corrupted checkpoint or changed the caller's existing revision list");
        require(!cad::DocumentRevisionHistory::restore(scenePath,firstId,restoredPath,&error)&&
                error.contains("SHA-256")&&readFile(restoredPath)==firstScene,
                "Corrupted snapshot was restored or changed the existing target file");

        const QString retentionPath=QDir(temporary.path()).filePath("retention.cadscene");
        require(writeFile(retentionPath,scene("token-a","A",true)),"Could not create retention-history fixture");
        QString retainedOldId,removedMiddleId,retainedNewestId;
        require(cad::DocumentRevisionHistory::create(retentionPath,"Baseline","Pinned baseline",&retainedOldId,&error),
                "Could not create the first retention fixture revision");
        require(cad::DocumentRevisionHistory::setTags(retentionPath,retainedOldId,{"release"},&error),
                "Could not tag the retention fixture baseline");
        QString automaticRetentionId;
        require(cad::DocumentRevisionHistory::createIfChanged(retentionPath,"Autosave","Unchanged baseline",&automaticRetentionId,&error)&&
                automaticRetentionId==retainedOldId,
                "Automatic checkpointing duplicated a tagged baseline");
        require(writeFile(retentionPath,scene("token-b","B",true)),"Could not update retention fixture scene");
        require(cad::DocumentRevisionHistory::createIfChanged(retentionPath,"Intermediate","Changed checkpoint",&removedMiddleId,&error)&&
                removedMiddleId!=retainedOldId,
                "Automatic checkpointing did not create a revision after scene bytes changed");
        QString duplicateMiddleId;
        require(cad::DocumentRevisionHistory::createIfChanged(retentionPath,"Autosave","Still unchanged",&duplicateMiddleId,&error)&&
                duplicateMiddleId==removedMiddleId,
                "Automatic checkpointing duplicated the latest matching scene bytes");
        require(writeFile(retentionPath,scene("token-c","C",false)),"Could not update final retention fixture scene");
        require(cad::DocumentRevisionHistory::create(retentionPath,"Current","Newest checkpoint",&retainedNewestId,&error),
                "Could not create the newest retention revision");
        int removedCount=-1;
        require(cad::DocumentRevisionHistory::prune(retentionPath,1,&removedCount,&error)&&removedCount==1,
                "Retention pruning did not remove exactly the untagged older checkpoint");
        QVector<cad::DocumentRevisionSummary> retainedList;
        require(cad::DocumentRevisionHistory::list(retentionPath,&retainedList,&error)&&retainedList.size()==2&&
                retainedList[0].id==retainedOldId&&retainedList[1].id==retainedNewestId,
                "Retention pruning did not preserve the newest revision and tagged baseline");
        require(!QFileInfo::exists(retentionPath+QStringLiteral(".history/snapshots/")+removedMiddleId+QStringLiteral(".cadscene")),
                "Pruned revision snapshot file was not removed");
        require(!cad::DocumentRevisionHistory::prune(retentionPath,0,&removedCount,&error),
                "A zero-checkpoint retention policy was accepted");

        const QString legacyPath=QDir(temporary.path()).filePath("legacy.cadscene");
        require(writeFile(legacyPath,scene("legacy-token","A",false)),"Could not create legacy-manifest fixture");
        QString legacyId;require(cad::DocumentRevisionHistory::create(legacyPath,"Legacy","Schema migration",&legacyId,&error),
                                 "Could not create the legacy-manifest snapshot");
        const QString legacyManifestPath=legacyPath+QStringLiteral(".history/manifest.json");
        QJsonObject legacyManifest=QJsonDocument::fromJson(readFile(legacyManifestPath)).object();
        legacyManifest.insert("schema","cadworkbench.revision-history.v1");
        QJsonArray legacyRecords=legacyManifest.value("revisions").toArray();
        for(int i=0;i<legacyRecords.size();++i){QJsonObject record=legacyRecords[i].toObject();record.remove("tags");legacyRecords[i]=record;}
        legacyManifest.insert("revisions",legacyRecords);
        require(writeFile(legacyManifestPath,QJsonDocument(legacyManifest).toJson(QJsonDocument::Compact)),
                "Could not rewrite the compatibility fixture as revision-history v1");
        QVector<cad::DocumentRevisionSummary> legacyList;
        require(cad::DocumentRevisionHistory::list(legacyPath,&legacyList,&error)&&legacyList.size()==1&&legacyList.first().tags.isEmpty(),
                "Legacy v1 revision manifests were not readable after adding tag support");
        require(cad::DocumentRevisionHistory::setTags(legacyPath,legacyId,{"migrated"},&error),
                "Could not update tags on a legacy revision manifest");
        const QJsonObject migratedManifest=QJsonDocument::fromJson(readFile(legacyManifestPath)).object();
        require(migratedManifest.value("schema").toString()=="cadworkbench.revision-history.v2",
                "Updating a legacy history did not write the current manifest schema");
        std::cout<<"PASS: "<<checks<<" document revision history assertions\n";
        return 0;
    }catch(const std::exception& exception){std::cerr<<"FAIL: "<<exception.what()<<"\n";return 1;}
}

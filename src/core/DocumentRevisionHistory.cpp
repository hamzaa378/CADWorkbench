#include "DocumentRevisionHistory.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMap>
#include <QSaveFile>
#include <QSet>
#include <QStringList>
#include <QUuid>

#include <algorithm>

namespace cad {
namespace {
constexpr qint64 kMaxSceneBytes = 96LL * 1024LL * 1024LL;
constexpr qint64 kMaxManifestBytes = 8LL * 1024LL * 1024LL;
constexpr int kMaxRevisionCount = 1000;
constexpr int kMaxFieldChanges = 20000;
constexpr int kMaxTagsPerRevision = 20;
constexpr int kMaxTagLength = 40;
const QString kManifestSchemaV1 = QStringLiteral("cadworkbench.revision-history.v1");
const QString kManifestSchema = QStringLiteral("cadworkbench.revision-history.v2");

bool fail(QString* error,const QString& message) {
    if(error)*error=message;
    return false;
}

QString historyRoot(const QString& scenePath) {
    return QFileInfo(scenePath).absoluteFilePath()+QStringLiteral(".history");
}

QString sha256(const QByteArray& bytes) {
    return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
}

bool validScene(const QByteArray& bytes,QJsonObject* root,QString* schema,int* bodyCount,QString* error) {
    if(bytes.isEmpty()||bytes.size()>kMaxSceneBytes)return fail(error,"Scene snapshot is empty or exceeds the 96 MiB native-file limit.");
    QJsonParseError parseError;
    const QJsonDocument document=QJsonDocument::fromJson(bytes,&parseError);
    if(parseError.error!=QJsonParseError::NoError||!document.isObject())return fail(error,"Scene snapshot is not valid JSON.");
    const QJsonObject parsed=document.object();
    const QString sceneSchema=parsed.value(QStringLiteral("schema")).toString();
    const QJsonValue rawParts=parsed.value(QStringLiteral("parts"));
    if(!sceneSchema.startsWith(QStringLiteral("cadworkbench.scene.v"))||!rawParts.isArray()||rawParts.toArray().size()>2000)
        return fail(error,"File is not a supported CADWorkbench scene snapshot.");
    QSet<QString> ids;
    for(const QJsonValue& rawPart:rawParts.toArray()){
        if(!rawPart.isObject())return fail(error,"Scene snapshot contains an invalid body record.");
        const QJsonObject part=rawPart.toObject();
        const QString id=part.value(QStringLiteral("id")).toString().trimmed();
        const QString name=part.value(QStringLiteral("name")).toString();
        if(id.isEmpty()||id.size()>64||name.trimmed().isEmpty()||name.size()>120||
           !part.value(QStringLiteral("brep")).isString()||part.value(QStringLiteral("brep")).toString().isEmpty()||ids.contains(id))
            return fail(error,"Scene snapshot contains a missing, duplicate, or malformed body identity or B-rep.");
        ids.insert(id);
    }
    if(root)*root=parsed;
    if(schema)*schema=sceneSchema;
    if(bodyCount)*bodyCount=rawParts.toArray().size();
    return true;
}

bool readFile(const QString& path,qint64 maxBytes,QByteArray* bytes,QString* error) {
    if(!bytes)return fail(error,"Provide a byte buffer for file input.");
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))return fail(error,QStringLiteral("Could not read %1: %2").arg(QFileInfo(path).fileName(),file.errorString()));
    if(file.size()<0||file.size()>maxBytes)return fail(error,QStringLiteral("%1 exceeds the supported size limit.").arg(QFileInfo(path).fileName()));
    QByteArray data=file.readAll();
    if(file.error()!=QFileDevice::NoError)return fail(error,QStringLiteral("Could not completely read %1.").arg(QFileInfo(path).fileName()));
    *bytes=std::move(data);return true;
}

QJsonObject summaryToJson(const DocumentRevisionSummary& revision) {
    QJsonObject item;
    item.insert(QStringLiteral("id"),revision.id);
    item.insert(QStringLiteral("label"),revision.label);
    item.insert(QStringLiteral("note"),revision.note);
    item.insert(QStringLiteral("created_utc"),revision.createdUtc);
    item.insert(QStringLiteral("snapshot_file"),revision.snapshotFile);
    item.insert(QStringLiteral("sha256"),revision.sha256);
    item.insert(QStringLiteral("scene_schema"),revision.sceneSchema);
    item.insert(QStringLiteral("body_count"),revision.bodyCount);
    item.insert(QStringLiteral("tags"),QJsonArray::fromStringList(revision.tags));
    return item;
}

bool parseSummary(const QJsonValue& raw,DocumentRevisionSummary* result) {
    if(!raw.isObject()||!result)return false;
    const QJsonObject item=raw.toObject();
    const QString id=item.value(QStringLiteral("id")).toString();
    const QUuid parsedId(id);
    const QString snapshot=item.value(QStringLiteral("snapshot_file")).toString();
    const QString hash=item.value(QStringLiteral("sha256")).toString();
    const int bodyCount=item.value(QStringLiteral("body_count")).toInt(-1);
    const QJsonValue rawTags=item.value(QStringLiteral("tags"));
    if((item.size()!=8&&item.size()!=9)||parsedId.isNull()||parsedId.toString(QUuid::WithoutBraces)!=id||snapshot!=(id+QStringLiteral(".cadscene"))||
       hash.size()!=64||!std::all_of(hash.cbegin(),hash.cend(),[](QChar c){return c.isDigit()||(c>='a'&&c<='f');})||
       !item.value(QStringLiteral("label")).isString()||item.value(QStringLiteral("label")).toString().trimmed().isEmpty()||
       item.value(QStringLiteral("label")).toString().size()>80||!item.value(QStringLiteral("note")).isString()||
       item.value(QStringLiteral("note")).toString().size()>500||!item.value(QStringLiteral("created_utc")).isString()||
       !item.value(QStringLiteral("scene_schema")).isString()||bodyCount<0||bodyCount>2000||
       (item.contains(QStringLiteral("tags"))&&(!rawTags.isArray()||rawTags.toArray().size()>kMaxTagsPerRevision)))
        return false;
    DocumentRevisionSummary parsed;parsed.id=id;parsed.label=item.value(QStringLiteral("label")).toString();
    parsed.note=item.value(QStringLiteral("note")).toString();parsed.createdUtc=item.value(QStringLiteral("created_utc")).toString();
    parsed.snapshotFile=snapshot;parsed.sha256=hash;parsed.sceneSchema=item.value(QStringLiteral("scene_schema")).toString();
    parsed.bodyCount=bodyCount;
    QSet<QString> tags;
    for(const QJsonValue& rawTag:rawTags.toArray()){
        if(!rawTag.isString())return false;
        const QString tag=rawTag.toString().trimmed();
        if(tag.isEmpty()||tag.size()>kMaxTagLength||tag.contains('\n')||tag.contains('\r')||tags.contains(tag))return false;
        tags.insert(tag);parsed.tags.append(tag);
    }
    *result=std::move(parsed);return true;
}

bool readManifest(const QString& scenePath,QVector<DocumentRevisionSummary>* revisions,QString* error) {
    if(!revisions)return fail(error,"Provide an output vector for document revisions.");
    const QString path=QDir(historyRoot(scenePath)).filePath(QStringLiteral("manifest.json"));
    if(!QFileInfo::exists(path)){revisions->clear();return true;}
    QByteArray bytes;if(!readFile(path,kMaxManifestBytes,&bytes,error))return false;
    QJsonParseError parseError;const QJsonDocument document=QJsonDocument::fromJson(bytes,&parseError);
    if(parseError.error!=QJsonParseError::NoError||!document.isObject())return fail(error,"Revision history manifest is not valid JSON.");
    const QJsonObject root=document.object();const QJsonValue rawRevisions=root.value(QStringLiteral("revisions"));
    const QString manifestSchema=root.value(QStringLiteral("schema")).toString();
    if(root.size()!=2||(manifestSchema!=kManifestSchema&&manifestSchema!=kManifestSchemaV1)||!rawRevisions.isArray()||
       rawRevisions.toArray().size()>kMaxRevisionCount)return fail(error,"Revision history manifest has an unsupported schema or invalid revision list.");
    QVector<DocumentRevisionSummary> parsed;QSet<QString> ids;
    for(const QJsonValue& raw:rawRevisions.toArray()){
        DocumentRevisionSummary revision;
        if(!parseSummary(raw,&revision)||ids.contains(revision.id))return fail(error,"Revision history manifest contains an invalid or duplicate revision record.");
        ids.insert(revision.id);parsed.append(std::move(revision));
    }
    *revisions=std::move(parsed);return true;
}

bool writeManifest(const QString& scenePath,const QVector<DocumentRevisionSummary>& revisions,QString* error) {
    if(revisions.size()>kMaxRevisionCount)return fail(error,"Revision history reached its 1,000 checkpoint limit.");
    QDir root;
    const QString path=historyRoot(scenePath);
    if(!root.mkpath(path))return fail(error,"Could not create the local document history folder.");
    QJsonArray array;for(const DocumentRevisionSummary& revision:revisions)array.append(summaryToJson(revision));
    QJsonObject manifest;manifest.insert(QStringLiteral("schema"),kManifestSchema);manifest.insert(QStringLiteral("revisions"),array);
    const QByteArray bytes=QJsonDocument(manifest).toJson(QJsonDocument::Compact);
    if(bytes.size()>kMaxManifestBytes)return fail(error,"Revision history manifest exceeds its size limit.");
    QSaveFile file(QDir(path).filePath(QStringLiteral("manifest.json")));
    if(!file.open(QIODevice::WriteOnly))return fail(error,QStringLiteral("Could not update the revision history manifest: %1").arg(file.errorString()));
    if(file.write(bytes)!=bytes.size()){file.cancelWriting();return fail(error,"Could not write the complete revision history manifest.");}
    if(!file.commit())return fail(error,QStringLiteral("Could not finalize the revision history manifest: %1").arg(file.errorString()));
    return true;
}

const DocumentRevisionSummary* findRevision(const QVector<DocumentRevisionSummary>& revisions,const QString& id) {
    const auto it=std::find_if(revisions.cbegin(),revisions.cend(),[&](const DocumentRevisionSummary& item){return item.id==id;});
    return it==revisions.cend()?nullptr:&*it;
}

bool readSnapshot(const QString& scenePath,const DocumentRevisionSummary& revision,QByteArray* bytes,
                  QJsonObject* root,QString* error) {
    const QString path=QDir(historyRoot(scenePath)).filePath(QStringLiteral("snapshots/")+revision.snapshotFile);
    QByteArray content;if(!readFile(path,kMaxSceneBytes,&content,error))return false;
    if(sha256(content)!=revision.sha256)return fail(error,"Revision snapshot failed its SHA-256 integrity check.");
    QString schema;int count=0;QJsonObject parsed;
    if(!validScene(content,&parsed,&schema,&count,error))return false;
    if(schema!=revision.sceneSchema||count!=revision.bodyCount)return fail(error,"Revision snapshot metadata does not match the history manifest.");
    if(bytes)*bytes=std::move(content);
    if(root)*root=std::move(parsed);
    return true;
}

QMap<QString,QJsonObject> bodyMap(const QJsonObject& root) {
    QMap<QString,QJsonObject> result;
    for(const QJsonValue& value:root.value(QStringLiteral("parts")).toArray()){
        const QJsonObject part=value.toObject();result.insert(part.value(QStringLiteral("id")).toString(),part);
    }
    return result;
}

QString geometryIdentity(const QJsonObject& body) {
    const QString token=body.value(QStringLiteral("geometry_token")).toString();
    const QString brepHash=sha256(body.value(QStringLiteral("brep")).toString().toUtf8());
    if(!token.isEmpty())return QStringLiteral("token:")+token+QStringLiteral(";brep:")+brepHash;
    return QStringLiteral("brep:")+brepHash;
}

QString metadataIdentity(QJsonObject body) {
    body.remove(QStringLiteral("brep"));body.remove(QStringLiteral("geometry_token"));
    return sha256(QJsonDocument(body).toJson(QJsonDocument::Compact));
}

QString escapedPathSegment(QString value) {
    value.replace('~',QStringLiteral("~0"));
    value.replace('/',QStringLiteral("~1"));
    return value;
}

void appendFieldChange(const QString& path,const QJsonValue& olderValue,bool hasOlder,
                       const QJsonValue& newerValue,bool hasNewer,
                       QVector<DocumentRevisionDiff::FieldChange>* output,bool* truncated) {
    if(!output||!truncated||*truncated)return;
    if(hasOlder&&hasNewer&&olderValue==newerValue)return;
    if(hasOlder&&hasNewer&&olderValue.isObject()&&newerValue.isObject()){
        const QJsonObject older=olderValue.toObject(),newer=newerValue.toObject();
        QSet<QString> keys;for(const QString& key:older.keys())keys.insert(key);for(const QString& key:newer.keys())keys.insert(key);
        QStringList ordered=keys.values();std::sort(ordered.begin(),ordered.end());
        for(const QString& key:ordered){
            if(*truncated)return;
            const bool oldContains=older.contains(key),newContains=newer.contains(key);
            appendFieldChange(path+QLatin1Char('/')+escapedPathSegment(key),older.value(key),oldContains,
                              newer.value(key),newContains,output,truncated);
        }
        return;
    }
    if(hasOlder&&hasNewer&&olderValue.isArray()&&newerValue.isArray()){
        const QJsonArray older=olderValue.toArray(),newer=newerValue.toArray();
        const qsizetype count=std::max(older.size(),newer.size());
        for(qsizetype index=0;index<count;++index){
            if(*truncated)return;
            const bool oldContains=index<older.size(),newContains=index<newer.size();
            appendFieldChange(path+QLatin1Char('/')+QString::number(index),
                              oldContains?older.at(index):QJsonValue{},oldContains,
                              newContains?newer.at(index):QJsonValue{},newContains,output,truncated);
        }
        return;
    }
    if(output->size()>=kMaxFieldChanges){*truncated=true;return;}
    DocumentRevisionDiff::FieldChange change;change.path=path;
    change.olderValue=hasOlder?olderValue:QJsonValue{};change.newerValue=hasNewer?newerValue:QJsonValue{};
    change.added=!hasOlder;change.removed=!hasNewer;output->append(std::move(change));
}

bool validateTags(const QStringList& input,QStringList* normalized,QString* error) {
    if(!normalized)return fail(error,"Provide an output list for revision tags.");
    if(input.size()>kMaxTagsPerRevision)return fail(error,QStringLiteral("A revision can have at most %1 tags.").arg(kMaxTagsPerRevision));
    QStringList result;QSet<QString> unique;
    for(const QString& raw:input){
        const QString tag=raw.trimmed();
        if(tag.isEmpty()||tag.size()>kMaxTagLength||tag.contains('\n')||tag.contains('\r'))
            return fail(error,QStringLiteral("Revision tags must contain 1–%1 single-line characters.").arg(kMaxTagLength));
        if(unique.contains(tag))return fail(error,"Revision tags must be unique.");
        unique.insert(tag);result.append(tag);
    }
    *normalized=std::move(result);return true;
}

QJsonValue featureHistory(const QJsonObject& body) {
    return body.value(QStringLiteral("parametric_definition")).toObject().value(QStringLiteral("feature_history"));
}
} // namespace

bool DocumentRevisionHistory::create(const QString& scenePath,const QString& label,const QString& note,
                                     QString* revisionId,QString* error) {
    if(scenePath.trimmed().isEmpty()||!revisionId)return fail(error,"Provide a saved scene path and an output for the new revision ID.");
    const QString cleanLabel=label.trimmed();
    if(cleanLabel.isEmpty()||cleanLabel.size()>80||cleanLabel.contains('\n')||cleanLabel.contains('\r')||note.size()>500)
        return fail(error,"Revision labels must contain 1–80 characters and notes at most 500 characters.");
    QByteArray bytes;if(!readFile(scenePath,kMaxSceneBytes,&bytes,error))return false;
    QJsonObject scene;QString schema;int bodyCount=0;
    if(!validScene(bytes,&scene,&schema,&bodyCount,error))return false;
    QVector<DocumentRevisionSummary> revisions;if(!readManifest(scenePath,&revisions,error))return false;
    if(revisions.size()>=kMaxRevisionCount)return fail(error,"Revision history reached its 1,000 checkpoint limit.");
    const QString id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    DocumentRevisionSummary summary;summary.id=id;summary.label=cleanLabel;summary.note=note.trimmed();
    summary.createdUtc=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    summary.snapshotFile=id+QStringLiteral(".cadscene");summary.sha256=sha256(bytes);summary.sceneSchema=schema;summary.bodyCount=bodyCount;
    const QString folder=QDir(historyRoot(scenePath)).filePath(QStringLiteral("snapshots"));
    if(!QDir().mkpath(folder))return fail(error,"Could not create the CAD revision snapshot folder.");
    QSaveFile snapshot(QDir(folder).filePath(summary.snapshotFile));
    if(!snapshot.open(QIODevice::WriteOnly))return fail(error,QStringLiteral("Could not create revision snapshot: %1").arg(snapshot.errorString()));
    if(snapshot.write(bytes)!=bytes.size()){snapshot.cancelWriting();return fail(error,"Could not write the complete CAD revision snapshot.");}
    if(!snapshot.commit())return fail(error,QStringLiteral("Could not finalize CAD revision snapshot: %1").arg(snapshot.errorString()));
    revisions.append(summary);
    if(!writeManifest(scenePath,revisions,error)){
        QFile::remove(QDir(folder).filePath(summary.snapshotFile));
        return false;
    }
    *revisionId=id;return true;
}

bool DocumentRevisionHistory::createIfChanged(const QString& scenePath,const QString& label,const QString& note,
                                              QString* revisionId,QString* error) {
    if(scenePath.trimmed().isEmpty()||!revisionId)return fail(error,"Provide a saved scene path and an output for the automatic revision ID.");
    const QString cleanLabel=label.trimmed();
    if(cleanLabel.isEmpty()||cleanLabel.size()>80||cleanLabel.contains('\n')||cleanLabel.contains('\r')||note.size()>500)
        return fail(error,"Revision labels must contain 1–80 characters and notes at most 500 characters.");
    QByteArray bytes;if(!readFile(scenePath,kMaxSceneBytes,&bytes,error))return false;
    if(!validScene(bytes,nullptr,nullptr,nullptr,error))return false;
    QVector<DocumentRevisionSummary> revisions;if(!readManifest(scenePath,&revisions,error))return false;
    if(!revisions.isEmpty()){
        const DocumentRevisionSummary& latest=revisions.last();
        if(!readSnapshot(scenePath,latest,nullptr,nullptr,error))return false;
        if(latest.sha256==sha256(bytes)){
            *revisionId=latest.id;
            if(error)error->clear();
            return true;
        }
    }
    return create(scenePath,cleanLabel,note,revisionId,error);
}

bool DocumentRevisionHistory::list(const QString& scenePath,QVector<DocumentRevisionSummary>* revisions,QString* error) {
    if(scenePath.trimmed().isEmpty())return fail(error,"Provide the saved scene path whose revisions should be listed.");
    if(!revisions)return fail(error,"Provide an output vector for document revisions.");
    QVector<DocumentRevisionSummary> parsed;if(!readManifest(scenePath,&parsed,error))return false;
    for(const DocumentRevisionSummary& revision:parsed){
        if(!readSnapshot(scenePath,revision,nullptr,nullptr,error))return false;
    }
    *revisions=std::move(parsed);return true;
}

bool DocumentRevisionHistory::compare(const QString& scenePath,const QString& olderId,const QString& newerId,
                                      DocumentRevisionDiff* diff,QString* error) {
    if(!diff)return fail(error,"Provide an output record for the revision comparison.");
    QVector<DocumentRevisionSummary> revisions;if(!readManifest(scenePath,&revisions,error))return false;
    const auto* older=findRevision(revisions,olderId);const auto* newer=findRevision(revisions,newerId);
    if(!older||!newer)return fail(error,"One or both document revision IDs were not found.");
    if(older==newer)return fail(error,"Choose two different document revisions to compare.");
    QJsonObject oldRoot,newRoot;
    if(!readSnapshot(scenePath,*older,nullptr,&oldRoot,error)||!readSnapshot(scenePath,*newer,nullptr,&newRoot,error))return false;
    const auto oldBodies=bodyMap(oldRoot),newBodies=bodyMap(newRoot);
    QSet<QString> ids;for(auto it=oldBodies.cbegin();it!=oldBodies.cend();++it)ids.insert(it.key());
    for(auto it=newBodies.cbegin();it!=newBodies.cend();++it)ids.insert(it.key());
    DocumentRevisionDiff result;result.olderRevisionId=olderId;result.newerRevisionId=newerId;
    result.olderSceneSchema=older->sceneSchema;result.newerSceneSchema=newer->sceneSchema;
    result.drawingReadinessPolicyChanged=oldRoot.value(QStringLiteral("drawing_readiness_policy"))!=newRoot.value(QStringLiteral("drawing_readiness_policy"));
    QJsonObject olderSceneFields=oldRoot,newerSceneFields=newRoot;
    olderSceneFields.remove(QStringLiteral("parts"));newerSceneFields.remove(QStringLiteral("parts"));
    appendFieldChange(QStringLiteral("scene"),olderSceneFields,true,newerSceneFields,true,
                      &result.fieldChanges,&result.fieldChangesTruncated);
    QStringList orderedIds=ids.values();std::sort(orderedIds.begin(),orderedIds.end());
    for(const QString& id:orderedIds){
        const bool inOld=oldBodies.contains(id),inNew=newBodies.contains(id);
        const bool historyChanged=inOld&&inNew&&featureHistory(oldBodies.value(id))!=featureHistory(newBodies.value(id));
        if(inOld&&inNew&&geometryIdentity(oldBodies.value(id))==geometryIdentity(newBodies.value(id))&&
           metadataIdentity(oldBodies.value(id))==metadataIdentity(newBodies.value(id))&&!historyChanged)continue;
        DocumentRevisionBodyChange change;change.bodyId=id;
        change.bodyName=inNew?newBodies.value(id).value(QStringLiteral("name")).toString():oldBodies.value(id).value(QStringLiteral("name")).toString();
        change.added=!inOld;change.removed=!inNew;
        if(inOld&&inNew){change.geometryChanged=geometryIdentity(oldBodies.value(id))!=geometryIdentity(newBodies.value(id));
            change.metadataChanged=metadataIdentity(oldBodies.value(id))!=metadataIdentity(newBodies.value(id));
            change.featureHistoryChanged=historyChanged;
            if(change.geometryChanged)
                appendFieldChange(QStringLiteral("parts/")+escapedPathSegment(id)+QStringLiteral("/geometry_fingerprint"),
                                  QJsonValue(geometryIdentity(oldBodies.value(id))),true,
                                  QJsonValue(geometryIdentity(newBodies.value(id))),true,
                                  &result.fieldChanges,&result.fieldChangesTruncated);
            QJsonObject olderMetadata=oldBodies.value(id),newerMetadata=newBodies.value(id);
            olderMetadata.remove(QStringLiteral("brep"));olderMetadata.remove(QStringLiteral("geometry_token"));
            newerMetadata.remove(QStringLiteral("brep"));newerMetadata.remove(QStringLiteral("geometry_token"));
            appendFieldChange(QStringLiteral("parts/")+escapedPathSegment(id),olderMetadata,true,newerMetadata,true,
                              &result.fieldChanges,&result.fieldChangesTruncated);}
        result.bodyChanges.append(std::move(change));
    }
    *diff=std::move(result);return true;
}

bool DocumentRevisionHistory::restore(const QString& scenePath,const QString& revisionId,const QString& targetPath,QString* error) {
    if(scenePath.trimmed().isEmpty()||targetPath.trimmed().isEmpty())return fail(error,"Provide both the source scene and restored-scene path.");
    QVector<DocumentRevisionSummary> revisions;if(!readManifest(scenePath,&revisions,error))return false;
    const auto* revision=findRevision(revisions,revisionId);
    if(!revision)return fail(error,"The requested document revision was not found.");
    QByteArray bytes;if(!readSnapshot(scenePath,*revision,&bytes,nullptr,error))return false;
    const QString target=QFileInfo(targetPath).absoluteFilePath();
    const QString history=QFileInfo(historyRoot(scenePath)).absoluteFilePath();
    if(target.startsWith(history+QDir::separator(),Qt::CaseInsensitive))return fail(error,"A restored scene cannot overwrite a file inside its revision-history folder.");
    QSaveFile file(target);
    if(!file.open(QIODevice::WriteOnly))return fail(error,QStringLiteral("Could not create restored scene: %1").arg(file.errorString()));
    if(file.write(bytes)!=bytes.size()){file.cancelWriting();return fail(error,"Could not write the complete restored scene.");}
    if(!file.commit())return fail(error,QStringLiteral("Could not finalize restored scene: %1").arg(file.errorString()));
    return true;
}

bool DocumentRevisionHistory::setTags(const QString& scenePath,const QString& revisionId,const QStringList& tags,
                                      QString* error) {
    if(scenePath.trimmed().isEmpty())return fail(error,"Provide the scene whose revision should be tagged.");
    QStringList normalized;if(!validateTags(tags,&normalized,error))return false;
    QVector<DocumentRevisionSummary> revisions;if(!readManifest(scenePath,&revisions,error))return false;
    int index=-1;for(int i=0;i<revisions.size();++i)if(revisions[i].id==revisionId){index=i;break;}
    if(index<0)return fail(error,"The requested document revision was not found.");
    if(!readSnapshot(scenePath,revisions[index],nullptr,nullptr,error))return false;
    revisions[index].tags=std::move(normalized);
    return writeManifest(scenePath,revisions,error);
}

bool DocumentRevisionHistory::prune(const QString& scenePath,int keepCount,int* removedCount,QString* error) {
    if(scenePath.trimmed().isEmpty())return fail(error,"Provide the saved scene whose revision history should be pruned.");
    if(keepCount<1||keepCount>kMaxRevisionCount)return fail(error,"Retention count must be between 1 and 1,000 revisions.");
    QVector<DocumentRevisionSummary> revisions;
    if(!list(scenePath,&revisions,error))return false;
    if(revisions.isEmpty()){if(removedCount)*removedCount=0;return true;}
    QSet<QString> retained;
    const qsizetype firstNewest=std::max<qsizetype>(
        0,revisions.size()-static_cast<qsizetype>(keepCount));
    for(qsizetype i=firstNewest;i<revisions.size();++i)retained.insert(revisions[i].id);
    for(const DocumentRevisionSummary& revision:revisions)if(!revision.tags.isEmpty())retained.insert(revision.id);
    QVector<DocumentRevisionSummary> kept,removed;
    kept.reserve(revisions.size());removed.reserve(revisions.size());
    for(const DocumentRevisionSummary& revision:revisions){
        if(retained.contains(revision.id))kept.append(revision);else removed.append(revision);
    }
    if(removed.isEmpty()){if(removedCount)*removedCount=0;return true;}
    if(!writeManifest(scenePath,kept,error))return false;
    const QString snapshotFolder=QDir(historyRoot(scenePath)).filePath(QStringLiteral("snapshots"));
    for(const DocumentRevisionSummary& revision:removed)
        QFile::remove(QDir(snapshotFolder).filePath(revision.snapshotFile));
    if(removedCount)*removedCount=removed.size();
    return true;
}

} // namespace cad

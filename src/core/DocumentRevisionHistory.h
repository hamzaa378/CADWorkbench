#pragma once

#include <QString>
#include <QStringList>
#include <QJsonValue>
#include <QVector>

namespace cad {

struct DocumentRevisionSummary {
    QString id;
    QString label;
    QString note;
    QString createdUtc;
    QString snapshotFile;
    QString sha256;
    QString sceneSchema;
    int bodyCount = 0;
    QStringList tags;
};

struct DocumentRevisionBodyChange {
    QString bodyId;
    QString bodyName;
    bool added = false;
    bool removed = false;
    bool geometryChanged = false;
    bool metadataChanged = false;
    bool featureHistoryChanged = false;
};

struct DocumentRevisionDiff {
    QString olderRevisionId;
    QString newerRevisionId;
    QString olderSceneSchema;
    QString newerSceneSchema;
    bool drawingReadinessPolicyChanged = false;
    // Reports added/removed bodies, geometry-token changes (or B-rep digest
    // changes for scenes without tokens), changes to the saved bounded
    // parametric feature-history list, and any remaining per-body JSON
    // changes. It compares history records but does not replay or edit them.
    QVector<DocumentRevisionBodyChange> bodyChanges;
    struct FieldChange {
        QString path;
        QJsonValue olderValue;
        QJsonValue newerValue;
        bool added = false;
        bool removed = false;
    };
    QVector<FieldChange> fieldChanges;
    bool fieldChangesTruncated = false;
};

// Local, immutable checkpoints for saved CADWorkbench scene files. The
// history is stored beside the scene in <scene>.history and remains offline.
// SHA-256 is verified before comparison or restore. Restore writes a target
// scene atomically and never modifies its source checkpoint. This service does
// not replace in-memory undo/redo or the part revision field.
class DocumentRevisionHistory {
public:
    static bool create(const QString& scenePath,const QString& label,const QString& note,
                       QString* revisionId,QString* error = nullptr);
    // Creates a checkpoint only if the current saved scene bytes differ from
    // the most recent checkpoint. Manual create() always creates a new record.
    static bool createIfChanged(const QString& scenePath,const QString& label,const QString& note,
                                QString* revisionId,QString* error = nullptr);
    static bool list(const QString& scenePath,QVector<DocumentRevisionSummary>* revisions,
                     QString* error = nullptr);
    static bool compare(const QString& scenePath,const QString& olderRevisionId,
                        const QString& newerRevisionId,DocumentRevisionDiff* diff,
                        QString* error = nullptr);
    // Restores a verified snapshot to targetPath using an atomic file replace.
    // A separate path is recommended so the current working file is retained.
    static bool restore(const QString& scenePath,const QString& revisionId,
                        const QString& targetPath,QString* error = nullptr);
    // Tags are stored in the manifest and do not change immutable snapshot bytes.
    static bool setTags(const QString& scenePath,const QString& revisionId,const QStringList& tags,
                        QString* error = nullptr);
    // Retains the newest keepCount checkpoints plus all tagged checkpoints.
    // The manifest is updated atomically before obsolete snapshot files are removed.
    static bool prune(const QString& scenePath,int keepCount,int* removedCount = nullptr,
                      QString* error = nullptr);
};

} // namespace cad

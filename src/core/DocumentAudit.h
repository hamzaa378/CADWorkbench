#pragma once

#include "SceneDocument.h"

#include <QByteArray>
#include <QString>
#include <QVector>

namespace cad {

// Read-only project-wide preflight. The findings describe observable document
// state; they are not certification of an engineering or manufacturing design.
enum class AuditSeverity { Information, Warning, Error };

struct AuditFinding {
    AuditSeverity severity = AuditSeverity::Information;
    QString area;
    QString code;
    QString bodyId;
    QString message;
    QString suggestedAction;
};

struct DocumentAuditReport {
    int bodyCount = 0;
    int errorCount = 0;
    int warningCount = 0;
    int informationCount = 0;
    QVector<AuditFinding> findings;
    bool passed() const noexcept { return errorCount == 0; }
    QString toPlainText() const;
    QByteArray toCsv() const;
};

// Does not mutate the model, acknowledge revision tokens, or save the project.
// Standards data checks are run once; additional findings cover model links
// and feature states that the project checklist does not inspect.
DocumentAuditReport runDocumentAudit(const SceneDocument& document);

} // namespace cad

#pragma once
#include "HoleTable.h"
#include "DrillingFeasibility.h"
#include <QByteArray>
#include <QString>
namespace cad {
struct DrillingReviewSettings {
    // Strict comma-separated <diameter-mm>:<usable-length-mm> entries.
    QString toolCatalog = QStringLiteral("5:30,8:30");
    double clearanceAboveStockMm = 10;
    double diameterToleranceMm = 0.05;
    bool fixtureExitReviewed = false;
};
// Creates a versioned, machine-neutral worksheet from fresh solid geometry.
// It does not generate G-code or approve material, fixtures or machining.
// Errors leave the output buffer unchanged.
bool buildDrillingWorksheetCsv(const Part& part,const DrillingReviewSettings& settings,
                              QByteArray* output,QString* error=nullptr);
bool exportDrillingWorksheetCsv(const Part& part,const DrillingReviewSettings& settings,
                               const QString& path,QString* error=nullptr);
}

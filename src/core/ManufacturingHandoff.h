#pragma once
#include <QString>

namespace cad {
class SceneDocument;

struct ManufacturingHandoffResult {
    QString folder;
    int bodyCount=0;
    int bomLines=0;
    int warningCount=0;
    int procurementExceptions=0;
    int inspectionRequirements=0;
    int axialHolesReviewed=0; // observed conservative Z-axis cylinders only
    int machiningBodiesRequiringReview=0; // not evaluated or excluded geometry
};

// Transactionally export an UNISSUED manufacturing handoff from one saved
// project revision. This is NOT design approval or ISO/ASME certification.
// On failure, output is unchanged and the transient export folder is deleted.
bool createManufacturingHandoff(const SceneDocument& document,
        const QString& destinationParent,ManufacturingHandoffResult* result,
        QString* error=nullptr);
} // namespace cad

#pragma once

#include "SceneDocument.h"
#include <QByteArray>
#include <QStringList>

namespace cad {

enum class OrthographicProjection { FirstAngle, ThirdAngle };
enum class DrawingSheetSize { A4Landscape, A3Landscape };
enum class DrawingAuxiliaryView { Isometric, SectionA };

struct DrawingTitleBlock {
    QString drawingNumber;
    QString drawnBy;
    QString checkedBy;
    QString issueDate;
    QString revision;
};

// Joins selected model bodies into one drawing source while retaining an
// aggregate revision token; source identities are persisted on the sheet.
bool buildDrawingAssemblyPart(const QVector<Part>& parts,Part* result,QString* error = nullptr);

// Produces an A4 or A3 landscape orthographic line drawing for one solid.
// Visible and hidden edges are represented separately. Only overall projected
// envelope dimensions are generated; feature dimensions and tolerances are not inferred.
bool buildOrthographicDrawingSvg(const Part& part, QByteArray* svg, QString* error = nullptr,
                                 double* chosenScale = nullptr, int* visiblePathCount = nullptr,
                                 int* hiddenPathCount = nullptr,
                                 OrthographicProjection projection = OrthographicProjection::ThirdAngle,
                                 DrawingSheetSize sheetSize = DrawingSheetSize::A4Landscape,
                                 DrawingAuxiliaryView auxiliaryView = DrawingAuxiliaryView::Isometric,
                                 double sectionDepthFraction = 0.5,
                                 const QStringList& generalNotes = {},
                                 const DrawingTitleBlock& titleBlock = {},
                                 const QMap<QString,QPointF>& viewCenters = {},
                                 const QMap<QString,QString>& viewIds = {},
                                 const QString& sheetId = {},
                                 const QVector<DrawingNoteAnnotation>& noteAnnotations = {},
                                 const QVector<DrawingDimensionAnnotation>& dimensionAnnotations = {},
                                 const DrawingPartsListAnnotation& partsListAnnotation = {},
                                 const QVector<BillOfMaterialsRow>& bomRows = {},
                                 const QStringList& sourceBodyIds = {},
                                 const QVector<DrawingBalloonAnnotation>& balloonAnnotations = {},
                                 const QVector<DrawingDetailAnnotation>& detailAnnotations = {},
                                 const QVector<DrawingSectionAnnotation>& sectionAnnotations = {},
                                 const QVector<DrawingEdgeDimensionAnnotation>& edgeDimensionAnnotations = {},
                                 const QVector<Part>& sourceBodies = {},
                                 const QVector<DrawingGdtFrameAnnotation>& gdtFrames = {});
bool exportOrthographicDrawingSvg(const Part& part, const QString& path, QString* error = nullptr,
                                  OrthographicProjection projection = OrthographicProjection::ThirdAngle,
                                  DrawingSheetSize sheetSize = DrawingSheetSize::A4Landscape,
                                  DrawingAuxiliaryView auxiliaryView = DrawingAuxiliaryView::Isometric,
                                  double sectionDepthFraction = 0.5,
                                  const QStringList& generalNotes = {},
                                  const DrawingTitleBlock& titleBlock = {},
                                  const QMap<QString,QPointF>& viewCenters = {},
                                  const QMap<QString,QString>& viewIds = {},
                                  const QString& sheetId = {},
                                  const QVector<DrawingNoteAnnotation>& noteAnnotations = {},
                                  const QVector<DrawingDimensionAnnotation>& dimensionAnnotations = {},
                                  const DrawingPartsListAnnotation& partsListAnnotation = {},
                                  const QVector<BillOfMaterialsRow>& bomRows = {},
                                  const QStringList& sourceBodyIds = {},
                                  const QVector<DrawingBalloonAnnotation>& balloonAnnotations = {},
                                  const QVector<DrawingDetailAnnotation>& detailAnnotations = {},
                                  const QVector<DrawingSectionAnnotation>& sectionAnnotations = {},
                                  const QVector<DrawingEdgeDimensionAnnotation>& edgeDimensionAnnotations = {},
                                  const QVector<Part>& sourceBodies = {},
                                 const QVector<DrawingGdtFrameAnnotation>& gdtFrames = {});
bool exportOrthographicDrawingPdf(const QByteArray& svg,const QString& path,DrawingSheetSize sheetSize,
                                  QString* error = nullptr);
// Read-only geometry-aware preflight for an individually edited linked drawing
// dimension. It uses the same witness resolver and projection rules as SVG
// export, but never modifies the document or acknowledges a source revision.
bool validateDrawingEdgeDimensionPreview(const Part& source,
                                         const DrawingEdgeDimensionAnnotation& dimension,
                                         QString* error = nullptr,
                                         double* nominalMm = nullptr);

// Read-only batch assessment for every persisted edge-linked drawing dimension.
// Checks source existence, the current edge witness and projected measurement
// using the same geometry preflight as actual SVG export. Stale but uniquely
// resolvable is a REVIEW state, never silently acknowledged as current.
struct DrawingDimensionAuditEntry {
    QString annotationId,sourceId,kind,view,status,detail;
    // Current nominal is read from the same verified SVG measurement used at
    // export, rounded to its 0.001 mm displayed precision. Ranges are manual
    // tolerance bands, not a statement of achievable manufacturing process.
    bool hasMeasurement=false;
    double nominalMm=0.0,minimumMm=0.0,maximumMm=0.0;
};
struct DrawingDimensionAudit {
    QVector<DrawingDimensionAuditEntry> entries;
    int current=0,needsRegeneration=0,broken=0,missing=0;
    bool readyForRegeneration() const {return broken==0&&missing==0;}
};
bool auditDrawingDimensionLinks(const QVector<Part>& currentSources,
                                const DrawingSheetSettings& saved,
                                DrawingDimensionAudit* report,QString* error=nullptr);

// Exports the generated sheet as editable DXF line, circle, and text entities.
// DXF is a detached snapshot; saved model links remain in the native scene.
bool buildOrthographicDrawingDxf(const QByteArray& svg,QByteArray* dxf,QString* error = nullptr);
bool exportOrthographicDrawingDxf(const QByteArray& svg,const QString& path,QString* error = nullptr);

// Screens an exported SVG for basic drawing-content readiness. It does not certify compliance.
StandardsCheckReport checkDrawingSvg(const QByteArray& svg, StandardsProfile profile,
                                     const DrawingReadinessPolicy& policy = {});
StandardsCheckReport checkDrawingSourceSnapshot(const Part& currentPart,const QByteArray& svg);
StandardsCheckReport checkDrawingSourceSnapshot(const QVector<Part>& currentParts,const QByteArray& svg);

} // namespace cad

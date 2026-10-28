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
                                 const QVector<DrawingSectionAnnotation>& sectionAnnotations = {});
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
                                  const QVector<DrawingSectionAnnotation>& sectionAnnotations = {});
bool exportOrthographicDrawingPdf(const QByteArray& svg,const QString& path,DrawingSheetSize sheetSize,
                                  QString* error = nullptr);

// Screens an exported SVG for basic drawing-content readiness. It does not certify compliance.
StandardsCheckReport checkDrawingSvg(const QByteArray& svg, StandardsProfile profile,
                                     const DrawingReadinessPolicy& policy = {});
StandardsCheckReport checkDrawingSourceSnapshot(const Part& currentPart,const QByteArray& svg);
StandardsCheckReport checkDrawingSourceSnapshot(const QVector<Part>& currentParts,const QByteArray& svg);

} // namespace cad

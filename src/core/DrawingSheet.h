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
                                 const DrawingTitleBlock& titleBlock = {});
bool exportOrthographicDrawingSvg(const Part& part, const QString& path, QString* error = nullptr,
                                  OrthographicProjection projection = OrthographicProjection::ThirdAngle,
                                  DrawingSheetSize sheetSize = DrawingSheetSize::A4Landscape,
                                  DrawingAuxiliaryView auxiliaryView = DrawingAuxiliaryView::Isometric,
                                  double sectionDepthFraction = 0.5,
                                  const QStringList& generalNotes = {},
                                  const DrawingTitleBlock& titleBlock = {});

// Screens an exported SVG for basic drawing-content readiness. It does not certify compliance.
StandardsCheckReport checkDrawingSvg(const QByteArray& svg, StandardsProfile profile);

} // namespace cad

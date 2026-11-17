#pragma once

#include <QByteArray>
#include <QPointF>
#include <QString>
#include <QVector>

namespace cad {

struct SheetMetalRule {
    double thicknessMm = 1.5;
    double insideBendRadiusMm = 1.5;
    double kFactor = 0.33;
};

struct SheetMetalFlatPattern {
    double flatLengthMm = 0.0;
    double bendAllowanceMm = 0.0;
    double bendDeductionMm = 0.0;
    double outsideSetbackMm = 0.0;
    double firstTangentMm = 0.0;
};

struct SheetMetalBend {
    double angleDegrees = 90.0;
    double insideRadiusMm = 1.5;
    double kFactor = 0.33;
};

// Signed angle controls the fold direction; flange lengths are straight
// lengths along the neutral surface. The folded model is faceted at bends.
struct FoldedSheetBend {
    double angleDegrees = 90.0;
    double insideRadiusMm = 1.5;
};

// Returns a closed XY cross-section outline for extrusion through sheetWidth.
// Bend arcs are sampled to keep the resulting solid portable across kernels.
bool buildFoldedSheetOutline(const QVector<double>& straightFlangeLengthsMm,
                             double sheetWidthMm,double thicknessMm,
                             const QVector<FoldedSheetBend>& bends,
                             QVector<QPointF>* outline,QString* error = nullptr);

struct MultiBendFlatPattern {
    double flatLengthMm = 0.0;
    double widthMm = 0.0;
    QVector<double> bendAllowancesMm;
    QVector<double> bendDeductionsMm;
    QVector<double> bendLinePositionsMm;
};

// Verify two independently produced development representations agree in
// length, width, every bend position, allowance and deduction. Atomic output:
// callers receive the existing error untouched on success only after checks.
// Does not certify manufactured unfold accuracy.
bool equivalentFoldedSheetDevelopment(const MultiBendFlatPattern& first,
                                      const MultiBendFlatPattern& second,
                                      QString* error=nullptr);

struct SheetMetalRelief {
    double widthMm = 0.0; // zero disables end reliefs
    double depthMm = 0.0;
};

// Optional shop-specific rule checks, disabled individually at zero.
// These are user-supplied planning limits, not certification requirements.
struct SheetMetalShopLimits {
    double minimumInsideRadiusToThickness = 0.0;
    double minimumStraightFlangeMm = 0.0; // after adjacent bend setbacks
};

// Checks a valid multi-bend development against user-supplied shop limits.
// Does not generate a folded solid or impose material/tooling standards.
bool validateMultiBendShopLimits(const QVector<double>& outsideFlangeLengthsMm,
                                 double sheetWidthMm,double thicknessMm,
                                 const QVector<SheetMetalBend>& bends,
                                 const SheetMetalShopLimits& limits,
                                 QString* error = nullptr);
// Folded-body flange lengths are already straight tangent-to-tangent lengths,
// so the straight-length limit applies directly. This is a project check,
// not a manufacturing certification or material-specific bend table.
bool validateFoldedSheetShopLimits(const QVector<double>& straightNeutralFlangeLengthsMm,
                                   double sheetWidthMm,double thicknessMm,
                                   const QVector<FoldedSheetBend>& bends,
                                   const SheetMetalShopLimits& limits,
                                   QString* error = nullptr);

// Parses comma-separated numeric inputs without discarding empty fields.
// Invalid input leaves the caller's existing vector unchanged.
bool parseSheetMetalNumberList(const QString& text,QVector<double>* values,QString* error = nullptr);

// Calculates a single-bend flat length from outside flange dimensions. This is
// a shop-planning aid; it does not model bend relief, springback, or tooling.
bool calculateOneBendFlatPattern(double outsideFlangeAMm,double outsideFlangeBMm,
                                 double sheetWidthMm,double bendAngleDegrees,
                                 const SheetMetalRule& rule,SheetMetalFlatPattern* result,
                                 QString* error = nullptr);
bool buildOneBendFlatPatternSvg(double outsideFlangeAMm,double outsideFlangeBMm,
                                double sheetWidthMm,double bendAngleDegrees,
                                const SheetMetalRule& rule,QByteArray* svg,
                                SheetMetalFlatPattern* result = nullptr,QString* error = nullptr);
bool exportOneBendFlatPatternSvg(double outsideFlangeAMm,double outsideFlangeBMm,
                                 double sheetWidthMm,double bendAngleDegrees,
                                 const SheetMetalRule& rule,const QString& path,
                                 SheetMetalFlatPattern* result = nullptr,QString* error = nullptr);

// Calculates a linear chain of outside flange dimensions with independent bend rules.
// This is a flat-pattern aid; it does not model relief, springback, or a folded solid.
bool calculateMultiBendFlatPattern(const QVector<double>& outsideFlangeLengthsMm,double sheetWidthMm,
                                   double thicknessMm,const QVector<SheetMetalBend>& bends,
                                   MultiBendFlatPattern* result,QString* error = nullptr);
bool buildMultiBendFlatPatternDxf(const QVector<double>& outsideFlangeLengthsMm,double sheetWidthMm,
                                 double thicknessMm,const QVector<SheetMetalBend>& bends,
                                 QByteArray* dxf,MultiBendFlatPattern* result = nullptr,
                                 QString* error = nullptr);
bool buildMultiBendFlatPatternDxf(const QVector<double>& outsideFlangeLengthsMm,double sheetWidthMm,
                                 double thicknessMm,const QVector<SheetMetalBend>& bends,
                                 const SheetMetalRelief& relief,QByteArray* dxf,
                                 MultiBendFlatPattern* result = nullptr,QString* error = nullptr);
bool exportMultiBendFlatPatternDxf(const QVector<double>& outsideFlangeLengthsMm,double sheetWidthMm,
                                  double thicknessMm,const QVector<SheetMetalBend>& bends,
                                  const QString& path,MultiBendFlatPattern* result = nullptr,
                                  QString* error = nullptr);
bool exportMultiBendFlatPatternDxf(const QVector<double>& outsideFlangeLengthsMm,double sheetWidthMm,
                                  double thicknessMm,const QVector<SheetMetalBend>& bends,
                                  const SheetMetalRelief& relief,const QString& path,
                                  MultiBendFlatPattern* result = nullptr,QString* error = nullptr);

// Develops saved folded-body centerline flange lengths directly. Signed fold
// directions are preserved in the model but use absolute bend angles for the
// flat; one uniform user-selected K-factor is applied to all bends.
bool calculateFoldedSheetFlatPattern(const QVector<double>& straightNeutralFlangeLengthsMm,
                                     double sheetWidthMm,double thicknessMm,
                                     const QVector<FoldedSheetBend>& bends,double kFactor,
                                     MultiBendFlatPattern* result,QString* error = nullptr);
bool calculateFoldedSheetFlatPattern(const QVector<double>& straightNeutralFlangeLengthsMm,
                                     double sheetWidthMm,double thicknessMm,
                                     const QVector<FoldedSheetBend>& bends,const QVector<double>& kFactors,
                                     MultiBendFlatPattern* result,QString* error = nullptr);
bool buildFoldedSheetFlatPatternDxf(const QVector<double>& straightNeutralFlangeLengthsMm,
                                   double sheetWidthMm,double thicknessMm,
                                   const QVector<FoldedSheetBend>& bends,double kFactor,
                                   QByteArray* dxf,MultiBendFlatPattern* result = nullptr,
                                   QString* error = nullptr);
bool buildFoldedSheetFlatPatternDxf(const QVector<double>& straightNeutralFlangeLengthsMm,
                                   double sheetWidthMm,double thicknessMm,
                                   const QVector<FoldedSheetBend>& bends,const QVector<double>& kFactors,
                                   QByteArray* dxf,MultiBendFlatPattern* result = nullptr,
                                   QString* error = nullptr);
bool buildFoldedSheetFlatPatternDxf(const QVector<double>& straightNeutralFlangeLengthsMm,
                                   double sheetWidthMm,double thicknessMm,
                                   const QVector<FoldedSheetBend>& bends,const QVector<double>& kFactors,
                                   const SheetMetalRelief& relief,QByteArray* dxf,
                                   MultiBendFlatPattern* result = nullptr,QString* error = nullptr);
// Read-only SVG preview of the *same validated* flat blank and bend stations
// as the corresponding folded-sheet DXF, including optional end reliefs.
// Both output buffers and the pattern remain unchanged if validation fails.
bool buildFoldedSheetFlatPatternSvg(const QVector<double>& straightNeutralFlangeLengthsMm,
                                   double sheetWidthMm,double thicknessMm,
                                   const QVector<FoldedSheetBend>& bends,const QVector<double>& kFactors,
                                   const SheetMetalRelief& relief,QByteArray* svg,
                                   MultiBendFlatPattern* result=nullptr,QString* error=nullptr);
bool exportFoldedSheetFlatPatternDxf(const QVector<double>& straightNeutralFlangeLengthsMm,
                                    double sheetWidthMm,double thicknessMm,
                                    const QVector<FoldedSheetBend>& bends,double kFactor,
                                    const QString& path,MultiBendFlatPattern* result = nullptr,
                                    QString* error = nullptr);

// Bend schedule for a folded body. Stations match the developed DXF and signed
// angles retain the modeled fold direction.
bool buildFoldedSheetBendTableCsv(const QVector<double>& straightNeutralFlangeLengthsMm,
                                  double sheetWidthMm,double thicknessMm,
                                  const QVector<FoldedSheetBend>& bends,const QVector<double>& kFactors,
                                  QByteArray* csv,QString* error = nullptr);
bool exportFoldedSheetBendTableCsv(const QVector<double>& straightNeutralFlangeLengthsMm,
                                   double sheetWidthMm,double thicknessMm,
                                   const QVector<FoldedSheetBend>& bends,const QVector<double>& kFactors,
                                   const QString& path,QString* error = nullptr);
bool exportFoldedSheetFlatPatternDxf(const QVector<double>& straightNeutralFlangeLengthsMm,
                                    double sheetWidthMm,double thicknessMm,
                                    const QVector<FoldedSheetBend>& bends,const QVector<double>& kFactors,
                                    const QString& path,MultiBendFlatPattern* result = nullptr,
                                    QString* error = nullptr);
bool exportFoldedSheetFlatPatternDxf(const QVector<double>& straightNeutralFlangeLengthsMm,
                                    double sheetWidthMm,double thicknessMm,
                                    const QVector<FoldedSheetBend>& bends,const QVector<double>& kFactors,
                                    const SheetMetalRelief& relief,const QString& path,
                                    MultiBendFlatPattern* result = nullptr,QString* error = nullptr);

} // namespace cad

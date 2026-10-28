#pragma once

#include <QByteArray>
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

struct MultiBendFlatPattern {
    double flatLengthMm = 0.0;
    double widthMm = 0.0;
    QVector<double> bendAllowancesMm;
    QVector<double> bendDeductionsMm;
    QVector<double> bendLinePositionsMm;
};

struct SheetMetalRelief {
    double widthMm = 0.0; // zero disables end reliefs
    double depthMm = 0.0;
};

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

} // namespace cad

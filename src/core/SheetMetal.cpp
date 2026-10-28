#include "SheetMetal.h"

#include <QSaveFile>
#include <QTextStream>
#include <QLocale>

#include <cmath>
#include <numbers>

namespace cad {
namespace {
bool fail(QString* error,const QString& message){if(error)*error=message;return false;}
bool positiveFinite(double value){return std::isfinite(value)&&value>0.00001&&value<=1000000.0;}
}

bool calculateOneBendFlatPattern(double flangeA,double flangeB,double sheetWidth,double angleDegrees,
                                 const SheetMetalRule& rule,SheetMetalFlatPattern* result,QString* error){
    if(!result)return fail(error,"Provide an output object for the flat-pattern calculation.");
    if(!positiveFinite(flangeA)||!positiveFinite(flangeB)||!positiveFinite(sheetWidth))
        return fail(error,"Outside flange lengths and sheet width must be finite positive dimensions up to 1,000,000 mm.");
    if(!std::isfinite(angleDegrees)||angleDegrees<15.0||angleDegrees>165.0)
        return fail(error,"A single-bend angle must be between 15 and 165 degrees.");
    if(!positiveFinite(rule.thicknessMm)||!std::isfinite(rule.insideBendRadiusMm)||rule.insideBendRadiusMm<0.0||
       rule.insideBendRadiusMm>1000000.0||!std::isfinite(rule.kFactor)||rule.kFactor<0.0||rule.kFactor>1.0)
        return fail(error,"Sheet-metal thickness, inside radius, or K-factor is outside the supported range.");
    const double angle=angleDegrees*std::numbers::pi/180.0;
    const double allowance=angle*(rule.insideBendRadiusMm+rule.kFactor*rule.thicknessMm);
    const double setback=(rule.insideBendRadiusMm+rule.thicknessMm)*std::tan(angle/2.0);
    const double deduction=2.0*setback-allowance;
    const double flatLength=flangeA+flangeB-deduction;
    if(!std::isfinite(flatLength)||flatLength<=0.00001||flangeA<=setback||flangeB<=setback)
        return fail(error,"Flange lengths are too short for the selected bend radius, thickness, and angle.");
    *result={flatLength,allowance,deduction,setback,flangeA-setback+allowance/2.0};
    return true;
}

bool buildOneBendFlatPatternSvg(double flangeA,double flangeB,double sheetWidth,double angleDegrees,
                               const SheetMetalRule& rule,QByteArray* svg,SheetMetalFlatPattern* result,QString* error){
    if(!svg)return fail(error,"Provide an output buffer for the flat-pattern SVG.");
    SheetMetalFlatPattern pattern;
    if(!calculateOneBendFlatPattern(flangeA,flangeB,sheetWidth,angleDegrees,rule,&pattern,error))return false;
    QByteArray bytes;QTextStream out(&bytes,QIODevice::WriteOnly);out.setLocale(QLocale::c());
    out<<"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""<<QString::number(pattern.flatLengthMm,'f',3)
       <<"mm\" height=\""<<QString::number(sheetWidth,'f',3)<<"mm\" viewBox=\"0 0 "
       <<QString::number(pattern.flatLengthMm,'f',6)<<" "<<QString::number(sheetWidth,'f',6)
       <<"\" data-document-type=\"single-bend-flat-pattern\" data-angle-degrees=\""
       <<QString::number(angleDegrees,'f',3)<<"\" data-thickness-mm=\""<<QString::number(rule.thicknessMm,'f',3)
       <<"\" data-inside-radius-mm=\""<<QString::number(rule.insideBendRadiusMm,'f',3)
       <<"\" data-k-factor=\""<<QString::number(rule.kFactor,'f',4)<<"\">\n"
       <<"<title>Single-bend sheet-metal flat pattern</title>\n"
       <<"<g id=\"blank-outline\" fill=\"none\" stroke=\"#111827\" stroke-width=\"0.25\"><rect x=\"0\" y=\"0\" width=\""
       <<QString::number(pattern.flatLengthMm,'f',6)<<"\" height=\""<<QString::number(sheetWidth,'f',6)<<"\"/></g>\n"
       <<"<g id=\"bend-line\" data-annotation=\"bend-line\" stroke=\"#d97706\" stroke-width=\"0.2\" stroke-dasharray=\"2 1\"><path d=\"M"
       <<QString::number(pattern.firstTangentMm,'f',6)<<" 0V"<<QString::number(sheetWidth,'f',6)<<"\"/></g>\n"
       <<"<g id=\"flat-pattern-notes\" font-family=\"Arial,sans-serif\" font-size=\"3\" fill=\"#111827\"><text x=\"2\" y=\"4\">FLAT PATTERN - ONE BEND</text><text x=\"2\" y=\"8\">"
       <<QString::number(angleDegrees,'f',1)<<" deg | t="<<QString::number(rule.thicknessMm,'f',3)<<" mm | R="
       <<QString::number(rule.insideBendRadiusMm,'f',3)<<" mm | K="<<QString::number(rule.kFactor,'f',3)
       <<"</text><text x=\"2\" y=\"12\">Flat length "<<QString::number(pattern.flatLengthMm,'f',3)<<" mm; width "
       <<QString::number(sheetWidth,'f',3)<<" mm</text></g></svg>\n";
    out.flush();if(out.status()!=QTextStream::Ok)return fail(error,"Could not format the flat-pattern SVG.");
    *svg=std::move(bytes);if(result)*result=pattern;return true;
}

bool exportOneBendFlatPatternSvg(double flangeA,double flangeB,double sheetWidth,double angleDegrees,
                                 const SheetMetalRule& rule,const QString& path,SheetMetalFlatPattern* result,QString* error){
    if(path.trimmed().isEmpty())return fail(error,"Provide a file path for the flat-pattern SVG.");
    QByteArray svg;if(!buildOneBendFlatPatternSvg(flangeA,flangeB,sheetWidth,angleDegrees,rule,&svg,nullptr,error))return false;
    QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))return fail(error,QStringLiteral("Could not create flat-pattern SVG: %1").arg(file.errorString()));
    if(file.write(svg)!=svg.size()){file.cancelWriting();return fail(error,"Could not write the complete flat-pattern SVG.");}
    if(!file.commit())return fail(error,QStringLiteral("Could not finalize flat-pattern SVG: %1").arg(file.errorString()));
    if(result&&!calculateOneBendFlatPattern(flangeA,flangeB,sheetWidth,angleDegrees,rule,result,error))return false;
    return true;
}

bool calculateMultiBendFlatPattern(const QVector<double>& flanges,double sheetWidth,double thickness,
                                   const QVector<SheetMetalBend>& bends,MultiBendFlatPattern* result,QString* error){
    if(!result)return fail(error,"Provide an output object for the multi-bend flat-pattern calculation.");
    if(bends.size()<2||bends.size()>50||flanges.size()!=bends.size()+1)
        return fail(error,"A multi-bend pattern needs 2–50 bends and exactly one more outside flange length than bends.");
    if(!positiveFinite(sheetWidth)||!positiveFinite(thickness))
        return fail(error,"Sheet width and thickness must be finite positive dimensions up to 1,000,000 mm.");
    for(double flange:flanges)if(!positiveFinite(flange))
        return fail(error,"Every outside flange length must be finite and positive.");
    QVector<double> allowances,setbacks,deductions;allowances.reserve(bends.size());setbacks.reserve(bends.size());deductions.reserve(bends.size());
    double totalFlange=0.0,totalDeduction=0.0;
    for(int i=0;i<bends.size();++i){
        const SheetMetalBend& bend=bends[i];
        if(!std::isfinite(bend.angleDegrees)||bend.angleDegrees<15.0||bend.angleDegrees>165.0||
           !std::isfinite(bend.insideRadiusMm)||bend.insideRadiusMm<0.0||bend.insideRadiusMm>1000000.0||
           !std::isfinite(bend.kFactor)||bend.kFactor<0.0||bend.kFactor>1.0)
            return fail(error,"Each bend angle must be 15–165 degrees, radius nonnegative, and K-factor from 0 to 1.");
        const double angle=bend.angleDegrees*std::numbers::pi/180.0;
        const double allowance=angle*(bend.insideRadiusMm+bend.kFactor*thickness);
        const double setback=(bend.insideRadiusMm+thickness)*std::tan(angle/2.0);
        const double deduction=2.0*setback-allowance;
        if(!std::isfinite(allowance)||!std::isfinite(setback)||!std::isfinite(deduction))
            return fail(error,"Bend rule calculation exceeded the supported numeric range.");
        allowances.append(allowance);setbacks.append(setback);deductions.append(deduction);totalDeduction+=deduction;
    }
    for(double flange:flanges)totalFlange+=flange;
    if(!std::isfinite(totalFlange)||!std::isfinite(totalDeduction))return fail(error,"Total flange or bend allowance exceeds the supported range.");
    for(int i=0;i<flanges.size();++i){
        const double required=(i>0?setbacks[i-1]:0.0)+(i<bends.size()?setbacks[i]:0.0);
        if(flanges[i]<=required)return fail(error,QStringLiteral("Outside flange %1 is too short for its adjacent bend setbacks.").arg(i+1));
    }
    const double flatLength=totalFlange-totalDeduction;
    if(!positiveFinite(flatLength))return fail(error,"Calculated multi-bend flat length is not a positive supported dimension.");
    QVector<double> lines;lines.reserve(bends.size());
    double position=flanges[0]-setbacks[0]+allowances[0]/2.0;lines.append(position);
    for(int i=1;i<bends.size();++i){
        position+=flanges[i]-setbacks[i-1]-setbacks[i]+allowances[i-1]/2.0+allowances[i]/2.0;
        if(!std::isfinite(position)||position<=lines.last()||position>=flatLength)
            return fail(error,"Calculated bend lines are not ordered within the blank outline.");
        lines.append(position);
    }
    *result={flatLength,sheetWidth,allowances,deductions,lines};return true;
}

bool buildMultiBendFlatPatternDxf(const QVector<double>& flanges,double sheetWidth,double thickness,
                                 const QVector<SheetMetalBend>& bends,QByteArray* dxf,
                                 MultiBendFlatPattern* result,QString* error){
    return buildMultiBendFlatPatternDxf(flanges,sheetWidth,thickness,bends,SheetMetalRelief{},dxf,result,error);
}

bool buildMultiBendFlatPatternDxf(const QVector<double>& flanges,double sheetWidth,double thickness,
                                 const QVector<SheetMetalBend>& bends,const SheetMetalRelief& relief,
                                 QByteArray* dxf,MultiBendFlatPattern* result,QString* error){
    if(!dxf)return fail(error,"Provide an output buffer for the multi-bend DXF.");
    MultiBendFlatPattern pattern;if(!calculateMultiBendFlatPattern(flanges,sheetWidth,thickness,bends,&pattern,error))return false;
    const bool noRelief=relief.widthMm==0.0&&relief.depthMm==0.0;
    if(!noRelief&&(!positiveFinite(relief.widthMm)||!positiveFinite(relief.depthMm)||relief.depthMm>=sheetWidth/2.0))
        return fail(error,"End relief width and depth must both be positive, with depth less than half the sheet width; use zeros to disable reliefs.");
    if(!noRelief){
        for(int i=0;i<pattern.bendLinePositionsMm.size();++i){
            const double x=pattern.bendLinePositionsMm[i];
            if(x<=relief.widthMm/2.0||pattern.flatLengthMm-x<=relief.widthMm/2.0||
               (i>0&&x-pattern.bendLinePositionsMm[i-1]<=relief.widthMm))
                return fail(error,"Relief notches would overlap one another or extend beyond the blank outline.");
        }
    }
    QByteArray bytes;QTextStream out(&bytes,QIODevice::WriteOnly);out.setLocale(QLocale::c());
    auto line=[&](const QString& layer,double x1,double y1,double x2,double y2){
        out<<"0\nLINE\n8\n"<<layer<<"\n10\n"<<QString::number(x1,'f',6)<<"\n20\n"<<QString::number(y1,'f',6)
           <<"\n30\n0.0\n11\n"<<QString::number(x2,'f',6)<<"\n21\n"<<QString::number(y2,'f',6)<<"\n31\n0.0\n";
    };
    out<<"0\nSECTION\n2\nHEADER\n9\n$INSUNITS\n70\n4\n0\nENDSEC\n"
       <<"0\nSECTION\n2\nTABLES\n0\nTABLE\n2\nLAYER\n70\n2\n"
       <<"0\nLAYER\n2\nOUTLINE\n70\n0\n62\n7\n6\nCONTINUOUS\n"
       <<"0\nLAYER\n2\nBEND\n70\n0\n62\n30\n6\nCONTINUOUS\n0\nENDTAB\n0\nENDSEC\n"
       <<"0\nSECTION\n2\nENTITIES\n";
    if(noRelief){
        line(QStringLiteral("OUTLINE"),0,0,pattern.flatLengthMm,0);
        line(QStringLiteral("OUTLINE"),pattern.flatLengthMm,0,pattern.flatLengthMm,sheetWidth);
        line(QStringLiteral("OUTLINE"),pattern.flatLengthMm,sheetWidth,0,sheetWidth);
        line(QStringLiteral("OUTLINE"),0,sheetWidth,0,0);
    }else{
        QVector<QPair<double,double>> intervals;intervals.reserve(pattern.bendLinePositionsMm.size());
        for(double x:pattern.bendLinePositionsMm)intervals.append({x-relief.widthMm/2.0,x+relief.widthMm/2.0});
        auto horizontalWithReliefs=[&](double y,double direction){
            double cursor=direction>0.0?0.0:pattern.flatLengthMm;
            if(direction>0.0){
                for(const auto& interval:intervals){line(QStringLiteral("OUTLINE"),cursor,y,interval.first,y);
                    line(QStringLiteral("OUTLINE"),interval.first,y,interval.first,y+relief.depthMm*direction);
                    line(QStringLiteral("OUTLINE"),interval.first,y+relief.depthMm*direction,interval.second,y+relief.depthMm*direction);
                    line(QStringLiteral("OUTLINE"),interval.second,y+relief.depthMm*direction,interval.second,y);cursor=interval.second;}
                line(QStringLiteral("OUTLINE"),cursor,y,pattern.flatLengthMm,y);
            }else{
                for(int i=intervals.size()-1;i>=0;--i){const auto& interval=intervals[i];line(QStringLiteral("OUTLINE"),cursor,y,interval.second,y);
                    line(QStringLiteral("OUTLINE"),interval.second,y,interval.second,y+relief.depthMm*direction);
                    line(QStringLiteral("OUTLINE"),interval.second,y+relief.depthMm*direction,interval.first,y+relief.depthMm*direction);
                    line(QStringLiteral("OUTLINE"),interval.first,y+relief.depthMm*direction,interval.first,y);cursor=interval.first;}
                line(QStringLiteral("OUTLINE"),cursor,y,0,y);
            }
        };
        line(QStringLiteral("OUTLINE"),pattern.flatLengthMm,0,pattern.flatLengthMm,sheetWidth);
        horizontalWithReliefs(sheetWidth,-1.0);
        line(QStringLiteral("OUTLINE"),0,sheetWidth,0,0);
        horizontalWithReliefs(0,1.0);
    }
    for(double x:pattern.bendLinePositionsMm)line(QStringLiteral("BEND"),x,0,x,sheetWidth);
    out<<"0\nENDSEC\n0\nEOF\n";out.flush();
    if(out.status()!=QTextStream::Ok)return fail(error,"Could not format the multi-bend DXF.");
    *dxf=std::move(bytes);if(result)*result=pattern;return true;
}

bool exportMultiBendFlatPatternDxf(const QVector<double>& flanges,double sheetWidth,double thickness,
                                  const QVector<SheetMetalBend>& bends,const QString& path,
                                  MultiBendFlatPattern* result,QString* error){
    return exportMultiBendFlatPatternDxf(flanges,sheetWidth,thickness,bends,SheetMetalRelief{},path,result,error);
}

bool exportMultiBendFlatPatternDxf(const QVector<double>& flanges,double sheetWidth,double thickness,
                                  const QVector<SheetMetalBend>& bends,const SheetMetalRelief& relief,
                                  const QString& path,MultiBendFlatPattern* result,QString* error){
    if(path.trimmed().isEmpty())return fail(error,"Provide a file path for the multi-bend DXF.");
    QByteArray dxf;if(!buildMultiBendFlatPatternDxf(flanges,sheetWidth,thickness,bends,relief,&dxf,nullptr,error))return false;
    QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))return fail(error,QStringLiteral("Could not create DXF: %1").arg(file.errorString()));
    if(file.write(dxf)!=dxf.size()){file.cancelWriting();return fail(error,"Could not write the complete multi-bend DXF.");}
    if(!file.commit())return fail(error,QStringLiteral("Could not finalize DXF: %1").arg(file.errorString()));
    if(result&&!calculateMultiBendFlatPattern(flanges,sheetWidth,thickness,bends,result,error))return false;
    return true;
}
} // namespace cad

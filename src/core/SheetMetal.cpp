#include "SheetMetal.h"
#include "FoldedContourTopology.h"

#include <QSaveFile>
#include <QTextStream>
#include <QLocale>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace cad {
namespace {
bool fail(QString* error,const QString& message){if(error)*error=message;return false;}
bool positiveFinite(double value){return std::isfinite(value)&&value>0.00001&&value<=1000000.0;}
}

bool parseSheetMetalNumberList(const QString& text,QVector<double>* values,QString* error){
    if(!values)return fail(error,"Provide an output vector for sheet-metal numeric inputs.");
    QVector<double> parsed;
    // SkipEmptyParts would silently shift bends/rules when an input contains
    // consecutive commas or a dangling separator.
    for(const QString& field:text.split(',',Qt::KeepEmptyParts)){
        bool ok=false;const double value=field.trimmed().toDouble(&ok);
        if(!ok||!std::isfinite(value))
            return fail(error,"Sheet-metal numeric lists cannot contain empty, invalid, or non-finite entries.");
        parsed.append(value);
    }
    *values=std::move(parsed);
    return true;
}

bool buildFoldedSheetOutline(const QVector<double>& flanges,double sheetWidth,double thickness,
                             const QVector<FoldedSheetBend>& bends,QVector<QPointF>* outline,QString* error){
    if(!outline)return fail(error,"Provide an output polygon for the folded sheet-metal profile.");
    if(bends.isEmpty()||bends.size()>50||flanges.size()!=bends.size()+1)
        return fail(error,"A folded profile needs 1–50 bends and one more straight flange than bends.");
    if(!positiveFinite(sheetWidth)||!positiveFinite(thickness))
        return fail(error,"Folded sheet width and thickness must be finite positive dimensions up to 1,000,000 mm.");
    for(double length:flanges)if(!positiveFinite(length))
        return fail(error,"Every folded straight flange must be finite and positive.");
    for(const FoldedSheetBend& bend:bends)
        if(!std::isfinite(bend.angleDegrees)||std::abs(bend.angleDegrees)<15.0||std::abs(bend.angleDegrees)>165.0||
           !positiveFinite(bend.insideRadiusMm))
            return fail(error,"Fold angles must be signed 15–165 degrees and inside radii must be finite positive dimensions.");

    constexpr int arcSegmentsPer90Degrees=18;
    QVector<QPointF> centerline;centerline.reserve(1+bends.size()*(arcSegmentsPer90Degrees*2+1));
    QPointF point(0.0,0.0);double heading=0.0;centerline.append(point);
    for(int i=0;i<flanges.size();++i){
        const QPointF direction(std::cos(heading),std::sin(heading));
        point+=direction*flanges[i];centerline.append(point);
        if(i==bends.size())break;
        const FoldedSheetBend& bend=bends[i];
        const double radians=bend.angleDegrees*std::numbers::pi/180.0;
        const double neutralRadius=bend.insideRadiusMm+thickness/2.0;
        const QPointF leftNormal(-direction.y(),direction.x());
        const QPointF center=point+leftNormal*(std::copysign(neutralRadius,radians));
        const QPointF radial=point-center;
        const int segments=std::max(2,static_cast<int>(std::ceil(std::abs(bend.angleDegrees)/90.0*arcSegmentsPer90Degrees)));
        const double start=std::atan2(radial.y(),radial.x());
        for(int step=1;step<=segments;++step){
            const double angle=start+radians*static_cast<double>(step)/segments;
            point=center+QPointF(std::cos(angle),std::sin(angle))*neutralRadius;
            centerline.append(point);
        }
        heading+=radians;
    }
    if(centerline.size()<4)return fail(error,"The folded profile did not produce enough geometry.");
    QVector<QPointF> left,right;left.reserve(centerline.size());right.reserve(centerline.size());
    const double half=thickness/2.0;
    for(int i=0;i<centerline.size();++i){
        QPointF tangent;
        if(i==0)tangent=centerline[1]-centerline[0];
        else if(i==centerline.size()-1)tangent=centerline[i]-centerline[i-1];
        else{
            QPointF before=centerline[i]-centerline[i-1],after=centerline[i+1]-centerline[i];
            const double beforeLength=std::hypot(before.x(),before.y()),afterLength=std::hypot(after.x(),after.y());
            if(beforeLength<=1e-12||afterLength<=1e-12)return fail(error,"The folded neutral path contains a degenerate segment.");
            before/=beforeLength;after/=afterLength;tangent=before+after;
        }
        const double tangentLength=std::hypot(tangent.x(),tangent.y());
        if(!std::isfinite(tangentLength)||tangentLength<=1e-10)return fail(error,"Adjacent folded segments reverse direction at a cusp.");
        const double miterScale=(i==0||i==centerline.size()-1)?1.0:2.0/tangentLength;
        tangent/=tangentLength;
        const QPointF normal(-tangent.y(),tangent.x());
        const double offset=std::min(half*miterScale,thickness*4.0);
        left.append(centerline[i]+normal*offset);right.append(centerline[i]-normal*offset);
    }
    QVector<QPointF> result;result.reserve(left.size()+right.size());
    for(const QPointF& p:left)result.append(p);
    for(auto it=right.crbegin();it!=right.crend();++it)result.append(*it);
    for(const QPointF& p:result)if(!std::isfinite(p.x())||!std::isfinite(p.y())||std::abs(p.x())>1000000.0||std::abs(p.y())>1000000.0)
        return fail(error,"The folded profile exceeds the supported coordinate range.");
    std::vector<detail::ContourPoint> contour;contour.reserve(result.size());
    for(const QPointF& p:result)contour.push_back({p.x(),p.y()});
    std::pair<std::size_t,std::size_t> crossedEdges{};
    if(!detail::simpleClosedContour(contour,&crossedEdges))
        return fail(error,QStringLiteral("Folded sheet profile overlaps, self-intersects, or collapses (contour edges %1 and %2). Adjust flange lengths, bend radii, angles, or thickness.")
            .arg(static_cast<int>(crossedEdges.first+1)).arg(static_cast<int>(crossedEdges.second+1)));
    *outline=std::move(result);return true;
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
    // A positive tangent clearance may vanish when SVG path and viewBox
    // coordinates are serialized to six decimal places.
    const QString tangent=QString::number(pattern.firstTangentMm,'f',6);
    if(tangent==QString::number(0.0,'f',6)||
       tangent==QString::number(pattern.flatLengthMm,'f',6))
        return fail(error,"Bend line coincides with the blank outline at six-decimal SVG precision.");
    QByteArray bytes;QTextStream out(&bytes,QIODevice::WriteOnly);out.setLocale(QLocale::c());
    out<<"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""<<QString::number(pattern.flatLengthMm,'f',6)
       <<"mm\" height=\""<<QString::number(sheetWidth,'f',6)<<"mm\" viewBox=\"0 0 "
       <<QString::number(pattern.flatLengthMm,'f',6)<<" "<<QString::number(sheetWidth,'f',6)
       <<"\" data-document-type=\"single-bend-flat-pattern\" data-angle-degrees=\""
       <<QString::number(angleDegrees,'g',17)<<"\" data-thickness-mm=\""<<QString::number(rule.thicknessMm,'g',17)
       <<"\" data-inside-radius-mm=\""<<QString::number(rule.insideBendRadiusMm,'g',17)
       <<"\" data-k-factor=\""<<QString::number(rule.kFactor,'g',17)<<"\">\n"
       <<"<title>Single-bend sheet-metal flat pattern</title>\n"
       <<"<g id=\"blank-outline\" fill=\"none\" stroke=\"#111827\" stroke-width=\"0.25\"><rect x=\"0\" y=\"0\" width=\""
       <<QString::number(pattern.flatLengthMm,'f',6)<<"\" height=\""<<QString::number(sheetWidth,'f',6)<<"\"/></g>\n"
       <<"<g id=\"bend-line\" data-annotation=\"bend-line\" stroke=\"#d97706\" stroke-width=\"0.2\" stroke-dasharray=\"2 1\"><path d=\"M"
       <<QString::number(pattern.firstTangentMm,'f',6)<<" 0V"<<QString::number(sheetWidth,'f',6)<<"\"/></g>\n"
       <<"<g id=\"flat-pattern-notes\" font-family=\"Arial,sans-serif\" font-size=\"3\" fill=\"#111827\"><text x=\"2\" y=\"4\">FLAT PATTERN - ONE BEND</text><text x=\"2\" y=\"8\">"
       <<QString::number(angleDegrees,'g',8)<<" deg | t="<<QString::number(rule.thicknessMm,'g',8)<<" mm | R="
       <<QString::number(rule.insideBendRadiusMm,'g',8)<<" mm | K="<<QString::number(rule.kFactor,'g',8)
       <<"</text><text x=\"2\" y=\"12\">Flat length "<<QString::number(pattern.flatLengthMm,'g',8)<<" mm; width "
       <<QString::number(sheetWidth,'g',8)<<" mm</text></g></svg>\n";
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

bool validateMultiBendShopLimits(const QVector<double>& flanges,double sheetWidth,double thickness,
                                 const QVector<SheetMetalBend>& bends,const SheetMetalShopLimits& limits,
                                 QString* error){
    if(!std::isfinite(limits.minimumInsideRadiusToThickness)||
       limits.minimumInsideRadiusToThickness<0.0||
       !std::isfinite(limits.minimumStraightFlangeMm)||
       limits.minimumStraightFlangeMm<0.0)
        return fail(error,"Shop limits must be finite and nonnegative; zero disables a check.");
    MultiBendFlatPattern pattern;
    if(!calculateMultiBendFlatPattern(flanges,sheetWidth,thickness,bends,&pattern,error))return false;
    for(int i=0;limits.minimumInsideRadiusToThickness>0.0&&i<bends.size();++i){
        if(bends[i].insideRadiusMm/thickness<limits.minimumInsideRadiusToThickness)
            return fail(error,QStringLiteral("Bend %1 violates the specified minimum inside-radius/thickness ratio.").arg(i+1));
    }
    for(int i=0;limits.minimumStraightFlangeMm>0.0&&i<flanges.size();++i){
        const double preceding=(i>0)?
            (bends[i-1].insideRadiusMm+thickness)*std::tan(bends[i-1].angleDegrees*std::numbers::pi/360.0):0.0;
        const double following=(i<bends.size())?
            (bends[i].insideRadiusMm+thickness)*std::tan(bends[i].angleDegrees*std::numbers::pi/360.0):0.0;
        const double straight=flanges[i]-preceding-following;
        if(straight<limits.minimumStraightFlangeMm)
            return fail(error,QStringLiteral("Flange %1 violates the specified minimum straight length after bend setbacks.").arg(i+1));
    }
    return true;
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
    // Physical bend stations may be distinct yet collide with each other or
    // the blank edge after six-decimal DXF coordinate serialization.
    const QString firstEdge=QString::number(0.0,'f',6);
    const QString lastEdge=QString::number(pattern.flatLengthMm,'f',6);
    QString previousStation;
    for(double position:pattern.bendLinePositionsMm){
        const QString station=QString::number(position,'f',6);
        if(!std::isfinite(position)||position<=0.0||position>=pattern.flatLengthMm||
           station==firstEdge||station==lastEdge||station==previousStation)
            return fail(error,"Bend stations collide with each other or the blank edge at six-decimal DXF precision.");
        previousStation=station;
    }
    const bool noRelief=relief.widthMm==0.0&&relief.depthMm==0.0;
    if(!noRelief&&(!positiveFinite(relief.widthMm)||!positiveFinite(relief.depthMm)||relief.depthMm>=sheetWidth/2.0))
        return fail(error,"End relief width and depth must both be positive, with depth less than half the sheet width; use zeros to disable reliefs.");
    if(!noRelief){
        // DXF coordinates use six decimal places. A positive remaining web can
        // still round both endpoints of the bend line to the same coordinate.
        if(QString::number(relief.depthMm,'f',6)==
           QString::number(sheetWidth-relief.depthMm,'f',6))
            return fail(error,"End relief leaves no bend line at six-decimal DXF precision.");
        // Even disjoint reliefs can leave a zero-length OUTLINE entity after
        // the DXF's six-decimal coordinate rounding. Check the gaps between
        // adjacent notches and the sheet edges at the actual export precision.
        QString previousRight=QString::number(0.0,'f',6);
        for(int i=0;i<pattern.bendLinePositionsMm.size();++i){
            const double x=pattern.bendLinePositionsMm[i];
            if(x<=relief.widthMm/2.0||pattern.flatLengthMm-x<=relief.widthMm/2.0||
               (i>0&&x-pattern.bendLinePositionsMm[i-1]<=relief.widthMm))
                return fail(error,"Relief notches would overlap one another or extend beyond the blank outline.");
            if(QString::number(x-relief.widthMm/2.0,'f',6)==previousRight)
                return fail(error,"End relief leaves a zero-length OUTLINE segment at six-decimal DXF precision.");
            previousRight=QString::number(x+relief.widthMm/2.0,'f',6);
        }
        if(previousRight==QString::number(pattern.flatLengthMm,'f',6))
            return fail(error,"End relief leaves a zero-length OUTLINE segment at six-decimal DXF precision.");
    }
    QByteArray bytes;QTextStream out(&bytes,QIODevice::WriteOnly);out.setLocale(QLocale::c());
    auto line=[&](const QString& layer,double x1,double y1,double x2,double y2){
        out<<"0\nLINE\n8\n"<<layer<<"\n10\n"<<QString::number(x1,'f',6)<<"\n20\n"<<QString::number(y1,'f',6)
           <<"\n30\n0.0\n11\n"<<QString::number(x2,'f',6)<<"\n21\n"<<QString::number(y2,'f',6)<<"\n31\n0.0\n";
    };
    out<<"0\nSECTION\n2\nHEADER\n9\n$ACADVER\n1\nAC1015\n9\n$INSUNITS\n70\n4\n0\nENDSEC\n"
       <<"0\nSECTION\n2\nTABLES\n0\nTABLE\n2\nLTYPE\n70\n1\n"
       <<"0\nLTYPE\n2\nCONTINUOUS\n70\n0\n3\nSolid line\n72\n65\n73\n0\n40\n0.0\n0\nENDTAB\n"
       <<"0\nTABLE\n2\nLAYER\n70\n2\n"
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
    // The rectangular end notches remove material at each bend station.
    // Keep the bend center lines inside the remaining blank material.
    for(double x:pattern.bendLinePositionsMm)
        line(QStringLiteral("BEND"),x,noRelief?0.0:relief.depthMm,
             x,noRelief?sheetWidth:sheetWidth-relief.depthMm);
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

bool calculateFoldedSheetFlatPattern(const QVector<double>& flanges,double sheetWidth,double thickness,
                                     const QVector<FoldedSheetBend>& bends,const QVector<double>& kFactors,
                                     MultiBendFlatPattern* result,QString* error){
    if(!result)return fail(error,"Provide an output object for the folded-sheet flat-pattern calculation.");
    if(bends.isEmpty()||bends.size()>50||flanges.size()!=bends.size()+1)
        return fail(error,"A folded-sheet flat pattern needs 1–50 bends and exactly one more straight flange than bends.");
    if(!positiveFinite(sheetWidth)||!positiveFinite(thickness)||kFactors.size()!=bends.size())
        return fail(error,"Sheet width and thickness must be finite positive dimensions; provide one K-factor per bend.");
    QVector<double> outsideFlanges;outsideFlanges.reserve(flanges.size());
    QVector<SheetMetalBend> rules;rules.reserve(bends.size());
    QVector<double> allowances,setbacks;allowances.reserve(bends.size());setbacks.reserve(bends.size());
    for(double flange:flanges){if(!positiveFinite(flange))return fail(error,"Every straight neutral-surface flange length must be finite and positive.");}
    for(int bendIndex=0;bendIndex<bends.size();++bendIndex){
        const FoldedSheetBend& bend=bends[bendIndex];const double kFactor=kFactors[bendIndex];
        const double angle=std::abs(bend.angleDegrees);
        if(!std::isfinite(bend.angleDegrees)||angle<15.0||angle>165.0||!positiveFinite(bend.insideRadiusMm)||
           !std::isfinite(kFactor)||kFactor<0.0||kFactor>1.0)
            return fail(error,"Folded bend angles must be signed 15–165 degrees, inside radii positive, and each K-factor from 0 to 1.");
        const double radians=angle*std::numbers::pi/180.0;
        const double allowance=radians*(bend.insideRadiusMm+kFactor*thickness);
        const double setback=(bend.insideRadiusMm+thickness)*std::tan(radians/2.0);
        if(!std::isfinite(allowance)||!std::isfinite(setback))return fail(error,"Folded bend development exceeds the supported numeric range.");
        allowances.append(allowance);setbacks.append(setback);rules.append({angle,bend.insideRadiusMm,kFactor});
    }
    for(int i=0;i<flanges.size();++i){
        const double setback=(i>0?setbacks[i-1]:0.0)+(i<bends.size()?setbacks[i]:0.0);
        const double outside=flanges[i]+setback;
        if(!positiveFinite(outside))return fail(error,"Converted outside flange dimension exceeds the supported range.");
        outsideFlanges.append(outside);
    }
    MultiBendFlatPattern pattern;
    if(bends.size()>1){
        if(!calculateMultiBendFlatPattern(outsideFlanges,sheetWidth,thickness,rules,&pattern,error))return false;
    }else{
        const double flat=flanges[0]+flanges[1]+allowances[0];
        const double station=flanges[0]+allowances[0]/2.0;
        if(!positiveFinite(flat)||!std::isfinite(station)||station<=0.0||station>=flat)
            return fail(error,"Calculated folded-sheet blank or bend line is outside the supported range.");
        pattern={flat,sheetWidth,allowances,{2.0*setbacks[0]-allowances[0]},{station}};
    }
    // Verify the converted outside dimensions conserve the neutral-path length.
    double neutralTotal=0.0;for(double value:flanges)neutralTotal+=value;
    for(double value:allowances)neutralTotal+=value;
    const double tolerance=std::max(1e-9,std::abs(neutralTotal)*1e-10);
    if(!std::isfinite(neutralTotal)||std::abs(pattern.flatLengthMm-neutralTotal)>tolerance)
        return fail(error,"Converted flange dimensions do not conserve the folded neutral-path development.");
    *result=std::move(pattern);return true;
}

bool calculateFoldedSheetFlatPattern(const QVector<double>& flanges,double sheetWidth,double thickness,
                                     const QVector<FoldedSheetBend>& bends,double kFactor,
                                     MultiBendFlatPattern* result,QString* error){
    return calculateFoldedSheetFlatPattern(flanges,sheetWidth,thickness,bends,QVector<double>(bends.size(),kFactor),result,error);
}

bool validateFoldedSheetShopLimits(const QVector<double>& flanges,double sheetWidth,double thickness,
                                   const QVector<FoldedSheetBend>& bends,const SheetMetalShopLimits& limits,
                                   QString* error){
    if(!std::isfinite(limits.minimumInsideRadiusToThickness)||limits.minimumInsideRadiusToThickness<0.0||
       !std::isfinite(limits.minimumStraightFlangeMm)||limits.minimumStraightFlangeMm<0.0)
        return fail(error,"Folded-sheet shop limits must be finite and nonnegative; zero disables a check.");
    MultiBendFlatPattern pattern;
    if(!calculateFoldedSheetFlatPattern(flanges,sheetWidth,thickness,bends,0.33,&pattern,error))return false;
    for(int index=0;limits.minimumInsideRadiusToThickness>0.0&&index<bends.size();++index)
        if(bends[index].insideRadiusMm/thickness<limits.minimumInsideRadiusToThickness)
            return fail(error,QStringLiteral("Folded bend %1 violates the minimum inside-radius/thickness ratio.").arg(index+1));
    for(int index=0;limits.minimumStraightFlangeMm>0.0&&index<flanges.size();++index)
        if(flanges[index]<limits.minimumStraightFlangeMm)
            return fail(error,QStringLiteral("Folded flange %1 is shorter than the requested straight length.").arg(index+1));
    return true;
}

bool buildFoldedSheetFlatPatternDxf(const QVector<double>& flanges,double sheetWidth,double thickness,
                                   const QVector<FoldedSheetBend>& bends,const QVector<double>& kFactors,
                                   QByteArray* dxf,MultiBendFlatPattern* result,QString* error){
    return buildFoldedSheetFlatPatternDxf(flanges,sheetWidth,thickness,bends,kFactors,SheetMetalRelief{},dxf,result,error);
}

bool buildFoldedSheetFlatPatternDxf(const QVector<double>& flanges,double sheetWidth,double thickness,
                                   const QVector<FoldedSheetBend>& bends,const QVector<double>& kFactors,
                                   const SheetMetalRelief& relief,QByteArray* dxf,
                                   MultiBendFlatPattern* result,QString* error){
    if(!dxf)return fail(error,"Provide an output buffer for the folded-sheet DXF.");
    MultiBendFlatPattern pattern;if(!calculateFoldedSheetFlatPattern(flanges,sheetWidth,thickness,bends,kFactors,&pattern,error))return false;
    const QString first=QString::number(0.0,'f',6),last=QString::number(pattern.flatLengthMm,'f',6);QString previous;
    for(double station:pattern.bendLinePositionsMm){
        const QString rounded=QString::number(station,'f',6);
        if(!std::isfinite(station)||station<=0.0||station>=pattern.flatLengthMm||rounded==first||rounded==last||rounded==previous)
            return fail(error,"Folded bend stations collide with each other or the blank edge at six-decimal DXF precision.");
        previous=rounded;
    }
    const bool noRelief=relief.widthMm==0.0&&relief.depthMm==0.0;
    if(!noRelief&&(!positiveFinite(relief.widthMm)||!positiveFinite(relief.depthMm)||relief.depthMm>=sheetWidth/2.0))
        return fail(error,"Folded bend-end relief width and depth must both be positive, with depth less than half the sheet width.");
    if(!noRelief){
        if(QString::number(relief.depthMm,'f',6)==QString::number(sheetWidth-relief.depthMm,'f',6))
            return fail(error,"Folded bend-end relief leaves no bend line at six-decimal DXF precision.");
        QString previousRight=first;
        for(int i=0;i<pattern.bendLinePositionsMm.size();++i){
            const double station=pattern.bendLinePositionsMm[i];
            if(station<=relief.widthMm/2.0||pattern.flatLengthMm-station<=relief.widthMm/2.0||
               (i>0&&station-pattern.bendLinePositionsMm[i-1]<=relief.widthMm))
                return fail(error,"Folded bend-end reliefs overlap or extend beyond the blank outline.");
            if(QString::number(station-relief.widthMm/2.0,'f',6)==previousRight)
                return fail(error,"Folded bend-end relief leaves a zero-length outline segment at six-decimal DXF precision.");
            previousRight=QString::number(station+relief.widthMm/2.0,'f',6);
        }
        if(previousRight==last)
            return fail(error,"Folded bend-end relief leaves a zero-length outline segment at six-decimal DXF precision.");
    }
    QByteArray bytes;QTextStream out(&bytes,QIODevice::WriteOnly);out.setLocale(QLocale::c());
    auto line=[&](const QString& layer,double x1,double y1,double x2,double y2){
        out<<"0\nLINE\n8\n"<<layer<<"\n10\n"<<QString::number(x1,'f',6)<<"\n20\n"<<QString::number(y1,'f',6)
           <<"\n30\n0.0\n11\n"<<QString::number(x2,'f',6)<<"\n21\n"<<QString::number(y2,'f',6)<<"\n31\n0.0\n";
    };
    out<<"0\nSECTION\n2\nHEADER\n9\n$ACADVER\n1\nAC1015\n9\n$INSUNITS\n70\n4\n0\nENDSEC\n"
       <<"0\nSECTION\n2\nTABLES\n0\nTABLE\n2\nLTYPE\n70\n1\n0\nLTYPE\n2\nCONTINUOUS\n70\n0\n3\nSolid line\n72\n65\n73\n0\n40\n0.0\n0\nENDTAB\n"
       <<"0\nTABLE\n2\nLAYER\n70\n2\n0\nLAYER\n2\nOUTLINE\n70\n0\n62\n7\n6\nCONTINUOUS\n"
       <<"0\nLAYER\n2\nBEND\n70\n0\n62\n30\n6\nCONTINUOUS\n0\nENDTAB\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n";
    if(noRelief){
        line(QStringLiteral("OUTLINE"),0,0,pattern.flatLengthMm,0);
        line(QStringLiteral("OUTLINE"),pattern.flatLengthMm,0,pattern.flatLengthMm,sheetWidth);
        line(QStringLiteral("OUTLINE"),pattern.flatLengthMm,sheetWidth,0,sheetWidth);
        line(QStringLiteral("OUTLINE"),0,sheetWidth,0,0);
    }else{
        QVector<QPair<double,double>> intervals;intervals.reserve(pattern.bendLinePositionsMm.size());
        for(double station:pattern.bendLinePositionsMm)
            intervals.append({station-relief.widthMm/2.0,station+relief.widthMm/2.0});
        double cursor=0.0;
        for(const auto& notch:intervals){
            line(QStringLiteral("OUTLINE"),cursor,0,notch.first,0);
            line(QStringLiteral("OUTLINE"),notch.first,0,notch.first,relief.depthMm);
            line(QStringLiteral("OUTLINE"),notch.first,relief.depthMm,notch.second,relief.depthMm);
            line(QStringLiteral("OUTLINE"),notch.second,relief.depthMm,notch.second,0);cursor=notch.second;
        }
        line(QStringLiteral("OUTLINE"),cursor,0,pattern.flatLengthMm,0);
        line(QStringLiteral("OUTLINE"),pattern.flatLengthMm,0,pattern.flatLengthMm,sheetWidth);
        cursor=pattern.flatLengthMm;
        for(int i=intervals.size()-1;i>=0;--i){const auto notch=intervals[i];
            line(QStringLiteral("OUTLINE"),cursor,sheetWidth,notch.second,sheetWidth);
            line(QStringLiteral("OUTLINE"),notch.second,sheetWidth,notch.second,sheetWidth-relief.depthMm);
            line(QStringLiteral("OUTLINE"),notch.second,sheetWidth-relief.depthMm,notch.first,sheetWidth-relief.depthMm);
            line(QStringLiteral("OUTLINE"),notch.first,sheetWidth-relief.depthMm,notch.first,sheetWidth);cursor=notch.first;
        }
        line(QStringLiteral("OUTLINE"),cursor,sheetWidth,0,sheetWidth);
        line(QStringLiteral("OUTLINE"),0,sheetWidth,0,0);
    }
    for(double station:pattern.bendLinePositionsMm)
        line(QStringLiteral("BEND"),station,noRelief?0.0:relief.depthMm,
             station,noRelief?sheetWidth:sheetWidth-relief.depthMm);
    out<<"0\nENDSEC\n0\nEOF\n";out.flush();
    if(out.status()!=QTextStream::Ok)return fail(error,"Could not format the folded-sheet DXF.");
    *dxf=std::move(bytes);if(result)*result=std::move(pattern);return true;
}

bool buildFoldedSheetFlatPatternDxf(const QVector<double>& flanges,double sheetWidth,double thickness,
                                   const QVector<FoldedSheetBend>& bends,double kFactor,
                                   QByteArray* dxf,MultiBendFlatPattern* result,QString* error){
    return buildFoldedSheetFlatPatternDxf(flanges,sheetWidth,thickness,bends,QVector<double>(bends.size(),kFactor),dxf,result,error);
}

bool equivalentFoldedSheetDevelopment(const MultiBendFlatPattern& first,
                                      const MultiBendFlatPattern& second,
                                      QString* error){
    if(first.bendLinePositionsMm.size()!=second.bendLinePositionsMm.size()||
       first.bendAllowancesMm.size()!=second.bendAllowancesMm.size()||
       first.bendDeductionsMm.size()!=second.bendDeductionsMm.size())
        return fail(error,"Development outputs have different bend inventories.");
    const double scale=std::max({1.0,std::abs(first.flatLengthMm),
        std::abs(second.flatLengthMm),std::abs(first.widthMm),std::abs(second.widthMm)});
    const double tolerance=std::max(1e-8,scale*1e-10);
    const auto agrees=[&](double a,double b){
        return std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=tolerance;
    };
    if(first.flatLengthMm<=0||second.flatLengthMm<=0||
       first.widthMm<=0||second.widthMm<=0||
       !agrees(first.flatLengthMm,second.flatLengthMm)||
       !agrees(first.widthMm,second.widthMm))
        return fail(error,"Developed blank dimensions disagree.");
    for(int i=0;i<first.bendLinePositionsMm.size();++i){
        if(!agrees(first.bendLinePositionsMm[i],second.bendLinePositionsMm[i])||
           !agrees(first.bendAllowancesMm[i],second.bendAllowancesMm[i])||
           !agrees(first.bendDeductionsMm[i],second.bendDeductionsMm[i]))
            return fail(error,"Developed bend stations or allowances disagree.");
    }
    if(error)error->clear();
    return true;
}

bool buildFoldedSheetFlatPatternSvg(const QVector<double>& flanges,double sheetWidth,double thickness,
                                    const QVector<FoldedSheetBend>& bends,const QVector<double>& kFactors,
                                    const SheetMetalRelief& relief,QByteArray* svg,
                                    MultiBendFlatPattern* result,QString* error){
    if(!svg)return fail(error,"Provide an output buffer for the folded-sheet preview SVG.");
    // The DXF is the manufacturing-output source of truth. Validate it first
    // so an SVG preview never shows a blank the DXF exporter would reject.
    QByteArray verifiedDxf;MultiBendFlatPattern pattern;
    if(!buildFoldedSheetFlatPatternDxf(flanges,sheetWidth,thickness,bends,kFactors,relief,
                                       &verifiedDxf,&pattern,error))return false;
    const auto fmt=[](double x){return QString::number(x,'f',6);};
    const bool notch=relief.widthMm>0.0;
    QStringList contour{QStringLiteral("M0,0")};
    const auto point=[&](double x,double y){contour.append("L"+fmt(x)+","+fmt(y));};
    if(notch){
        for(double station:pattern.bendLinePositionsMm){
            const double left=station-relief.widthMm/2.0,right=station+relief.widthMm/2.0;
            point(left,0);point(left,relief.depthMm);point(right,relief.depthMm);point(right,0);
        }
    }
    point(pattern.flatLengthMm,0);point(pattern.flatLengthMm,sheetWidth);
    if(notch){
        for(int i=pattern.bendLinePositionsMm.size()-1;i>=0;--i){
            const double station=pattern.bendLinePositionsMm[i];
            const double left=station-relief.widthMm/2.0,right=station+relief.widthMm/2.0;
            point(right,sheetWidth);point(right,sheetWidth-relief.depthMm);
            point(left,sheetWidth-relief.depthMm);point(left,sheetWidth);
        }
    }
    point(0,sheetWidth);contour.append("Z");
    QByteArray bytes;QTextStream out(&bytes,QIODevice::WriteOnly);out.setLocale(QLocale::c());
    out<<"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""<<fmt(pattern.flatLengthMm)
       <<"mm\" height=\""<<fmt(sheetWidth)<<"mm\" viewBox=\"0 0 "
       <<fmt(pattern.flatLengthMm)<<" "<<fmt(sheetWidth)
       <<"\" data-document-type=\"folded-sheet-flat-preview\" data-bend-count=\""
       <<pattern.bendLinePositionsMm.size()<<"\">\n"
       <<"<title>Folded sheet metal preview - geometry matches validated DXF</title>\n"
       <<"<path id=\"blank-outline\" fill=\"#f1f5f9\" stroke=\"#111827\" stroke-width=\"0.25\" d=\""
       <<contour.join(' ')<<"\"/>\n";
    for(int i=0;i<pattern.bendLinePositionsMm.size();++i){
        const double station=pattern.bendLinePositionsMm[i];
        out<<"<path id=\"bend-"<<(i+1)<<"\" data-angle-degrees=\""
           <<QString::number(bends[i].angleDegrees,'g',17)
           <<"\" fill=\"none\" stroke=\"#b45309\" stroke-dasharray=\"2,1\" stroke-width=\"0.2\" d=\"M"
           <<fmt(station)<<","<<fmt(notch?relief.depthMm:0.0)
           <<" V"<<fmt(notch?sheetWidth-relief.depthMm:sheetWidth)<<"\"/>\n";
    }
    out<<"</svg>\n";out.flush();
    if(out.status()!=QTextStream::Ok)return fail(error,"Could not format the folded-sheet preview SVG.");
    *svg=std::move(bytes);
    if(result)*result=std::move(pattern);
    if(error)error->clear();return true;
}

bool exportFoldedSheetFlatPatternDxf(const QVector<double>& flanges,double sheetWidth,double thickness,
                                    const QVector<FoldedSheetBend>& bends,const QVector<double>& kFactors,
                                    const QString& path,MultiBendFlatPattern* result,QString* error){
    return exportFoldedSheetFlatPatternDxf(flanges,sheetWidth,thickness,bends,kFactors,SheetMetalRelief{},path,result,error);
}

bool exportFoldedSheetFlatPatternDxf(const QVector<double>& flanges,double sheetWidth,double thickness,
                                    const QVector<FoldedSheetBend>& bends,const QVector<double>& kFactors,
                                    const SheetMetalRelief& relief,const QString& path,
                                    MultiBendFlatPattern* result,QString* error){
    if(path.trimmed().isEmpty())return fail(error,"Provide a file path for the folded-sheet DXF.");
    QByteArray bytes;MultiBendFlatPattern pattern;
    if(!buildFoldedSheetFlatPatternDxf(flanges,sheetWidth,thickness,bends,kFactors,relief,
                                       &bytes,result?&pattern:nullptr,error))return false;
    QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))return fail(error,QStringLiteral("Could not create folded-sheet DXF: %1").arg(file.errorString()));
    if(file.write(bytes)!=bytes.size()){file.cancelWriting();return fail(error,"Could not write the complete folded-sheet DXF.");}
    if(!file.commit())return fail(error,QStringLiteral("Could not finalize folded-sheet DXF: %1").arg(file.errorString()));
    if(result)*result=std::move(pattern);
    return true;
}

bool exportFoldedSheetFlatPatternDxf(const QVector<double>& flanges,double sheetWidth,double thickness,
                                    const QVector<FoldedSheetBend>& bends,double kFactor,
                                    const QString& path,MultiBendFlatPattern* result,QString* error){
    return exportFoldedSheetFlatPatternDxf(flanges,sheetWidth,thickness,bends,QVector<double>(bends.size(),kFactor),path,result,error);
}

bool buildFoldedSheetBendTableCsv(const QVector<double>& flanges,double sheetWidth,double thickness,
                                  const QVector<FoldedSheetBend>& bends,const QVector<double>& kFactors,
                                  QByteArray* csv,QString* error){
    if(!csv)return fail(error,"Provide an output buffer for the folded-sheet bend table.");
    MultiBendFlatPattern pattern;
    if(!calculateFoldedSheetFlatPattern(flanges,sheetWidth,thickness,bends,kFactors,&pattern,error))return false;
    QByteArray bytes;QTextStream out(&bytes,QIODevice::WriteOnly);out.setLocale(QLocale::c());
    out<<"bend_index,direction,angle_degrees,inside_radius_mm,k_factor,bend_allowance_mm,bend_deduction_mm,bend_station_mm,flat_length_mm,sheet_width_mm\n";
    const auto value=[](double number){return QString::number(number,'g',17);};
    for(int i=0;i<bends.size();++i){
        out<<(i+1)<<','<<(bends[i].angleDegrees>0.0?"UP":"DOWN")<<','
           <<value(bends[i].angleDegrees)<<','<<value(bends[i].insideRadiusMm)<<','
           <<value(kFactors[i])<<','<<value(pattern.bendAllowancesMm[i])<<','
           <<value(pattern.bendDeductionsMm[i])<<','<<value(pattern.bendLinePositionsMm[i])<<','
           <<value(pattern.flatLengthMm)<<','<<value(sheetWidth)<<'\n';
    }
    out.flush();if(out.status()!=QTextStream::Ok)return fail(error,"Could not format the folded-sheet bend table.");
    *csv=std::move(bytes);return true;
}

bool exportFoldedSheetBendTableCsv(const QVector<double>& flanges,double sheetWidth,double thickness,
                                   const QVector<FoldedSheetBend>& bends,const QVector<double>& kFactors,
                                   const QString& path,QString* error){
    if(path.trimmed().isEmpty())return fail(error,"Provide a file path for the folded-sheet bend table.");
    QByteArray bytes;if(!buildFoldedSheetBendTableCsv(flanges,sheetWidth,thickness,bends,kFactors,&bytes,error))return false;
    QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))
        return fail(error,QStringLiteral("Could not create folded-sheet bend table: %1").arg(file.errorString()));
    if(file.write(bytes)!=bytes.size()){file.cancelWriting();return fail(error,"Could not write the complete folded-sheet bend table.");}
    if(!file.commit())return fail(error,QStringLiteral("Could not finalize folded-sheet bend table: %1").arg(file.errorString()));
    return true;
}
} // namespace cad

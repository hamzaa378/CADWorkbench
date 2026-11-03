#include "DrawingSheet.h"

#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>
#include <QXmlStreamReader>
#include <algorithm>
#include <cmath>

namespace cad {
namespace {
struct DxfContext {
    double scale=1.0;
    double textSize=3.0;
    QString layer=QStringLiteral("SHEET");
    QString stroke;
    bool skip=false;
    double textRotationDegrees=0.0;
    double rotationPivotX=0.0,rotationPivotY=0.0;
    QString clipDefinitionId;
    QString activeClipId;
};
struct DxfClipCircle {double x=0.0,y=0.0,radius=0.0;};

bool parseNumber(QStringView raw,double* number){
    if(!number)return false;
    bool okay=false;const double value=raw.toString().toDouble(&okay);
    if(!okay||!std::isfinite(value)||std::abs(value)>1e9)return false;
    *number=value;return true;
}

bool parseNumbers(const QString& raw,QVector<double>* values){
    static const QRegularExpression token(QStringLiteral(R"([-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)"));
    QString remainder=raw;remainder.replace(token,QString());
    remainder.remove(QRegularExpression(QStringLiteral(R"([\s,])")));
    if(!remainder.isEmpty())return false;
    values->clear();
    auto matches=token.globalMatch(raw);
    while(matches.hasNext()){
        double number=0.0;
        if(!parseNumber(QStringView(matches.next().captured(0)),&number))return false;
        values->append(number);
    }
    return true;
}

QString dxfText(const QString& source){
    QString result;result.reserve(source.size());
    for(QChar character:source){
        if(character=='\n'||character=='\r'||character.isNull())continue;
        if(character.unicode()<128)result.append(character);
        else result.append(QStringLiteral("\\U+%1").arg(static_cast<int>(character.unicode()),4,16,QLatin1Char('0')).toUpper());
    }
    return result;
}
}

bool buildOrthographicDrawingDxf(const QByteArray& svg,QByteArray* dxf,QString* error){
    auto fail=[error](const QString& message){if(error)*error=message;return false;};
    if(!dxf)return fail(QStringLiteral("Provide an output buffer for the drawing DXF."));
    if(svg.isEmpty()||svg.size()>32*1024*1024)return fail(QStringLiteral("Drawing SVG is empty or too large for DXF conversion."));
    QXmlStreamReader reader(svg);
    QVector<DxfContext> stack;
    QHash<QString,DxfClipCircle> clipCircles;
    QByteArray bytes;QTextStream out(&bytes,QIODevice::WriteOnly);out.setLocale(QLocale::c());
    out << "0\nSECTION\n2\nHEADER\n9\n$ACADVER\n1\nAC1015\n9\n$INSUNITS\n70\n4\n0\nENDSEC\n"
           "0\nSECTION\n2\nTABLES\n0\nTABLE\n2\nLTYPE\n70\n3\n"
           "0\nLTYPE\n2\nCONTINUOUS\n70\n0\n3\nSolid\n72\n65\n73\n0\n40\n0\n"
           "0\nLTYPE\n2\nHIDDEN\n70\n0\n3\nHidden\n72\n65\n73\n2\n40\n3\n49\n2\n74\n0\n49\n-1\n74\n0\n"
           "0\nLTYPE\n2\nCENTER\n70\n0\n3\nCenter\n72\n65\n73\n4\n40\n9\n49\n6\n74\n0\n49\n-1\n74\n0\n49\n1\n74\n0\n49\n-1\n74\n0\n"
           "0\nENDTAB\n0\nTABLE\n2\nLAYER\n70\n11\n";
    for(const QString& layer:{"SHEET","GEOMETRY","HIDDEN","CENTER","DIMENSIONS","ANNOTATIONS","BOM","DETAILS","DETAIL_VIEW","DETAIL_HIDDEN","DETAIL_CENTER"}){
        const bool hidden=layer=="HIDDEN"||layer=="DETAIL_HIDDEN";
        const bool center=layer=="CENTER"||layer=="DETAIL_CENTER";
        out << "0\nLAYER\n2\n" << layer << "\n70\n0\n62\n" << (hidden?8:7)
            << "\n6\n" << (hidden?"HIDDEN":(center?"CENTER":"CONTINUOUS")) << '\n';
    }
    out << "0\nENDTAB\n0\nENDSEC\n0\nSECTION\n2\nENTITIES\n";
    double sheetWidth=0.0,sheetHeight=0.0;
    int entities=0;bool rootSeen=false;
    auto line=[&](double x1,double y1,double x2,double y2,const DxfContext& context)->bool{
        x1*=context.scale;x2*=context.scale;y1=sheetHeight-y1*context.scale;y2=sheetHeight-y2*context.scale;
        if(!std::isfinite(x1)||!std::isfinite(y1)||!std::isfinite(x2)||!std::isfinite(y2)||
           std::abs(x1)>1e9||std::abs(x2)>1e9||std::abs(y1)>1e9||std::abs(y2)>1e9)return false;
        if(std::hypot(x2-x1,y2-y1)<1e-9)return true;
        out << "0\nLINE\n8\n" << context.layer << "\n10\n" << x1 << "\n20\n" << y1
            << "\n11\n" << x2 << "\n21\n" << y2 << '\n';
        ++entities;return true;
    };
    auto clippedLine=[&](double x1,double y1,double x2,double y2,const DxfContext& context)->bool{
        if(context.activeClipId.isEmpty())return line(x1,y1,x2,y2,context);
        if(!clipCircles.contains(context.activeClipId))return false;
        const DxfClipCircle clip=clipCircles.value(context.activeClipId);
        const double dx=x2-x1,dy=y2-y1,aa=dx*dx+dy*dy;
        if(aa<1e-18)return true;
        QVector<double> cuts{0.0,1.0};
        const double ox=x1-clip.x,oy=y1-clip.y;
        const double bb=2*(ox*dx+oy*dy),cc=ox*ox+oy*oy-clip.radius*clip.radius;
        const double discriminant=bb*bb-4*aa*cc;
        if(discriminant>=0){
            const double root=std::sqrt(discriminant);
            for(const double t:{(-bb-root)/(2*aa),(-bb+root)/(2*aa)})
                if(t>0&&t<1)cuts.append(t);
        }
        std::sort(cuts.begin(),cuts.end());
        for(int i=1;i<cuts.size();++i){
            const double first=cuts[i-1],last=cuts[i],mid=(first+last)*0.5;
            if(std::hypot(x1+mid*dx-clip.x,y1+mid*dy-clip.y)>clip.radius+1e-8)continue;
            if(!line(x1+first*dx,y1+first*dy,x1+last*dx,y1+last*dy,context))return false;
        }
        return true;
    };
    while(!reader.atEnd()){
        reader.readNext();
        if(reader.isEndElement()){
            if(!stack.isEmpty())stack.removeLast();
            continue;
        }
        if(!reader.isStartElement())continue;
        const QString name=reader.name().toString();
        const QXmlStreamAttributes attributes=reader.attributes();
        DxfContext context=stack.isEmpty()?DxfContext{}:stack.last();
        if(name=="svg"){
            if(rootSeen||!attributes.hasAttribute("data-cadworkbench-source-body-id"))
                return fail(QStringLiteral("DXF conversion requires a CADWorkbench drawing SVG."));
            QVector<double> viewBox;
            if(!parseNumbers(attributes.value("viewBox").toString(),&viewBox)||viewBox.size()!=4||
               viewBox[0]!=0||viewBox[1]!=0||viewBox[2]<=0||viewBox[3]<=0||
               viewBox[2]>1000||viewBox[3]>1000)return fail(QStringLiteral("Invalid drawing SVG sheet dimensions."));
            sheetWidth=viewBox[2];sheetHeight=viewBox[3];rootSeen=true;
        }
        if(name=="defs"||name=="clipPath")context.skip=true;
        if(name=="clipPath")context.clipDefinitionId=attributes.value("id").toString();
        if(attributes.hasAttribute("clip-path")){
            const QString reference=attributes.value("clip-path").toString();
            if(!reference.startsWith("url(#")||!reference.endsWith(')'))
                return fail(QStringLiteral("Unsupported drawing detail clip reference."));
            context.activeClipId=reference.mid(5,reference.size()-6);
        }
        if(attributes.hasAttribute("data-view-label"))context.layer=QStringLiteral("GEOMETRY");
        if(attributes.hasAttribute("data-drawing-annotation")){
            const QString annotation=attributes.value("data-drawing-annotation").toString();
            context.layer=annotation=="detail-view"?QStringLiteral("DETAIL_VIEW"):
                annotation.contains("dimension")?QStringLiteral("DIMENSIONS"):
                annotation.contains("bom-")||annotation=="linked-parts-list"?QStringLiteral("BOM"):
                annotation.contains("detail")||annotation.contains("section")?QStringLiteral("DETAILS"):
                QStringLiteral("ANNOTATIONS");
        }
        if(attributes.hasAttribute("stroke"))context.stroke=attributes.value("stroke").toString();
        if(attributes.hasAttribute("font-size")){
            double size=0.0;if(!parseNumber(attributes.value("font-size"),&size)||size<=0)return fail(QStringLiteral("Invalid drawing SVG font size."));
            context.textSize=size;
        }
        if(attributes.hasAttribute("transform")){
            const QString transform=attributes.value("transform").toString();
            static const QRegularExpression scaleExpression(QStringLiteral(R"(^scale\(([-+0-9.eE]+)\)$)"));
            static const QRegularExpression rotationExpression(QStringLiteral(R"(^rotate\(([-+0-9.eE]+)[ ,]+([-+0-9.eE]+)[ ,]+([-+0-9.eE]+)\)$)"));
            const auto match=scaleExpression.match(transform);
            double scale=0.0;
            if(match.hasMatch()&&parseNumber(QStringView(match.captured(1)),&scale)&&scale>0&&scale<=1000)context.scale*=scale;
            else{
                const auto rotation=rotationExpression.match(transform);
                if(name!="text"||!rotation.hasMatch()||
                   !parseNumber(QStringView(rotation.captured(1)),&context.textRotationDegrees)||
                   !parseNumber(QStringView(rotation.captured(2)),&context.rotationPivotX)||
                   !parseNumber(QStringView(rotation.captured(3)),&context.rotationPivotY))
                    return fail(QStringLiteral("Unsupported drawing SVG transform for DXF conversion."));
            }
        }
        if(attributes.hasAttribute("stroke-dasharray")&&(context.layer=="GEOMETRY"||context.layer=="DETAIL_VIEW")){
            const bool center=attributes.value("stroke-dasharray").toString().startsWith("6,");
            context.layer=context.layer=="DETAIL_VIEW"?(center?QStringLiteral("DETAIL_CENTER"):QStringLiteral("DETAIL_HIDDEN")):
                (center?QStringLiteral("CENTER"):QStringLiteral("HIDDEN"));
        }
        stack.append(context);
        if(context.skip&&name=="circle"&&!context.clipDefinitionId.isEmpty()){
            DxfClipCircle clip;
            if(!parseNumber(attributes.value("cx"),&clip.x)||!parseNumber(attributes.value("cy"),&clip.y)||
               !parseNumber(attributes.value("r"),&clip.radius)||clip.radius<=0)
                return fail(QStringLiteral("Invalid drawing detail clip circle."));
            clipCircles.insert(context.clipDefinitionId,clip);
        }
        if(context.skip)continue;
        if(name=="polyline"){
            QVector<double> coordinates;
            if(!parseNumbers(attributes.value("points").toString(),&coordinates)||coordinates.size()<4||coordinates.size()%2)
                return fail(QStringLiteral("Invalid drawing SVG polyline."));
            for(int i=2;i<coordinates.size();i+=2)
                if(!clippedLine(coordinates[i-2],coordinates[i-1],coordinates[i],coordinates[i+1],context))
                    return fail(QStringLiteral("Drawing polyline coordinates exceed the DXF range."));
        }else if(name=="rect"){
            // The white SVG canvas is a display background, not drawing geometry.
            if(attributes.value("fill")=="white"&&context.stroke.isEmpty())continue;
            double x=0,y=0,w=0,h=0;
            if(!parseNumber(attributes.value("x"),&x)||!parseNumber(attributes.value("y"),&y)||
               !parseNumber(attributes.value("width"),&w)||!parseNumber(attributes.value("height"),&h)||w<=0||h<=0)
                return fail(QStringLiteral("Invalid drawing SVG rectangle."));
            if(!line(x,y,x+w,y,context)||!line(x+w,y,x+w,y+h,context)||
               !line(x+w,y+h,x,y+h,context)||!line(x,y+h,x,y,context))
                return fail(QStringLiteral("Drawing rectangle coordinates exceed the DXF range."));
        }else if(name=="circle"){
            double cx=0,cy=0,radius=0;
            if(!parseNumber(attributes.value("cx"),&cx)||!parseNumber(attributes.value("cy"),&cy)||
               !parseNumber(attributes.value("r"),&radius)||radius<=0)return fail(QStringLiteral("Invalid drawing SVG circle."));
            out << "0\nCIRCLE\n8\n" << context.layer << "\n10\n" << cx*context.scale
                << "\n20\n" << sheetHeight-cy*context.scale << "\n40\n" << radius*context.scale << '\n';
            ++entities;
        }else if(name=="path"){
            const QString path=attributes.value("d").toString();
            static const QRegularExpression tokens(QStringLiteral(R"([MmLlHhVvZz]|[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)"));
            QString remainder=path;remainder.replace(tokens,QString());remainder.remove(QRegularExpression(QStringLiteral(R"([\s,])")));
            if(!remainder.isEmpty())return fail(QStringLiteral("Unsupported drawing SVG path command for DXF conversion."));
            QStringList parts;auto matches=tokens.globalMatch(path);while(matches.hasNext())parts.append(matches.next().captured(0));
            double x=0,y=0,startX=0,startY=0;QChar command;
            for(int i=0;i<parts.size();){
                if(parts[i].size()==1&&parts[i][0].isLetter()){
                    command=parts[i++][0];
                    if(command=='Z'||command=='z'){
                        if(!line(x,y,startX,startY,context))return fail(QStringLiteral("Drawing path exceeds the DXF range."));
                        x=startX;y=startY;continue;
                    }
                }
                if(command.isNull())return fail(QStringLiteral("Invalid drawing SVG path."));
                if(command=='M'||command=='m'||command=='L'||command=='l'){
                    if(i+1>=parts.size())return fail(QStringLiteral("Incomplete drawing SVG path point."));
                    double px=0,py=0;
                    if(!parseNumber(QStringView(parts[i++]),&px)||!parseNumber(QStringView(parts[i++]),&py))
                        return fail(QStringLiteral("Invalid drawing SVG path point."));
                    if(command.isLower()){px+=x;py+=y;}
                    if(command=='M'||command=='m'){x=startX=px;y=startY=py;command=command=='M'?'L':'l';}
                    else {if(!line(x,y,px,py,context))return fail(QStringLiteral("Drawing path exceeds the DXF range."));x=px;y=py;}
                }else if(command=='H'||command=='h'||command=='V'||command=='v'){
                    if(i>=parts.size())return fail(QStringLiteral("Incomplete drawing SVG path coordinate."));
                    double value=0;if(!parseNumber(QStringView(parts[i++]),&value))return fail(QStringLiteral("Invalid drawing SVG path coordinate."));
                    const double px=command=='H'?value:(command=='h'?x+value:x);
                    const double py=command=='V'?value:(command=='v'?y+value:y);
                    if(!line(x,y,px,py,context))return fail(QStringLiteral("Drawing path exceeds the DXF range."));
                    x=px;y=py;
                }else return fail(QStringLiteral("Unsupported drawing SVG path command for DXF conversion."));
            }
        }else if(name=="text"){
            double x=0,y=0;
            if(!parseNumber(attributes.value("x"),&x)||!parseNumber(attributes.value("y"),&y))
                return fail(QStringLiteral("Invalid drawing SVG text position."));
            const QString value=dxfText(reader.readElementText(QXmlStreamReader::IncludeChildElements));
            stack.removeLast();
            if(value.isEmpty())continue;
            if(context.textRotationDegrees!=0.0){
                const double radians=context.textRotationDegrees*std::acos(-1.0)/180.0;
                const double dx=x-context.rotationPivotX,dy=y-context.rotationPivotY;
                x=context.rotationPivotX+std::cos(radians)*dx-std::sin(radians)*dy;
                y=context.rotationPivotY+std::sin(radians)*dx+std::cos(radians)*dy;
            }
            const double textX=x*context.scale,textY=sheetHeight-y*context.scale;
            out << "0\nTEXT\n8\n" << context.layer << "\n10\n" << textX << "\n20\n" << textY
                << "\n40\n" << context.textSize*context.scale << "\n1\n" << value << '\n';
            if(context.textRotationDegrees!=0.0)out << "50\n" << -context.textRotationDegrees << '\n';
            if(attributes.value("text-anchor")=="middle")
                out << "72\n1\n11\n" << textX << "\n21\n" << textY << '\n';
            ++entities;
        }
    }
    if(reader.hasError())return fail(QStringLiteral("Invalid drawing SVG XML: ")+reader.errorString());
    if(!rootSeen||entities<10)return fail(QStringLiteral("The drawing SVG contains insufficient geometry for DXF export."));
    out << "0\nENDSEC\n0\nEOF\n";out.flush();
    if(out.status()!=QTextStream::Ok)return fail(QStringLiteral("Could not format the drawing DXF."));
    *dxf=std::move(bytes);return true;
}

bool exportOrthographicDrawingDxf(const QByteArray& svg,const QString& path,QString* error){
    if(path.trimmed().isEmpty()){if(error)*error="Provide a drawing DXF path.";return false;}
    QByteArray bytes;if(!buildOrthographicDrawingDxf(svg,&bytes,error))return false;
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly)){if(error)*error=QStringLiteral("Could not open drawing DXF: ")+file.errorString();return false;}
    if(file.write(bytes)!=bytes.size()||!file.commit()){
        if(error)*error=QStringLiteral("Could not write drawing DXF: ")+file.errorString();return false;
    }
    return true;
}
} // namespace cad

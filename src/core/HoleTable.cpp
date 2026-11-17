#include "HoleTable.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <Bnd_Box.hxx>
#include <GeomAbs_CurveType.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <Standard_Failure.hxx>
#include <exception>
#include <QSaveFile>
#include <QTextStream>
#include <QLocale>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace cad {
namespace {
bool fail(QString* error,const QString& message){if(error)*error=message;return false;}
bool finite(double value){return std::isfinite(value)&&std::abs(value)<1e6;}
bool near(double a,double b,double tolerance=1e-4){return std::abs(a-b)<=tolerance;}
QString csvCell(QString value){
    // Prevent spreadsheet formula execution, including leading whitespace.
    const QString trimmed=value.trimmed();
    if(!trimmed.isEmpty()&&QStringLiteral("=+-@\t\r").contains(trimmed.front()))value.prepend(QChar(0x27));
    value.replace('"',QStringLiteral("\"\""));
    return QStringLiteral("\"")+value+QStringLiteral("\"");
}
}
bool inspectAxialHoles(const Part& part,AxialHoleAudit* result,QString* error){
    if(!result)return fail(error,"Provide an output for the hole inspection.");
    if(part.shape.IsNull()||part.isSurfaceBody||!BRepCheck_Analyzer(part.shape).IsValid())
        return fail(error,"Hole inspection requires a valid solid body.");
    try{
    // A multi-solid compound has no unambiguous shared drilling envelope.
    // Inspect each physical part independently rather than merging its holes.
    int solidCount=0;
    for(TopExp_Explorer solid(part.shape,TopAbs_SOLID);solid.More();solid.Next())++solidCount;
    if(solidCount!=1)return fail(error,"Inspect one connected solid at a time.");
    Bnd_Box shapeBounds;BRepBndLib::AddOptimal(part.shape,shapeBounds,false,false);
    if(shapeBounds.IsVoid())return fail(error,"The body has no finite machining envelope.");
    double x0,y0,z0,x1,y1,z1;
    shapeBounds.Get(x0,y0,z0,x1,y1,z1);
    if(!finite(x0)||!finite(y0)||!finite(x1)||!finite(y1)||
       !finite(z0)||!finite(z1)||z1-z0<1e-5)
        return fail(error,"The body has an invalid Z machining envelope.");
    AxialHoleAudit candidate;
    candidate.sourceBodyId=part.id;candidate.sourceGeometryToken=part.geometryToken;
    candidate.stockTopZ=z1;candidate.stockBottomZ=z0;
    for(TopExp_Explorer faces(part.shape,TopAbs_FACE);faces.More();faces.Next()){
        const TopoDS_Face face=TopoDS::Face(faces.Current());
        BRepAdaptor_Surface surface(face);
        if(surface.GetType()!=GeomAbs_Cylinder)continue;
        const auto cylinder=surface.Cylinder();
        const auto axis=cylinder.Axis().Direction();
        // OCCT's reversed cylinder orientation distinguishes material-facing
        // internal walls from exterior shafts on valid orientable solids.
        if(face.Orientation()!=TopAbs_REVERSED||std::abs(axis.Z())<0.99999){
            ++candidate.excludedCylinders;continue;
        }
        const auto origin=cylinder.Location();
        const double radius=cylinder.Radius();
        if(!finite(origin.X())||!finite(origin.Y())||!finite(radius)||radius<1e-5){
            ++candidate.excludedCylinders;continue;
        }
        // Require at least two complete, separated circular rim edges. A
        // partial cylindrical wall (slot/open pocket) is not a round hole.
        QVector<double> rims;
        for(TopExp_Explorer edges(face,TopAbs_EDGE);edges.More();edges.Next()){
            BRepAdaptor_Curve curve(TopoDS::Edge(edges.Current()));
            if(curve.GetType()!=GeomAbs_Circle)continue;
            if(std::abs(curve.LastParameter()-curve.FirstParameter()-2*std::numbers::pi)>1e-4)continue;
            const auto circle=curve.Circle();
            if(!near(circle.Radius(),radius)||std::abs(circle.Axis().Direction().Z())<0.99999||
               !near(circle.Location().X(),origin.X())||!near(circle.Location().Y(),origin.Y()))continue;
            if(std::none_of(rims.cbegin(),rims.cend(),[&](double z){return near(z,circle.Location().Z());}))
                rims.append(circle.Location().Z());
        }
        if(rims.size()<2){++candidate.excludedCylinders;continue;}
        std::sort(rims.begin(),rims.end());
        const double lower=rims.first(),upper=rims.last();
        if(!finite(lower)||!finite(upper)||upper-lower<1e-5){++candidate.excludedCylinders;continue;}
        // Deduplicate the same full cylinder represented by repeated faces.
        bool duplicate=false;
        for(const auto& previous:candidate.holes)
            if(near(previous.xMm,origin.X())&&near(previous.yMm,origin.Y())&&
               near(previous.diameterMm,2*radius)&&near(previous.zMinMm,lower)&&
               near(previous.zMaxMm,upper)){duplicate=true;break;}
        if(duplicate)continue;
        AxialHoleRecord hole;
        hole.xMm=origin.X();hole.yMm=origin.Y();hole.diameterMm=2*radius;
        hole.zMinMm=lower;hole.zMaxMm=upper;
        hole.throughEnvelope=lower<=z0+1e-3&&upper>=z1-1e-3;
        candidate.holes.append(hole);
        if(candidate.holes.size()>512)return fail(error,"Hole count exceeds the 512-record inspection limit.");
    }
    std::sort(candidate.holes.begin(),candidate.holes.end(),[](const auto& a,const auto& b){
        if(!near(a.xMm,b.xMm))return a.xMm<b.xMm;
        if(!near(a.yMm,b.yMm))return a.yMm<b.yMm;
        if(!near(a.zMinMm,b.zMinMm))return a.zMinMm<b.zMinMm;
        return a.diameterMm<b.diameterMm;
    });
    for(int i=0;i<candidate.holes.size();++i)candidate.holes[i].number=i+1;
    *result=std::move(candidate);
    if(error)error->clear();
    return true;
    }catch(const Standard_Failure& failure){
        return fail(error,QStringLiteral("OpenCascade hole inspection failed: %1")
                            .arg(QString::fromUtf8(failure.what())));
    }catch(const std::exception& failure){
        return fail(error,QStringLiteral("Hole inspection failed: %1")
                            .arg(QString::fromUtf8(failure.what())));
    }
}
bool buildAxialHoleTableCsv(const AxialHoleAudit& audit,QByteArray* csv,QString* error){
    if(!csv)return fail(error,"Provide an output buffer for the hole table.");
    if(audit.sourceBodyId.isEmpty()||audit.sourceGeometryToken.isEmpty()||audit.holes.size()>512)
        return fail(error,"Hole table has no valid source identity or exceeds its row limit.");
    QByteArray bytes;
    QTextStream text(&bytes,QIODevice::WriteOnly);text.setLocale(QLocale::c());
    text << "CADWorkbench geometric axial-hole audit,not a verified manufacturing callout\n";
    text << "Source body,"<<csvCell(audit.sourceBodyId)<<"\n";
    text << "Source geometry revision,"<<csvCell(audit.sourceGeometryToken)<<"\n";
    text << "Excluded/unsupported cylindrical faces,"<<audit.excludedCylinders<<"\n";
    text << "Hole,X (mm),Y (mm),Diameter (mm),Z minimum (mm),Z maximum (mm),Axial extent (mm),Envelope spanning\n";
    for(const auto& hole:audit.holes){
        if(hole.number<1||!finite(hole.xMm)||!finite(hole.yMm)||!finite(hole.diameterMm)||
           !finite(hole.zMinMm)||!finite(hole.zMaxMm)||hole.diameterMm<=0||
           hole.zMaxMm<=hole.zMinMm)return fail(error,"Invalid hole record.");
        const auto number=[](double value){return QString::number(value,'f',4);};
        text << "H" << hole.number << ',' << number(hole.xMm) << ',' << number(hole.yMm)
             << ',' << number(hole.diameterMm) << ',' << number(hole.zMinMm)
             << ',' << number(hole.zMaxMm) << ',' << number(hole.zMaxMm-hole.zMinMm)
             << ',' << (hole.throughEnvelope?"envelope-spanning":"not-envelope-spanning") << '\n';
    }
    text.flush();
    *csv=std::move(bytes);
    if(error)error->clear();
    return true;
}
bool exportAxialHoleTableCsv(const Part& part,const QString& path,QString* error){
    if(path.trimmed().isEmpty())return fail(error,"Provide a CSV output path.");
    AxialHoleAudit audit;QByteArray bytes;
    if(!inspectAxialHoles(part,&audit,error)||!buildAxialHoleTableCsv(audit,&bytes,error))return false;
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly))return fail(error,file.errorString());
    if(file.write(bytes)!=bytes.size()){
        const QString message=file.errorString();file.cancelWriting();
        return fail(error,message);
    }
    if(!file.commit())return fail(error,file.errorString());
    if(error)error->clear();
    return true;
}
} // namespace cad

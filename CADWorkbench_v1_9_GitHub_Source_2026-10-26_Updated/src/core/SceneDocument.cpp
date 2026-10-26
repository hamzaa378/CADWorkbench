#include "SceneDocument.h"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepOffsetAPI_DraftAngle.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRep_Builder.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepTools.hxx>
#include <GProp_GProps.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <Geom_BSplineCurve.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <Standard_Failure.hxx>
#include <StlAPI_Writer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopExp.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <NCollection_List.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopExp.hxx>
#include <NCollection_IndexedMap.hxx>
#include <NCollection_HArray1.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Elips.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_GTrsf.hxx>
#include <gp_Mat.hxx>
#include <gp_Pln.hxx>
#include <gp_Vec.hxx>

#include <QColor>
#include <QFile>
#include <QSet>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <cmath>
#include <algorithm>
#include <limits>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace cad {
namespace {
constexpr qsizetype kMaxFileBytes = 96 * 1024 * 1024;
constexpr qsizetype kMaxStepImportBytes = 512 * 1024 * 1024;
constexpr int kMaxObjects = 2000;
constexpr int kHistoryDepth = 40;
const QString kSchema1 = QStringLiteral("cadworkbench.scene.v1");
const QString kSchema2 = QStringLiteral("cadworkbench.scene.v2");

bool finitePositive(double v) { return std::isfinite(v) && v > 1e-5 && v <= 1e6; }
bool finiteOffset(double v) { return std::isfinite(v) && std::abs(v) <= 1e6; }
QString generatedId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
const char* primitiveName(Primitive kind) {
    switch(kind) {
        case Primitive::Box: return "Box";
        case Primitive::Cylinder: return "Cylinder";
        case Primitive::Sphere: return "Sphere";
        case Primitive::Cone: return "Cone";
        case Primitive::Torus: return "Torus";
        case Primitive::RegularPrism: return "Regular Prism";
        case Primitive::Capsule: return "Capsule";
        case Primitive::Ellipsoid: return "Ellipsoid";
        case Primitive::Tube: return "Tube";
        case Primitive::Wedge: return "Wedge";
        case Primitive::PyramidFrustum: return "Pyramid Frustum";
        case Primitive::HexNut: return "Hex Nut";
    }
    return "Solid";
}
QString brepToText(const TopoDS_Shape& shape) {
    std::ostringstream out;
    BRepTools::Write(shape, out);
    if (!out.good()) throw std::runtime_error("B-rep serialization failed");
    return QString::fromLatin1(QByteArray::fromStdString(out.str()).toBase64());
}
TopoDS_Shape brepFromText(const QString& encoded) {
    const QByteArray bytes = QByteArray::fromBase64(encoded.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (bytes.isEmpty()) throw std::runtime_error("Invalid or empty B-rep payload");
    std::istringstream in(bytes.toStdString());
    TopoDS_Shape shape;
    BRep_Builder builder;
    BRepTools::Read(shape, in, builder);
    if (shape.IsNull() || in.bad()) throw std::runtime_error("Failed to parse B-rep payload");
    return shape;
}
bool isHexColor(const QString& text) {
    if (text.size() != 7 || text[0] != QLatin1Char('#')) return false;
    for (int i = 1; i < 7; ++i) {
        const ushort c = text[i].unicode();
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}
bool hasSolid(const TopoDS_Shape& shape) {
    for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next()) return true;
    return false;
}
} // namespace

void SceneDocument::setError(QString* error, const QString& text) { if (error) *error = text; }

bool SceneDocument::validShape(const TopoDS_Shape& shape, QString* error) {
    if (shape.IsNull()) { setError(error, "The kernel returned a null shape."); return false; }
    if (!hasSolid(shape)) { setError(error, "A modeling operation must produce at least one solid."); return false; }
    if (!BRepCheck_Analyzer(shape).IsValid()) {
        setError(error, "Geometry failed OCCT topology validation."); return false;
    }
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props);
    if (!std::isfinite(props.Mass()) || props.Mass() <= 1e-12) {
        setError(error, "Solid has zero or invalid volume."); return false;
    }
    return true;
}

const Part* SceneDocument::find(const QString& id) const {
    for (const Part& part : parts_) if (part.id == id) return &part;
    return nullptr;
}

QString SceneDocument::uniqueName(const QString& base) const {
    QString candidate = base;
    int suffix = 2;
    bool occupied = true;
    while (occupied) {
        occupied = false;
        for (const auto& part : parts_) if (part.name.compare(candidate, Qt::CaseInsensitive) == 0) {
            occupied = true;
            candidate = base + QStringLiteral(" %1").arg(suffix++);
            break;
        }
    }
    return candidate;
}

void SceneDocument::checkpoint() {
    QVector<Part> snapshotParts = parts_;
    for (auto& part : snapshotParts) part.shape = BRepBuilderAPI_Copy(part.shape).Shape();
    undo_.append({std::move(snapshotParts), revision_});
    if (undo_.size() > kHistoryDepth) undo_.removeFirst();
    redo_.clear();
    revision_ = nextRevision_++;
}

bool SceneDocument::addPrimitive(Primitive kind, const QVector<double>& dimensions, QString* error) {
    int required = 0;
    switch(kind) {
        case Primitive::Box: required = 3; break;
        case Primitive::Cylinder: required = 2; break;
        case Primitive::Sphere: required = 1; break;
        case Primitive::Cone: required = 3; break;
        case Primitive::Torus: required = 2; break;
        case Primitive::RegularPrism: required = 3; break; // circumradius, height, side count
        case Primitive::Capsule: required = 2; break; // radius, distance between cap centers
        case Primitive::Ellipsoid: required = 3; break; // X/Y/Z radii
        case Primitive::Tube: required = 3; break; // outer radius, inner radius, height
        case Primitive::Wedge: required = 4; break; // width, depth, height, top width
        case Primitive::PyramidFrustum: required = 5; break; // base X/Y, top X/Y, height
        case Primitive::HexNut: required = 3; break; // outer radius, bore radius, height
    }
    if (dimensions.size() != required) { setError(error, "Wrong number of dimensions."); return false; }
    for (double v : dimensions) if (!finitePositive(v)) {
        setError(error, "Dimensions must be finite and between 0.00001 and 1,000,000 mm."); return false;
    }
    if (kind == Primitive::Torus && dimensions[0] <= dimensions[1]) {
        setError(error, "The torus major radius must exceed its tube radius."); return false;
    }
    if (kind == Primitive::RegularPrism &&
        (dimensions[2] < 3 || dimensions[2] > 64 || std::floor(dimensions[2]) != dimensions[2])) {
        setError(error, "A regular prism needs an integer side count from 3 through 64."); return false;
    }
    if (kind == Primitive::Tube && dimensions[1] >= dimensions[0]) {
        setError(error, "The tube inner radius must be smaller than its outer radius."); return false;
    }
    if (kind == Primitive::Wedge && dimensions[3] > dimensions[0]) {
        setError(error, "The wedge top width must not exceed its base width."); return false;
    }
    if (kind == Primitive::PyramidFrustum &&
        (dimensions[2] > dimensions[0] || dimensions[3] > dimensions[1])) {
        setError(error, "Pyramid-frustum top dimensions must not exceed its base dimensions."); return false;
    }
    if (kind == Primitive::HexNut && dimensions[1] >= dimensions[0] * std::cos(std::numbers::pi / 6.0)) {
        setError(error, "The hex-nut bore must be smaller than the hexagon's inradius."); return false;
    }
    if (parts_.size() >= kMaxObjects) { setError(error, "Scene object limit reached."); return false; }
    try {
        TopoDS_Shape shape;
        switch (kind) {
            case Primitive::Box: shape = BRepPrimAPI_MakeBox(dimensions[0], dimensions[1], dimensions[2]).Shape(); break;
            case Primitive::Cylinder: shape = BRepPrimAPI_MakeCylinder(dimensions[0], dimensions[1]).Shape(); break;
            case Primitive::Sphere: shape = BRepPrimAPI_MakeSphere(dimensions[0]).Shape(); break;
            case Primitive::Cone: shape = BRepPrimAPI_MakeCone(dimensions[0], dimensions[1], dimensions[2]).Shape(); break;
            case Primitive::Torus: shape = BRepPrimAPI_MakeTorus(dimensions[0], dimensions[1]).Shape(); break;
            case Primitive::RegularPrism: {
                BRepBuilderAPI_MakePolygon polygon;
                const int sides = static_cast<int>(dimensions[2]);
                for (int i = 0; i < sides; ++i) {
                    const double angle = 2.0 * std::numbers::pi * i / sides;
                    polygon.Add(gp_Pnt(dimensions[0] * std::cos(angle),
                                       dimensions[0] * std::sin(angle), 0.0));
                }
                polygon.Close();
                const TopoDS_Face base = BRepBuilderAPI_MakeFace(polygon.Wire()).Face();
                shape = BRepPrimAPI_MakePrism(base, gp_Vec(0, 0, dimensions[1])).Shape();
                break;
            }
            case Primitive::Capsule: {
                const double radius = dimensions[0], centerSpan = dimensions[1];
                const TopoDS_Shape shaft = BRepPrimAPI_MakeCylinder(radius, centerSpan).Shape();
                const TopoDS_Shape lower = BRepPrimAPI_MakeSphere(gp_Pnt(0, 0, 0), radius).Shape();
                const TopoDS_Shape upper = BRepPrimAPI_MakeSphere(gp_Pnt(0, 0, centerSpan), radius).Shape();
                BRepAlgoAPI_Fuse first(shaft, lower);
                first.Build();
                if (!first.IsDone()) { setError(error, "Capsule lower cap could not be fused."); return false; }
                BRepAlgoAPI_Fuse second(first.Shape(), upper);
                second.Build();
                if (!second.IsDone()) { setError(error, "Capsule upper cap could not be fused."); return false; }
                shape = second.Shape();
                break;
            }
            case Primitive::Ellipsoid: {
                const gp_GTrsf transform(
                    gp_Mat(dimensions[0], 0, 0, 0, dimensions[1], 0, 0, 0, dimensions[2]),
                    gp_XYZ(0, 0, 0));
                shape = BRepBuilderAPI_GTransform(BRepPrimAPI_MakeSphere(1.0).Shape(), transform, false).Shape();
                break;
            }
            case Primitive::Tube: {
                const TopoDS_Shape outer = BRepPrimAPI_MakeCylinder(dimensions[0], dimensions[2]).Shape();
                const TopoDS_Shape inner = BRepPrimAPI_MakeCylinder(dimensions[1], dimensions[2]).Shape();
                BRepAlgoAPI_Cut cut(outer, inner);
                cut.Build();
                if (!cut.IsDone()) { setError(error, "Tube bore could not be cut."); return false; }
                shape = cut.Shape();
                break;
            }
            case Primitive::Wedge: {
                BRepBuilderAPI_MakePolygon polygon;
                polygon.Add(gp_Pnt(0, 0, 0));
                polygon.Add(gp_Pnt(dimensions[0], 0, 0));
                polygon.Add(gp_Pnt(dimensions[3], 0, dimensions[2]));
                polygon.Add(gp_Pnt(0, 0, dimensions[2]));
                polygon.Close();
                const TopoDS_Face profile = BRepBuilderAPI_MakeFace(polygon.Wire()).Face();
                shape = BRepPrimAPI_MakePrism(profile, gp_Vec(0, dimensions[1], 0)).Shape();
                break;
            }
            case Primitive::PyramidFrustum: {
                BRepBuilderAPI_MakePolygon base, top;
                base.Add(gp_Pnt(0,0,0)); base.Add(gp_Pnt(dimensions[0],0,0));
                base.Add(gp_Pnt(dimensions[0],dimensions[1],0)); base.Add(gp_Pnt(0,dimensions[1],0)); base.Close();
                const double xOffset=(dimensions[0]-dimensions[2])/2.0;
                const double yOffset=(dimensions[1]-dimensions[3])/2.0;
                top.Add(gp_Pnt(xOffset,yOffset,dimensions[4]));
                top.Add(gp_Pnt(xOffset+dimensions[2],yOffset,dimensions[4]));
                top.Add(gp_Pnt(xOffset+dimensions[2],yOffset+dimensions[3],dimensions[4]));
                top.Add(gp_Pnt(xOffset,yOffset+dimensions[3],dimensions[4])); top.Close();
                BRepOffsetAPI_ThruSections loft(true,true);
                loft.AddWire(base.Wire()); loft.AddWire(top.Wire()); loft.Build();
                if (!loft.IsDone()) { setError(error,"Pyramid frustum loft failed."); return false; }
                shape=loft.Shape();
                break;
            }
            case Primitive::HexNut: {
                BRepBuilderAPI_MakePolygon boundary;
                for (int i=0;i<6;++i) {
                    const double angle=2.0*std::numbers::pi*i/6.0;
                    boundary.Add(gp_Pnt(dimensions[0]*std::cos(angle),dimensions[0]*std::sin(angle),0));
                }
                boundary.Close();
                const TopoDS_Face base=BRepBuilderAPI_MakeFace(boundary.Wire()).Face();
                const TopoDS_Shape blank=BRepPrimAPI_MakePrism(base,gp_Vec(0,0,dimensions[2])).Shape();
                const TopoDS_Shape bore=BRepPrimAPI_MakeCylinder(dimensions[1],dimensions[2]).Shape();
                BRepAlgoAPI_Cut cut(blank,bore); cut.Build();
                if (!cut.IsDone()) { setError(error,"Hex-nut bore could not be cut."); return false; }
                shape=cut.Shape();
                break;
            }
        }
        if (!validShape(shape, error)) return false;
        const QString name = uniqueName(QString::fromLatin1(primitiveName(kind)));
        checkpoint();
        parts_.append({generatedId(), name, shape});
        return true;
    } catch (const Standard_Failure& e) { setError(error, QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error, QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::loftRectangles(double bottomX,double bottomY,double bottomWidth,double bottomDepth,
                                   double topX,double topY,double topWidth,double topDepth,double height,QString* error) {
    if(!finiteOffset(bottomX)||!finiteOffset(bottomY)||!finiteOffset(topX)||!finiteOffset(topY)||
       !finitePositive(bottomWidth)||!finitePositive(bottomDepth)||!finitePositive(topWidth)||
       !finitePositive(topDepth)||!finitePositive(height)){
        setError(error,"Loft origins and section dimensions must be finite and positive.");return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        auto rectangleWire=[](double x,double y,double w,double d,double z){
            BRepBuilderAPI_MakePolygon polygon;
            polygon.Add(gp_Pnt(x,y,z));polygon.Add(gp_Pnt(x+w,y,z));polygon.Add(gp_Pnt(x+w,y+d,z));
            polygon.Add(gp_Pnt(x,y+d,z));polygon.Close();
            return polygon.Wire();
        };
        BRepOffsetAPI_ThruSections loft(true,true);
        loft.AddWire(rectangleWire(bottomX,bottomY,bottomWidth,bottomDepth,0));
        loft.AddWire(rectangleWire(topX,topY,topWidth,topDepth,height));
        loft.Build();
        if(!loft.IsDone()){setError(error,"The kernel could not loft between the rectangular sections.");return false;}
        const TopoDS_Shape result=loft.Shape();
        if(!validShape(result,error))return false;
        const QString name=uniqueName(QStringLiteral("Lofted Rectangle"));
        checkpoint();parts_.append({generatedId(),name,result});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::loftEllipticalSections(double bottomSemiAxisA,double bottomSemiAxisB,
                                           double topSemiAxisA,double topSemiAxisB,double height,
                                           double offsetX,double offsetY,QString* error) {
    if(!finitePositive(bottomSemiAxisA)||!finitePositive(bottomSemiAxisB)||
       !finitePositive(topSemiAxisA)||!finitePositive(topSemiAxisB)||!finitePositive(height)||
       !finiteOffset(offsetX)||!finiteOffset(offsetY)){
        setError(error,"Elliptical loft semi-axes and height must be positive; top offsets must be finite.");return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        const auto ellipseWire=[](double centerX,double centerY,double axisA,double axisB,double z){
            const double major=std::max(axisA,axisB),minor=std::min(axisA,axisB);
            const gp_Dir majorDirection=axisA>=axisB?gp_Dir(1,0,0):gp_Dir(0,1,0);
            const gp_Ax2 frame(gp_Pnt(centerX,centerY,z),gp_Dir(0,0,1),majorDirection);
            return BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Elips(frame,major,minor)).Edge()).Wire();
        };
        BRepOffsetAPI_ThruSections loft(true,true);
        loft.AddWire(ellipseWire(0,0,bottomSemiAxisA,bottomSemiAxisB,0));
        loft.AddWire(ellipseWire(offsetX,offsetY,topSemiAxisA,topSemiAxisB,height));
        loft.Build();
        if(!loft.IsDone()){setError(error,"The kernel could not loft between the elliptical sections.");return false;}
        const TopoDS_Shape result=loft.Shape();
        if(!validShape(result,error))return false;
        const QString name=uniqueName(QStringLiteral("Lofted Ellipse"));
        checkpoint();parts_.append({generatedId(),name,result});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Elliptical loft kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::addOrientedCylinder(double radius,double height,double ox,double oy,double oz,double ax,double ay,double az,QString* error) {
    if(!finitePositive(radius)||!finitePositive(height)||!finiteOffset(ox)||!finiteOffset(oy)||!finiteOffset(oz)||
       !std::isfinite(ax)||!std::isfinite(ay)||!std::isfinite(az)||std::sqrt(ax*ax+ay*ay+az*az)<1e-8){
        setError(error,"Cylinder dimensions, origin, or axis are invalid.");return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        const TopoDS_Shape shape=BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(ox,oy,oz),gp_Dir(ax,ay,az)),radius,height).Shape();
        if(!validShape(shape,error))return false;
        const QString name=uniqueName(QStringLiteral("Oriented Cylinder"));checkpoint();parts_.append({generatedId(),name,shape});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::addOrientedCone(double bottomRadius,double topRadius,double height,double ox,double oy,double oz,
                                    double ax,double ay,double az,QString* error) {
    if(!finitePositive(bottomRadius)||!std::isfinite(topRadius)||topRadius<0||topRadius>1e6||!finitePositive(height)||
       !finiteOffset(ox)||!finiteOffset(oy)||!finiteOffset(oz)||!std::isfinite(ax)||!std::isfinite(ay)||!std::isfinite(az)||
       std::sqrt(ax*ax+ay*ay+az*az)<1e-8){setError(error,"Cone dimensions, origin, or axis are invalid.");return false;}
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        const TopoDS_Shape shape=BRepPrimAPI_MakeCone(gp_Ax2(gp_Pnt(ox,oy,oz),gp_Dir(ax,ay,az)),bottomRadius,topRadius,height).Shape();
        if(!validShape(shape,error))return false;
        const QString name=uniqueName(QStringLiteral("Oriented Cone"));checkpoint();parts_.append({generatedId(),name,shape});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::addTorusSegment(double majorRadius,double tubeRadius,double angleDegrees,QString* error) {
    if(!finitePositive(majorRadius)||!finitePositive(tubeRadius)||majorRadius<=tubeRadius||
       !std::isfinite(angleDegrees)||angleDegrees<=1.0||angleDegrees>360.0){
        setError(error,"Torus major radius must exceed tube radius; segment angle must be over 1 and at most 360 degrees.");return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        const TopoDS_Shape shape=BRepPrimAPI_MakeTorus(majorRadius,tubeRadius,angleDegrees*std::numbers::pi/180.0).Shape();
        if(!validShape(shape,error))return false;
        const QString name=uniqueName(QStringLiteral("Torus Segment"));checkpoint();parts_.append({generatedId(),name,shape});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::sweepCircularProfile(double radius,double pathLength,double lateralOffset,double verticalRise,QString* error) {
    return addSweepProfile(true,false,radius,radius,pathLength,lateralOffset,verticalRise,error);
}

bool SceneDocument::sweepRectangularProfile(double width,double height,double pathLength,double lateralOffset,double verticalRise,QString* error) {
    return addSweepProfile(false,false,width,height,pathLength,lateralOffset,verticalRise,error);
}

bool SceneDocument::sweepEllipticalProfile(double semiAxisA,double semiAxisB,double pathLength,double lateralOffset,
                                            double verticalRise,QString* error) {
    return addSweepProfile(false,true,semiAxisA,semiAxisB,pathLength,lateralOffset,verticalRise,error);
}

bool SceneDocument::addHelicalSweep(double profileRadius,double coilRadius,double pitch,double turns,QString* error) {
    if(!finitePositive(profileRadius)||!finitePositive(coilRadius)||!finitePositive(pitch)||
       !finitePositive(turns)||turns<0.25||turns>20.0||coilRadius<=profileRadius||pitch<=2.0*profileRadius){
        setError(error,"Helix requires positive dimensions, at least 0.25 and at most 20 turns, coil radius greater than wire radius, and pitch greater than wire diameter.");
        return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        constexpr int samplesPerTurn=24;
        const int sampleCount=std::max(6,static_cast<int>(std::ceil(turns*samplesPerTurn)));
        Handle(NCollection_HArray1<gp_Pnt>) points=new NCollection_HArray1<gp_Pnt>(1,sampleCount+1);
        for(int i=0;i<=sampleCount;++i){
            const double angle=2.0*std::numbers::pi*turns*static_cast<double>(i)/sampleCount;
            points->SetValue(i+1,gp_Pnt(coilRadius*std::cos(angle),coilRadius*std::sin(angle),pitch*turns*static_cast<double>(i)/sampleCount));
        }
        GeomAPI_Interpolate interpolation(points,false,1e-6);
        interpolation.Perform();
        if(!interpolation.IsDone()){setError(error,"Could not interpolate the helical path.");return false;}
        const Handle(Geom_BSplineCurve) centerline=interpolation.Curve();
        const TopoDS_Wire spine=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(centerline).Edge()).Wire();
        gp_Pnt start,tangentPoint;gp_Vec tangent;
        centerline->D1(centerline->FirstParameter(),start,tangent);
        if(tangent.SquareMagnitude()<=1e-16){setError(error,"Helical path has no initial direction.");return false;}
        tangent.Normalize();
        gp_Vec reference(0,0,1);reference-=tangent*tangent.Dot(reference);
        if(reference.SquareMagnitude()<=1e-8){reference=gp_Vec(0,1,0);reference-=tangent*tangent.Dot(reference);}
        const gp_Ax2 frame(start,gp_Dir(tangent),gp_Dir(reference));
        const TopoDS_Wire profile=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(frame,profileRadius)).Edge()).Wire();
        BRepOffsetAPI_MakePipeShell pipe(spine);
        pipe.Add(profile);
        pipe.Build();
        if(!pipe.IsDone()||!pipe.MakeSolid()){
            setError(error,"Open CASCADE could not build a solid from this helical path.");return false;
        }
        const TopoDS_Shape shape=pipe.Shape();
        if(!validShape(shape,error))return false;
        const QString name=uniqueName(QStringLiteral("Helical Sweep"));
        checkpoint();parts_.append({generatedId(),name,shape});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Helical sweep kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::addVariablePitchHelicalSweep(double profileRadius,double coilRadius,double startPitch,
                                                  double endPitch,double turns,QString* error) {
    if(!finitePositive(profileRadius)||!finitePositive(coilRadius)||!finitePositive(startPitch)||
       !finitePositive(endPitch)||!finitePositive(turns)||turns<0.25||turns>20.0||
       coilRadius<=profileRadius||startPitch<=2.0*profileRadius||endPitch<=2.0*profileRadius){
        setError(error,"Variable-pitch helix requires positive dimensions, 0.25–20 turns, coil radius greater than wire radius, and both pitches greater than wire diameter.");
        return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        constexpr int samplesPerTurn=32;
        const int sampleCount=std::max(8,static_cast<int>(std::ceil(turns*samplesPerTurn)));
        Handle(NCollection_HArray1<gp_Pnt>) points=new NCollection_HArray1<gp_Pnt>(1,sampleCount+1);
        for(int i=0;i<=sampleCount;++i){
            const double t=static_cast<double>(i)/sampleCount;
            const double angle=2.0*std::numbers::pi*turns*t;
            const double z=turns*(startPitch*t+0.5*(endPitch-startPitch)*t*t);
            points->SetValue(i+1,gp_Pnt(coilRadius*std::cos(angle),coilRadius*std::sin(angle),z));
        }
        GeomAPI_Interpolate interpolation(points,false,1e-6);
        interpolation.Perform();
        if(!interpolation.IsDone()){setError(error,"Could not interpolate the variable-pitch helical path.");return false;}
        const Handle(Geom_BSplineCurve) centerline=interpolation.Curve();
        const TopoDS_Wire spine=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(centerline).Edge()).Wire();
        gp_Pnt start;gp_Vec tangent;
        centerline->D1(centerline->FirstParameter(),start,tangent);
        if(tangent.SquareMagnitude()<=1e-16){setError(error,"Variable-pitch path has no initial direction.");return false;}
        tangent.Normalize();
        gp_Vec reference(0,0,1);reference-=tangent*tangent.Dot(reference);
        if(reference.SquareMagnitude()<=1e-8){reference=gp_Vec(0,1,0);reference-=tangent*tangent.Dot(reference);}
        const gp_Ax2 frame(start,gp_Dir(tangent),gp_Dir(reference));
        const TopoDS_Wire profile=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(frame,profileRadius)).Edge()).Wire();
        BRepOffsetAPI_MakePipeShell pipe(spine);
        pipe.Add(profile);pipe.Build();
        if(!pipe.IsDone()||!pipe.MakeSolid()){
            setError(error,"Open CASCADE could not build a solid from this variable-pitch path.");return false;
        }
        const TopoDS_Shape shape=pipe.Shape();
        if(!validShape(shape,error))return false;
        const QString name=uniqueName(QStringLiteral("Variable Pitch Helix"));
        checkpoint();parts_.append({generatedId(),name,shape});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Variable-pitch helix kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::addTaperedHelicalSweep(double profileRadius,double startCoilRadius,double endCoilRadius,
                                            double pitch,double turns,QString* error) {
    if(!finitePositive(profileRadius)||!finitePositive(startCoilRadius)||!finitePositive(endCoilRadius)||
       !finitePositive(pitch)||!finitePositive(turns)||turns<0.25||turns>20.0||
       startCoilRadius<=profileRadius||endCoilRadius<=profileRadius||pitch<=2.0*profileRadius){
        setError(error,"Tapered helix requires positive dimensions, 0.25–20 turns, both coil radii greater than wire radius, and pitch greater than wire diameter.");
        return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        constexpr int samplesPerTurn=32;
        const int sampleCount=std::max(8,static_cast<int>(std::ceil(turns*samplesPerTurn)));
        Handle(NCollection_HArray1<gp_Pnt>) points=new NCollection_HArray1<gp_Pnt>(1,sampleCount+1);
        for(int i=0;i<=sampleCount;++i){
            const double t=static_cast<double>(i)/sampleCount;
            const double angle=2.0*std::numbers::pi*turns*t;
            const double radius=startCoilRadius+(endCoilRadius-startCoilRadius)*t;
            points->SetValue(i+1,gp_Pnt(radius*std::cos(angle),radius*std::sin(angle),pitch*turns*t));
        }
        GeomAPI_Interpolate interpolation(points,false,1e-6);
        interpolation.Perform();
        if(!interpolation.IsDone()){setError(error,"Could not interpolate the tapered helical path.");return false;}
        const Handle(Geom_BSplineCurve) centerline=interpolation.Curve();
        const TopoDS_Wire spine=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(centerline).Edge()).Wire();
        gp_Pnt start;gp_Vec tangent;
        centerline->D1(centerline->FirstParameter(),start,tangent);
        if(tangent.SquareMagnitude()<=1e-16){setError(error,"Tapered helical path has no initial direction.");return false;}
        tangent.Normalize();
        gp_Vec reference(0,0,1);reference-=tangent*tangent.Dot(reference);
        if(reference.SquareMagnitude()<=1e-8){reference=gp_Vec(0,1,0);reference-=tangent*tangent.Dot(reference);}
        const gp_Ax2 frame(start,gp_Dir(tangent),gp_Dir(reference));
        const TopoDS_Wire profile=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(frame,profileRadius)).Edge()).Wire();
        BRepOffsetAPI_MakePipeShell pipe(spine);
        pipe.Add(profile);pipe.Build();
        if(!pipe.IsDone()||!pipe.MakeSolid()){
            setError(error,"Open CASCADE could not build a solid from this tapered helical path.");return false;
        }
        const TopoDS_Shape shape=pipe.Shape();
        if(!validShape(shape,error))return false;
        const QString name=uniqueName(QStringLiteral("Tapered Helix"));
        checkpoint();parts_.append({generatedId(),name,shape});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Tapered helix kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::addSweepProfile(bool circular,bool elliptical,double width,double height,double pathLength,double lateralOffset,
                                    double verticalRise,QString* error) {
    if(!finitePositive(width)||(!circular&&!finitePositive(height))||!finitePositive(pathLength)||
       !finiteOffset(lateralOffset)||!finiteOffset(verticalRise)){
        setError(error,"Sweep dimensions must be finite and positive; path offsets must be within ±1,000,000 mm.");return false;
    }
    if(parts_.size()>=kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    try{
        const gp_Pnt start(0,0,0),middle(pathLength*0.5,lateralOffset*0.35,verticalRise*0.5),
                     end(pathLength,lateralOffset,verticalRise);
        Handle(NCollection_HArray1<gp_Pnt>) points=new NCollection_HArray1<gp_Pnt>(1,3);
        points->SetValue(1,start);points->SetValue(2,middle);points->SetValue(3,end);
        GeomAPI_Interpolate interpolation(points,false,1e-6);
        interpolation.Perform();
        if(!interpolation.IsDone()){setError(error,"Could not interpolate the sweep path.");return false;}
        const Handle(Geom_BSplineCurve) centerline=interpolation.Curve();
        TopoDS_Wire spine=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(centerline).Edge()).Wire();
        gp_Pnt tangentPoint;gp_Vec tangent;
        centerline->D1(centerline->FirstParameter(),tangentPoint,tangent);
        if(tangent.SquareMagnitude()<=1e-16){setError(error,"Sweep path has no initial direction.");return false;}
        tangent.Normalize();
        gp_Vec reference(0,0,1);reference-=tangent*tangent.Dot(reference);
        if(reference.SquareMagnitude()<=1e-8){reference=gp_Vec(0,1,0);reference-=tangent*tangent.Dot(reference);}
        const gp_Ax2 frame(start,gp_Dir(tangent),gp_Dir(reference));
        TopoDS_Wire profile;
        if(circular){
            profile=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(frame,width)).Edge()).Wire();
        }else if(elliptical){
            const double major=std::max(width,height),minor=std::min(width,height);
            profile=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Elips(frame,major,minor)).Edge()).Wire();
        }else{
            const gp_Vec x(frame.XDirection()),y(frame.YDirection());
            const auto pointAt=[&](double u,double v){gp_Pnt point=start;point.Translate(x*u+y*v);return point;};
            BRepBuilderAPI_MakePolygon section;
            section.Add(pointAt(-width/2,-height/2));section.Add(pointAt(width/2,-height/2));
            section.Add(pointAt(width/2,height/2));section.Add(pointAt(-width/2,height/2));section.Close();
            if(!section.IsDone()){setError(error,"Could not construct the sweep profile.");return false;}
            profile=section.Wire();
        }
        BRepOffsetAPI_MakePipeShell pipe(spine);
        pipe.Add(profile);
        pipe.Build();
        if(!pipe.IsDone()||!pipe.MakeSolid()){
            setError(error,"Open CASCADE could not build a solid from this profile and path.");return false;
        }
        const TopoDS_Shape shape=pipe.Shape();
        if(!validShape(shape,error))return false;
        const QString baseName=circular?QStringLiteral("Circular Sweep"):(elliptical?QStringLiteral("Elliptical Sweep"):QStringLiteral("Rectangular Sweep"));
        const QString name=uniqueName(baseName);
        checkpoint();parts_.append({generatedId(),name,shape});return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("Sweep kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::translate(const QString& id, double x, double y, double z, QString* error) {
    if (!finiteOffset(x) || !finiteOffset(y) || !finiteOffset(z)) { setError(error, "Translation offset out of range."); return false; }
    const Part* part = find(id);
    if (!part) { setError(error, "Object not found."); return false; }
    if (x == 0 && y == 0 && z == 0) return true;
    try {
        gp_Trsf transform;
        transform.SetTranslation(gp_Vec(x,y,z));
        const TopoDS_Shape result = BRepBuilderAPI_Transform(part->shape, transform, true).Shape();
        if (!validShape(result, error)) return false;
        checkpoint();
        for (auto& target : parts_) if (target.id == id) { target.shape = result; target.geometryToken = QUuid::createUuid().toString(QUuid::WithoutBraces); break; }
        return true;
    } catch (const Standard_Failure& e) { setError(error, QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error, QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::rotate(const QString& id, Axis axis, double degrees, QString* error) {
    if (!std::isfinite(degrees) || std::abs(degrees) > 36000) { setError(error, "Angle must be finite and within Â±36,000 degrees."); return false; }
    const Part* part = find(id);
    if (!part) { setError(error, "Object not found."); return false; }
    if (degrees == 0.0) return true;
    try {
        const gp_Dir direction = axis == Axis::X ? gp_Dir(1,0,0) : axis == Axis::Y ? gp_Dir(0,1,0) : gp_Dir(0,0,1);
        gp_Trsf transform;
        transform.SetRotation(gp_Ax1(gp_Pnt(0,0,0), direction), degrees * std::numbers::pi / 180.0);
        const TopoDS_Shape result = BRepBuilderAPI_Transform(part->shape, transform, true).Shape();
        if (!validShape(result, error)) return false;
        checkpoint();
        for (auto& target : parts_) if (target.id == id) { target.shape = result; target.geometryToken = QUuid::createUuid().toString(QUuid::WithoutBraces); break; }
        return true;
    } catch (const Standard_Failure& e) { setError(error, QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error, QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::scale(const QString& id, double factor, QString* error) {
    if (!std::isfinite(factor) || factor < 1e-4 || factor > 1e4) { setError(error, "Scale must be 0.0001â€“10,000."); return false; }
    const Part* part = find(id);
    if (!part) { setError(error, "Object not found."); return false; }
    if (factor == 1.0) return true;
    try {
        gp_Trsf transform;
        transform.SetScale(gp_Pnt(0,0,0), factor);
        const TopoDS_Shape result = BRepBuilderAPI_Transform(part->shape, transform, true).Shape();
        if (!validShape(result, error)) return false;
        checkpoint();
        for (auto& target : parts_) if (target.id == id) { target.shape = result; target.geometryToken = QUuid::createUuid().toString(QUuid::WithoutBraces); break; }
        return true;
    } catch (const Standard_Failure& e) { setError(error, QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error, QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::scaleNonUniform(const QString& id,double factorX,double factorY,double factorZ,QString* error) {
    constexpr double minimumFactor=1e-4,maximumFactor=1e4;
    const auto validFactor=[=](double factor){return std::isfinite(factor)&&factor>=minimumFactor&&factor<=maximumFactor;};
    if(!validFactor(factorX)||!validFactor(factorY)||!validFactor(factorZ)){
        setError(error,"Nonuniform scale factors must be finite and between 0.0001 and 10,000.");return false;
    }
    const Part* part=find(id);
    if(!part){setError(error,"Object not found.");return false;}
    if(factorX==1.0&&factorY==1.0&&factorZ==1.0)return true;
    try{
        const gp_GTrsf transform(gp_Mat(factorX,0,0,0,factorY,0,0,0,factorZ),gp_XYZ(0,0,0));
        const TopoDS_Shape result=BRepBuilderAPI_GTransform(part->shape,transform,true).Shape();
        if(!validShape(result,error))return false;
        checkpoint();
        for(auto& target:parts_)if(target.id==id){target.shape=result;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::duplicate(const QString& id, double dx, double dy, double dz, QString* error) {
    const Part* part = find(id);
    if (!part) { setError(error, "Object not found."); return false; }
    if (!finiteOffset(dx) || !finiteOffset(dy) || !finiteOffset(dz)) { setError(error, "Duplicate offset out of range."); return false; }
    if (parts_.size() >= kMaxObjects) { setError(error, "Scene object limit reached."); return false; }
    try {
        gp_Trsf trsf;
        trsf.SetTranslation(gp_Vec(dx,dy,dz));
        const TopoDS_Shape result = BRepBuilderAPI_Transform(part->shape, trsf, true).Shape();
        if (!validShape(result, error)) return false;
        const Part copied{generatedId(), uniqueName(part->name + " Copy"), result, part->visible, part->color};
        checkpoint();
        parts_.append(copied);
        return true;
    } catch (const Standard_Failure& e) { setError(error, QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error, QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::booleanOperation(BooleanOp op, const QString& firstId, const QString& secondId, QString* error) {
    if (firstId == secondId) { setError(error, "Choose two different solids."); return false; }
    const Part* a = find(firstId);
    const Part* b = find(secondId);
    if (!a || !b) { setError(error, "Both solids must exist."); return false; }
    try {
        TopoDS_Shape result;
        if (op == BooleanOp::Fuse) {
            BRepAlgoAPI_Fuse algo(a->shape,b->shape);
            algo.Build();
            if (!algo.IsDone() || algo.HasErrors()) { setError(error, "Boolean union failed."); return false; }
            result = algo.Shape();
        } else if (op == BooleanOp::Cut) {
            BRepAlgoAPI_Cut algo(a->shape,b->shape);
            algo.Build();
            if (!algo.IsDone() || algo.HasErrors()) { setError(error, "Boolean cut failed."); return false; }
            result = algo.Shape();
        } else {
            BRepAlgoAPI_Common algo(a->shape,b->shape);
            algo.Build();
            if (!algo.IsDone() || algo.HasErrors()) { setError(error, "Boolean intersection failed."); return false; }
            result = algo.Shape();
        }
        if (!validShape(result, error)) return false;
        const QString label = op == BooleanOp::Fuse ? "Union" : op == BooleanOp::Cut ? "Cut" : "Intersection";
        const QString name = uniqueName(label);
        const QString color = a->color;
        checkpoint();
        for (int i = parts_.size()-1; i >= 0; --i) {
            if (parts_[i].id == firstId || parts_[i].id == secondId) parts_.removeAt(i);
        }
        parts_.append({generatedId(), name, result, true, color});
        return true;
    } catch (const Standard_Failure& e) { setError(error, QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error, QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::measureMinimumDistance(const QString& firstId,const QString& secondId,
                                           MinimumDistanceResult* result,QString* error) const {
    if (!result) { setError(error,"A distance result is required."); return false; }
    if (firstId==secondId) { setError(error,"Choose two different solids."); return false; }
    const Part* first=find(firstId);
    const Part* second=find(secondId);
    if (!first||!second) { setError(error,"Both solids must exist."); return false; }
    if (first->shape.IsNull()||second->shape.IsNull()) { setError(error,"Both solids must contain geometry."); return false; }
    try {
        BRepExtrema_DistShapeShape measurement(first->shape,second->shape);
        measurement.Perform();
        if (!measurement.IsDone()||measurement.NbSolution()<1) {
            setError(error,"The CAD kernel could not find a closest-point solution.");
            return false;
        }
        const gp_Pnt a=measurement.PointOnShape1(1);
        const gp_Pnt b=measurement.PointOnShape2(1);
        MinimumDistanceResult candidate;
        candidate.distance=measurement.Value();
        candidate.firstX=a.X(); candidate.firstY=a.Y(); candidate.firstZ=a.Z();
        candidate.secondX=b.X(); candidate.secondY=b.Y(); candidate.secondZ=b.Z();
        *result=candidate;
        return true;
    } catch (const Standard_Failure& e) {
        setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));
    } catch (const std::exception& e) {
        setError(error,QString::fromUtf8(e.what()));
    }
    return false;
}

bool SceneDocument::measureInterference(const QString& firstId,const QString& secondId,
                                        InterferenceResult* result,QString* error) const {
    if(!result){setError(error,"An interference result is required.");return false;}
    if(firstId==secondId){setError(error,"Choose two different solids.");return false;}
    const Part* first=find(firstId);const Part* second=find(secondId);
    if(!first||!second){setError(error,"Both solids must exist.");return false;}
    if(first->shape.IsNull()||second->shape.IsNull()){setError(error,"Both solids must contain geometry.");return false;}
    try {
        BRepAlgoAPI_Common common(first->shape,second->shape);
        common.Build();
        if(!common.IsDone()||common.Shape().IsNull()){
            setError(error,"The CAD kernel could not compute the shared volume.");return false;
        }
        GProp_GProps properties;
        BRepGProp::VolumeProperties(common.Shape(),properties);
        InterferenceResult candidate;
        candidate.volume=std::max(0.0,properties.Mass());
        const gp_Pnt center=properties.CentreOfMass();
        candidate.centroidX=center.X();candidate.centroidY=center.Y();candidate.centroidZ=center.Z();
        *result=candidate;
        return true;
    } catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
      catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::diagnoseBody(const QString& id,BodyDiagnostics* result,QString* error) const {
    if(!result){setError(error,"Body diagnostics output is null.");return false;}
    const Part* part=find(id);
    if(!part||part->shape.IsNull()){setError(error,"The selected body does not exist or has no geometry.");return false;}
    try {
        BodyDiagnostics candidate;
        candidate.valid=BRepCheck_Analyzer(part->shape).IsValid();
        const auto count=[&](TopAbs_ShapeEnum type){
            NCollection_IndexedMap<TopoDS_Shape,TopTools_ShapeMapHasher> uniqueShapes;
            TopExp::MapShapes(part->shape,type,uniqueShapes);
            return uniqueShapes.Extent();
        };
        candidate.solids=count(TopAbs_SOLID);candidate.shells=count(TopAbs_SHELL);
        candidate.faces=count(TopAbs_FACE);candidate.wires=count(TopAbs_WIRE);
        candidate.edges=count(TopAbs_EDGE);candidate.vertices=count(TopAbs_VERTEX);
        GProp_GProps volumeProperties,areaProperties;
        BRepGProp::VolumeProperties(part->shape,volumeProperties);
        BRepGProp::SurfaceProperties(part->shape,areaProperties);
        candidate.volume=volumeProperties.Mass();
        candidate.surfaceArea=areaProperties.Mass();
        const gp_Pnt center=volumeProperties.CentreOfMass();
        candidate.centerX=center.X();candidate.centerY=center.Y();candidate.centerZ=center.Z();
        Bnd_Box bounds;
        BRepBndLib::AddOptimal(part->shape,bounds,false,false);
        if(bounds.IsVoid()){setError(error,"The selected body has no measurable bounds.");return false;}
        bounds.Get(candidate.minX,candidate.minY,candidate.minZ,
                   candidate.maxX,candidate.maxY,candidate.maxZ);
        candidate.valid=candidate.valid&&candidate.solids>0&&candidate.volume>1e-9&&
            std::isfinite(candidate.volume)&&std::isfinite(candidate.surfaceArea);
        *result=candidate;
        return true;
    } catch(const Standard_Failure& failure){
        setError(error,QStringLiteral("Geometry diagnostics failed: %1").arg(failure.what()));
    } catch(const std::exception& failure){
        setError(error,QString("Geometry diagnostics failed: %1").arg(failure.what()));
    }
    return false;
}

bool SceneDocument::measureMassFromDensity(const QString& id,double densityKgPerM3,
                                            DensityMassProperties* result,QString* error) const {
    if(!result){setError(error,"Mass-properties output is null.");return false;}
    if(!finitePositive(densityKgPerM3)){setError(error,"Density must be finite, positive, and no greater than 1,000,000 kg/m^3.");return false;}
    const Part* part=find(id);
    if(!part||part->shape.IsNull()){setError(error,"The selected body does not exist or has no geometry.");return false;}
    try{
        if(!BRepCheck_Analyzer(part->shape).IsValid()){
            setError(error,"Mass properties require valid solid geometry.");return false;
        }
        GProp_GProps volumeProperties;
        BRepGProp::VolumeProperties(part->shape,volumeProperties);
        const double volume=volumeProperties.Mass();
        const double mass=volume*densityKgPerM3/1e9; // mm^3 to m^3
        const gp_Pnt center=volumeProperties.CentreOfMass();
        if(!std::isfinite(volume)||volume<=1e-9||!std::isfinite(mass)){
            setError(error,"The selected body has no measurable positive volume.");return false;
        }
        DensityMassProperties candidate;
        candidate.volume=volume;candidate.densityKgPerM3=densityKgPerM3;candidate.massKg=mass;
        candidate.centerX=center.X();candidate.centerY=center.Y();candidate.centerZ=center.Z();
        *result=candidate;
        return true;
    }catch(const Standard_Failure& failure){
        setError(error,QStringLiteral("Mass-property calculation failed: %1").arg(failure.what()));
    }catch(const std::exception& failure){
        setError(error,QString("Mass-property calculation failed: %1").arg(failure.what()));
    }
    return false;
}

bool SceneDocument::measurePlanarSection(const QString& id,int normalAxis,double offset,
                                         SectionMeasurement* result,QString* error) const {
    if(!result){setError(error,"Section-measurement output is null.");return false;}
    if(normalAxis<0||normalAxis>2||!std::isfinite(offset)||std::abs(offset)>1e6){
        setError(error,"Section axis must be X, Y, or Z and offset must be finite and within ±1,000,000 mm.");return false;
    }
    const Part* part=find(id);
    if(!part||part->shape.IsNull()){setError(error,"The selected body does not exist or has no geometry.");return false;}
    try{
        const gp_Pnt origin(normalAxis==0?offset:0.0,normalAxis==1?offset:0.0,normalAxis==2?offset:0.0);
        const gp_Dir normal(normalAxis==0?1.0:0.0,normalAxis==1?1.0:0.0,normalAxis==2?1.0:0.0);
        BRepAlgoAPI_Section section(part->shape,gp_Pln(origin,normal),false);
        section.Build();
        if(!section.IsDone()||section.HasErrors()){setError(error,"The kernel could not calculate the planar section.");return false;}
        NCollection_IndexedMap<TopoDS_Shape,TopTools_ShapeMapHasher> edges;
        TopExp::MapShapes(section.Shape(),TopAbs_EDGE,edges);
        SectionMeasurement candidate;
        candidate.edgeCount=edges.Extent();
        for(int i=1;i<=edges.Extent();++i){
            GProp_GProps properties;
            BRepGProp::LinearProperties(edges(i),properties);
            candidate.totalLength+=properties.Mass();
        }
        if(!std::isfinite(candidate.totalLength)){setError(error,"Section contour length is not finite.");return false;}
        *result=candidate;
        return true;
    }catch(const Standard_Failure& failure){
        setError(error,QStringLiteral("Planar section calculation failed: %1").arg(failure.what()));
    }catch(const std::exception& failure){
        setError(error,QString("Planar section calculation failed: %1").arg(failure.what()));
    }
    return false;
}

bool SceneDocument::summarizeSurfaceTypes(const QString& id,SurfaceTypeSummary* result,
                                          QString* error) const {
    if(!result){setError(error,"Surface-summary output is null.");return false;}
    const Part* part=find(id);
    if(!part||part->shape.IsNull()){setError(error,"The selected body does not exist or has no geometry.");return false;}
    try {
        SurfaceTypeSummary candidate;
        NCollection_IndexedMap<TopoDS_Shape,TopTools_ShapeMapHasher> uniqueFaces;
        TopExp::MapShapes(part->shape,TopAbs_FACE,uniqueFaces);
        for(int faceIndex=1;faceIndex<=uniqueFaces.Extent();++faceIndex){
            const TopoDS_Face face=TopoDS::Face(uniqueFaces(faceIndex));
            const GeomAbs_SurfaceType type=BRepAdaptor_Surface(face,true).GetType();
            GProp_GProps properties;
            BRepGProp::SurfaceProperties(face,properties);
            const double area=properties.Mass();
            if(!std::isfinite(area)||area<0.0){setError(error,"A face has a non-finite surface area.");return false;}
            int* count=&candidate.otherFaces;
            double* totalArea=&candidate.otherArea;
            switch(type){
            case GeomAbs_Plane: count=&candidate.planarFaces;totalArea=&candidate.planarArea;break;
            case GeomAbs_Cylinder: count=&candidate.cylindricalFaces;totalArea=&candidate.cylindricalArea;break;
            case GeomAbs_Cone: count=&candidate.conicalFaces;totalArea=&candidate.conicalArea;break;
            case GeomAbs_Sphere: count=&candidate.sphericalFaces;totalArea=&candidate.sphericalArea;break;
            case GeomAbs_Torus: count=&candidate.toroidalFaces;totalArea=&candidate.toroidalArea;break;
            default: break;
            }
            ++*count;
            *totalArea+=area;
        }
        *result=candidate;
        return true;
    } catch(const Standard_Failure& failure){
        setError(error,QStringLiteral("Surface classification failed: %1").arg(failure.what()));
    } catch(const std::exception& failure){
        setError(error,QString("Surface classification failed: %1").arg(failure.what()));
    }
    return false;
}

bool SceneDocument::measureFaceArea(const FaceReference& reference,double* area,QString* error) const {
    if(!area){setError(error,"Face-area output is null.");return false;}
    const Part* part=find(reference.partId);
    if(!part||part->shape.IsNull()){setError(error,"The selected face's body does not exist or has no geometry.");return false;}
    if(reference.geometryToken.isEmpty()||reference.geometryToken!=part->geometryToken){
        setError(error,"The selected face reference is stale because the body geometry changed.");return false;
    }
    if(reference.faceIndex<1){setError(error,"Face index must be a positive one-based index.");return false;}
    try {
        int index=0;
        for(TopExp_Explorer explorer(part->shape,TopAbs_FACE);explorer.More();explorer.Next()){
            if(++index!=reference.faceIndex)continue;
            GProp_GProps properties;
            BRepGProp::SurfaceProperties(TopoDS::Face(explorer.Current()),properties);
            const double candidate=properties.Mass();
            if(!std::isfinite(candidate)||candidate<=0.0){setError(error,"The selected face has no measurable positive area.");return false;}
            *area=candidate;
            return true;
        }
        setError(error,"The selected face index is outside the body's topology.");
    } catch(const Standard_Failure& failure){
        setError(error,QStringLiteral("Face-area measurement failed: %1").arg(failure.what()));
    } catch(const std::exception& failure){
        setError(error,QString("Face-area measurement failed: %1").arg(failure.what()));
    }
    return false;
}

bool SceneDocument::measureEdgeLength(const EdgeReference& reference,double* length,QString* error) const {
    if(!length){setError(error,"Edge-length output is null.");return false;}
    const Part* part=find(reference.partId);
    if(!part||part->shape.IsNull()){setError(error,"The selected edge's body does not exist or has no geometry.");return false;}
    if(reference.geometryToken.isEmpty()||reference.geometryToken!=part->geometryToken){
        setError(error,"The selected edge reference is stale because the body geometry changed.");return false;
    }
    if(reference.edgeIndex<1){setError(error,"Edge index must be a positive one-based index.");return false;}
    try {
        int index=0;
        for(TopExp_Explorer explorer(part->shape,TopAbs_EDGE);explorer.More();explorer.Next()){
            if(++index!=reference.edgeIndex)continue;
            GProp_GProps properties;
            BRepGProp::LinearProperties(TopoDS::Edge(explorer.Current()),properties);
            const double candidate=properties.Mass();
            if(!std::isfinite(candidate)||candidate<=0.0){setError(error,"The selected edge has no measurable positive length.");return false;}
            *length=candidate;
            return true;
        }
        setError(error,"The selected edge index is outside the body's topology.");
    } catch(const Standard_Failure& failure){
        setError(error,QStringLiteral("Edge-length measurement failed: %1").arg(failure.what()));
    } catch(const std::exception& failure){
        setError(error,QString("Edge-length measurement failed: %1").arg(failure.what()));
    }
    return false;
}

bool SceneDocument::measureVertexCoordinates(const VertexReference& reference,double* x,double* y,double* z,QString* error) const {
    if(!x||!y||!z){setError(error,"Vertex-coordinate output is null.");return false;}
    const Part* part=find(reference.partId);
    if(!part||part->shape.IsNull()){setError(error,"The selected vertex's body does not exist or has no geometry.");return false;}
    if(reference.geometryToken.isEmpty()||reference.geometryToken!=part->geometryToken){
        setError(error,"The selected vertex reference is stale because the body geometry changed.");return false;
    }
    if(reference.vertexIndex<1){setError(error,"Vertex index must be a positive one-based index.");return false;}
    try {
        int index=0;
        for(TopExp_Explorer explorer(part->shape,TopAbs_VERTEX);explorer.More();explorer.Next()){
            if(++index!=reference.vertexIndex)continue;
            const gp_Pnt point=BRep_Tool::Pnt(TopoDS::Vertex(explorer.Current()));
            if(!std::isfinite(point.X())||!std::isfinite(point.Y())||!std::isfinite(point.Z())){
                setError(error,"The selected vertex has non-finite coordinates.");return false;
            }
            *x=point.X();*y=point.Y();*z=point.Z();return true;
        }
        setError(error,"The selected vertex index is outside the body's topology.");
    } catch(const Standard_Failure& failure){
        setError(error,QStringLiteral("Vertex-coordinate measurement failed: %1").arg(failure.what()));
    } catch(const std::exception& failure){
        setError(error,QString("Vertex-coordinate measurement failed: %1").arg(failure.what()));
    }
    return false;
}

bool SceneDocument::measureVertexDistance(const VertexReference& first,const VertexReference& second,double* distance,QString* error) const {
    if(!distance){setError(error,"Vertex-distance output is null.");return false;}
    double ax=0,ay=0,az=0,bx=0,by=0,bz=0;
    if(!measureVertexCoordinates(first,&ax,&ay,&az,error)||!measureVertexCoordinates(second,&bx,&by,&bz,error))return false;
    const double result=std::hypot(bx-ax,by-ay,bz-az);
    if(!std::isfinite(result)){setError(error,"The selected vertices have an invalid separation.");return false;}
    *distance=result;
    return true;
}

bool SceneDocument::filletEdge(const EdgeReference& reference, double radius, QString* error) {
    FilletCommand command;
    if (!prepareFillet(reference, radius, &command, error)) return false;
    const FilletPreview preview = buildFilletPreview(command);
    if (preview.shape.IsNull()) { setError(error, preview.error); return false; }
    return commitFillet(command, preview.shape, error);
}

bool SceneDocument::filletAllEdges(const QString& id,double radius,QString* error) {
    if(!finitePositive(radius)){setError(error,"Fillet radius must be finite and positive.");return false;}
    const Part* part=find(id);if(!part){setError(error,"Object not found.");return false;}
    try{
        BRepFilletAPI_MakeFillet operation(part->shape);
        int edgeCount=0;
        for(TopExp_Explorer explorer(part->shape,TopAbs_EDGE);explorer.More();explorer.Next()){
            operation.Add(radius,TopoDS::Edge(explorer.Current()));++edgeCount;
        }
        if(edgeCount==0){setError(error,"The body has no edges to fillet.");return false;}
        operation.Build();
        if(!operation.IsDone()){setError(error,"The selected radius cannot be applied to every body edge.");return false;}
        const TopoDS_Shape result=operation.Shape();if(!validShape(result,error))return false;
        checkpoint();for(Part& target:parts_)if(target.id==id){target.shape=result;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::prepareFillet(const EdgeReference& reference, double radius, FilletCommand* command, QString* error) const {
    return prepareFillet(QVector<EdgeReference>{reference},radius,command,error);
}

bool SceneDocument::prepareFillet(const QVector<EdgeReference>& references,double radius,FilletCommand* command,QString* error) const {
    if (!command) { setError(error, "Fillet command output is unavailable."); return false; }
    if (!std::isfinite(radius) || radius < 0.00001 || radius > 1e6) {
        setError(error, "Fillet radius must be finite and between 0.00001 and 1,000,000 mm.");
        return false;
    }
    if (references.isEmpty()) { setError(error,"Select at least one edge to fillet."); return false; }
    const EdgeReference& reference=references.first();
    const Part* part = find(reference.partId);
    if (!part) { setError(error, "The selected body no longer exists."); return false; }
    QVector<TopoDS_Edge> resolved;
    QSet<int> uniqueIndices;
    for (const EdgeReference& selected:references) {
        if (selected.partId!=reference.partId) { setError(error,"Selected fillet edges must belong to one body."); return false; }
        if (selected.geometryToken.isEmpty()||part->geometryToken!=selected.geometryToken) {
            setError(error,"A selected edge is stale because its body geometry has changed. Select the edges again."); return false;
        }
        if (selected.edgeIndex<1||uniqueIndices.contains(selected.edgeIndex)) {
            setError(error,"A selected edge reference is invalid or duplicated."); return false;
        }
        uniqueIndices.insert(selected.edgeIndex);
        TopoDS_Edge edge;int index=0;
        for (TopExp_Explorer explorer(part->shape,TopAbs_EDGE);explorer.More();explorer.Next())
            if (++index==selected.edgeIndex) { edge=TopoDS::Edge(explorer.Current());break; }
        if (edge.IsNull()) { setError(error,"A selected edge could not be resolved. Select the edges again."); return false; }
        resolved.append(edge);
    }
    command->edge = reference;
    command->edges=references;
    command->sourceShape = part->shape;
    command->sourceEdge = resolved.first();
    command->sourceEdges=resolved;
    command->radius = radius;
    return true;
}

FilletPreview SceneDocument::buildFilletPreview(const FilletCommand& command) {
    FilletPreview preview;
    try {
        if (command.sourceShape.IsNull() || (command.sourceEdges.isEmpty()&&command.sourceEdge.IsNull())) {
            preview.error = "The fillet preview command is incomplete.";
            return preview;
        }
        BRepFilletAPI_MakeFillet operation(command.sourceShape);
        if(command.sourceEdges.isEmpty()) operation.Add(command.radius,command.sourceEdge);
        else for(const TopoDS_Edge& edge:command.sourceEdges) operation.Add(command.radius,edge);
        operation.Build();
        if (!operation.IsDone()) { preview.error = "The kernel could not build a fillet for this edge and radius."; return preview; }
        preview.shape = operation.Shape();
        if (!validShape(preview.shape, &preview.error)) preview.shape = TopoDS_Shape();
    } catch (const Standard_Failure& e) { preview.error = QStringLiteral("CAD kernel error: %1").arg(e.what()); }
      catch (const std::exception& e) { preview.error = QString::fromUtf8(e.what()); }
    return preview;
}

bool SceneDocument::commitFillet(const FilletCommand& command, const TopoDS_Shape& preview, QString* error) {
    const Part* part = find(command.edge.partId);
    if (!part) { setError(error, "The selected body no longer exists."); return false; }
    if (part->geometryToken != command.edge.geometryToken) {
        setError(error, "The fillet preview is stale because the body geometry changed. Select the edge again.");
        return false;
    }
    if (!validShape(preview, error)) return false;
    checkpoint();
    for (auto& target : parts_) if (target.id == command.edge.partId) {
        target.shape = preview;
        target.geometryToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
        break;
    }
    return true;
}

bool SceneDocument::chamferEdge(const EdgeReference& reference, double distance, QString* error) {
    ChamferCommand command;
    if (!prepareChamfer(reference, distance, &command, error)) return false;
    const ChamferPreview preview = buildChamferPreview(command);
    if (preview.shape.IsNull()) { setError(error, preview.error); return false; }
    return commitChamfer(command, preview.shape, error);
}

bool SceneDocument::chamferAllEdges(const QString& id,double distance,QString* error) {
    if(!finitePositive(distance)){setError(error,"Chamfer distance must be finite and positive.");return false;}
    const Part* part=find(id);if(!part){setError(error,"Object not found.");return false;}
    try{
        BRepFilletAPI_MakeChamfer operation(part->shape);
        int edgeCount=0;
        for(TopExp_Explorer explorer(part->shape,TopAbs_EDGE);explorer.More();explorer.Next()){
            operation.Add(distance,TopoDS::Edge(explorer.Current()));++edgeCount;
        }
        if(edgeCount==0){setError(error,"The body has no edges to chamfer.");return false;}
        operation.Build();
        if(!operation.IsDone()){setError(error,"The selected distance cannot be applied to every body edge.");return false;}
        const TopoDS_Shape result=operation.Shape();if(!validShape(result,error))return false;
        checkpoint();for(Part& target:parts_)if(target.id==id){target.shape=result;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::draftFace(const QString& id,int faceIndex,Axis pullAxis,double neutralPlaneOffset,
                              double angleDegrees,QString* error) {
    if(faceIndex<1||!finiteOffset(neutralPlaneOffset)||!std::isfinite(angleDegrees)||
       angleDegrees==0.0||std::abs(angleDegrees)>45.0||
       (pullAxis!=Axis::X&&pullAxis!=Axis::Y&&pullAxis!=Axis::Z)){
        setError(error,"Draft face index, pull axis, neutral plane, or angle is invalid.");return false;
    }
    const Part* part=find(id);if(!part){setError(error,"Object not found.");return false;}
    TopoDS_Face face;int index=0;
    for(TopExp_Explorer explorer(part->shape,TopAbs_FACE);explorer.More();explorer.Next())
        if(++index==faceIndex){face=TopoDS::Face(explorer.Current());break;}
    if(face.IsNull()){setError(error,"The selected face index is outside this body's face list.");return false;}
    try{
        gp_Dir direction(0,0,1);
        if(pullAxis==Axis::X)direction=gp_Dir(1,0,0);else if(pullAxis==Axis::Y)direction=gp_Dir(0,1,0);
        gp_Pnt planePoint(0,0,0);
        if(pullAxis==Axis::X)planePoint.SetX(neutralPlaneOffset);
        else if(pullAxis==Axis::Y)planePoint.SetY(neutralPlaneOffset);
        else planePoint.SetZ(neutralPlaneOffset);
        const gp_Pln neutralPlane(planePoint,direction);
        BRepOffsetAPI_DraftAngle operation(part->shape);
        operation.Add(face,direction,angleDegrees*std::numbers::pi/180.0,neutralPlane,true);
        if(!operation.AddDone()){setError(error,"The requested face cannot be drafted against this neutral plane.");return false;}
        operation.Build();
        if(!operation.IsDone()){setError(error,"The CAD kernel could not complete the face draft.");return false;}
        const TopoDS_Shape result=operation.Shape();if(!validShape(result,error))return false;
        checkpoint();for(Part& target:parts_)if(target.id==id){target.shape=result;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::prepareChamfer(const EdgeReference& reference, double distance, ChamferCommand* command, QString* error) const {
    return prepareChamfer(QVector<EdgeReference>{reference},distance,command,error);
}

bool SceneDocument::prepareChamfer(const QVector<EdgeReference>& references,double distance,ChamferCommand* command,QString* error) const {
    if (!command) { setError(error, "Chamfer command output is unavailable."); return false; }
    if (!std::isfinite(distance) || distance < 0.00001 || distance > 1e6) {
        setError(error, "Chamfer distance must be finite and between 0.00001 and 1,000,000 mm.");
        return false;
    }
    if(references.isEmpty()){setError(error,"Select at least one edge to chamfer.");return false;}
    const EdgeReference& reference=references.first();
    const Part* part = find(reference.partId);
    if (!part) { setError(error, "The selected body no longer exists."); return false; }
    QVector<TopoDS_Edge> resolved;QSet<int> uniqueIndices;
    for(const EdgeReference& selected:references){
        if(selected.partId!=reference.partId){setError(error,"Selected chamfer edges must belong to one body.");return false;}
        if(selected.geometryToken.isEmpty()||part->geometryToken!=selected.geometryToken){
            setError(error,"A selected edge is stale because its body geometry has changed. Select the edges again.");return false;
        }
        if(selected.edgeIndex<1||uniqueIndices.contains(selected.edgeIndex)){
            setError(error,"A selected edge reference is invalid or duplicated.");return false;
        }
        uniqueIndices.insert(selected.edgeIndex);TopoDS_Edge edge;int index=0;
        for(TopExp_Explorer explorer(part->shape,TopAbs_EDGE);explorer.More();explorer.Next())
            if(++index==selected.edgeIndex){edge=TopoDS::Edge(explorer.Current());break;}
        if(edge.IsNull()){setError(error,"A selected edge could not be resolved. Select the edges again.");return false;}
        resolved.append(edge);
    }
    command->edge = reference;
    command->edges=references;
    command->sourceShape = part->shape;
    command->sourceEdge = resolved.first();
    command->sourceEdges=resolved;
    command->distance = distance;
    return true;
}

bool SceneDocument::prepareAsymmetricChamfer(const EdgeReference& reference,double firstDistance,
                                              double secondDistance,ChamferCommand* command,QString* error) const {
    if (!command) { setError(error,"Chamfer command output is unavailable."); return false; }
    if (!std::isfinite(firstDistance)||!std::isfinite(secondDistance)||firstDistance<0.00001||
        secondDistance<0.00001||firstDistance>1e6||secondDistance>1e6) {
        setError(error,"Both chamfer distances must be finite and between 0.00001 and 1,000,000 mm."); return false;
    }
    const Part* part=find(reference.partId);
    if (!part) { setError(error,"The selected body no longer exists."); return false; }
    if (reference.geometryToken.isEmpty()||part->geometryToken!=reference.geometryToken) {
        setError(error,"The selected edge is stale because its body geometry has changed. Select the edge again."); return false;
    }
    if (reference.edgeIndex<1) { setError(error,"The selected edge reference is invalid."); return false; }
    TopoDS_Edge edge;int index=0;
    for (TopExp_Explorer explorer(part->shape,TopAbs_EDGE);explorer.More();explorer.Next())
        if (++index==reference.edgeIndex) { edge=TopoDS::Edge(explorer.Current());break; }
    if (edge.IsNull()) { setError(error,"The selected edge could not be resolved. Select it again."); return false; }
    TopoDS_Face referenceFace;
    for (TopExp_Explorer faces(part->shape,TopAbs_FACE);faces.More()&&referenceFace.IsNull();faces.Next()) {
        const TopoDS_Face face=TopoDS::Face(faces.Current());
        for (TopExp_Explorer faceEdges(face,TopAbs_EDGE);faceEdges.More();faceEdges.Next())
            if (edge.IsSame(faceEdges.Current())) { referenceFace=face;break; }
    }
    if (referenceFace.IsNull()) { setError(error,"The selected edge does not belong to a planarizable body face."); return false; }
    command->edge=reference;command->sourceShape=part->shape;command->sourceEdge=edge;
    command->referenceFace=referenceFace;command->distance=firstDistance;command->secondDistance=secondDistance;
    return true;
}

ChamferPreview SceneDocument::buildChamferPreview(const ChamferCommand& command) {
    ChamferPreview preview;
    try {
        if (command.sourceShape.IsNull() || (command.sourceEdges.isEmpty()&&command.sourceEdge.IsNull())) {
            preview.error = "The chamfer preview command is incomplete.";
            return preview;
        }
        BRepFilletAPI_MakeChamfer operation(command.sourceShape);
        if (command.secondDistance>0.0) {
            if (command.referenceFace.IsNull()) { preview.error="The asymmetric chamfer reference face is missing.";return preview; }
            operation.Add(command.distance,command.secondDistance,command.sourceEdge,command.referenceFace);
        } else if(command.sourceEdges.isEmpty()) operation.Add(command.distance,command.sourceEdge);
        else for(const TopoDS_Edge& edge:command.sourceEdges) operation.Add(command.distance,edge);
        operation.Build();
        if (!operation.IsDone()) { preview.error = "The kernel could not build a chamfer for this edge and distance."; return preview; }
        preview.shape = operation.Shape();
        if (!validShape(preview.shape, &preview.error)) preview.shape = TopoDS_Shape();
    } catch (const Standard_Failure& e) { preview.error = QStringLiteral("CAD kernel error: %1").arg(e.what()); }
      catch (const std::exception& e) { preview.error = QString::fromUtf8(e.what()); }
    return preview;
}

bool SceneDocument::commitChamfer(const ChamferCommand& command, const TopoDS_Shape& preview, QString* error) {
    const Part* part = find(command.edge.partId);
    if (!part) { setError(error, "The selected body no longer exists."); return false; }
    if (part->geometryToken != command.edge.geometryToken) {
        setError(error, "The chamfer preview is stale because the body geometry changed. Select the edge again.");
        return false;
    }
    if (!validShape(preview, error)) return false;
    checkpoint();
    for (auto& target : parts_) if (target.id == command.edge.partId) {
        target.shape = preview;
        target.geometryToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
        break;
    }
    return true;
}

bool SceneDocument::prepareExtrusion(SketchProfile profile, double originX, double originY,
                                     double widthOrDiameter, double depth, double height,
                                     ExtrusionCommand* command, QString* error) const {
    return prepareExtrusion(profile, originX, originY, widthOrDiameter, depth, height,
                            ExtrusionMode::NewBody, QString(), command, error);
}

bool SceneDocument::prepareExtrusion(SketchProfile profile, double originX, double originY,
                                     double widthOrDiameter, double depth, double height,
                                     ExtrusionMode mode, const QString& targetPartId,
                                     ExtrusionCommand* command, QString* error) const {
    if (!command) { setError(error, "Extrusion command output is unavailable."); return false; }
    if (mode == ExtrusionMode::NewBody && parts_.size() >= kMaxObjects) {
        setError(error, "Scene object limit reached."); return false;
    }
    if (!finiteOffset(originX) || !finiteOffset(originY)) {
        setError(error, "Profile origin must be finite and within +/-1,000,000 mm.");
        return false;
    }
    if (!finitePositive(widthOrDiameter) || !finitePositive(height)) {
        setError(error, "Profile size and extrusion height must be between 0.00001 and 1,000,000 mm.");
        return false;
    }
    if ((profile == SketchProfile::Rectangle || profile == SketchProfile::Ellipse || profile == SketchProfile::Slot ||
         profile == SketchProfile::RoundedRectangle || profile == SketchProfile::Ring) &&
        !finitePositive(depth)) {
        setError(error, "The sketch profile depth must be between 0.00001 and 1,000,000 mm."); return false;
    }
    if ((profile == SketchProfile::Ellipse || profile == SketchProfile::Slot) && depth > widthOrDiameter) {
        setError(error, "Ellipse minor diameter or slot width must not exceed its major dimension."); return false;
    }
    if(profile==SketchProfile::Ring&&depth>=widthOrDiameter){setError(error,"Ring inner diameter must be smaller than its outer diameter.");return false;}
    if (profile == SketchProfile::RegularPolygon &&
        (!std::isfinite(depth) || depth < 3 || depth > 64 || std::floor(depth) != depth)) {
        setError(error, "Regular polygon sides must be an integer from 3 through 64."); return false;
    }
    const Part* target = nullptr;
    if (mode == ExtrusionMode::Join || mode == ExtrusionMode::Cut) {
        target = find(targetPartId);
        if (!target) { setError(error, "Select an existing target body for Join or Cut."); return false; }
    } else if (mode != ExtrusionMode::NewBody) {
        setError(error, "The extrusion operation is not supported."); return false;
    }
    command->profile = profile;
    command->sketchPlane = SketchPlane::XY;
    command->mode = mode;
    command->targetPartId = target ? target->id : QString();
    command->targetGeometryToken = target ? target->geometryToken : QString();
    // Boolean preview builders may update topology flags on their input handles.
    // Keep the document's body detached so preview remains strictly non-mutating.
    command->targetShape = target ? BRepBuilderAPI_Copy(target->shape).Shape() : TopoDS_Shape();
    command->originX = originX;
    command->originY = originY;
    command->widthOrDiameter = widthOrDiameter;
    command->depth = depth;
    command->height = height;
    command->symmetric = false;
    command->throughAll = false;
    command->taperDegrees = 0.0;
    command->cornerRadius = 0.0;
    command->sourceRevision = revision_;
    return true;
}

bool SceneDocument::prepareSymmetricExtrusion(SketchProfile profile,double originX,double originY,double widthOrDiameter,
                                               double depth,double height,ExtrusionMode mode,const QString& targetPartId,
                                               ExtrusionCommand* command,QString* error) const {
    if(!prepareExtrusion(profile,originX,originY,widthOrDiameter,depth,height,mode,targetPartId,command,error))return false;
    command->symmetric=true;
    return true;
}

bool SceneDocument::prepareTaperedExtrusion(SketchProfile profile,double originX,double originY,double widthOrDiameter,
                                             double depth,double height,double taperDegrees,ExtrusionMode mode,
                                             const QString& targetPartId,ExtrusionCommand* command,QString* error) const {
    if(profile!=SketchProfile::Rectangle&&profile!=SketchProfile::Circle){
        setError(error,"Tapered extrusion currently supports rectangle and circle profiles.");return false;
    }
    if(!std::isfinite(taperDegrees)||std::abs(taperDegrees)>45.0||taperDegrees==0.0){
        setError(error,"Taper angle must be non-zero and between -45 and 45 degrees.");return false;
    }
    if(!prepareExtrusion(profile,originX,originY,widthOrDiameter,depth,height,mode,targetPartId,command,error))return false;
    const double inset=height*std::tan(taperDegrees*std::numbers::pi/180.0);
    if((profile==SketchProfile::Circle&&widthOrDiameter/2.0-inset<=1e-5)||
       (profile==SketchProfile::Rectangle&&(widthOrDiameter-2.0*inset<=1e-5||depth-2.0*inset<=1e-5))){
        setError(error,"The taper angle collapses the top profile. Reduce the angle or height.");return false;
    }
    command->taperDegrees=taperDegrees;
    return true;
}

bool SceneDocument::prepareRoundedRectangleExtrusion(double originX,double originY,double width,double depth,double cornerRadius,
                                                       double height,ExtrusionMode mode,const QString& targetPartId,
                                                       ExtrusionCommand* command,QString* error) const {
    if(!finitePositive(cornerRadius)||cornerRadius>=std::min(width,depth)/2.0){
        setError(error,"Corner radius must be positive and less than half the smaller rectangle dimension.");return false;
    }
    if(!prepareExtrusion(SketchProfile::RoundedRectangle,originX,originY,width,depth,height,mode,targetPartId,command,error))return false;
    command->cornerRadius=cornerRadius;
    return true;
}

ExtrusionPreview SceneDocument::buildExtrusionPreview(const ExtrusionCommand& command) {
    ExtrusionPreview preview;
    try {
        if (!finiteOffset(command.originX) || !finiteOffset(command.originY) || !finiteOffset(command.planeOffset) ||
            !finitePositive(command.widthOrDiameter) || !finitePositive(command.height) ||
            ((command.profile == SketchProfile::Rectangle || command.profile == SketchProfile::Ellipse ||
              command.profile == SketchProfile::Slot || command.profile == SketchProfile::RoundedRectangle ||
              command.profile == SketchProfile::Ring) && !finitePositive(command.depth)) ||
            ((command.profile == SketchProfile::Ellipse || command.profile == SketchProfile::Slot) &&
             command.depth > command.widthOrDiameter) ||
            (command.profile==SketchProfile::Ring&&command.depth>=command.widthOrDiameter) ||
            (command.profile==SketchProfile::RoundedRectangle&&
             (!finitePositive(command.cornerRadius)||command.cornerRadius>=std::min(command.widthOrDiameter,command.depth)/2.0)) ||
            (command.profile == SketchProfile::RegularPolygon &&
             (!std::isfinite(command.depth) || command.depth < 3 || command.depth > 64 ||
              std::floor(command.depth) != command.depth)) ||
            !std::isfinite(command.taperDegrees)||std::abs(command.taperDegrees)>45.0||
            (command.symmetric&&command.taperDegrees!=0.0)||
            (command.sketchPlane!=SketchPlane::XY&&command.sketchPlane!=SketchPlane::XZ&&command.sketchPlane!=SketchPlane::YZ)) {
            preview.error = "The sketch extrusion parameters are invalid.";
            return preview;
        }

        double extrusionHeight=command.height;
        if(command.throughAll){
            if(command.mode!=ExtrusionMode::Cut||command.symmetric||command.taperDegrees!=0.0||command.targetShape.IsNull()){
                preview.error="Through-all extrusion requires an untapered, one-sided Cut with a valid target body.";
                return preview;
            }
            Bnd_Box targetBounds;
            BRepBndLib::AddOptimal(command.targetShape,targetBounds,false,false);
            if(targetBounds.IsVoid()) { preview.error="The through-all target body has no measurable bounds.";return preview; }
            double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;
            targetBounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
            double low=0,high=0,start=0,worldDirection=0;
            if(command.sketchPlane==SketchPlane::XY){low=zmin;high=zmax;start=command.planeOffset;worldDirection=1;}
            else if(command.sketchPlane==SketchPlane::XZ){low=ymin;high=ymax;start=-command.planeOffset;worldDirection=-1;}
            else {low=xmin;high=xmax;start=command.planeOffset;worldDirection=1;}
            worldDirection*=command.reverseDirection?-1.0:1.0;
            const double distance=worldDirection>0?high-start:start-low;
            const double clearance=std::max(1e-3,(high-low)*1e-6);
            if(!std::isfinite(distance)||distance<=0||distance+clearance>1e6){
                preview.error="The through-all direction does not cross the target body or exceeds the modeling limit.";
                return preview;
            }
            extrusionHeight=distance+clearance;
        }

        TopoDS_Face profileFace;
        if (command.profile == SketchProfile::Rectangle) {
            BRepBuilderAPI_MakePolygon boundary;
            boundary.Add(gp_Pnt(command.originX, command.originY, 0.0));
            boundary.Add(gp_Pnt(command.originX + command.widthOrDiameter, command.originY, 0.0));
            boundary.Add(gp_Pnt(command.originX + command.widthOrDiameter,
                                command.originY + command.depth, 0.0));
            boundary.Add(gp_Pnt(command.originX, command.originY + command.depth, 0.0));
            boundary.Close();
            if (!boundary.IsDone()) { preview.error = "The rectangle sketch boundary could not be closed."; return preview; }
            BRepBuilderAPI_MakeFace faceMaker(boundary.Wire());
            if (!faceMaker.IsDone()) { preview.error = "The rectangle sketch could not form a planar face."; return preview; }
            profileFace = faceMaker.Face();
        } else if (command.profile == SketchProfile::Circle) {
            const gp_Circ circle(gp_Ax2(gp_Pnt(command.originX, command.originY, 0.0), gp_Dir(0,0,1)),
                                 command.widthOrDiameter / 2.0);
            BRepBuilderAPI_MakeEdge edgeMaker(circle);
            if (!edgeMaker.IsDone()) { preview.error = "The circle sketch edge could not be built."; return preview; }
            BRepBuilderAPI_MakeWire wireMaker(edgeMaker.Edge());
            if (!wireMaker.IsDone()) { preview.error = "The circle sketch boundary could not be closed."; return preview; }
            BRepBuilderAPI_MakeFace faceMaker(wireMaker.Wire());
            if (!faceMaker.IsDone()) { preview.error = "The circle sketch could not form a planar face."; return preview; }
            profileFace = faceMaker.Face();
        } else if (command.profile == SketchProfile::Ellipse) {
            const gp_Elips ellipse(gp_Ax2(gp_Pnt(command.originX,command.originY,0.0),gp_Dir(0,0,1)),
                                   command.widthOrDiameter/2.0,command.depth/2.0);
            BRepBuilderAPI_MakeEdge edgeMaker(ellipse);
            BRepBuilderAPI_MakeWire wireMaker(edgeMaker.Edge());
            BRepBuilderAPI_MakeFace faceMaker(wireMaker.Wire());
            if (!faceMaker.IsDone()) { preview.error = "The ellipse sketch could not form a planar face."; return preview; }
            profileFace = faceMaker.Face();
        } else if (command.profile == SketchProfile::Slot) {
            const double radius=command.depth/2.0;
            const double left=command.originX+radius, right=command.originX+command.widthOrDiameter-radius;
            const double bottom=command.originY, top=command.originY+command.depth;
            const gp_Circ rightCap(gp_Ax2(gp_Pnt(right,bottom+radius,0),gp_Dir(0,0,1)),radius);
            const gp_Circ leftCap(gp_Ax2(gp_Pnt(left,bottom+radius,0),gp_Dir(0,0,1)),radius);
            BRepBuilderAPI_MakeWire wireMaker;
            wireMaker.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(left,bottom,0),gp_Pnt(right,bottom,0)).Edge());
            wireMaker.Add(BRepBuilderAPI_MakeEdge(rightCap,-std::numbers::pi/2.0,std::numbers::pi/2.0).Edge());
            wireMaker.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(right,top,0),gp_Pnt(left,top,0)).Edge());
            wireMaker.Add(BRepBuilderAPI_MakeEdge(leftCap,std::numbers::pi/2.0,3.0*std::numbers::pi/2.0).Edge());
            BRepBuilderAPI_MakeFace faceMaker(wireMaker.Wire());
            if (!wireMaker.IsDone() || !faceMaker.IsDone()) { preview.error = "The slot sketch could not form a closed planar face."; return preview; }
            profileFace = faceMaker.Face();
        } else if(command.profile==SketchProfile::Ring){
            const gp_Ax2 axis(gp_Pnt(command.originX,command.originY,0),gp_Dir(0,0,1));
            BRepBuilderAPI_MakeWire outer(BRepBuilderAPI_MakeEdge(gp_Circ(axis,command.widthOrDiameter/2.0)).Edge());
            BRepBuilderAPI_MakeWire inner(BRepBuilderAPI_MakeEdge(gp_Circ(axis,command.depth/2.0)).Edge());
            BRepBuilderAPI_MakeFace faceMaker(outer.Wire());faceMaker.Add(TopoDS::Wire(inner.Wire().Reversed()));
            if(!faceMaker.IsDone()){preview.error="The ring sketch could not form a planar face.";return preview;}
            profileFace=faceMaker.Face();
        } else if(command.profile==SketchProfile::RoundedRectangle){
            const double r=command.cornerRadius,x=command.originX,y=command.originY,w=command.widthOrDiameter,h=command.depth;
            BRepBuilderAPI_MakeWire wire;
            wire.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(x+r,y,0),gp_Pnt(x+w-r,y,0)).Edge());
            wire.Add(BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(gp_Pnt(x+w-r,y+r,0),gp_Dir(0,0,1)),r),-std::numbers::pi/2,0).Edge());
            wire.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(x+w,y+r,0),gp_Pnt(x+w,y+h-r,0)).Edge());
            wire.Add(BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(gp_Pnt(x+w-r,y+h-r,0),gp_Dir(0,0,1)),r),0,std::numbers::pi/2).Edge());
            wire.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(x+w-r,y+h,0),gp_Pnt(x+r,y+h,0)).Edge());
            wire.Add(BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(gp_Pnt(x+r,y+h-r,0),gp_Dir(0,0,1)),r),std::numbers::pi/2,std::numbers::pi).Edge());
            wire.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(x,y+h-r,0),gp_Pnt(x,y+r,0)).Edge());
            wire.Add(BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(gp_Pnt(x+r,y+r,0),gp_Dir(0,0,1)),r),std::numbers::pi,3*std::numbers::pi/2).Edge());
            BRepBuilderAPI_MakeFace faceMaker(wire.Wire());
            if(!wire.IsDone()||!faceMaker.IsDone()){preview.error="The rounded-rectangle sketch could not form a planar face.";return preview;}
            profileFace=faceMaker.Face();
        } else if (command.profile == SketchProfile::Triangle || command.profile == SketchProfile::RegularPolygon) {
            const int sides=command.profile == SketchProfile::Triangle ? 3 : static_cast<int>(command.depth);
            BRepBuilderAPI_MakePolygon boundary;
            for (int i=0;i<sides;++i) {
                const double angle=2.0*std::numbers::pi*i/sides;
                boundary.Add(gp_Pnt(command.originX+command.widthOrDiameter/2.0*std::cos(angle),
                                    command.originY+command.widthOrDiameter/2.0*std::sin(angle),0.0));
            }
            boundary.Close();
            BRepBuilderAPI_MakeFace faceMaker(boundary.Wire());
            if (!boundary.IsDone() || !faceMaker.IsDone()) { preview.error = "The polygon sketch could not form a planar face."; return preview; }
            profileFace=faceMaker.Face();
        } else {
            preview.error = "The requested sketch profile is not supported.";
            return preview;
        }

        TopoDS_Shape tool;
        if(command.taperDegrees!=0.0){
            const double inset=command.height*std::tan(command.taperDegrees*std::numbers::pi/180.0);
            BRepBuilderAPI_MakeWire bottom,top;
            if(command.profile==SketchProfile::Rectangle){
                const double topWidth=command.widthOrDiameter-2.0*inset;
                const double topDepth=command.depth-2.0*inset;
                const double offsetX=inset,offsetY=inset;
                BRepBuilderAPI_MakePolygon bottomPolygon,topPolygon;
                bottomPolygon.Add(gp_Pnt(command.originX,command.originY,0));
                bottomPolygon.Add(gp_Pnt(command.originX+command.widthOrDiameter,command.originY,0));
                bottomPolygon.Add(gp_Pnt(command.originX+command.widthOrDiameter,command.originY+command.depth,0));
                bottomPolygon.Add(gp_Pnt(command.originX,command.originY+command.depth,0));bottomPolygon.Close();
                const double endZ=command.reverseDirection?-command.height:command.height;
                topPolygon.Add(gp_Pnt(command.originX+offsetX,command.originY+offsetY,endZ));
                topPolygon.Add(gp_Pnt(command.originX+offsetX+topWidth,command.originY+offsetY,endZ));
                topPolygon.Add(gp_Pnt(command.originX+offsetX+topWidth,command.originY+offsetY+topDepth,endZ));
                topPolygon.Add(gp_Pnt(command.originX+offsetX,command.originY+offsetY+topDepth,endZ));topPolygon.Close();
                bottom.Add(bottomPolygon.Wire());top.Add(topPolygon.Wire());
            }else{
                const gp_Circ bottomCircle(gp_Ax2(gp_Pnt(command.originX,command.originY,0),gp_Dir(0,0,1)),command.widthOrDiameter/2.0);
                const gp_Circ topCircle(gp_Ax2(gp_Pnt(command.originX,command.originY,command.reverseDirection?-command.height:command.height),gp_Dir(0,0,1)),command.widthOrDiameter/2.0-inset);
                bottom.Add(BRepBuilderAPI_MakeEdge(bottomCircle).Edge());top.Add(BRepBuilderAPI_MakeEdge(topCircle).Edge());
            }
            BRepOffsetAPI_ThruSections loft(true,true);
            loft.AddWire(bottom.Wire());loft.AddWire(top.Wire());loft.Build();
            if(!loft.IsDone()){preview.error="The kernel could not build the tapered extrusion.";return preview;}
            tool=loft.Shape();
        }else{
            const double direction=command.reverseDirection?-1.0:1.0;
            BRepPrimAPI_MakePrism prism(profileFace,gp_Vec(0,0,direction*extrusionHeight),true);
            prism.Build();
            if(!prism.IsDone()){preview.error="The kernel could not extrude the sketch profile.";return preview;}
            tool=prism.Shape();
            if(command.symmetric){
                gp_Trsf shift;shift.SetTranslation(gp_Vec(0,0,-direction*command.height/2.0));
                tool=BRepBuilderAPI_Transform(tool,shift,true).Shape();
            }
        }
        if(command.planeOffset!=0.0){
            gp_Trsf offsetTransform;
            offsetTransform.SetTranslation(gp_Vec(0,0,command.planeOffset));
            tool=BRepBuilderAPI_Transform(tool,offsetTransform,true).Shape();
        }
        if(command.sketchPlane!=SketchPlane::XY){
            gp_Trsf planeTransform;
            if(command.sketchPlane==SketchPlane::XZ)
                planeTransform.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(1,0,0)),std::numbers::pi/2.0);
            else
                planeTransform.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(1,1,1)),2.0*std::numbers::pi/3.0);
            tool=BRepBuilderAPI_Transform(tool,planeTransform,true).Shape();
        }
        if (command.mode == ExtrusionMode::NewBody) {
            preview.shape = tool;
        } else if (command.mode == ExtrusionMode::Join) {
            if (command.targetShape.IsNull()) { preview.error = "The Join target body is unavailable."; return preview; }
            BRepAlgoAPI_Fuse operation(command.targetShape, tool);
            operation.Build();
            if (!operation.IsDone() || operation.HasErrors()) {
                preview.error = "The extrusion could not be joined to the target body.";
                return preview;
            }
            preview.shape = operation.Shape();
        } else if (command.mode == ExtrusionMode::Cut) {
            if (command.targetShape.IsNull()) { preview.error = "The Cut target body is unavailable."; return preview; }
            BRepAlgoAPI_Cut operation(command.targetShape, tool);
            operation.Build();
            if (!operation.IsDone() || operation.HasErrors()) {
                preview.error = "The extrusion could not cut the target body.";
                return preview;
            }
            preview.shape = operation.Shape();
        } else {
            preview.error = "The requested extrusion operation is not supported.";
            return preview;
        }
        if (!validShape(preview.shape, &preview.error)) preview.shape = TopoDS_Shape();
    } catch (const Standard_Failure& e) { preview.error = QStringLiteral("CAD kernel error: %1").arg(e.what()); }
      catch (const std::exception& e) { preview.error = QString::fromUtf8(e.what()); }
    return preview;
}

bool SceneDocument::commitExtrusion(const ExtrusionCommand& command, const TopoDS_Shape& preview, QString* error) {
    if (command.sourceRevision != revision_) {
        setError(error, "The extrusion preview is stale because the document changed. Recreate the preview.");
        return false;
    }
    if (!validShape(preview, error)) return false;
    if (command.mode == ExtrusionMode::NewBody) {
        if (parts_.size() >= kMaxObjects) { setError(error, "Scene object limit reached."); return false; }
        QString profileName;
        switch(command.profile) {
            case SketchProfile::Rectangle: profileName="Rectangle"; break;
            case SketchProfile::Circle: profileName="Circle"; break;
            case SketchProfile::Ellipse: profileName="Ellipse"; break;
            case SketchProfile::Slot: profileName="Slot"; break;
            case SketchProfile::Triangle: profileName="Triangle"; break;
            case SketchProfile::RegularPolygon: profileName="Polygon"; break;
            case SketchProfile::RoundedRectangle: profileName="Rounded Rectangle"; break;
            case SketchProfile::Ring: profileName="Ring"; break;
        }
        const QString baseName = command.symmetric ? QStringLiteral("Symmetric Extruded ")+profileName :
                                 command.taperDegrees!=0.0 ? QStringLiteral("Tapered Extruded ")+profileName :
                                 QStringLiteral("Extruded ")+profileName;
        const QString name = uniqueName(baseName);
        checkpoint();
        parts_.append({generatedId(), name, preview});
        return true;
    }
    if (command.mode != ExtrusionMode::Join && command.mode != ExtrusionMode::Cut) {
        setError(error, "The extrusion operation is not supported."); return false;
    }
    Part* target = nullptr;
    for (auto& part : parts_) if (part.id == command.targetPartId) { target = &part; break; }
    if (!target) { setError(error, "The target body no longer exists."); return false; }
    if (command.targetGeometryToken.isEmpty() || target->geometryToken != command.targetGeometryToken) {
        setError(error, "The target body changed after the extrusion preview. Recreate the preview.");
        return false;
    }
    checkpoint();
    target->shape = preview;
    target->geometryToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return true;
}

bool SceneDocument::prepareRevolve(double radialOffset, double axialOrigin, double radialWidth,
                                   double axialHeight, double angleDegrees, RevolveCommand* command,
                                   QString* error) const {
    if (!command) { setError(error, "Revolve command output is unavailable."); return false; }
    if (parts_.size() >= kMaxObjects) { setError(error, "Scene object limit reached."); return false; }
    if (!std::isfinite(radialOffset) || radialOffset < 0.0 || radialOffset > 1e6 ||
        !finiteOffset(axialOrigin)) {
        setError(error, "Revolve profile origin is out of range."); return false;
    }
    if (!finitePositive(radialWidth) || !finitePositive(axialHeight)) {
        setError(error, "Revolve profile width and height must be between 0.00001 and 1,000,000 mm.");
        return false;
    }
    if (!std::isfinite(angleDegrees) || angleDegrees < 1.0 || angleDegrees > 360.0) {
        setError(error, "Revolve angle must be between 1 and 360 degrees."); return false;
    }
    command->radialOffset = radialOffset;
    command->axialOrigin = axialOrigin;
    command->radialWidth = radialWidth;
    command->axialHeight = axialHeight;
    command->angleDegrees = angleDegrees;
    command->axisIndex=2;command->axisOriginX=0;command->axisOriginY=0;command->axisOriginZ=0;
    command->sourceRevision = revision_;
    return true;
}

bool SceneDocument::prepareRevolveAboutAxis(double radialOffset,double axialOrigin,double radialWidth,double axialHeight,
                                             double angleDegrees,int axisIndex,double originX,double originY,double originZ,
                                             RevolveCommand* command,QString* error) const {
    if(axisIndex<0||axisIndex>2||!finiteOffset(originX)||!finiteOffset(originY)||!finiteOffset(originZ)){
        setError(error,"Choose X, Y, or Z and an axis origin within +/-1,000,000 mm.");return false;
    }
    if(!prepareRevolve(radialOffset,axialOrigin,radialWidth,axialHeight,angleDegrees,command,error))return false;
    command->axisIndex=axisIndex;command->axisOriginX=originX;command->axisOriginY=originY;command->axisOriginZ=originZ;
    return true;
}

RevolvePreview SceneDocument::buildRevolvePreview(const RevolveCommand& command) {
    RevolvePreview preview;
    try {
        if (!std::isfinite(command.radialOffset) || command.radialOffset < 0.0 || command.radialOffset > 1e6 ||
            !finiteOffset(command.axialOrigin) || !finitePositive(command.radialWidth) ||
            !finitePositive(command.axialHeight) || !std::isfinite(command.angleDegrees) ||
            command.angleDegrees < 1.0 || command.angleDegrees > 360.0 || command.axisIndex<0 || command.axisIndex>2 ||
            !finiteOffset(command.axisOriginX)||!finiteOffset(command.axisOriginY)||!finiteOffset(command.axisOriginZ)) {
            preview.error = "The revolve profile parameters are invalid.";
            return preview;
        }
        BRepBuilderAPI_MakePolygon boundary;
        boundary.Add(gp_Pnt(command.radialOffset,0.0,command.axialOrigin));
        boundary.Add(gp_Pnt(command.radialOffset+command.radialWidth,0.0,command.axialOrigin));
        boundary.Add(gp_Pnt(command.radialOffset+command.radialWidth,0.0,command.axialOrigin+command.axialHeight));
        boundary.Add(gp_Pnt(command.radialOffset,0.0,command.axialOrigin+command.axialHeight));
        boundary.Close();
        if (!boundary.IsDone()) { preview.error = "The revolve rectangle could not be closed."; return preview; }
        BRepBuilderAPI_MakeFace faceMaker(boundary.Wire());
        if (!faceMaker.IsDone()) { preview.error = "The revolve rectangle could not form a planar face."; return preview; }

        const gp_Ax1 axis(gp_Pnt(0,0,0),gp_Dir(0,0,1));
        const double angleRadians = command.angleDegrees * std::numbers::pi / 180.0;
        BRepPrimAPI_MakeRevol revolver(faceMaker.Face(),axis,angleRadians,true);
        revolver.Build();
        if (!revolver.IsDone()) { preview.error = "The kernel could not revolve the rectangular profile."; return preview; }
        preview.shape = revolver.Shape();
        if(command.axisIndex!=2||command.axisOriginX!=0||command.axisOriginY!=0||command.axisOriginZ!=0){
            gp_Trsf placement;
            if(command.axisIndex==0)placement.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(0,1,0)),std::numbers::pi/2.0);
            else if(command.axisIndex==1)placement.SetRotation(gp_Ax1(gp_Pnt(0,0,0),gp_Dir(1,0,0)),-std::numbers::pi/2.0);
            placement.SetTranslationPart(gp_Vec(command.axisOriginX,command.axisOriginY,command.axisOriginZ));
            preview.shape=BRepBuilderAPI_Transform(preview.shape,placement,true).Shape();
        }
        if (!validShape(preview.shape,&preview.error)) preview.shape = TopoDS_Shape();
    } catch (const Standard_Failure& e) { preview.error = QStringLiteral("CAD kernel error: %1").arg(e.what()); }
      catch (const std::exception& e) { preview.error = QString::fromUtf8(e.what()); }
    return preview;
}

bool SceneDocument::commitRevolve(const RevolveCommand& command, const TopoDS_Shape& preview, QString* error) {
    if (command.sourceRevision != revision_) {
        setError(error, "The revolve preview is stale because the document changed. Recreate the preview.");
        return false;
    }
    if (parts_.size() >= kMaxObjects) { setError(error, "Scene object limit reached."); return false; }
    if (!validShape(preview,error)) return false;
    const QString name = uniqueName(QStringLiteral("Revolved Rectangle"));
    checkpoint();
    parts_.append({generatedId(),name,preview});
    return true;
}

bool SceneDocument::mirror(const QString& id, Axis planeNormal, double planeOffset, QString* error) {
    if (!finiteOffset(planeOffset)) { setError(error,"Mirror plane offset is out of range."); return false; }
    if (parts_.size() >= kMaxObjects) { setError(error,"Scene object limit reached."); return false; }
    const Part* source = find(id);
    if (!source) { setError(error,"Object not found."); return false; }
    const QString sourceName = source->name;
    const bool visible = source->visible;
    const QString color = source->color;
    try {
        gp_Pnt point(0,0,0);
        gp_Dir normal(1,0,0);
        if (planeNormal == Axis::X) { point.SetX(planeOffset); normal = gp_Dir(1,0,0); }
        else if (planeNormal == Axis::Y) { point.SetY(planeOffset); normal = gp_Dir(0,1,0); }
        else { point.SetZ(planeOffset); normal = gp_Dir(0,0,1); }
        gp_Trsf transform;
        transform.SetMirror(gp_Ax2(point,normal));
        const TopoDS_Shape reflected = BRepBuilderAPI_Transform(source->shape,transform,true).Shape();
        if (!validShape(reflected,error)) return false;
        const Part copy{generatedId(),uniqueName(sourceName+QStringLiteral(" Mirror")),reflected,visible,color};
        checkpoint();
        parts_.append(copy);
        return true;
    } catch (const Standard_Failure& e) { setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error,QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::mirrorAcrossPlane(const QString& id,double nx,double ny,double nz,double planeOffset,QString* error) {
    if (!std::isfinite(nx)||!std::isfinite(ny)||!std::isfinite(nz)||!finiteOffset(planeOffset)||
        std::sqrt(nx*nx+ny*ny+nz*nz)<1e-8) {
        setError(error,"Enter a finite, non-zero plane normal and an offset within +/-1,000,000 mm."); return false;
    }
    if (parts_.size()>=kMaxObjects) { setError(error,"Scene object limit reached."); return false; }
    const Part* source=find(id);
    if (!source) { setError(error,"Object not found."); return false; }
    const QString sourceName=source->name;
    const bool visible=source->visible;
    const QString color=source->color;
    try {
        const gp_Dir normal(nx,ny,nz);
        const gp_Pnt point(normal.X()*planeOffset,normal.Y()*planeOffset,normal.Z()*planeOffset);
        gp_Trsf transform;
        transform.SetMirror(gp_Ax2(point,normal));
        const TopoDS_Shape reflected=BRepBuilderAPI_Transform(source->shape,transform,true).Shape();
        if (!validShape(reflected,error)) return false;
        const Part copy{generatedId(),uniqueName(sourceName+QStringLiteral(" Plane Mirror")),reflected,visible,color};
        checkpoint(); parts_.append(copy); return true;
    } catch (const Standard_Failure& e) { setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error,QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::rotateAboutAxis(const QString& id,double ox,double oy,double oz,
                                    double ax,double ay,double az,double degrees,QString* error) {
    if (!finiteOffset(ox)||!finiteOffset(oy)||!finiteOffset(oz)||!std::isfinite(ax)||!std::isfinite(ay)||
        !std::isfinite(az)||std::sqrt(ax*ax+ay*ay+az*az)<1e-8||!std::isfinite(degrees)||std::abs(degrees)>36000) {
        setError(error,"Rotation origin, axis, or angle is invalid."); return false;
    }
    const Part* source=find(id);
    if (!source) { setError(error,"Object not found."); return false; }
    if (degrees==0.0) return true;
    try {
        gp_Trsf transform;
        transform.SetRotation(gp_Ax1(gp_Pnt(ox,oy,oz),gp_Dir(ax,ay,az)),degrees*std::numbers::pi/180.0);
        const TopoDS_Shape result=BRepBuilderAPI_Transform(source->shape,transform,true).Shape();
        if (!validShape(result,error)) return false;
        checkpoint();
        for (Part& part:parts_) if (part.id==id) {
            part.shape=result; part.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces); break;
        }
        return true;
    } catch (const Standard_Failure& e) { setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error,QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::linearPattern(const QString& id, Axis axis, int instanceCount, double spacing, QString* error) {
    if (instanceCount < 2 || instanceCount > 1000) {
        setError(error,"Linear pattern count must be between 2 and 1,000 total instances."); return false;
    }
    if (!std::isfinite(spacing) || std::abs(spacing) < 0.00001 || std::abs(spacing) > 1e6 ||
        std::abs(spacing) * (instanceCount-1) > 1e6) {
        setError(error,"Pattern spacing and total offset must be finite and within 1,000,000 mm."); return false;
    }
    if (parts_.size()+instanceCount-1 > kMaxObjects) { setError(error,"Scene object limit reached."); return false; }
    const Part* source = find(id);
    if (!source) { setError(error,"Object not found."); return false; }
    const TopoDS_Shape sourceShape = source->shape;
    const QString sourceName = source->name;
    const bool visible = source->visible;
    const QString color = source->color;
    QVector<Part> copies;
    QSet<QString> reservedNames;
    try {
        for (int index=1; index<instanceCount; ++index) {
            const double offset = spacing * index;
            const gp_Vec translation = axis == Axis::X ? gp_Vec(offset,0,0) :
                                       axis == Axis::Y ? gp_Vec(0,offset,0) : gp_Vec(0,0,offset);
            gp_Trsf transform;
            transform.SetTranslation(translation);
            const TopoDS_Shape result = BRepBuilderAPI_Transform(sourceShape,transform,true).Shape();
            if (!validShape(result,error)) return false;
            const QString baseName = sourceName + QStringLiteral(" Pattern %1").arg(index+1);
            QString name = uniqueName(baseName);
            int suffix = 2;
            while (reservedNames.contains(name.toCaseFolded())) {
                name = uniqueName(baseName+QStringLiteral(" %1").arg(suffix++));
            }
            reservedNames.insert(name.toCaseFolded());
            copies.append({generatedId(),name,result,visible,color});
        }
        checkpoint();
        for (const Part& copy : copies) parts_.append(copy);
        return true;
    } catch (const Standard_Failure& e) { setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error,QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::linearPatternAlongVector(const QString& id,double directionX,double directionY,double directionZ,
                                              int instanceCount,double spacing,QString* error) {
    if(instanceCount<2||instanceCount>1000){setError(error,"Vector pattern count must be between 2 and 1,000 total instances.");return false;}
    if(!std::isfinite(directionX)||!std::isfinite(directionY)||!std::isfinite(directionZ)||
       std::max({std::abs(directionX),std::abs(directionY),std::abs(directionZ)})>1e6){
        setError(error,"Pattern direction components must be finite and within ±1,000,000.");return false;
    }
    const double magnitude=std::hypot(directionX,directionY,directionZ);
    if(!std::isfinite(magnitude)||magnitude<=1e-12){setError(error,"Pattern direction vector must not be zero.");return false;}
    if(!std::isfinite(spacing)||std::abs(spacing)<1e-5||std::abs(spacing)>1e6||
       std::abs(spacing)*(instanceCount-1)>1e6){
        setError(error,"Pattern spacing and total offset must be finite and within 1,000,000 mm.");return false;
    }
    if(parts_.size()+instanceCount-1>kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    const Part* source=find(id);
    if(!source){setError(error,"Object not found.");return false;}
    const TopoDS_Shape sourceShape=source->shape;
    const QString sourceName=source->name;
    const bool visible=source->visible;
    const QString color=source->color;
    const gp_Vec direction(directionX/magnitude,directionY/magnitude,directionZ/magnitude);
    QVector<Part> copies;
    QSet<QString> reservedNames;
    try{
        for(int index=1;index<instanceCount;++index){
            gp_Trsf transform;
            transform.SetTranslation(direction*(spacing*index));
            const TopoDS_Shape result=BRepBuilderAPI_Transform(sourceShape,transform,true).Shape();
            if(!validShape(result,error))return false;
            const QString baseName=sourceName+QStringLiteral(" Vector Pattern %1").arg(index+1);
            QString name=uniqueName(baseName);int suffix=2;
            while(reservedNames.contains(name.toCaseFolded()))name=uniqueName(baseName+QStringLiteral(" %1").arg(suffix++));
            reservedNames.insert(name.toCaseFolded());
            copies.append({generatedId(),name,result,visible,color});
        }
        checkpoint();
        for(const Part& copy:copies)parts_.append(copy);
        return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::rectangularPattern(const QString& id,Axis firstAxis,int firstCount,double firstSpacing,
                                       Axis secondAxis,int secondCount,double secondSpacing,QString* error) {
    if(firstCount<1||secondCount<1||firstCount>1000||secondCount>1000||firstCount>1000/secondCount||
       (firstAxis!=Axis::X&&firstAxis!=Axis::Y&&firstAxis!=Axis::Z)||
       (secondAxis!=Axis::X&&secondAxis!=Axis::Y&&secondAxis!=Axis::Z)||firstAxis==secondAxis) {
        setError(error,"Choose two different axes and a pattern with 2 to 1,000 total instances."); return false;
    }
    const int total=firstCount*secondCount;
    if(total<2||total>1000){setError(error,"Choose a pattern with 2 to 1,000 total instances.");return false;}
    auto spacingValid=[](int count,double spacing) {
        return std::isfinite(spacing)&&std::abs(spacing)<=1e6&&
               (count==1|| (std::abs(spacing)>=1e-5&&std::abs(spacing)*(count-1)<=1e6));
    };
    if (!spacingValid(firstCount,firstSpacing)||!spacingValid(secondCount,secondSpacing)) {
        setError(error,"Pattern spacings and total offsets must be within 1,000,000 mm."); return false;
    }
    if (parts_.size()+total-1>kMaxObjects) { setError(error,"Scene object limit reached."); return false; }
    const Part* source=find(id);
    if (!source) { setError(error,"Object not found."); return false; }
    const TopoDS_Shape sourceShape=source->shape;
    const QString sourceName=source->name;
    const bool visible=source->visible;
    const QString color=source->color;
    auto axisVector=[](Axis axis,double amount) {
        return axis==Axis::X?gp_Vec(amount,0,0):axis==Axis::Y?gp_Vec(0,amount,0):gp_Vec(0,0,amount);
    };
    QVector<Part> copies;
    QSet<QString> reservedNames;
    try {
        for(int i=0;i<firstCount;++i) for(int j=0;j<secondCount;++j) {
            if(i==0&&j==0) continue;
            gp_Trsf transform;
            transform.SetTranslation(axisVector(firstAxis,firstSpacing*i)+axisVector(secondAxis,secondSpacing*j));
            const TopoDS_Shape result=BRepBuilderAPI_Transform(sourceShape,transform,true).Shape();
            if(!validShape(result,error)) return false;
            const QString baseName=sourceName+QStringLiteral(" Rect Pattern %1-%2").arg(i+1).arg(j+1);
            QString name=uniqueName(baseName); int suffix=2;
            while(reservedNames.contains(name.toCaseFolded())) name=uniqueName(baseName+QStringLiteral(" %1").arg(suffix++));
            reservedNames.insert(name.toCaseFolded());
            copies.append({generatedId(),name,result,visible,color});
        }
        checkpoint(); for(const Part& copy:copies) parts_.append(copy); return true;
    } catch(const Standard_Failure& e) { setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch(const std::exception& e) { setError(error,QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::rectangularPatternAlongVectors(const QString& id,double firstX,double firstY,double firstZ,
                                                    int firstCount,double firstSpacing,double secondX,double secondY,
                                                    double secondZ,int secondCount,double secondSpacing,QString* error) {
    if(firstCount<1||secondCount<1||firstCount>1000||secondCount>1000||firstCount>1000/secondCount){
        setError(error,"Choose a two-direction pattern with 2 to 1,000 total instances.");return false;
    }
    const int total=firstCount*secondCount;
    if(total<2||total>1000){setError(error,"Choose a pattern with 2 to 1,000 total instances.");return false;}
    auto vectorValid=[](double x,double y,double z){
        return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z)&&
               std::max({std::abs(x),std::abs(y),std::abs(z)})<=1e6;
    };
    if(!vectorValid(firstX,firstY,firstZ)||!vectorValid(secondX,secondY,secondZ)){
        setError(error,"Pattern direction components must be finite and within ±1,000,000.");return false;
    }
    const double firstMagnitude=std::hypot(firstX,firstY,firstZ);
    const double secondMagnitude=std::hypot(secondX,secondY,secondZ);
    if(!std::isfinite(firstMagnitude)||!std::isfinite(secondMagnitude)||firstMagnitude<=1e-12||secondMagnitude<=1e-12){
        setError(error,"Both pattern direction vectors must be nonzero.");return false;
    }
    const gp_Vec firstDirection(firstX/firstMagnitude,firstY/firstMagnitude,firstZ/firstMagnitude);
    const gp_Vec secondDirection(secondX/secondMagnitude,secondY/secondMagnitude,secondZ/secondMagnitude);
    if(firstDirection.Crossed(secondDirection).Magnitude()<=1e-8){
        setError(error,"Pattern direction vectors must not be parallel.");return false;
    }
    auto spacingValid=[](int count,double spacing){
        return std::isfinite(spacing)&&std::abs(spacing)<=1e6&&
               (count==1||(std::abs(spacing)>=1e-5&&std::abs(spacing)*(count-1)<=1e6));
    };
    if(!spacingValid(firstCount,firstSpacing)||!spacingValid(secondCount,secondSpacing)){
        setError(error,"Pattern spacings and total offsets must be within 1,000,000 mm.");return false;
    }
    if(parts_.size()+total-1>kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    const Part* source=find(id);
    if(!source){setError(error,"Object not found.");return false;}
    const TopoDS_Shape sourceShape=source->shape;
    const QString sourceName=source->name;
    const bool visible=source->visible;
    const QString color=source->color;
    QVector<Part> copies;
    QSet<QString> reservedNames;
    try{
        for(int i=0;i<firstCount;++i)for(int j=0;j<secondCount;++j){
            if(i==0&&j==0)continue;
            gp_Trsf transform;
            transform.SetTranslation(firstDirection*(firstSpacing*i)+secondDirection*(secondSpacing*j));
            const TopoDS_Shape result=BRepBuilderAPI_Transform(sourceShape,transform,true).Shape();
            if(!validShape(result,error))return false;
            const QString baseName=sourceName+QStringLiteral(" Vector Rect Pattern %1-%2").arg(i+1).arg(j+1);
            QString name=uniqueName(baseName);int suffix=2;
            while(reservedNames.contains(name.toCaseFolded()))name=uniqueName(baseName+QStringLiteral(" %1").arg(suffix++));
            reservedNames.insert(name.toCaseFolded());
            copies.append({generatedId(),name,result,visible,color});
        }
        checkpoint();for(const Part& copy:copies)parts_.append(copy);return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::prepareHole(const QString& id, double x, double y, double entryZ, double diameter,
                                double depth, bool throughAll, HoleCommand* command, QString* error) const {
    if (!command) { setError(error,"Hole command output is unavailable."); return false; }
    if (!finiteOffset(x) || !finiteOffset(y) || !finiteOffset(entryZ)) {
        setError(error,"Hole location must be finite and within +/-1,000,000 mm."); return false;
    }
    if (!finitePositive(diameter)) { setError(error,"Hole diameter must be between 0.00001 and 1,000,000 mm."); return false; }
    if (!throughAll && !finitePositive(depth)) { setError(error,"Blind hole depth must be between 0.00001 and 1,000,000 mm."); return false; }
    const Part* part=find(id);
    if (!part) { setError(error,"Object not found."); return false; }
    command->partId=id;
    command->geometryToken=part->geometryToken;
    command->sourceShape=BRepBuilderAPI_Copy(part->shape).Shape();
    command->x=x;
    command->y=y;
    command->entryZ=entryZ;
    command->diameter=diameter;
    command->depth=depth;
    command->throughAll=throughAll;
    return true;
}

bool SceneDocument::prepareCounterbore(const QString& id,double x,double y,double entryZ,double diameter,double depth,
                                       bool throughAll,double headDiameter,double headDepth,HoleCommand* command,
                                       QString* error) const {
    if(!finitePositive(headDiameter)||headDiameter<=diameter||!finitePositive(headDepth)||
       (!throughAll&&headDepth>depth)){
        setError(error,"Counterbore head diameter must exceed the drill diameter; its depth must fit within a blind hole.");return false;
    }
    if(!prepareHole(id,x,y,entryZ,diameter,depth,throughAll,command,error))return false;
    command->style=HoleStyle::Counterbore;command->headDiameter=headDiameter;command->headDepth=headDepth;
    return true;
}

bool SceneDocument::prepareCountersink(const QString& id,double x,double y,double entryZ,double diameter,double depth,
                                       bool throughAll,double headDiameter,double includedAngleDegrees,
                                       HoleCommand* command,QString* error) const {
    if(!finitePositive(headDiameter)||headDiameter<=diameter||!std::isfinite(includedAngleDegrees)||
       includedAngleDegrees<=1.0||includedAngleDegrees>=179.0){
        setError(error,"Countersink head diameter must exceed the drill diameter and included angle must be between 1 and 179 degrees.");return false;
    }
    if(!prepareHole(id,x,y,entryZ,diameter,depth,throughAll,command,error))return false;
    command->style=HoleStyle::Countersink;command->headDiameter=headDiameter;command->sinkAngleDegrees=includedAngleDegrees;
    return true;
}

HolePreview SceneDocument::buildHolePreview(const HoleCommand& command) {
    HolePreview preview;
    try {
        if (command.partId.isEmpty() || command.sourceShape.IsNull() ||
            !finiteOffset(command.x) || !finiteOffset(command.y) || !finiteOffset(command.entryZ) ||
            !finitePositive(command.diameter) || (!command.throughAll && !finitePositive(command.depth)) ||
            (command.style!=HoleStyle::Simple&&(!finitePositive(command.headDiameter)||command.headDiameter<=command.diameter)) ||
            (command.style==HoleStyle::Counterbore&&(!finitePositive(command.headDepth)||(!command.throughAll&&command.headDepth>command.depth))) ||
            (command.style==HoleStyle::Countersink&&(!std::isfinite(command.sinkAngleDegrees)||command.sinkAngleDegrees<=1||command.sinkAngleDegrees>=179))) {
            preview.error="The hole command is incomplete or invalid.";
            return preview;
        }
        double cutDepth=command.depth;
        double margin=0.01;
        if (command.throughAll) {
            Bnd_Box bounds;
            BRepBndLib::Add(command.sourceShape,bounds);
            if (bounds.IsVoid()) { preview.error="The selected body has no measurable bounds."; return preview; }
            double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;
            bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
            margin=std::max(0.01,(zmax-zmin)*1e-4);
            cutDepth=command.entryZ-zmin+2.0*margin;
            if (!finitePositive(cutDepth)) { preview.error="The hole entry point is below the selected body's lower bound."; return preview; }
        }
        const double startZ=command.entryZ+margin;
        const double toolLength=cutDepth+margin;
        if (!finiteOffset(startZ) || !finitePositive(toolLength)) { preview.error="Hole cutter dimensions are out of range."; return preview; }
        const gp_Ax2 axis(gp_Pnt(command.x,command.y,startZ),gp_Dir(0,0,-1));
        const TopoDS_Shape cutter=BRepPrimAPI_MakeCylinder(axis,command.diameter/2.0,toolLength).Shape();
        BRepAlgoAPI_Cut operation(command.sourceShape,cutter);
        operation.Build();
        if (!operation.IsDone()) { preview.error="The kernel could not cut the requested hole."; return preview; }
        preview.shape=operation.Shape();
        if(command.style==HoleStyle::Counterbore){
            const TopoDS_Shape headCutter=BRepPrimAPI_MakeCylinder(axis,command.headDiameter/2.0,command.headDepth+margin).Shape();
            BRepAlgoAPI_Cut headCut(preview.shape,headCutter);headCut.Build();
            if(!headCut.IsDone()){preview.shape=TopoDS_Shape();preview.error="The counterbore recess could not be cut.";return preview;}
            preview.shape=headCut.Shape();
        } else if(command.style==HoleStyle::Countersink){
            const double sinkDepth=(command.headDiameter-command.diameter)/(2.0*std::tan(command.sinkAngleDegrees*std::numbers::pi/360.0));
            if(!finitePositive(sinkDepth)){preview.shape=TopoDS_Shape();preview.error="The countersink depth is invalid.";return preview;}
            const double drillRadius=command.diameter/2.0;
            const double radialSlope=(command.headDiameter/2.0-drillRadius)/sinkDepth;
            const TopoDS_Shape sinkCutter=BRepPrimAPI_MakeCone(axis,command.headDiameter/2.0+radialSlope*margin,
                                                              drillRadius,sinkDepth+margin).Shape();
            BRepAlgoAPI_Cut sinkCut(preview.shape,sinkCutter);sinkCut.Build();
            if(!sinkCut.IsDone()){preview.shape=TopoDS_Shape();preview.error="The countersink recess could not be cut.";return preview;}
            preview.shape=sinkCut.Shape();
        }
        if (!validShape(preview.shape,&preview.error)) { preview.shape=TopoDS_Shape(); return preview; }
        GProp_GProps before,after;
        BRepGProp::VolumeProperties(command.sourceShape,before);
        BRepGProp::VolumeProperties(preview.shape,after);
        const double removed=before.Mass()-after.Mass();
        if (!std::isfinite(removed) || removed<=std::max(1e-9,before.Mass()*1e-12)) {
            preview.shape=TopoDS_Shape();
            preview.error="The hole cutter does not intersect the selected body.";
        }
    } catch (const Standard_Failure& e) { preview.error=QStringLiteral("CAD kernel error: %1").arg(e.what()); }
      catch (const std::exception& e) { preview.error=QString::fromUtf8(e.what()); }
    return preview;
}

bool SceneDocument::commitHole(const HoleCommand& command, const TopoDS_Shape& preview, QString* error) {
    const Part* part=find(command.partId);
    if (!part) { setError(error,"The selected body no longer exists."); return false; }
    if (part->geometryToken!=command.geometryToken) {
        setError(error,"The hole preview is stale because the body geometry changed. Select the body again."); return false;
    }
    if (!validShape(preview,error)) return false;
    checkpoint();
    for (auto& target:parts_) if (target.id==command.partId) {
        target.shape=preview;
        target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
        break;
    }
    return true;
}

bool SceneDocument::prepareDovetailGroove(const QString& id,double centerX,double centerY,double entryZ,
                                          double mouthWidth,double bottomWidth,double depth,double length,
                                          double angleDegrees,DovetailGrooveCommand* command,QString* error) const {
    if(!command){setError(error,"The dovetail-groove command output is missing.");return false;}
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||
       !finitePositive(mouthWidth)||!finitePositive(bottomWidth)||bottomWidth<=mouthWidth||
       !finitePositive(depth)||!finitePositive(length)||!std::isfinite(angleDegrees)||
       angleDegrees < -360.0||angleDegrees > 360.0||
       std::abs(centerX)+bottomWidth/2.0>1e6||std::abs(centerY)+length/2.0>1e6||
       std::abs(entryZ-depth)>1e6){
        setError(error,"Dovetail groove dimensions are invalid. The undercut width must exceed the mouth width.");return false;
    }
    const Part* part=find(id);
    if(!part){setError(error,"Object not found.");return false;}
    command->partId=id;
    command->geometryToken=part->geometryToken;
    command->sourceShape=BRepBuilderAPI_Copy(part->shape).Shape();
    command->centerX=centerX;command->centerY=centerY;command->entryZ=entryZ;
    command->mouthWidth=mouthWidth;command->bottomWidth=bottomWidth;
    command->depth=depth;command->length=length;command->angleDegrees=angleDegrees;
    return true;
}

DovetailGroovePreview SceneDocument::buildDovetailGroovePreview(const DovetailGrooveCommand& command) {
    DovetailGroovePreview preview;
    try{
        if(command.partId.isEmpty()||command.sourceShape.IsNull()||!finiteOffset(command.centerX)||
           !finiteOffset(command.centerY)||!finiteOffset(command.entryZ)||!finitePositive(command.mouthWidth)||
           !finitePositive(command.bottomWidth)||command.bottomWidth<=command.mouthWidth||
           !finitePositive(command.depth)||!finitePositive(command.length)||!std::isfinite(command.angleDegrees)||
           command.angleDegrees < -360.0||command.angleDegrees > 360.0){
            preview.error="The dovetail-groove command is incomplete or invalid.";return preview;
        }
        const double margin=std::max(0.01,command.depth*1e-4);
        const double halfLength=command.length/2.0+margin;
        const double topZ=command.entryZ+margin;
        BRepBuilderAPI_MakePolygon polygon;
        polygon.Add(gp_Pnt(command.centerX-command.mouthWidth/2.0,command.centerY-halfLength,topZ));
        polygon.Add(gp_Pnt(command.centerX+command.mouthWidth/2.0,command.centerY-halfLength,topZ));
        polygon.Add(gp_Pnt(command.centerX+command.bottomWidth/2.0,command.centerY-halfLength,command.entryZ-command.depth));
        polygon.Add(gp_Pnt(command.centerX-command.bottomWidth/2.0,command.centerY-halfLength,command.entryZ-command.depth));
        polygon.Close();
        if(!polygon.IsDone()){preview.error="The dovetail cutter profile could not be constructed.";return preview;}
        const TopoDS_Face profile=BRepBuilderAPI_MakeFace(polygon.Wire()).Face();
        const TopoDS_Shape straightCutter=BRepPrimAPI_MakePrism(profile,gp_Vec(0,2.0*halfLength,0)).Shape();
        gp_Trsf rotation;
        rotation.SetRotation(gp_Ax1(gp_Pnt(command.centerX,command.centerY,0),gp_Dir(0,0,1)),
                             command.angleDegrees*std::numbers::pi/180.0);
        const TopoDS_Shape cutter=BRepBuilderAPI_Transform(straightCutter,rotation,true).Shape();
        BRepAlgoAPI_Cut operation(command.sourceShape,cutter);operation.Build();
        if(!operation.IsDone()){preview.error="The kernel could not cut the requested dovetail groove.";return preview;}
        preview.shape=operation.Shape();
        if(!validShape(preview.shape,&preview.error)){preview.shape=TopoDS_Shape();return preview;}
        GProp_GProps before,after;
        BRepGProp::VolumeProperties(command.sourceShape,before);
        BRepGProp::VolumeProperties(preview.shape,after);
        const double removed=before.Mass()-after.Mass();
        if(!std::isfinite(removed)||removed<=std::max(1e-9,before.Mass()*1e-12)){
            preview.shape=TopoDS_Shape();preview.error="The dovetail cutter does not intersect the selected body.";
        }
    }catch(const Standard_Failure& e){preview.error=QStringLiteral("CAD kernel error: %1").arg(e.what());}
     catch(const std::exception& e){preview.error=QString::fromUtf8(e.what());}
    return preview;
}

bool SceneDocument::commitDovetailGroove(const DovetailGrooveCommand& command,const TopoDS_Shape& preview,QString* error){
    const Part* part=find(command.partId);
    if(!part){setError(error,"The selected body no longer exists.");return false;}
    if(part->geometryToken!=command.geometryToken){setError(error,"The dovetail-groove preview is stale because the body geometry changed. Select the body again.");return false;}
    if(!validShape(preview,error))return false;
    checkpoint();
    for(Part& target:parts_)if(target.id==command.partId){
        target.shape=preview;
        target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
        break;
    }
    return true;
}

bool SceneDocument::prepareTSlotGroove(const QString& id,double centerX,double centerY,double entryZ,
                                        double neckWidth,double undercutWidth,double depth,double undercutHeight,
                                        double length,double angleDegrees,TSlotGrooveCommand* command,QString* error) const {
    if(!command){setError(error,"The T-slot command output is missing.");return false;}
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||
       !finitePositive(neckWidth)||!finitePositive(undercutWidth)||undercutWidth<=neckWidth||
       !finitePositive(depth)||!finitePositive(undercutHeight)||undercutHeight>=depth||
       !finitePositive(length)||!std::isfinite(angleDegrees)||angleDegrees < -360.0||angleDegrees > 360.0||
       std::abs(centerX)+undercutWidth/2.0>1e6||std::abs(centerY)+length/2.0>1e6||
       std::abs(entryZ-depth)>1e6){
        setError(error,"T-slot dimensions are invalid. The undercut must be wider than the neck and shallower than the total depth.");return false;
    }
    const Part* part=find(id);
    if(!part){setError(error,"Object not found.");return false;}
    command->partId=id;command->geometryToken=part->geometryToken;
    command->sourceShape=BRepBuilderAPI_Copy(part->shape).Shape();
    command->centerX=centerX;command->centerY=centerY;command->entryZ=entryZ;
    command->neckWidth=neckWidth;command->undercutWidth=undercutWidth;
    command->depth=depth;command->undercutHeight=undercutHeight;
    command->length=length;command->angleDegrees=angleDegrees;
    return true;
}

TSlotGroovePreview SceneDocument::buildTSlotGroovePreview(const TSlotGrooveCommand& command){
    TSlotGroovePreview preview;
    try{
        if(command.partId.isEmpty()||command.sourceShape.IsNull()||!finiteOffset(command.centerX)||
           !finiteOffset(command.centerY)||!finiteOffset(command.entryZ)||!finitePositive(command.neckWidth)||
           !finitePositive(command.undercutWidth)||command.undercutWidth<=command.neckWidth||
           !finitePositive(command.depth)||!finitePositive(command.undercutHeight)||command.undercutHeight>=command.depth||
           !finitePositive(command.length)||!std::isfinite(command.angleDegrees)||
           command.angleDegrees < -360.0||command.angleDegrees > 360.0){
            preview.error="The T-slot command is incomplete or invalid.";return preview;
        }
        const double margin=std::max(0.01,command.depth*1e-4);
        const double halfLength=command.length/2.0+margin;
        const double topZ=command.entryZ+margin;
        const double shoulderZ=command.entryZ-command.depth+command.undercutHeight;
        const double bottomZ=command.entryZ-command.depth;
        BRepBuilderAPI_MakePolygon polygon;
        polygon.Add(gp_Pnt(command.centerX-command.neckWidth/2.0,command.centerY-halfLength,topZ));
        polygon.Add(gp_Pnt(command.centerX+command.neckWidth/2.0,command.centerY-halfLength,topZ));
        polygon.Add(gp_Pnt(command.centerX+command.neckWidth/2.0,command.centerY-halfLength,shoulderZ));
        polygon.Add(gp_Pnt(command.centerX+command.undercutWidth/2.0,command.centerY-halfLength,shoulderZ));
        polygon.Add(gp_Pnt(command.centerX+command.undercutWidth/2.0,command.centerY-halfLength,bottomZ));
        polygon.Add(gp_Pnt(command.centerX-command.undercutWidth/2.0,command.centerY-halfLength,bottomZ));
        polygon.Add(gp_Pnt(command.centerX-command.undercutWidth/2.0,command.centerY-halfLength,shoulderZ));
        polygon.Add(gp_Pnt(command.centerX-command.neckWidth/2.0,command.centerY-halfLength,shoulderZ));
        polygon.Close();
        if(!polygon.IsDone()){preview.error="The T-slot cutter profile could not be constructed.";return preview;}
        const TopoDS_Face profile=BRepBuilderAPI_MakeFace(polygon.Wire()).Face();
        const TopoDS_Shape straightCutter=BRepPrimAPI_MakePrism(profile,gp_Vec(0,2.0*halfLength,0)).Shape();
        gp_Trsf rotation;
        rotation.SetRotation(gp_Ax1(gp_Pnt(command.centerX,command.centerY,0),gp_Dir(0,0,1)),
                             command.angleDegrees*std::numbers::pi/180.0);
        const TopoDS_Shape cutter=BRepBuilderAPI_Transform(straightCutter,rotation,true).Shape();
        BRepAlgoAPI_Cut operation(command.sourceShape,cutter);operation.Build();
        if(!operation.IsDone()){preview.error="The kernel could not cut the requested T-slot.";return preview;}
        preview.shape=operation.Shape();
        if(!validShape(preview.shape,&preview.error)){preview.shape=TopoDS_Shape();return preview;}
        GProp_GProps before,after;
        BRepGProp::VolumeProperties(command.sourceShape,before);
        BRepGProp::VolumeProperties(preview.shape,after);
        const double removed=before.Mass()-after.Mass();
        if(!std::isfinite(removed)||removed<=std::max(1e-9,before.Mass()*1e-12)){
            preview.shape=TopoDS_Shape();preview.error="The T-slot cutter does not intersect the selected body.";
        }
    }catch(const Standard_Failure& e){preview.error=QStringLiteral("CAD kernel error: %1").arg(e.what());}
     catch(const std::exception& e){preview.error=QString::fromUtf8(e.what());}
    return preview;
}

bool SceneDocument::commitTSlotGroove(const TSlotGrooveCommand& command,const TopoDS_Shape& preview,QString* error){
    const Part* part=find(command.partId);
    if(!part){setError(error,"The selected body no longer exists.");return false;}
    if(part->geometryToken!=command.geometryToken){setError(error,"The T-slot preview is stale because the body geometry changed. Select the body again.");return false;}
    if(!validShape(preview,error))return false;
    checkpoint();
    for(Part& target:parts_)if(target.id==command.partId){
        target.shape=preview;
        target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);
        break;
    }
    return true;
}

bool SceneDocument::prepareRoundedSlot(const QString& id,double centerX,double centerY,double entryZ,
                                        double width,double length,double depth,double angleDegrees,
                                        RoundedSlotCommand* command,QString* error) const {
    if(!command){setError(error,"The rounded-slot command output is missing.");return false;}
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||
       !finitePositive(width)||!finitePositive(length)||length<width||!finitePositive(depth)||
       !std::isfinite(angleDegrees)||angleDegrees < -360.0||angleDegrees > 360.0||
       std::abs(centerX)+length/2.0>1e6||std::abs(centerY)+length/2.0>1e6||
       std::abs(entryZ-depth)>1e6){
        setError(error,"Rounded-slot dimensions are invalid. Length must be at least the slot width.");return false;
    }
    const Part* part=find(id);
    if(!part){setError(error,"Object not found.");return false;}
    command->partId=id;command->geometryToken=part->geometryToken;
    command->sourceShape=BRepBuilderAPI_Copy(part->shape).Shape();
    command->centerX=centerX;command->centerY=centerY;command->entryZ=entryZ;
    command->width=width;command->length=length;command->depth=depth;command->angleDegrees=angleDegrees;
    return true;
}

RoundedSlotPreview SceneDocument::buildRoundedSlotPreview(const RoundedSlotCommand& command){
    RoundedSlotPreview preview;
    try{
        if(command.partId.isEmpty()||command.sourceShape.IsNull()||!finiteOffset(command.centerX)||
           !finiteOffset(command.centerY)||!finiteOffset(command.entryZ)||!finitePositive(command.width)||
           !finitePositive(command.length)||command.length<command.width||!finitePositive(command.depth)||
           !std::isfinite(command.angleDegrees)||command.angleDegrees < -360.0||command.angleDegrees > 360.0){
            preview.error="The rounded-slot command is incomplete or invalid.";return preview;
        }
        const double radius=command.width/2.0;
        const double endOffset=(command.length-command.width)/2.0;
        const double margin=std::max(0.01,command.depth*1e-4);
        const double bottomZ=command.entryZ-command.depth;
        TopoDS_Shape cutter;
        if(endOffset>1e-8){
            cutter=BRepPrimAPI_MakeBox(gp_Pnt(command.centerX-radius,command.centerY-endOffset,bottomZ),
                                       command.width,2.0*endOffset,command.depth+margin).Shape();
        }
        const TopoDS_Shape firstEnd=BRepPrimAPI_MakeCylinder(
            gp_Ax2(gp_Pnt(command.centerX,command.centerY-endOffset,bottomZ),gp_Dir(0,0,1)),
            radius,command.depth+margin).Shape();
        if(endOffset<=1e-8)cutter=firstEnd;
        else {
            const TopoDS_Shape secondEnd=BRepPrimAPI_MakeCylinder(
                gp_Ax2(gp_Pnt(command.centerX,command.centerY+endOffset,bottomZ),gp_Dir(0,0,1)),
                radius,command.depth+margin).Shape();
            BRepAlgoAPI_Fuse firstJoin(cutter,firstEnd);firstJoin.Build();
            if(!firstJoin.IsDone()){preview.error="The rounded-slot cutter could not be joined.";return preview;}
            BRepAlgoAPI_Fuse fullJoin(firstJoin.Shape(),secondEnd);fullJoin.Build();
            if(!fullJoin.IsDone()){preview.error="The rounded-slot cutter could not be joined.";return preview;}
            cutter=fullJoin.Shape();
        }
        gp_Trsf rotation;
        rotation.SetRotation(gp_Ax1(gp_Pnt(command.centerX,command.centerY,0),gp_Dir(0,0,1)),
                             command.angleDegrees*std::numbers::pi/180.0);
        cutter=BRepBuilderAPI_Transform(cutter,rotation,true).Shape();
        BRepAlgoAPI_Cut operation(command.sourceShape,cutter);operation.Build();
        if(!operation.IsDone()){preview.error="The kernel could not cut the requested rounded slot.";return preview;}
        preview.shape=operation.Shape();
        if(!validShape(preview.shape,&preview.error)){preview.shape=TopoDS_Shape();return preview;}
        GProp_GProps before,after;
        BRepGProp::VolumeProperties(command.sourceShape,before);BRepGProp::VolumeProperties(preview.shape,after);
        const double removed=before.Mass()-after.Mass();
        if(!std::isfinite(removed)||removed<=std::max(1e-9,before.Mass()*1e-12)){
            preview.shape=TopoDS_Shape();preview.error="The rounded-slot cutter does not intersect the selected body.";
        }
    }catch(const Standard_Failure& e){preview.error=QStringLiteral("CAD kernel error: %1").arg(e.what());}
     catch(const std::exception& e){preview.error=QString::fromUtf8(e.what());}
    return preview;
}

bool SceneDocument::commitRoundedSlot(const RoundedSlotCommand& command,const TopoDS_Shape& preview,QString* error){
    const Part* part=find(command.partId);
    if(!part){setError(error,"The selected body no longer exists.");return false;}
    if(part->geometryToken!=command.geometryToken){setError(error,"The rounded-slot preview is stale because the body geometry changed. Select the body again.");return false;}
    if(!validShape(preview,error))return false;
    checkpoint();
    for(Part& target:parts_)if(target.id==command.partId){
        target.shape=preview;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;
    }
    return true;
}

bool SceneDocument::prepareKeyholeSlot(const QString& id,double centerX,double centerY,double entryZ,
                                        double headDiameter,double throatWidth,double throatLength,double depth,
                                        double angleDegrees,KeyholeSlotCommand* command,QString* error) const {
    if(!command){setError(error,"The keyhole-slot command output is missing.");return false;}
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||
       !finitePositive(headDiameter)||!finitePositive(throatWidth)||throatWidth>=headDiameter||
       !finitePositive(throatLength)||!finitePositive(depth)||!std::isfinite(angleDegrees)||
       angleDegrees < -360.0||angleDegrees > 360.0||
       std::abs(centerX)+headDiameter/2.0+throatLength>1e6||
       std::abs(centerY)+headDiameter/2.0+throatLength>1e6||std::abs(entryZ-depth)>1e6){
        setError(error,"Keyhole-slot dimensions are invalid. The throat must be narrower than the round head.");return false;
    }
    const Part* part=find(id);
    if(!part){setError(error,"Object not found.");return false;}
    command->partId=id;command->geometryToken=part->geometryToken;
    command->sourceShape=BRepBuilderAPI_Copy(part->shape).Shape();
    command->centerX=centerX;command->centerY=centerY;command->entryZ=entryZ;
    command->headDiameter=headDiameter;command->throatWidth=throatWidth;
    command->throatLength=throatLength;command->depth=depth;command->angleDegrees=angleDegrees;
    return true;
}

KeyholeSlotPreview SceneDocument::buildKeyholeSlotPreview(const KeyholeSlotCommand& command){
    KeyholeSlotPreview preview;
    try{
        if(command.partId.isEmpty()||command.sourceShape.IsNull()||!finiteOffset(command.centerX)||
           !finiteOffset(command.centerY)||!finiteOffset(command.entryZ)||!finitePositive(command.headDiameter)||
           !finitePositive(command.throatWidth)||command.throatWidth>=command.headDiameter||
           !finitePositive(command.throatLength)||!finitePositive(command.depth)||
           !std::isfinite(command.angleDegrees)||command.angleDegrees < -360.0||command.angleDegrees > 360.0){
            preview.error="The keyhole-slot command is incomplete or invalid.";return preview;
        }
        const double margin=std::max(0.01,command.depth*1e-4);
        const double bottomZ=command.entryZ-command.depth;
        const double height=command.depth+margin;
        const TopoDS_Shape head=BRepPrimAPI_MakeCylinder(
            gp_Ax2(gp_Pnt(command.centerX,command.centerY,bottomZ),gp_Dir(0,0,1)),
            command.headDiameter/2.0,height).Shape();
        const TopoDS_Shape throat=BRepPrimAPI_MakeBox(
            gp_Pnt(command.centerX-command.throatWidth/2.0,command.centerY,bottomZ),
            command.throatWidth,command.throatLength,height).Shape();
        BRepAlgoAPI_Fuse cutterJoin(head,throat);cutterJoin.Build();
        if(!cutterJoin.IsDone()){preview.error="The keyhole-slot cutter could not be joined.";return preview;}
        gp_Trsf rotation;
        rotation.SetRotation(gp_Ax1(gp_Pnt(command.centerX,command.centerY,0),gp_Dir(0,0,1)),
                             command.angleDegrees*std::numbers::pi/180.0);
        const TopoDS_Shape cutter=BRepBuilderAPI_Transform(cutterJoin.Shape(),rotation,true).Shape();
        BRepAlgoAPI_Cut operation(command.sourceShape,cutter);operation.Build();
        if(!operation.IsDone()){preview.error="The kernel could not cut the requested keyhole slot.";return preview;}
        preview.shape=operation.Shape();
        if(!validShape(preview.shape,&preview.error)){preview.shape=TopoDS_Shape();return preview;}
        GProp_GProps before,after;
        BRepGProp::VolumeProperties(command.sourceShape,before);BRepGProp::VolumeProperties(preview.shape,after);
        const double removed=before.Mass()-after.Mass();
        if(!std::isfinite(removed)||removed<=std::max(1e-9,before.Mass()*1e-12)){
            preview.shape=TopoDS_Shape();preview.error="The keyhole-slot cutter does not intersect the selected body.";
        }
    }catch(const Standard_Failure& e){preview.error=QStringLiteral("CAD kernel error: %1").arg(e.what());}
     catch(const std::exception& e){preview.error=QString::fromUtf8(e.what());}
    return preview;
}

bool SceneDocument::commitKeyholeSlot(const KeyholeSlotCommand& command,const TopoDS_Shape& preview,QString* error){
    const Part* part=find(command.partId);
    if(!part){setError(error,"The selected body no longer exists.");return false;}
    if(part->geometryToken!=command.geometryToken){setError(error,"The keyhole-slot preview is stale because the body geometry changed. Select the body again.");return false;}
    if(!validShape(preview,error))return false;
    checkpoint();
    for(Part& target:parts_)if(target.id==command.partId){
        target.shape=preview;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;
    }
    return true;
}

bool SceneDocument::prepareRoundedSlotPattern(const QString& id,double centerX,double centerY,double entryZ,
                                               double width,double length,double depth,double angleDegrees,
                                               int instanceCount,double spacingX,double spacingY,
                                               RoundedSlotPatternCommand* command,QString* error) const {
    if(!command){setError(error,"The rounded-slot pattern command output is missing.");return false;}
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||!finitePositive(width)||
       !finitePositive(length)||length<width||!finitePositive(depth)||!std::isfinite(angleDegrees)||
       angleDegrees < -360.0||angleDegrees > 360.0||instanceCount<2||instanceCount>100||
       !std::isfinite(spacingX)||!std::isfinite(spacingY)||
       (std::abs(spacingX)<1e-5&&std::abs(spacingY)<1e-5)||
       std::max(std::abs(spacingX),std::abs(spacingY))*(instanceCount-1)>1e6||
       std::abs(centerX)+std::abs(spacingX)*(instanceCount-1)+length/2.0>1e6||
       std::abs(centerY)+std::abs(spacingY)*(instanceCount-1)+length/2.0>1e6||
       std::abs(entryZ-depth)>1e6){
        setError(error,"Rounded-slot pattern dimensions are invalid. Use 2 to 100 instances and nonzero spacing.");return false;
    }
    const Part* part=find(id);
    if(!part){setError(error,"Object not found.");return false;}
    command->partId=id;command->geometryToken=part->geometryToken;
    command->sourceShape=BRepBuilderAPI_Copy(part->shape).Shape();
    command->centerX=centerX;command->centerY=centerY;command->entryZ=entryZ;
    command->width=width;command->length=length;command->depth=depth;command->angleDegrees=angleDegrees;
    command->instanceCount=instanceCount;command->spacingX=spacingX;command->spacingY=spacingY;
    return true;
}

RoundedSlotPatternPreview SceneDocument::buildRoundedSlotPatternPreview(const RoundedSlotPatternCommand& command){
    RoundedSlotPatternPreview preview;
    if(command.sourceShape.IsNull()||command.partId.isEmpty()||command.instanceCount<2||command.instanceCount>100||
       !std::isfinite(command.spacingX)||!std::isfinite(command.spacingY)||
       (std::abs(command.spacingX)<1e-5&&std::abs(command.spacingY)<1e-5)){
        preview.error="The rounded-slot pattern command is incomplete or invalid.";return preview;
    }
    TopoDS_Shape working=command.sourceShape;
    GProp_GProps before,after;BRepGProp::VolumeProperties(command.sourceShape,before);
    for(int i=0;i<command.instanceCount;++i){
        RoundedSlotCommand slot;
        slot.partId=command.partId;slot.geometryToken=command.geometryToken;slot.sourceShape=working;
        slot.centerX=command.centerX+command.spacingX*i;
        slot.centerY=command.centerY+command.spacingY*i;
        slot.entryZ=command.entryZ;slot.width=command.width;slot.length=command.length;
        slot.depth=command.depth;slot.angleDegrees=command.angleDegrees;
        const RoundedSlotPreview slotPreview=buildRoundedSlotPreview(slot);
        if(slotPreview.shape.IsNull()){
            preview.error=QStringLiteral("Rounded slot %1 failed: %2").arg(i+1).arg(slotPreview.error);return preview;
        }
        working=slotPreview.shape;
    }
    if(!validShape(working,&preview.error))return preview;
    BRepGProp::VolumeProperties(working,after);
    const double removed=before.Mass()-after.Mass();
    if(!std::isfinite(removed)||removed<=std::max(1e-9,before.Mass()*1e-12)){
        preview.error="The rounded-slot pattern did not remove material.";return preview;
    }
    preview.shape=working;
    return preview;
}

bool SceneDocument::commitRoundedSlotPattern(const RoundedSlotPatternCommand& command,const TopoDS_Shape& preview,QString* error){
    const Part* part=find(command.partId);
    if(!part){setError(error,"The selected body no longer exists.");return false;}
    if(part->geometryToken!=command.geometryToken){setError(error,"The rounded-slot pattern preview is stale because the body geometry changed. Select the body again.");return false;}
    if(!validShape(preview,error))return false;
    checkpoint();
    for(Part& target:parts_)if(target.id==command.partId){
        target.shape=preview;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;
    }
    return true;
}

bool SceneDocument::holePattern(const QString& id,double x,double y,double entryZ,double diameter,double depth,bool throughAll,
                                int countX,double spacingX,int countY,double spacingY,QString* error) {
    if(countX<1||countY<1||countX>1000||countY>1000||countX>1000/countY||countX*countY<2){
        setError(error,"Hole grid must contain between 2 and 1,000 instances.");return false;
    }
    auto spacingValid=[](int count,double spacing){return std::isfinite(spacing)&&std::abs(spacing)<=1e6&&
        (count==1||(std::abs(spacing)>=1e-5&&std::abs(spacing)*(count-1)<=1e6));};
    if(!finiteOffset(x)||!finiteOffset(y)||!finiteOffset(entryZ)||!finitePositive(diameter)||
       (!throughAll&&!finitePositive(depth))||!spacingValid(countX,spacingX)||!spacingValid(countY,spacingY)){
        setError(error,"Hole-grid dimensions, location, or spacing are invalid.");return false;
    }
    const Part* source=find(id);if(!source){setError(error,"Object not found.");return false;}
    const QString sourceToken=source->geometryToken;
    TopoDS_Shape working=BRepBuilderAPI_Copy(source->shape).Shape();
    for(int ix=0;ix<countX;++ix)for(int iy=0;iy<countY;++iy){
        HoleCommand command;command.partId=id;command.geometryToken=sourceToken;command.sourceShape=working;
        command.x=x+spacingX*ix;command.y=y+spacingY*iy;command.entryZ=entryZ;
        command.diameter=diameter;command.depth=depth;command.throughAll=throughAll;
        const HolePreview preview=buildHolePreview(command);
        if(preview.shape.IsNull()){setError(error,QStringLiteral("Hole at grid position %1, %2 failed: %3").arg(ix+1).arg(iy+1).arg(preview.error));return false;}
        working=preview.shape;
    }
    if(!validShape(working,error))return false;
    checkpoint();
    for(Part& target:parts_)if(target.id==id){target.shape=working;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
    return true;
}

bool SceneDocument::circularHolePattern(const QString& id,double centerX,double centerY,double entryZ,double boltCircleRadius,
                                       double diameter,double depth,bool throughAll,int instanceCount,
                                       double totalAngleDegrees,QString* error) {
    if(instanceCount<2||instanceCount>1000){setError(error,"Circular hole pattern count must be between 2 and 1,000.");return false;}
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||!finitePositive(boltCircleRadius)||
       std::abs(centerX)+boltCircleRadius>1e6||std::abs(centerY)+boltCircleRadius>1e6||!finitePositive(diameter)||
       (!throughAll&&!finitePositive(depth))||!std::isfinite(totalAngleDegrees)||totalAngleDegrees<1||totalAngleDegrees>360){
        setError(error,"Circular hole pattern location, dimensions, or angle are invalid.");return false;
    }
    const Part* source=find(id);
    if(!source){setError(error,"Object not found.");return false;}
    const QString geometryToken=source->geometryToken;
    TopoDS_Shape working=BRepBuilderAPI_Copy(source->shape).Shape();
    const bool fullCircle=std::abs(totalAngleDegrees-360.0)<1e-9;
    const double denominator=fullCircle?instanceCount:instanceCount-1;
    for(int index=0;index<instanceCount;++index){
        const double angle=totalAngleDegrees*index/denominator*std::numbers::pi/180.0;
        HoleCommand command;command.partId=id;command.geometryToken=geometryToken;command.sourceShape=working;
        command.x=centerX+boltCircleRadius*std::cos(angle);command.y=centerY+boltCircleRadius*std::sin(angle);
        command.entryZ=entryZ;command.diameter=diameter;command.depth=depth;command.throughAll=throughAll;
        const HolePreview preview=buildHolePreview(command);
        if(preview.shape.IsNull()){
            setError(error,QStringLiteral("Hole %1 of %2 failed: %3").arg(index+1).arg(instanceCount).arg(preview.error));return false;
        }
        working=preview.shape;
    }
    if(!validShape(working,error))return false;
    checkpoint();
    for(Part& target:parts_)if(target.id==id){target.shape=working;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
    return true;
}

bool SceneDocument::circularCountersinkPattern(const QString& id,double centerX,double centerY,double entryZ,double boltCircleRadius,
                                               double diameter,double depth,bool throughAll,double headDiameter,
                                               double includedAngleDegrees,int instanceCount,double totalAngleDegrees,
                                               QString* error){
    if(instanceCount<2||instanceCount>1000){setError(error,"Circular countersink pattern count must be between 2 and 1,000.");return false;}
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||!finitePositive(boltCircleRadius)||
       std::abs(centerX)+boltCircleRadius>1e6||std::abs(centerY)+boltCircleRadius>1e6||!finitePositive(diameter)||
       !finitePositive(headDiameter)||headDiameter<=diameter||(!throughAll&&!finitePositive(depth))||
       !std::isfinite(includedAngleDegrees)||includedAngleDegrees<=1||includedAngleDegrees>=179||
       !std::isfinite(totalAngleDegrees)||totalAngleDegrees<1||totalAngleDegrees>360){
        setError(error,"Circular countersink pattern location, dimensions, angle, or count are invalid.");return false;
    }
    const Part* source=find(id);if(!source){setError(error,"Object not found.");return false;}
    const QString geometryToken=source->geometryToken;
    TopoDS_Shape working=BRepBuilderAPI_Copy(source->shape).Shape();
    const bool fullCircle=std::abs(totalAngleDegrees-360.0)<1e-9;
    const double denominator=fullCircle?instanceCount:instanceCount-1;
    for(int index=0;index<instanceCount;++index){
        const double angle=totalAngleDegrees*index/denominator*std::numbers::pi/180.0;
        HoleCommand command;command.partId=id;command.geometryToken=geometryToken;command.sourceShape=working;
        command.x=centerX+boltCircleRadius*std::cos(angle);command.y=centerY+boltCircleRadius*std::sin(angle);
        command.entryZ=entryZ;command.diameter=diameter;command.depth=depth;command.throughAll=throughAll;
        command.style=HoleStyle::Countersink;command.headDiameter=headDiameter;command.sinkAngleDegrees=includedAngleDegrees;
        const HolePreview preview=buildHolePreview(command);
        if(preview.shape.IsNull()){
            setError(error,QStringLiteral("Countersink %1 of %2 failed: %3").arg(index+1).arg(instanceCount).arg(preview.error));return false;
        }
        working=preview.shape;
    }
    if(!validShape(working,error))return false;
    checkpoint();
    for(Part& target:parts_)if(target.id==id){target.shape=working;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
    return true;
}

bool SceneDocument::circularCounterborePattern(const QString& id,double centerX,double centerY,double entryZ,double boltCircleRadius,
                                               double diameter,double depth,bool throughAll,double headDiameter,double headDepth,
                                               int instanceCount,double totalAngleDegrees,QString* error){
    if(instanceCount<2||instanceCount>1000){setError(error,"Circular counterbore pattern count must be between 2 and 1,000.");return false;}
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||!finitePositive(boltCircleRadius)||
       std::abs(centerX)+boltCircleRadius>1e6||std::abs(centerY)+boltCircleRadius>1e6||!finitePositive(diameter)||
       !finitePositive(headDiameter)||headDiameter<=diameter||!finitePositive(headDepth)||
       (!throughAll&&(!finitePositive(depth)||headDepth>depth))||!std::isfinite(totalAngleDegrees)||
       totalAngleDegrees<1||totalAngleDegrees>360){
        setError(error,"Circular counterbore pattern location, dimensions, depth, angle, or count are invalid.");return false;
    }
    const Part* source=find(id);if(!source){setError(error,"Object not found.");return false;}
    const QString geometryToken=source->geometryToken;
    TopoDS_Shape working=BRepBuilderAPI_Copy(source->shape).Shape();
    const bool fullCircle=std::abs(totalAngleDegrees-360.0)<1e-9;
    const double denominator=fullCircle?instanceCount:instanceCount-1;
    for(int index=0;index<instanceCount;++index){
        const double angle=totalAngleDegrees*index/denominator*std::numbers::pi/180.0;
        HoleCommand command;command.partId=id;command.geometryToken=geometryToken;command.sourceShape=working;
        command.x=centerX+boltCircleRadius*std::cos(angle);command.y=centerY+boltCircleRadius*std::sin(angle);
        command.entryZ=entryZ;command.diameter=diameter;command.depth=depth;command.throughAll=throughAll;
        command.style=HoleStyle::Counterbore;command.headDiameter=headDiameter;command.headDepth=headDepth;
        const HolePreview preview=buildHolePreview(command);
        if(preview.shape.IsNull()){
            setError(error,QStringLiteral("Counterbore %1 of %2 failed: %3").arg(index+1).arg(instanceCount).arg(preview.error));return false;
        }
        working=preview.shape;
    }
    if(!validShape(working,error))return false;
    checkpoint();
    for(Part& target:parts_)if(target.id==id){target.shape=working;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
    return true;
}

bool SceneDocument::linearCounterborePattern(const QString& id,double centerX,double centerY,double entryZ,double diameter,
                                             double depth,bool throughAll,double headDiameter,double headDepth,
                                             int instanceCount,double spacingX,double spacingY,
                                             QString* error){
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||!finitePositive(diameter)||
       !finitePositive(headDiameter)||headDiameter<=diameter||!finitePositive(headDepth)||
       (!throughAll&&(!finitePositive(depth)||headDepth>depth))||instanceCount<2||instanceCount>100||
       !std::isfinite(spacingX)||!std::isfinite(spacingY)||(std::abs(spacingX)<1e-5&&std::abs(spacingY)<1e-5)||
       std::max(std::abs(spacingX),std::abs(spacingY))*(instanceCount-1)>1e6||
       std::abs(centerX)+std::abs(spacingX)*(instanceCount-1)+headDiameter>1e6||
       std::abs(centerY)+std::abs(spacingY)*(instanceCount-1)+headDiameter>1e6||std::abs(entryZ-depth)>1e6){
        setError(error,"Counterbore pattern dimensions are invalid. Use 2 to 100 instances and nonzero spacing.");return false;
    }
    const Part* source=find(id);if(!source){setError(error,"Object not found.");return false;}
    const QString token=source->geometryToken;TopoDS_Shape working=BRepBuilderAPI_Copy(source->shape).Shape();
    for(int index=0;index<instanceCount;++index){
        HoleCommand command;command.partId=id;command.geometryToken=token;command.sourceShape=working;
        command.x=centerX+spacingX*index;command.y=centerY+spacingY*index;command.entryZ=entryZ;
        command.diameter=diameter;command.depth=depth;command.throughAll=throughAll;
        command.style=HoleStyle::Counterbore;command.headDiameter=headDiameter;command.headDepth=headDepth;
        const HolePreview preview=buildHolePreview(command);
        if(preview.shape.IsNull()){
            setError(error,QStringLiteral("Counterbore %1 of %2 failed: %3").arg(index+1).arg(instanceCount).arg(preview.error));return false;
        }
        working=preview.shape;
    }
    if(!validShape(working,error))return false;
    checkpoint();
    for(Part& target:parts_)if(target.id==id){target.shape=working;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
    return true;
}

bool SceneDocument::linearCountersinkPattern(const QString& id,double centerX,double centerY,double entryZ,double diameter,
                                             double depth,bool throughAll,double headDiameter,double includedAngleDegrees,
                                             int instanceCount,double spacingX,double spacingY,QString* error){
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||!finitePositive(diameter)||
       !finitePositive(headDiameter)||headDiameter<=diameter||(!throughAll&&!finitePositive(depth))||
       !std::isfinite(includedAngleDegrees)||includedAngleDegrees<=1||includedAngleDegrees>=179||
       instanceCount<2||instanceCount>100||!std::isfinite(spacingX)||!std::isfinite(spacingY)||
       (std::abs(spacingX)<1e-5&&std::abs(spacingY)<1e-5)||
       std::max(std::abs(spacingX),std::abs(spacingY))*(instanceCount-1)>1e6||
       std::abs(centerX)+std::abs(spacingX)*(instanceCount-1)+headDiameter>1e6||
       std::abs(centerY)+std::abs(spacingY)*(instanceCount-1)+headDiameter>1e6||std::abs(entryZ-depth)>1e6){
        setError(error,"Countersink pattern dimensions are invalid. Use 2 to 100 instances and nonzero spacing.");return false;
    }
    const Part* source=find(id);if(!source){setError(error,"Object not found.");return false;}
    const QString token=source->geometryToken;TopoDS_Shape working=BRepBuilderAPI_Copy(source->shape).Shape();
    for(int index=0;index<instanceCount;++index){
        HoleCommand command;command.partId=id;command.geometryToken=token;command.sourceShape=working;
        command.x=centerX+spacingX*index;command.y=centerY+spacingY*index;command.entryZ=entryZ;
        command.diameter=diameter;command.depth=depth;command.throughAll=throughAll;
        command.style=HoleStyle::Countersink;command.headDiameter=headDiameter;command.sinkAngleDegrees=includedAngleDegrees;
        const HolePreview preview=buildHolePreview(command);
        if(preview.shape.IsNull()){
            setError(error,QStringLiteral("Countersink %1 of %2 failed: %3").arg(index+1).arg(instanceCount).arg(preview.error));return false;
        }
        working=preview.shape;
    }
    if(!validShape(working,error))return false;
    checkpoint();
    for(Part& target:parts_)if(target.id==id){target.shape=working;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
    return true;
}

bool SceneDocument::linearKeyholeSlotPattern(const QString& id,double centerX,double centerY,double entryZ,double headDiameter,
                                              double throatWidth,double throatLength,double depth,double angleDegrees,
                                              int instanceCount,double spacingX,double spacingY,QString* error){
    if(!finiteOffset(centerX)||!finiteOffset(centerY)||!finiteOffset(entryZ)||!finitePositive(headDiameter)||
       !finitePositive(throatWidth)||throatWidth>=headDiameter||!finitePositive(throatLength)||!finitePositive(depth)||
       !std::isfinite(angleDegrees)||angleDegrees < -360||angleDegrees > 360||instanceCount<2||instanceCount>100||
       !std::isfinite(spacingX)||!std::isfinite(spacingY)||(std::abs(spacingX)<1e-5&&std::abs(spacingY)<1e-5)||
       std::max(std::abs(spacingX),std::abs(spacingY))*(instanceCount-1)>1e6||
       std::abs(centerX)+std::abs(spacingX)*(instanceCount-1)+throatLength+headDiameter>1e6||
       std::abs(centerY)+std::abs(spacingY)*(instanceCount-1)+throatLength+headDiameter>1e6||std::abs(entryZ-depth)>1e6){
        setError(error,"Keyhole-slot pattern dimensions are invalid. Use 2 to 100 instances and nonzero spacing.");return false;
    }
    const Part* source=find(id);if(!source){setError(error,"Object not found.");return false;}
    const QString token=source->geometryToken;TopoDS_Shape working=BRepBuilderAPI_Copy(source->shape).Shape();
    for(int index=0;index<instanceCount;++index){
        KeyholeSlotCommand command;command.partId=id;command.geometryToken=token;command.sourceShape=working;
        command.centerX=centerX+spacingX*index;command.centerY=centerY+spacingY*index;command.entryZ=entryZ;
        command.headDiameter=headDiameter;command.throatWidth=throatWidth;command.throatLength=throatLength;
        command.depth=depth;command.angleDegrees=angleDegrees;
        const KeyholeSlotPreview preview=buildKeyholeSlotPreview(command);
        if(preview.shape.IsNull()){
            setError(error,QStringLiteral("Keyhole slot %1 failed: %2").arg(index+1).arg(preview.error));return false;
        }
        working=preview.shape;
    }
    if(!validShape(working,error))return false;
    checkpoint();
    for(Part& target:parts_)if(target.id==id){target.shape=working;target.geometryToken=QUuid::createUuid().toString(QUuid::WithoutBraces);break;}
    return true;
}

bool SceneDocument::circularPattern(const QString& id, Axis axis, double centerU, double centerV,
                                   int instanceCount, double totalAngleDegrees, QString* error) {
    if (instanceCount<2 || instanceCount>1000) {
        setError(error,"Circular pattern count must be between 2 and 1,000 total instances."); return false;
    }
    if (!finiteOffset(centerU) || !finiteOffset(centerV)) { setError(error,"Pattern center is out of range."); return false; }
    if (!std::isfinite(totalAngleDegrees) || totalAngleDegrees<1.0 || totalAngleDegrees>360.0) {
        setError(error,"Circular pattern angle must be between 1 and 360 degrees."); return false;
    }
    if (parts_.size()+instanceCount-1>kMaxObjects) { setError(error,"Scene object limit reached."); return false; }
    const Part* source=find(id);
    if (!source) { setError(error,"Object not found."); return false; }
    const TopoDS_Shape sourceShape=source->shape;
    const QString sourceName=source->name;
    const bool visible=source->visible;
    const QString color=source->color;
    gp_Pnt center(0,0,0);
    gp_Dir direction(0,0,1);
    if (axis==Axis::X) { center.SetY(centerU); center.SetZ(centerV); direction=gp_Dir(1,0,0); }
    else if (axis==Axis::Y) { center.SetX(centerU); center.SetZ(centerV); direction=gp_Dir(0,1,0); }
    else { center.SetX(centerU); center.SetY(centerV); direction=gp_Dir(0,0,1); }
    const bool fullCircle=std::abs(totalAngleDegrees-360.0)<1e-9;
    const double denominator=fullCircle ? instanceCount : instanceCount-1;
    QVector<Part> copies;
    QSet<QString> reservedNames;
    try {
        for (int index=1;index<instanceCount;++index) {
            const double angle=totalAngleDegrees*index/denominator*std::numbers::pi/180.0;
            gp_Trsf transform;
            transform.SetRotation(gp_Ax1(center,direction),angle);
            const TopoDS_Shape result=BRepBuilderAPI_Transform(sourceShape,transform,true).Shape();
            if (!validShape(result,error)) return false;
            const QString baseName=sourceName+QStringLiteral(" Circular Pattern %1").arg(index+1);
            QString name=uniqueName(baseName);
            int suffix=2;
            while (reservedNames.contains(name.toCaseFolded())) name=uniqueName(baseName+QStringLiteral(" %1").arg(suffix++));
            reservedNames.insert(name.toCaseFolded());
            copies.append({generatedId(),name,result,visible,color});
        }
        checkpoint();
        for (const Part& copy:copies) parts_.append(copy);
        return true;
    } catch (const Standard_Failure& e) { setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error,QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::circularPatternAboutAxis(const QString& id,double originX,double originY,double originZ,
                                             double axisX,double axisY,double axisZ,int instanceCount,
                                             double totalAngleDegrees,QString* error) {
    if(instanceCount<2||instanceCount>1000){setError(error,"Circular pattern count must be between 2 and 1,000.");return false;}
    const double axisMagnitude=std::hypot(axisX,axisY,axisZ);
    if(!finiteOffset(originX)||!finiteOffset(originY)||!finiteOffset(originZ)||
       !std::isfinite(axisX)||!std::isfinite(axisY)||!std::isfinite(axisZ)||
       !std::isfinite(axisMagnitude)||axisMagnitude<1e-8||axisMagnitude>1e6){
        setError(error,"Enter a finite axis origin and a finite non-zero axis direction.");return false;
    }
    if(!std::isfinite(totalAngleDegrees)||totalAngleDegrees<1||totalAngleDegrees>360){
        setError(error,"Circular pattern angle must be between 1 and 360 degrees.");return false;
    }
    if(parts_.size()+instanceCount-1>kMaxObjects){setError(error,"Scene object limit reached.");return false;}
    const Part* source=find(id);if(!source){setError(error,"Object not found.");return false;}
    const TopoDS_Shape sourceShape=source->shape;const QString sourceName=source->name;
    const bool visible=source->visible;const QString color=source->color;
    const gp_Ax1 axis(gp_Pnt(originX,originY,originZ),gp_Dir(axisX,axisY,axisZ));
    const bool fullCircle=std::abs(totalAngleDegrees-360.0)<1e-9;
    const double denominator=fullCircle?instanceCount:instanceCount-1;
    QVector<Part> copies;QSet<QString> reservedNames;
    try{
        for(int index=1;index<instanceCount;++index){
            gp_Trsf transform;
            transform.SetRotation(axis,totalAngleDegrees*index/denominator*std::numbers::pi/180.0);
            const TopoDS_Shape result=BRepBuilderAPI_Transform(sourceShape,transform,true).Shape();
            if(!validShape(result,error))return false;
            const QString baseName=sourceName+QStringLiteral(" Axis Pattern %1").arg(index+1);
            QString name=uniqueName(baseName);int suffix=2;
            while(reservedNames.contains(name.toCaseFolded()))name=uniqueName(baseName+QStringLiteral(" %1").arg(suffix++));
            reservedNames.insert(name.toCaseFolded());copies.append({generatedId(),name,result,visible,color});
        }
        checkpoint();for(const Part& copy:copies)parts_.append(copy);return true;
    }catch(const Standard_Failure& e){setError(error,QStringLiteral("CAD kernel error: %1").arg(e.what()));}
     catch(const std::exception& e){setError(error,QString::fromUtf8(e.what()));}
    return false;
}

bool SceneDocument::splitBody(const QString& id, Axis axis, double planeOffset, QString* error) {
    const Part* source=find(id);
    if (!source) { setError(error,"Object not found."); return false; }
    if (!finiteOffset(planeOffset)) { setError(error,"Split plane offset must be finite and within +/-1,000,000 mm."); return false; }
    if (axis!=Axis::X && axis!=Axis::Y && axis!=Axis::Z) { setError(error,"Choose X, Y, or Z as the split plane normal."); return false; }
    Bnd_Box bounds;
    BRepBndLib::Add(source->shape,bounds);
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;
    bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    const double low=axis==Axis::X ? xmin : axis==Axis::Y ? ymin : zmin;
    const double high=axis==Axis::X ? xmax : axis==Axis::Y ? ymax : zmax;
    if (planeOffset<=low+1e-8 || planeOffset>=high-1e-8) {
        setError(error,"The split plane must pass through the interior of the selected body."); return false;
    }
    const double margin=std::max({xmax-xmin,ymax-ymin,zmax-zmin,1.0})*2.0+1.0;
    const double ax0=xmin-margin, ay0=ymin-margin, az0=zmin-margin;
    const double ax1=xmax+margin, ay1=ymax+margin, az1=zmax+margin;
    double leftX=ax0,leftY=ay0,leftZ=az0,rightX=ax0,rightY=ay0,rightZ=az0;
    double leftX1=ax1,leftY1=ay1,leftZ1=az1,rightX1=ax1,rightY1=ay1,rightZ1=az1;
    if (axis==Axis::X) { leftX1=planeOffset; rightX=planeOffset; }
    else if (axis==Axis::Y) { leftY1=planeOffset; rightY=planeOffset; }
    else { leftZ1=planeOffset; rightZ=planeOffset; }
    try {
        BRepAlgoAPI_Common lower(source->shape,BRepPrimAPI_MakeBox(gp_Pnt(leftX,leftY,leftZ),gp_Pnt(leftX1,leftY1,leftZ1)).Shape());
        lower.Build();
        BRepAlgoAPI_Common upper(source->shape,BRepPrimAPI_MakeBox(gp_Pnt(rightX,rightY,rightZ),gp_Pnt(rightX1,rightY1,rightZ1)).Shape());
        upper.Build();
        if (!lower.IsDone() || !upper.IsDone()) { setError(error,"OCCT could not split the body at that plane."); return false; }
        TopoDS_Shape lowerShape=lower.Shape(),upperShape=upper.Shape();
        if (!validShape(lowerShape,error) || !validShape(upperShape,error)) return false;
        const Part sourceCopy=*source;
        Part second=sourceCopy;
        second.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        second.name=uniqueName(sourceCopy.name+QStringLiteral(" Split 2"));
        Part first=sourceCopy;
        first.shape=lowerShape;
        second.shape=upperShape;
        checkpoint();
        for (auto& part:parts_) if (part.id==id) { part=std::move(first); break; }
        parts_.append(std::move(second));
        return true;
    } catch (const Standard_Failure& failure) {
        setError(error,QStringLiteral("OCCT split failed: %1").arg(QString::fromUtf8(failure.what())));
        return false;
    }
}

bool SceneDocument::shellBody(const QString& id, Axis openingAxis, bool positiveSide,
                              double thickness, QString* error) {
    return shellBodyWithOpenings(id,{{openingAxis,positiveSide}},thickness,error);
}

bool SceneDocument::shellBodyWithOpenings(const QString& id,const QVector<ShellOpeningFace>& requestedOpenings,
                                          double thickness,QString* error) {
    const Part* source=find(id);
    if (!source) { setError(error,"Object not found."); return false; }
    if (requestedOpenings.isEmpty() || requestedOpenings.size()>6 || !finitePositive(thickness)) {
        setError(error,"Choose one to six unique planar opening faces and a positive shell thickness."); return false;
    }
    int requestedSideMask=0;
    for(const ShellOpeningFace& item:requestedOpenings){
        if(item.axis!=Axis::X&&item.axis!=Axis::Y&&item.axis!=Axis::Z){
            setError(error,"Choose a valid opening direction.");return false;
        }
        const int side=static_cast<int>(item.axis)*2+(item.positiveSide?1:0);
        const int sideBit=1<<side;
        if(requestedSideMask&sideBit){setError(error,"An opening face may only be selected once.");return false;}
        requestedSideMask|=sideBit;
    }
    Bnd_Box bounds;
    BRepBndLib::Add(source->shape,bounds);
    double xmin=0,ymin=0,zmin=0,xmax=0,ymax=0,zmax=0;
    bounds.Get(xmin,ymin,zmin,xmax,ymax,zmax);
    const double scale=std::max({xmax-xmin,ymax-ymin,zmax-zmin,1.0});
    QVector<TopoDS_Face> openings;
    for(int side=0;side<6;++side){
        if(!(requestedSideMask&(1<<side)))continue;
        const Axis axis=static_cast<Axis>(side/2);
        const bool positive=(side%2)==1;
        const double target=axis==Axis::X?(positive?xmax:xmin):
                            axis==Axis::Y?(positive?ymax:ymin):(positive?zmax:zmin);
        const gp_Dir requestedAxis=axis==Axis::X?gp_Dir(1,0,0):axis==Axis::Y?gp_Dir(0,1,0):gp_Dir(0,0,1);
        TopoDS_Face opening;
        double best=std::numeric_limits<double>::infinity();
        for(TopExp_Explorer explorer(source->shape,TopAbs_FACE);explorer.More();explorer.Next()){
            const TopoDS_Face face=TopoDS::Face(explorer.Current());
            BRepAdaptor_Surface surface(face,true);
            if(surface.GetType()!=GeomAbs_Plane)continue;
            const gp_Dir normal=surface.Plane().Axis().Direction();
            if(std::abs(normal.Dot(requestedAxis))<1.0-1e-7)continue;
            GProp_GProps properties;
            BRepGProp::SurfaceProperties(face,properties);
            const gp_Pnt center=properties.CentreOfMass();
            const double coordinate=axis==Axis::X?center.X():axis==Axis::Y?center.Y():center.Z();
            const double distance=std::abs(coordinate-target);
            if(distance<best&&distance<scale*1e-5+1e-7){best=distance;opening=face;}
        }
        if(opening.IsNull()){
            setError(error,QStringLiteral("No planar face was found on the %1%2 side of the selected body.")
                     .arg(positive?QStringLiteral("+"):QStringLiteral("-"))
                     .arg(axis==Axis::X?QStringLiteral("X"):axis==Axis::Y?QStringLiteral("Y"):QStringLiteral("Z")));
            return false;
        }
        openings.append(opening);
    }
    try {
        NCollection_List<TopoDS_Shape> openingList;
        for(const TopoDS_Face& opening:openings)openingList.Append(opening);
        BRepOffsetAPI_MakeThickSolid operation;
        operation.MakeThickSolidByJoin(source->shape,openingList,-thickness,1e-3,BRepOffset_Skin);
        if (!operation.IsDone()) { setError(error,"OCCT could not shell the body. Try a smaller thickness or another opening face."); return false; }
        TopoDS_Shape result=operation.Shape();
        if (!validShape(result,error)) return false;
        GProp_GProps oldVolume,newVolume;
        BRepGProp::VolumeProperties(source->shape,oldVolume);
        BRepGProp::VolumeProperties(result,newVolume);
        if (newVolume.Mass()>=oldVolume.Mass()-1e-9) { setError(error,"Shell did not remove material from the selected body."); return false; }
        checkpoint();
        for (auto& part:parts_) if (part.id==id) { part.shape=result; break; }
        return true;
    } catch (const Standard_Failure& failure) {
        setError(error,QStringLiteral("OCCT shell failed: %1").arg(QString::fromUtf8(failure.what())));
        return false;
    }
}

bool SceneDocument::rename(const QString& id, const QString& name, QString* error) {
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || trimmed.size() > 120 || trimmed.contains(QLatin1Char('\n'))) {
        setError(error, "Names must contain 1â€“120 characters on a single line."); return false;
    }
    for (const auto& part : parts_) if (part.id != id && part.name.compare(trimmed, Qt::CaseInsensitive) == 0) {
        setError(error, "Another body already uses this name."); return false;
    }
    for (const auto& part : parts_) if (part.id == id) {
        if (part.name == trimmed) return true;
        checkpoint();
        // A checkpoint can reallocate the vector, so never hold an element reference across it.
        for (auto& item : parts_) if (item.id == id) { item.name = trimmed; break; }
        return true;
    }
    setError(error, "Object not found."); return false;
}

bool SceneDocument::setVisibility(const QString& id, bool visible, QString* error) {
    for (const auto& part : parts_) if (part.id == id) {
        if (part.visible == visible) return true;
        checkpoint();
        for (auto& item : parts_) if (item.id == id) { item.visible = visible; break; }
        return true;
    }
    setError(error, "Object not found."); return false;
}

bool SceneDocument::setColor(const QString& id, const QString& color, QString* error) {
    if (!isHexColor(color)) { setError(error, "Color must be #RRGGBB."); return false; }
    for (const auto& part : parts_) if (part.id == id) {
        if (part.color.compare(color, Qt::CaseInsensitive) == 0) return true;
        checkpoint();
        for (auto& item : parts_) if (item.id == id) { item.color = color.toLower(); break; }
        return true;
    }
    setError(error, "Object not found."); return false;
}

bool SceneDocument::erase(const QString& id, QString* error) { return eraseMany({id}, error); }
bool SceneDocument::eraseMany(const QStringList& ids, QString* error) {
    QSet<QString> requested;
    for (const auto& id : ids) requested.insert(id);
    if (requested.isEmpty()) return true;
    for (const auto& id : requested) if (!find(id)) { setError(error, "One or more selected bodies no longer exist."); return false; }
    checkpoint();
    for (int i = parts_.size()-1; i >= 0; --i) if (requested.contains(parts_[i].id)) parts_.removeAt(i);
    return true;
}

void SceneDocument::clear() { if (!parts_.isEmpty()) { checkpoint(); parts_.clear(); } }
void SceneDocument::newDocument() {
    parts_.clear(); undo_.clear(); redo_.clear(); path_.clear();
    revision_ = 0; savedRevision_ = 0; nextRevision_ = 1;
}
bool SceneDocument::undo() {
    if (undo_.isEmpty()) return false;
    QVector<Part> snapshotParts = parts_;
    for (auto& part : snapshotParts) part.shape = BRepBuilderAPI_Copy(part.shape).Shape();
    redo_.append({std::move(snapshotParts),revision_});
    const Snapshot previous = undo_.takeLast();
    parts_ = previous.parts; revision_ = previous.revision;
    return true;
}
bool SceneDocument::redo() {
    if (redo_.isEmpty()) return false;
    QVector<Part> snapshotParts = parts_;
    for (auto& part : snapshotParts) part.shape = BRepBuilderAPI_Copy(part.shape).Shape();
    undo_.append({std::move(snapshotParts),revision_});
    if (undo_.size() > kHistoryDepth) undo_.removeFirst();
    const Snapshot next = redo_.takeLast();
    parts_ = next.parts; revision_ = next.revision;
    return true;
}

bool SceneDocument::saveAs(const QString& path, QString* error) {
    if (path.trimmed().isEmpty()) { setError(error, "Provide a file path."); return false; }
    try {
        QJsonArray array;
        for (const auto& part : parts_) {
            QJsonObject item;
            item.insert("id",part.id); item.insert("name",part.name);
            item.insert("brep",brepToText(part.shape));
            item.insert("visible",part.visible); item.insert("color",part.color);
            array.append(item);
        }
        QJsonObject root;
        root.insert("schema",kSchema2);
        root.insert("units","mm");
        root.insert("parts",array);
        const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
        if (bytes.size() > kMaxFileBytes) { setError(error, "Project exceeds native format size limit (96 MiB)."); return false; }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) { setError(error,file.errorString()); return false; }
        if (file.write(bytes) != bytes.size()) { setError(error,file.errorString()); file.cancelWriting(); return false; }
        if (!file.commit()) { setError(error,file.errorString()); return false; }
        path_ = path; savedRevision_ = revision_;
        return true;
    } catch (const Standard_Failure& e) { setError(error,QStringLiteral("Kernel save error: %1").arg(e.what())); }
      catch (const std::exception& e) { setError(error,QString::fromUtf8(e.what())); }
    return false;
}

bool SceneDocument::load(const QString& path, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { setError(error,file.errorString()); return false; }
    if (file.size() > kMaxFileBytes) { setError(error,"Project exceeds size limit (96 MiB)."); return false; }
    const QByteArray bytes = file.read(kMaxFileBytes + 1);
    if (bytes.size() > kMaxFileBytes) { setError(error,"Project exceeds size limit (96 MiB)."); return false; }
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(bytes,&parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) { setError(error,"Invalid project JSON."); return false; }
    const QJsonObject root = json.object();
    const QString schema = root.value("schema").toString();
    if ((schema != kSchema1 && schema != kSchema2) || root.value("units").toString() != "mm" || !root.value("parts").isArray()) {
        setError(error,"Unsupported project schema or units."); return false;
    }
    const QJsonArray array = root.value("parts").toArray();
    if (array.size() > kMaxObjects) { setError(error,"Project exceeds object limit."); return false; }
    QVector<Part> candidate;
    QSet<QString> ids;
    QSet<QString> names;
    try {
        for (const auto& value : array) {
            if (!value.isObject()) { setError(error,"Invalid part object."); return false; }
            const QJsonObject item = value.toObject();
            const QString id = item.value("id").toString();
            const QString name = item.value("name").toString();
            const QString payload = item.value("brep").toString();
            if (id.isEmpty() || name.trimmed().isEmpty() || name.size() > 120 || payload.isEmpty() ||
                ids.contains(id) || names.contains(name.toLower())) {
                setError(error,"Invalid or duplicate body metadata."); return false;
            }
            bool visible = true;
            QString color = "#6c9dc2";
            if (schema == kSchema2) {
                if (!item.value("visible").isBool() || !item.value("color").isString()) {
                    setError(error,"Invalid v2 appearance fields."); return false;
                }
                visible = item.value("visible").toBool(); color = item.value("color").toString();
                if (!isHexColor(color)) { setError(error,"Invalid body color."); return false; }
            }
            const TopoDS_Shape shape = brepFromText(payload);
            if (!validShape(shape,error)) return false;
            candidate.append({id,name,shape,visible,color});
            ids.insert(id); names.insert(name.toLower());
        }
    } catch (const Standard_Failure& e) { setError(error,QStringLiteral("Invalid B-rep: %1").arg(e.what())); return false; }
      catch (const std::exception& e) { setError(error,QString::fromUtf8(e.what())); return false; }
    // Transactional load: no part of the original document is touched until all validation succeeds.
    parts_ = std::move(candidate);
    undo_.clear(); redo_.clear(); path_ = path;
    revision_ = 0; savedRevision_ = 0; nextRevision_ = 1;
    return true;
}

bool SceneDocument::importSTEP(const QString& path, QString* error) {
    if (path.trimmed().isEmpty()) { setError(error,"Provide a STEP file path."); return false; }
    const QFileInfo fileInfo(path);
    if (!fileInfo.exists() || !fileInfo.isFile() || fileInfo.size() <= 0) {
        setError(error,"STEP file does not exist or is empty."); return false;
    }
    if (fileInfo.size() > kMaxStepImportBytes) {
        setError(error,"STEP file exceeds the 512 MiB import limit."); return false;
    }
    if (parts_.size() >= kMaxObjects) { setError(error,"Scene object limit reached."); return false; }
    try {
        STEPControl_Reader reader;
        if (reader.ReadFile(QFile::encodeName(path).constData()) != IFSelect_RetDone) {
            setError(error,"Could not read the STEP file."); return false;
        }
        if (reader.TransferRoots() <= 0) {
            setError(error,"The STEP file contains no transferable geometry."); return false;
        }
        const TopoDS_Shape combined = reader.OneShape();
        if (combined.IsNull()) { setError(error,"The STEP file contains no geometry."); return false; }

        NCollection_IndexedMap<TopoDS_Shape,TopTools_ShapeMapHasher> solids;
        TopExp::MapShapes(combined,TopAbs_SOLID,solids);
        if (solids.Extent()==0) { setError(error,"The STEP file contains no solid bodies."); return false; }
        if (parts_.size() + solids.Extent() > kMaxObjects) {
            setError(error,"Import would exceed the scene object limit."); return false;
        }

        const QString fileBase = fileInfo.completeBaseName().trimmed();
        const QString baseName = fileBase.isEmpty() ? QStringLiteral("Imported STEP") : fileBase;
        QVector<Part> imported;
        imported.reserve(solids.Extent());
        QSet<QString> reservedNames;
        for (int index=1; index<=solids.Extent(); ++index) {
            const TopoDS_Shape shape = solids(index);
            if (!validShape(shape,error)) return false;
            QString candidate = solids.Extent()==1 ? baseName : baseName + QStringLiteral(" %1").arg(index);
            const QString requested = candidate;
            int suffix=2;
            while (reservedNames.contains(candidate.toCaseFolded()) || uniqueName(candidate)!=candidate) {
                candidate=requested+QStringLiteral(" %1").arg(suffix++);
            }
            reservedNames.insert(candidate.toCaseFolded());
            imported.append({generatedId(),candidate,shape,true,"#6c9dc2"});
        }

        checkpoint();
        for (Part& part : imported) parts_.append(std::move(part));
        return true;
    } catch (const Standard_Failure& e) {
        setError(error,QStringLiteral("STEP import kernel error: %1").arg(e.what()));
    } catch (const std::exception& e) {
        setError(error,QString::fromUtf8(e.what()));
    }
    return false;
}

bool SceneDocument::exportSTEP(const QString& path, QString* error) const {
    if (parts_.isEmpty()) { setError(error,"No geometry to export."); return false; }
    try {
        STEPControl_Writer writer;
        for (const auto& part : parts_) {
            if (writer.Transfer(part.shape,STEPControl_AsIs) != IFSelect_RetDone) { setError(error,"STEP transfer failed."); return false; }
        }
        if (writer.Write(QFile::encodeName(path).constData()) != IFSelect_RetDone) { setError(error,"STEP write failed."); return false; }
        return true;
    } catch (const Standard_Failure& e) { setError(error,QStringLiteral("STEP kernel error: %1").arg(e.what())); return false; }
}
bool SceneDocument::exportSTL(const QString& path, QString* error) const {
    if (parts_.isEmpty()) { setError(error,"No geometry to export."); return false; }
    try {
        TopoDS_Compound compound; BRep_Builder builder; builder.MakeCompound(compound);
        for (const auto& part : parts_) builder.Add(compound,part.shape);
        BRepMesh_IncrementalMesh mesher(compound,0.20,false,0.35,true);
        mesher.Perform();
        if (!mesher.IsDone()) { setError(error,"STL tessellation failed."); return false; }
        StlAPI_Writer writer;
        writer.ASCIIMode() = false;
        if (!writer.Write(compound,QFile::encodeName(path).constData())) { setError(error,"STL write failed."); return false; }
        return true;
    } catch (const Standard_Failure& e) { setError(error,QStringLiteral("STL kernel error: %1").arg(e.what())); return false; }
}
} // namespace cad


#include "DrawingSheet.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRepBndLib.hxx>
#include <BRep_Builder.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <Bnd_Box.hxx>
#include <HLRAlgo_Projector.hxx>
#include <HLRBRep_Algo.hxx>
#include <HLRBRep_HLRToShape.hxx>
#include <HLRBRep_TypeOfResultingEdge.hxx>
#include <QHash>
#include <QCryptographicHash>
#include <QSet>
#include <QDate>
#include <QPointF>
#include <QSaveFile>
#include <QLocale>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QTextStream>
#include <QBuffer>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QPainter>
#include <QSvgRenderer>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax1.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pln.hxx>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace cad {
namespace {
struct Point2 { double x = 0.0, y = 0.0; };
struct Stroke { QVector<Point2> points; bool hidden = false; bool hatch = false; bool centerline = false; };
struct DiameterCallout { QString id; QString featureReference; Point2 center; double radius = 0.0; };
struct ViewGeometry {
    QVector<Stroke> strokes;
    QVector<DiameterCallout> diameters;
    double minX = 0.0, minY = 0.0, maxX = 0.0, maxY = 0.0;
    bool hasBounds = false;
};

void fail(QString* error, const QString& text) { if (error) *error = text; }
QString xml(QString value) {
    value.replace('&', "&amp;"); value.replace('<', "&lt;"); value.replace('>', "&gt;");
    value.replace('"', "&quot;"); value.replace('\'', "&apos;");
    return value;
}
void include(ViewGeometry& view, const Point2& p) {
    if (!view.hasBounds) {
        view.minX = view.maxX = p.x; view.minY = view.maxY = p.y; view.hasBounds = true;
    } else {
        view.minX = std::min(view.minX, p.x); view.maxX = std::max(view.maxX, p.x);
        view.minY = std::min(view.minY, p.y); view.maxY = std::max(view.maxY, p.y);
    }
}

void collectEdges(const TopoDS_Shape& shape, bool hidden, ViewGeometry& view, bool useZAsVertical = false) {
    for (TopExp_Explorer it(shape, TopAbs_EDGE); it.More(); it.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(it.Current());
        BRepAdaptor_Curve curve(edge);
        const double first = curve.FirstParameter(), last = curve.LastParameter();
        if (!std::isfinite(first) || !std::isfinite(last) || last <= first) continue;
        const int samples = curve.GetType() == GeomAbs_Line ? 1 : 72;
        Stroke stroke; stroke.hidden = hidden; stroke.points.reserve(samples + 1);
        for (int i = 0; i <= samples; ++i) {
            const double u = first + (last - first) * static_cast<double>(i) / samples;
            gp_Pnt p; curve.D0(u, p);
            if (!std::isfinite(p.X()) || !std::isfinite(p.Y())) continue;
            Point2 point{p.X(), useZAsVertical ? p.Z() : p.Y()};
            stroke.points.append(point); include(view, point);
        }
        if (stroke.points.size() >= 2) view.strokes.append(std::move(stroke));
    }
}

ViewGeometry projectSectionAtDepth(const Part& part,const QString& planeAxis,double sectionDepthFraction,double* coordinateOut) {
    ViewGeometry view;
    Bnd_Box bounds;
    BRepBndLib::AddOptimal(part.shape, bounds, false, false);
    if (bounds.IsVoid()) return view;
    double minX, minY, minZ, maxX, maxY, maxZ;
    bounds.Get(minX, minY, minZ, maxX, maxY, maxZ);
    const double minimum=planeAxis=="X"?minX:(planeAxis=="Z"?minZ:minY);
    const double maximum=planeAxis=="X"?maxX:(planeAxis=="Z"?maxZ:maxY);
    const double coordinate=minimum+(maximum-minimum)*sectionDepthFraction;
    if(coordinateOut)*coordinateOut=coordinate;
    gp_Dir normal(0,1,0);gp_Pnt origin(0,coordinate,0);
    if(planeAxis=="X"){normal=gp_Dir(1,0,0);origin=gp_Pnt(coordinate,0,0);}
    else if(planeAxis=="Z"){normal=gp_Dir(0,0,1);origin=gp_Pnt(0,0,coordinate);}
    BRepAlgoAPI_Section section(part.shape, gp_Pln(origin,normal), false);
    section.Build();
    if (!section.IsDone() || section.HasErrors() || section.Shape().IsNull()) return view;
    for(TopExp_Explorer it(section.Shape(),TopAbs_EDGE);it.More();it.Next()){
        BRepAdaptor_Curve curve(TopoDS::Edge(it.Current()));const double first=curve.FirstParameter(),last=curve.LastParameter();
        if(!std::isfinite(first)||!std::isfinite(last)||last<=first)continue;
        const int samples=curve.GetType()==GeomAbs_Line?1:72;Stroke stroke;stroke.points.reserve(samples+1);
        for(int i=0;i<=samples;++i){const double u=first+(last-first)*static_cast<double>(i)/samples;gp_Pnt p;curve.D0(u,p);
            Point2 point=planeAxis=="X"?Point2{p.Y(),p.Z()}:(planeAxis=="Z"?Point2{p.X(),p.Y()}:Point2{p.X(),p.Z()});
            stroke.points.append(point);include(view,point);
        }
        if(stroke.points.size()>=2)view.strokes.append(std::move(stroke));
    }
    // Clip evenly spaced 45-degree hatch lines against the section contours.
    // Pairing intersections with the even-odd rule also leaves holes unhatched.
    if (view.hasBounds) {
        const double step = 3.0;
        const double start = std::floor((view.minX + view.minY) / step) * step;
        const double end = view.maxX + view.maxY;
        for (double sum = start; sum <= end; sum += step) {
            QVector<double> differences;
            for (const Stroke& contour : view.strokes) {
                for (qsizetype i = 1; i < contour.points.size(); ++i) {
                    const Point2& a = contour.points[i - 1];
                    const Point2& b = contour.points[i];
                    const double sa = a.x + a.y, sb = b.x + b.y;
                    if ((sa <= sum && sb > sum) || (sb <= sum && sa > sum)) {
                        const double t = (sum - sa) / (sb - sa);
                        differences.append((a.y - a.x) + t * ((b.y - b.x) - (a.y - a.x)));
                    }
                }
            }
            std::sort(differences.begin(), differences.end());
            for (qsizetype i = 1; i < differences.size(); i += 2) {
                const double t0 = differences[i - 1], t1 = differences[i];
                if (t1 - t0 < 0.4) continue;
                Stroke hatch; hatch.hatch = true;
                hatch.points.append({(sum - t0) * 0.5, (sum + t0) * 0.5});
                hatch.points.append({(sum - t1) * 0.5, (sum + t1) * 0.5});
                view.strokes.append(std::move(hatch));
            }
        }
    }
    return view;
}

ViewGeometry project(const Part& part, const gp_Dir& normal, const gp_Dir& xDirection) {
    ViewGeometry view;
    occ::handle<HLRBRep_Algo> algo = new HLRBRep_Algo();
    algo->Projector(HLRAlgo_Projector(gp_Ax2(gp_Pnt(0, 0, 0), normal, xDirection)));
    algo->Add(part.shape);
    algo->Update();
    algo->Hide();
    HLRBRep_HLRToShape results(algo);
    const std::array<HLRBRep_TypeOfResultingEdge, 4> types = {
        HLRBRep_Sharp, HLRBRep_Rg1Line, HLRBRep_RgNLine, HLRBRep_OutLine
    };
    for (auto type : types) {
        collectEdges(results.CompoundOfEdges(type, true, false), false, view);
        collectEdges(results.CompoundOfEdges(type, false, false), true, view);
    }
    const gp_Dir yDirection = normal.Crossed(xDirection);
    auto projectPoint = [&xDirection, &yDirection](const gp_Pnt& p) {
        return Point2{p.X() * xDirection.X() + p.Y() * xDirection.Y() + p.Z() * xDirection.Z(),
                      p.X() * yDirection.X() + p.Y() * yDirection.Y() + p.Z() * yDirection.Z()};
    };
    auto appendCenterline = [&view](const Point2& first, const Point2& last) {
        Stroke stroke; stroke.centerline = true;
        stroke.points.append(first); stroke.points.append(last);
        include(view, first); include(view, last);
        view.strokes.append(std::move(stroke));
    };
    for (TopExp_Explorer it(part.shape, TopAbs_FACE); it.More(); it.Next()) {
        BRepAdaptor_Surface surface(TopoDS::Face(it.Current()), true);
        if (surface.GetType() != GeomAbs_Cylinder) continue;
        const gp_Cylinder cylinder = surface.Cylinder();
        const gp_Ax1 axis = cylinder.Axis();
        const double normalComponent = axis.Direction().Dot(normal);
        if (std::abs(normalComponent) > 0.999999) {
            const Point2 center = projectPoint(axis.Location());
            const double half = cylinder.Radius() * 0.6;
            if (half <= 1e-4) continue;
            appendCenterline({center.x - half, center.y}, {center.x + half, center.y});
            appendCenterline({center.x, center.y - half}, {center.x, center.y + half});
            const bool alreadyDimensioned = std::any_of(view.diameters.cbegin(), view.diameters.cend(),
                [&center, &cylinder](const DiameterCallout& callout) {
                    return std::hypot(callout.center.x - center.x, callout.center.y - center.y) < 1e-5
                        && std::abs(callout.radius - cylinder.Radius()) < 1e-5;
                });
            if (!alreadyDimensioned) {
                const gp_Dir& direction=axis.Direction();const gp_Pnt& location=axis.Location();
                const QString featureKey=QStringLiteral("%1|%2|%3|%4|%5|%6|%7")
                    .arg(location.X(),0,'f',6).arg(location.Y(),0,'f',6).arg(location.Z(),0,'f',6)
                    .arg(direction.X(),0,'f',6).arg(direction.Y(),0,'f',6).arg(direction.Z(),0,'f',6)
                    .arg(cylinder.Radius(),0,'f',6);
                const QString featureReference=QStringLiteral("cylinder-%1").arg(QString::fromLatin1(
                    QCryptographicHash::hash(featureKey.toUtf8(),QCryptographicHash::Sha256).toHex().left(16)));
                const QString dimensionKey=featureReference+QStringLiteral("|%1|%2|%3")
                    .arg(normal.X(),0,'f',3).arg(normal.Y(),0,'f',3).arg(normal.Z(),0,'f',3);
                const QString dimensionId=QStringLiteral("dimension-%1").arg(QString::fromLatin1(
                    QCryptographicHash::hash(dimensionKey.toUtf8(),QCryptographicHash::Sha256).toHex().left(16)));
                view.diameters.append({dimensionId,featureReference,center,cylinder.Radius()});
            }
            continue;
        }
        const double firstV = surface.FirstVParameter(), lastV = surface.LastVParameter();
        if (!std::isfinite(firstV) || !std::isfinite(lastV) || lastV <= firstV) continue;
        const gp_Vec direction(axis.Direction());
        const gp_Pnt origin = axis.Location();
        const Point2 first = projectPoint(origin.Translated(direction * firstV));
        const Point2 last = projectPoint(origin.Translated(direction * lastV));
        if (std::hypot(last.x - first.x, last.y - first.y) > 1e-5) appendCenterline(first, last);
    }
    return view;
}

double width(const ViewGeometry& v) { return v.maxX - v.minX; }
double height(const ViewGeometry& v) { return v.maxY - v.minY; }

void renderView(QTextStream& out, const ViewGeometry& view, const QString& label,
                double cx, double cy, double scale, int* visible, int* hidden,const QString& viewId = {}) {
    out << "<g data-view-label=\"" << xml(label) << "\"";
    if(!viewId.isEmpty())out << " data-view-id=\"" << xml(viewId) << "\"";
    out << " data-view-center-x=\"" << QString::number(cx,'f',3) << "\" data-view-center-y=\"" << QString::number(cy,'f',3) << "\"";
    out << ">\n";
    const double mx = (view.minX + view.maxX) * 0.5;
    const double my = (view.minY + view.maxY) * 0.5;
    out << "<g fill=\"none\" stroke=\"#111827\" stroke-width=\"0.28\" stroke-linecap=\"round\" stroke-linejoin=\"round\">\n";
    for (const Stroke& stroke : view.strokes) {
        out << "<polyline points=\"";
        for (const Point2& p : stroke.points) {
            const double x = cx + (p.x - mx) * scale;
            const double y = cy - (p.y - my) * scale;
            out << QString::number(x, 'f', 3) << ',' << QString::number(y, 'f', 3) << ' ';
        }
        out << "\"";
        if (stroke.hatch) out << " stroke=\"#6b7280\" stroke-width=\"0.12\"";
        if (stroke.centerline) out << " stroke=\"#4b5563\" stroke-width=\"0.16\" stroke-dasharray=\"6,1,1,1\"";
        if (stroke.hidden) { out << " stroke-dasharray=\"2,1\""; if (hidden) ++*hidden; }
        else if (visible) ++*visible;
        out << "/>\n";
    }
    out << "</g>\n";
    for (const DiameterCallout& callout : view.diameters) {
        const double centerX = cx + (callout.center.x - mx) * scale;
        const double centerY = cy - (callout.center.y - my) * scale;
        const double radius = callout.radius * scale;
        const double startX = centerX + radius * 0.68;
        const double startY = centerY - radius * 0.68;
        const double textX = centerX + radius + 9.0;
        const double textY = centerY - radius - 2.0;
        out << "<g data-drawing-annotation=\"feature-linked-dimension\" data-annotation-id=\"" << xml(callout.id)
            << "\" data-feature-reference=\"" << xml(callout.featureReference)
            << "\" data-feature-type=\"cylindrical-diameter\" data-parameter=\"diameter\" data-value-mm=\""
            << QString::number(callout.radius*2.0,'f',3) << "\"><g fill=\"none\" stroke=\"#1f2937\" stroke-width=\"0.18\"><path d=\"M"
            << startX << ' ' << startY << "L" << textX - 1.0 << ' ' << textY + 1.0
            << "\"/></g><text x=\"" << textX << "\" y=\"" << textY
            << "\" font-family=\"Arial,sans-serif\" font-size=\"3\" fill=\"#111827\">Ø"
            << QString::number(callout.radius * 2.0, 'f', 3) << " mm</text></g>\n";
    }
    out << "<text x=\"" << QString::number(cx, 'f', 2) << "\" y=\""
        << QString::number(cy + std::max(4.0, height(view) * scale * 0.5 + 23.0), 'f', 2)
        << "\" text-anchor=\"middle\" font-family=\"Arial,sans-serif\" font-size=\"3.5\">"
        << xml(label) << "</text>\n";
    out << "</g>\n";
}

void renderDetailView(QTextStream& out,const ViewGeometry& source,const DrawingDetailAnnotation& detail,
                      double sourceCx,double sourceCy,double baseScale,double sheetScale,int index) {
    const double sourceMidX=(source.minX+source.maxX)*0.5,sourceMidY=(source.minY+source.maxY)*0.5;
    const double clipX=detail.xMm/sheetScale,clipY=detail.yMm/sheetScale,clipRadius=detail.radiusMm/sheetScale;
    const double detailScale=clipRadius/(detail.captureRadiusMm*baseScale);
    const double sourcePaperX=sourceCx+(detail.sourceXmm-sourceMidX)*baseScale;
    const double sourcePaperY=sourceCy-(detail.sourceYmm-sourceMidY)*baseScale;
    const QString clipId=QStringLiteral("detail-clip-%1").arg(QString::fromLatin1(
        QCryptographicHash::hash(detail.id.toUtf8(),QCryptographicHash::Sha256).toHex().left(16)));
    const QString label=QStringLiteral("DETAIL %1").arg(QChar('A'+index));
    out << "<g data-drawing-annotation=\"detail-source\" data-annotation-id=\"" << xml(detail.id)
        << "\" data-source-view=\"" << xml(detail.sourceView) << "\"><circle cx=\"" << sourcePaperX
        << "\" cy=\"" << sourcePaperY << "\" r=\"" << detail.captureRadiusMm*baseScale
        << "\" fill=\"none\" stroke=\"#2563eb\" stroke-width=\"0.28\" stroke-dasharray=\"1.5,0.8\"/></g>\n"
        << "<defs><clipPath id=\"" << clipId << "\" clipPathUnits=\"userSpaceOnUse\"><circle cx=\""
        << clipX << "\" cy=\"" << clipY << "\" r=\"" << clipRadius << "\"/></clipPath></defs>\n"
        << "<g data-drawing-annotation=\"detail-view\" data-annotation-id=\"" << xml(detail.id)
        << "\" data-source-view=\"" << xml(detail.sourceView) << "\" data-detail-scale=\""
        << QString::number(detailScale,'f',4) << "\" clip-path=\"url(#" << clipId
        << ")\" fill=\"none\" stroke=\"#111827\" stroke-width=\"0.28\">\n";
    for(const Stroke& stroke:source.strokes){
        out << "<polyline points=\"";
        for(const Point2& point:stroke.points){
            const double x=clipX+(point.x-detail.sourceXmm)*baseScale*detailScale;
            const double y=clipY-(point.y-detail.sourceYmm)*baseScale*detailScale;
            out << QString::number(x,'f',3) << ',' << QString::number(y,'f',3) << ' ';
        }
        out << "\"";
        if(stroke.hatch)out << " stroke=\"#6b7280\" stroke-width=\"0.12\"";
        if(stroke.centerline)out << " stroke=\"#4b5563\" stroke-width=\"0.16\" stroke-dasharray=\"6,1,1,1\"";
        if(stroke.hidden)out << " stroke-dasharray=\"2,1\"";
        out << "/>\n";
    }
    out << "</g><circle data-detail-boundary=\"true\" cx=\"" << clipX << "\" cy=\"" << clipY
        << "\" r=\"" << clipRadius << "\" fill=\"none\" stroke=\"#111827\" stroke-width=\"0.4\"/>"
        << "<text x=\"" << clipX << "\" y=\"" << clipY+clipRadius+4.0 << "\" text-anchor=\"middle\" "
        << "font-family=\"Arial,sans-serif\" font-size=\"3.5\">" << label << "</text>\n";
    const double dx=clipX-sourcePaperX,dy=clipY-sourcePaperY,distance=std::hypot(dx,dy);
    if(distance>detail.radiusMm/sheetScale+detail.captureRadiusMm*baseScale){
        const double ux=dx/distance,uy=dy/distance;
        const double x1=sourcePaperX+ux*detail.captureRadiusMm*baseScale;
        const double y1=sourcePaperY+uy*detail.captureRadiusMm*baseScale;
        const double x2=clipX-ux*clipRadius,y2=clipY-uy*clipRadius;
        out << "<path data-detail-leader=\"" << xml(detail.id) << "\" d=\"M" << x1 << ' ' << y1
            << 'L' << x2 << ' ' << y2 << "\" fill=\"none\" stroke=\"#2563eb\" stroke-width=\"0.25\"/>\n";
    }
}

void renderHorizontalDimension(QTextStream& out, const ViewGeometry& view, double cx, double cy,
                               double scale, const QString& axis,double offset,const QString& id,double sheetScale) {
    const double halfWidth = width(view) * scale * 0.5;
    const double edgeY = cy + height(view) * scale * 0.5;
    const double y = edgeY + offset/sheetScale;
    const double x1 = cx - halfWidth, x2 = cx + halfWidth;
    out << "<g data-drawing-annotation=\"overall-dimension\" data-axis=\"" << xml(axis) << "\" data-annotation-id=\"" << xml(id) << "\" data-offset-mm=\"" << QString::number(offset,'f',3) << "\">"
        << "<g fill=\"none\" stroke=\"#1f2937\" stroke-width=\"0.18\">"
        << "<path d=\"M" << x1 << ' ' << edgeY + 1.0 << "V" << y + 1.5
        << " M" << x2 << ' ' << edgeY + 1.0 << "V" << y + 1.5
        << " M" << x1 << ' ' << y << "H" << x2 << "\"/>"
        << "<path fill=\"#1f2937\" d=\"M" << x1 << ' ' << y << "l2 -0.8v1.6z M"
        << x2 << ' ' << y << "l-2 -0.8v1.6z\"/></g>"
        << "<text x=\"" << cx << "\" y=\"" << y - 1.2
        << "\" text-anchor=\"middle\" font-family=\"Arial,sans-serif\" font-size=\"3\" fill=\"#111827\">"
        << axis << " " << QString::number(width(view), 'f', 3) << " mm</text></g>\n";
}

void renderVerticalDimension(QTextStream& out, const ViewGeometry& view, double cx, double cy,
                             double scale, const QString& axis,double offset,const QString& id,double sheetScale) {
    const double halfHeight = height(view) * scale * 0.5;
    const double edgeX = cx - width(view) * scale * 0.5;
    const double x = edgeX - offset/sheetScale;
    const double y1 = cy - halfHeight, y2 = cy + halfHeight;
    out << "<g data-drawing-annotation=\"overall-dimension\" data-axis=\"" << xml(axis) << "\" data-annotation-id=\"" << xml(id) << "\" data-offset-mm=\"" << QString::number(offset,'f',3) << "\">"
        << "<g fill=\"none\" stroke=\"#1f2937\" stroke-width=\"0.18\">"
        << "<path d=\"M" << edgeX - 1.0 << ' ' << y1 << "H" << x - 1.5
        << " M" << edgeX - 1.0 << ' ' << y2 << "H" << x - 1.5
        << " M" << x << ' ' << y1 << "V" << y2 << "\"/>"
        << "<path fill=\"#1f2937\" d=\"M" << x << ' ' << y1 << "l-0.8 2h1.6z M"
        << x << ' ' << y2 << "l-0.8 -2h1.6z\"/></g>"
        << "<text transform=\"rotate(-90 " << x - 1.2 << ' ' << cy << ")\" x=\"" << x - 1.2
        << "\" y=\"" << cy << "\" text-anchor=\"middle\" font-family=\"Arial,sans-serif\" font-size=\"3\" fill=\"#111827\">"
        << axis << " " << QString::number(height(view), 'f', 3) << " mm</text></g>\n";
}

void renderProjectionSymbol(QTextStream& out, OrthographicProjection projection) {
    // Schematic truncated-cone projection symbol in the reserved title-block band.
    // First-angle places the end view to the right; third-angle places it to the left.
    const bool thirdAngle = projection == OrthographicProjection::ThirdAngle;
    const double circleX = thirdAngle ? 261.0 : 282.0;
    out << "<g data-projection-symbol=\"" << (thirdAngle ? "third-angle" : "first-angle")
        << "\" fill=\"none\" stroke=\"#111827\" stroke-width=\"0.25\">"
        << "<circle cx=\"" << circleX << "\" cy=\"165\" r=\"3\"/>"
        << "<path d=\"M267 162V168 M278 163.5V166.5 M267 162L278 163.5 M267 168L278 166.5\"/>"
        << "</g>\n";
}
}

bool buildDrawingAssemblyPart(const QVector<Part>& parts,Part* result,QString* error) {
    if(!result){fail(error,"Provide an output body for the drawing assembly.");return false;}
    if(parts.isEmpty()||parts.size()>50){fail(error,"A drawing source must contain 1 to 50 selected bodies.");return false;}
    QVector<Part> ordered=parts;
    std::sort(ordered.begin(),ordered.end(),[](const Part& a,const Part& b){return a.id<b.id;});
    QSet<QString> ids;QByteArray identity;BRep_Builder builder;TopoDS_Compound compound;builder.MakeCompound(compound);
    for(const Part& part:ordered){
        if(part.id.trimmed().isEmpty()||ids.contains(part.id)||part.shape.IsNull()||!BRepCheck_Analyzer(part.shape).IsValid()){
            fail(error,"Drawing sources must be distinct bodies with valid solid geometry.");return false;
        }
        ids.insert(part.id);identity.append(part.id.toUtf8());identity.append('|');identity.append(part.geometryToken.toUtf8());identity.append('\n');
        builder.Add(compound,part.shape);
    }
    Part drawing=ordered.first();drawing.shape=compound;
    drawing.geometryToken=QString::fromLatin1(QCryptographicHash::hash(identity,QCryptographicHash::Sha256).toHex());
    if(ordered.size()>1){
        drawing.name=QStringLiteral("Assembly drawing (%1 bodies)").arg(ordered.size());
        drawing.description=QStringLiteral("Assembly drawing (%1 bodies)").arg(ordered.size());
    }
    *result=std::move(drawing);return true;
}

bool buildOrthographicDrawingSvg(const Part& part, QByteArray* svg, QString* error,
                                 double* chosenScale, int* visiblePathCount, int* hiddenPathCount,
                                 OrthographicProjection projection, DrawingSheetSize sheetSize,
                                 DrawingAuxiliaryView auxiliaryView, double sectionDepthFraction,
                                 const QStringList& generalNotes, const DrawingTitleBlock& titleBlock,
                                 const QMap<QString,QPointF>& viewCenters,const QMap<QString,QString>& viewIds,
                                 const QString& sheetId,const QVector<DrawingNoteAnnotation>& noteAnnotations,
                                 const QVector<DrawingDimensionAnnotation>& dimensionAnnotations,
                                 const DrawingPartsListAnnotation& partsListAnnotation,
                                 const QVector<BillOfMaterialsRow>& bomRows,const QStringList& sourceBodyIds,
                                 const QVector<DrawingBalloonAnnotation>& balloonAnnotations,
                                 const QVector<DrawingDetailAnnotation>& detailAnnotations,
                                 const QVector<DrawingSectionAnnotation>& sectionAnnotations) {
    if (!svg) { fail(error, "Provide an output buffer for the drawing SVG."); return false; }
    if (part.shape.IsNull()) { fail(error, "The selected body has no geometry to project."); return false; }
    if (!std::isfinite(sectionDepthFraction) || sectionDepthFraction <= 0.0 || sectionDepthFraction >= 1.0) {
        fail(error, "Section depth must be a finite fraction strictly between 0 and 1."); return false;
    }
    if (generalNotes.size() > 5) { fail(error, "A drawing can contain at most five general-note lines."); return false; }
    for (const QString& note : generalNotes) {
        if (note.trimmed().isEmpty() || note.trimmed().size() > 60 ||
            std::any_of(note.cbegin(), note.cend(), [](QChar c) { return c.category() == QChar::Other_Control; })) {
            fail(error, "Each general note must contain 1-60 printable characters on one line."); return false;
        }
    }
    if(noteAnnotations.size()>5){fail(error,"A drawing can contain at most five persisted note annotations.");return false;}
    const double notePageWidth=sheetSize==DrawingSheetSize::A3Landscape?420.0:297.0;
    const double notePageHeight=sheetSize==DrawingSheetSize::A3Landscape?297.0:210.0;
    for(const DrawingNoteAnnotation& note:noteAnnotations){
        if(note.id.trimmed().isEmpty()||note.text.trimmed().isEmpty()||note.text.size()>60||
           std::any_of(note.text.cbegin(),note.text.cend(),[](QChar c){return c.category()==QChar::Other_Control;})||
           !std::isfinite(note.xMm)||!std::isfinite(note.yMm)||note.xMm<10||note.xMm>notePageWidth-10||note.yMm<10||note.yMm>notePageHeight-10){
            fail(error,"Each drawing note needs a stable ID, printable text, and an on-sheet position.");return false;
        }
    }
    if(!dimensionAnnotations.isEmpty()&&dimensionAnnotations.size()!=3){fail(error,"A drawing sheet requires exactly three persistent overall dimensions (X, Y, Z).");return false;}
    QMap<QString,DrawingDimensionAnnotation> dimensions;
    for(const DrawingDimensionAnnotation& dimension:dimensionAnnotations){
        if((dimension.axis!="X"&&dimension.axis!="Y"&&dimension.axis!="Z")||dimension.id.trimmed().isEmpty()||
           !std::isfinite(dimension.offsetMm)||dimension.offsetMm<5.0||dimension.offsetMm>60.0||dimensions.contains(dimension.axis)){
            fail(error,"Each persistent drawing dimension needs a unique axis, stable ID, and 5-60 mm offset.");return false;
        }
        dimensions.insert(dimension.axis,dimension);
    }
    if(partsListAnnotation.visible&&
       (partsListAnnotation.id.trimmed().isEmpty()||!std::isfinite(partsListAnnotation.xMm)||!std::isfinite(partsListAnnotation.yMm)||
        partsListAnnotation.xMm<10||partsListAnnotation.yMm<10||bomRows.isEmpty()||bomRows.size()>18)){
        fail(error,"A linked drawing parts list needs a stable ID, on-sheet position, and 1-18 BOM rows.");return false;
    }
    if(balloonAnnotations.size()>50){fail(error,"A drawing sheet can contain at most 50 BOM balloons.");return false;}
    QStringList balloonSourceIds=sourceBodyIds;if(balloonSourceIds.isEmpty())balloonSourceIds.append(part.id);
    QSet<QString> balloonIds;
    for(const DrawingBalloonAnnotation& balloon:balloonAnnotations){
        bool foundInBom=false;for(const BillOfMaterialsRow& row:bomRows)if(row.bodyIds.contains(balloon.sourceBodyId)){foundInBom=true;break;}
        if(balloon.id.trimmed().isEmpty()||balloonIds.contains(balloon.id)||balloon.sourceBodyId.trimmed().isEmpty()||
           !balloonSourceIds.contains(balloon.sourceBodyId)||!foundInBom||!std::isfinite(balloon.xMm)||!std::isfinite(balloon.yMm)||
           balloon.xMm<8.0||balloon.xMm>(sheetSize==DrawingSheetSize::A3Landscape?412.0:289.0)||
           balloon.yMm<8.0||balloon.yMm>(sheetSize==DrawingSheetSize::A3Landscape?289.0:202.0)){
            fail(error,"Each drawing balloon needs a unique ID, a body in the source-scoped BOM, and an on-sheet position.");return false;
        }
        balloonIds.insert(balloon.id);
    }
    if(detailAnnotations.size()>4){fail(error,"A drawing sheet can contain at most four detail views.");return false;}
    QSet<QString> detailIds;
    const QSet<QString> detailSourceViews={QStringLiteral("TOP"),QStringLiteral("FRONT"),QStringLiteral("RIGHT")};
    for(const DrawingDetailAnnotation& detail:detailAnnotations){
        if(detail.id.trimmed().isEmpty()||detailIds.contains(detail.id)||balloonIds.contains(detail.id)||
           !detailSourceViews.contains(detail.sourceView)||!std::isfinite(detail.sourceXmm)||!std::isfinite(detail.sourceYmm)||
           !std::isfinite(detail.captureRadiusMm)||detail.captureRadiusMm<=0.0||!std::isfinite(detail.xMm)||
           !std::isfinite(detail.yMm)||!std::isfinite(detail.radiusMm)||detail.radiusMm<8||detail.radiusMm>40||
           detail.xMm-detail.radiusMm<10||detail.xMm+detail.radiusMm>notePageWidth-10||
           detail.yMm-detail.radiusMm<10||detail.yMm+detail.radiusMm>notePageHeight-10){
            fail(error,"Each detail view needs a unique ID, valid projected source, capture center, and on-sheet circular position.");return false;
        }
        detailIds.insert(detail.id);
    }
    if(sectionAnnotations.size()>4){fail(error,"A drawing sheet can contain at most four separately placed section views.");return false;}
    QSet<QString> sectionIds;
    for(const DrawingSectionAnnotation& section:sectionAnnotations){
        if(section.id.trimmed().isEmpty()||sectionIds.contains(section.id)||detailIds.contains(section.id)||balloonIds.contains(section.id)||
           !std::isfinite(section.depthFraction)||section.depthFraction<0.01||section.depthFraction>0.99||
           !std::isfinite(section.xMm)||!std::isfinite(section.yMm)||section.xMm<20||section.xMm>notePageWidth-20||
           section.yMm<30||section.yMm>notePageHeight-30){
            fail(error,"Each independent section needs a unique ID, depth from 1 to 99 percent, and an on-sheet center.");return false;
        }
        sectionIds.insert(section.id);
    }
    const QString drawingNumber = titleBlock.drawingNumber.trimmed().isEmpty()
        ? part.partNumber : titleBlock.drawingNumber.trimmed();
    const QString drawnBy = titleBlock.drawnBy.trimmed();
    const QString checkedBy = titleBlock.checkedBy.trimmed();
    const QString issueDate = titleBlock.issueDate.trimmed();
    const QString revision = titleBlock.revision.trimmed().isEmpty()
        ? part.revision : titleBlock.revision.trimmed();
    auto validTitleField = [](const QString& value, qsizetype maximum) {
        return value.size() <= maximum && std::none_of(value.cbegin(), value.cend(), [](QChar c) {
            return c.category() == QChar::Other_Control;
        });
    };
    if (!validTitleField(drawingNumber, 32) || !validTitleField(drawnBy, 40) ||
        !validTitleField(checkedBy, 40) || !validTitleField(issueDate, 20)) {
        fail(error, "Drawing number is limited to 32 characters; author fields to 40; date to 20. Control characters are not allowed.");
        return false;
    }
    if (!validTitleField(revision, 16)) {
        fail(error, "Drawing revision is limited to 16 printable characters.");
        return false;
    }
    if (!issueDate.isEmpty()) {
        const QDate parsedDate = QDate::fromString(issueDate, Qt::ISODate);
        if (!parsedDate.isValid() || parsedDate.toString(Qt::ISODate) != issueDate) {
            fail(error, "Drawing issue date must use YYYY-MM-DD format.");
            return false;
        }
    }
    try {
        const ViewGeometry front = project(part, gp_Dir(0, -1, 0), gp_Dir(1, 0, 0));
        const ViewGeometry top = project(part, gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
        const ViewGeometry right = project(part, gp_Dir(1, 0, 0), gp_Dir(0, -1, 0));
        const ViewGeometry isometric = project(part, gp_Dir(1, -1, 1), gp_Dir(1, 1, 0));
        double sectionY = 0.0;
        const ViewGeometry section = auxiliaryView == DrawingAuxiliaryView::SectionA
            ? projectSectionAtDepth(part,QStringLiteral("Y"),sectionDepthFraction,&sectionY) : ViewGeometry{};
        const ViewGeometry& auxiliary = auxiliaryView == DrawingAuxiliaryView::SectionA ? section : isometric;
        if (!front.hasBounds || !top.hasBounds || !right.hasBounds || !auxiliary.hasBounds) {
            fail(error, auxiliaryView == DrawingAuxiliaryView::SectionA
                ? "The section view at the body's mid-depth did not produce a closed section outline."
                : "Hidden-line projection did not produce complete front, top, right, and isometric views."); return false;
        }
        QVector<ViewGeometry> savedSectionViews;QVector<double> savedSectionCoordinate;
        savedSectionViews.reserve(sectionAnnotations.size());savedSectionCoordinate.reserve(sectionAnnotations.size());
        for(const DrawingSectionAnnotation& annotation:sectionAnnotations){
            double coordinate=0.0;ViewGeometry savedSection=projectSectionAtDepth(part,annotation.planeAxis,annotation.depthFraction,&coordinate);
            if(!savedSection.hasBounds){fail(error,"A saved section depth did not produce a closed section contour.");return false;}
            savedSectionViews.append(std::move(savedSection));savedSectionCoordinate.append(coordinate);
        }
        for(const DrawingDetailAnnotation& detail:detailAnnotations){
            const ViewGeometry& source=detail.sourceView=="TOP"?top:(detail.sourceView=="RIGHT"?right:front);
            if(detail.sourceXmm<source.minX||detail.sourceXmm>source.maxX||detail.sourceYmm<source.minY||detail.sourceYmm>source.maxY){
                fail(error,"A drawing detail center must lie inside the selected projected view bounds.");return false;
            }
        }
        double scale = 1.0;
        auto fit = [&scale](double available, double extent) { if (extent > 1e-9) scale = std::min(scale, available / extent); };
        fit(118.0, width(front)); fit(68.0, height(front));
        fit(118.0, width(top)); fit(38.0, height(top));
        fit(78.0, width(right)); fit(68.0, height(right));
        fit(78.0, width(auxiliary)); fit(45.0, height(auxiliary));
        for(const ViewGeometry& savedSection:savedSectionViews){fit(78.0,width(savedSection));fit(45.0,height(savedSection));}
        scale = std::clamp(scale, 1e-5, 1.0);

        // Third-angle places the top and right-side views on the corresponding sides
        // of the front view; first-angle reverses both placements.
        const bool thirdAngle = projection == OrthographicProjection::ThirdAngle;
        const bool a3 = sheetSize == DrawingSheetSize::A3Landscape;
        const double sheetScale = a3 ? (420.0 / 297.0) : 1.0;
        auto center=[&](const QString& key,double defaultX,double defaultY){
            const auto it=viewCenters.constFind(key);
            return it==viewCenters.cend()?QPointF(defaultX,defaultY):QPointF(it.value().x()/sheetScale,it.value().y()/sheetScale);
        };
        const QPointF topCenter=center(QStringLiteral("TOP"),thirdAngle?79.0:155.0,thirdAngle?47.0:143.0);
        const QPointF frontCenter=center(QStringLiteral("FRONT"),thirdAngle?79.0:155.0,thirdAngle?125.0:70.0);
        const QPointF rightCenter=center(QStringLiteral("RIGHT"),thirdAngle?204.0:45.0,thirdAngle?125.0:70.0);
        const QPointF auxiliaryCenter=center(QStringLiteral("AUXILIARY"),240.0,thirdAngle?47.0:145.0);
        const double topX=topCenter.x(),topY=topCenter.y(),frontX=frontCenter.x(),frontY=frontCenter.y();
        const double rightX=rightCenter.x(),rightY=rightCenter.y(),isoX=auxiliaryCenter.x(),isoY=auxiliaryCenter.y();
        const double titleX = thirdAngle ? 151.0 : 10.0;
        const double notesX = thirdAngle ? 12.0 : 151.0;
        const double titleY = 171.0;
        const double col2 = titleX + 69.0, col3 = titleX + 103.0;
        const QString projectionName = thirdAngle ? "THIRD-ANGLE" : "FIRST-ANGLE";
        const bool sectionSelected = auxiliaryView == DrawingAuxiliaryView::SectionA;
        const double sectionLabelPercent = std::round(sectionDepthFraction * 100.0);
        const QString sectionDepthLabel = std::abs(sectionLabelPercent - 50.0) < 1e-9
            ? QStringLiteral("AT MID-DEPTH")
            : QStringLiteral("AT %1% DEPTH (Y=%2 mm)")
                .arg(sectionLabelPercent, 0, 'f', 0).arg(sectionY, 0, 'f', 3);
        const double sheetWidth = a3 ? 420.0 : 297.0;
        const double sheetHeight = a3 ? 297.0 : 210.0;
        const double actualScale = scale * sheetScale;
        const QString scaleLabel = actualScale >= 1.0
            ? QString("SCALE %1:1").arg(actualScale, 0, 'f', 2)
            : QString("SCALE 1:%1").arg(1.0 / actualScale, 0, 'f', 2);

        QByteArray bytes;
        QTextStream out(&bytes, QIODevice::WriteOnly);
        out.setLocale(QLocale::c());
        QStringList drawingSourceIds=sourceBodyIds;if(drawingSourceIds.isEmpty())drawingSourceIds.append(part.id);
        out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << sheetWidth << "mm\" height=\""
            << sheetHeight << "mm\" viewBox=\"0 0 " << sheetWidth << ' ' << sheetHeight
            << "\" data-cadworkbench-source-body-id=\"" << xml(drawingSourceIds.first())
            << "\" data-cadworkbench-source-body-ids=\"" << xml(drawingSourceIds.join(','))
            << "\" data-cadworkbench-source-geometry-token=\"" << xml(part.geometryToken)
            << "\" data-cadworkbench-link-status=\"export-snapshot\"";
        if(!sheetId.isEmpty())out << " data-cadworkbench-sheet-id=\"" << xml(sheetId) << "\"";
        out << ">\n"
            << "<rect x=\"0\" y=\"0\" width=\"" << sheetWidth << "\" height=\"" << sheetHeight << "\" fill=\"white\"/>\n"
            << "<g transform=\"scale(" << QString::number(sheetScale, 'f', 6) << ")\">\n"
            << "<rect x=\"10\" y=\"10\" width=\"277\" height=\"190\" fill=\"none\" stroke=\"#111827\" stroke-width=\"0.35\"/>\n"
            << "<text x=\"12\" y=\"17\" font-family=\"Arial,sans-serif\" font-size=\"3.5\">" << projectionName
            << " TECHNICAL DRAWING - OVERALL ENVELOPE DIMENSIONS ONLY</text>\n"
            << "<text x=\"12\" y=\"23\" font-family=\"Arial,sans-serif\" font-size=\"3\">MATERIAL: "
            << xml(part.materialName) << " | " << scaleLabel << " | UNITS: mm</text>\n";
        int visible = 0, hidden = 0;
        renderView(out, top, "TOP", topX, topY, scale, &visible, &hidden,viewIds.value(QStringLiteral("TOP")));
        renderView(out, front, "FRONT", frontX, frontY, scale, &visible, &hidden,viewIds.value(QStringLiteral("FRONT")));
        renderView(out, right, "RIGHT", rightX, rightY, scale, &visible, &hidden,viewIds.value(QStringLiteral("RIGHT")));
        renderView(out, auxiliary, sectionSelected ? "SECTION A-A" : "ISOMETRIC", isoX, isoY,
                   scale, &visible, &hidden,viewIds.value(QStringLiteral("AUXILIARY")));
        for(int sectionIndex=0;sectionIndex<savedSectionViews.size();++sectionIndex){
            const DrawingSectionAnnotation& annotation=sectionAnnotations[sectionIndex];
            const QChar letter('A'+sectionIndex+(sectionSelected?1:0));
            const QString label=QStringLiteral("SECTION %1-%1").arg(letter);
            renderView(out,savedSectionViews[sectionIndex],label,annotation.xMm/sheetScale,annotation.yMm/sheetScale,
                       scale,&visible,&hidden);
            const ViewGeometry& markerView=annotation.planeAxis=="Z"?front:top;
            const QPointF markerCenter=annotation.planeAxis=="Z"?frontCenter:topCenter;
            const double coordinateMid=annotation.planeAxis=="X"?(top.minX+top.maxX)*0.5:
                (annotation.planeAxis=="Y"?(top.minY+top.maxY)*0.5:(front.minY+front.maxY)*0.5);
            const double markerCoordinate=savedSectionCoordinate[sectionIndex];
            const bool verticalCut=annotation.planeAxis=="X";
            const double cutCenterX=verticalCut?markerCenter.x()+(markerCoordinate-coordinateMid)*scale:markerCenter.x();
            const double cutCenterY=verticalCut?markerCenter.y():markerCenter.y()-(markerCoordinate-coordinateMid)*scale;
            const double halfExtent=(verticalCut?height(markerView):width(markerView))*scale*0.5;
            const double sectionX1=verticalCut?cutCenterX:cutCenterX-halfExtent;
            const double sectionY1=verticalCut?cutCenterY-halfExtent:cutCenterY;
            const double sectionX2=verticalCut?cutCenterX:cutCenterX+halfExtent;
            const double sectionY2=verticalCut?cutCenterY+halfExtent:cutCenterY;
            out << "<g data-drawing-annotation=\"section-cut\" data-annotation-id=\"" << xml(annotation.id)
                << "\" data-depth-fraction=\"" << QString::number(annotation.depthFraction,'f',4)
                << "\" data-plane-axis=\"" << xml(annotation.planeAxis) << "\" data-source-view=\""
                << (annotation.planeAxis=="Z"?"FRONT":"TOP")
                << "\" fill=\"none\" stroke=\"#111827\" stroke-width=\"0.35\"><path stroke-dasharray=\"2,1\" d=\"M"
                << sectionX1 << ' ' << sectionY1 << "L" << sectionX2 << ' ' << sectionY2 << "\"/><path fill=\"#111827\" d=\"M"
                << sectionX1 << ' ' << sectionY1 << "l-1.4 -3h2.8z M" << sectionX2 << ' ' << sectionY2
                << "l-1.4 -3h2.8z\"/></g><g data-section-label=\"" << xml(annotation.id)
                << "\" fill=\"#111827\" font-family=\"Arial,sans-serif\" font-size=\"3.5\"><text x=\""
                << sectionX1-2 << "\" y=\"" << sectionY1-2 << "\">" << letter << "</text><text x=\""
                << sectionX2+1 << "\" y=\"" << sectionY2-2 << "\">" << letter << "</text></g>\n";
        }
        for(int detailIndex=0;detailIndex<detailAnnotations.size();++detailIndex){
            const DrawingDetailAnnotation& detail=detailAnnotations[detailIndex];
            const ViewGeometry& source=detail.sourceView=="TOP"?top:(detail.sourceView=="RIGHT"?right:front);
            const QPointF sourceCenter=detail.sourceView=="TOP"?topCenter:(detail.sourceView=="RIGHT"?rightCenter:frontCenter);
            renderDetailView(out,source,detail,sourceCenter.x(),sourceCenter.y(),scale,sheetScale,detailIndex);
        }
        if (sectionSelected) {
            const double halfWidth = width(top) * scale * 0.5;
            const double lineY = topY - (sectionY - (top.minY + top.maxY) * 0.5) * scale;
            const double x1 = topX - halfWidth, x2 = topX + halfWidth;
            out << "<g fill=\"none\" stroke=\"#111827\" stroke-width=\"0.35\">"
                << "<path stroke-dasharray=\"2,1\" d=\"M" << x1 << ' ' << lineY << "H" << x2 << "\"/>"
                << "<path fill=\"#111827\" d=\"M" << x1 << ' ' << lineY << "l-1.4 -3h2.8z M"
                << x2 << ' ' << lineY << "l-1.4 -3h2.8z\"/></g>"
                << "<g fill=\"#111827\" font-family=\"Arial,sans-serif\" font-size=\"3.5\">"
                << "<text x=\"" << x1 - 2.0 << "\" y=\"" << lineY - 2.0 << "\">A</text>"
                << "<text x=\"" << x2 + 1.0 << "\" y=\"" << lineY - 2.0 << "\">A</text>"
                << "<text x=\"" << topX << "\" y=\""
                << lineY - height(top) * scale * 0.5 - 4.0
                << "\" text-anchor=\"middle\">CUT PLANE A-A " << xml(sectionDepthLabel) << "</text></g>\n";
        }
        out << "<g fill=\"none\">\n";
        const auto dimensionFor=[&dimensions](const QString& axis){return dimensions.value(axis,DrawingDimensionAnnotation{QStringLiteral("auto-")+axis,axis,9.0});};
        const auto dimX=dimensionFor("X"),dimY=dimensionFor("Y"),dimZ=dimensionFor("Z");
        renderHorizontalDimension(out, front, frontX, frontY, scale, "X",dimX.offsetMm,dimX.id,sheetScale);
        renderVerticalDimension(out, front, frontX, frontY, scale, "Z",dimZ.offsetMm,dimZ.id,sheetScale);
        renderVerticalDimension(out, top, topX, topY, scale, "Y",dimY.offsetMm,dimY.id,sheetScale);
        renderProjectionSymbol(out, projection);
        out << "</g>\n";
        out << "<g fill=\"none\" stroke=\"#111827\" stroke-width=\"0.25\">"
            << "<rect x=\"" << titleX << "\" y=\"" << titleY << "\" width=\"136\" height=\"29\"/>"
            << "<path d=\"M" << titleX << " 180H" << titleX + 136.0 << " M" << titleX << " 190H"
            << titleX + 136.0 << " M" << col2 << " 171V190 M" << col3 << " 180V190 M" << titleX + 36.0
            << " 190V200 M" << titleX + 70.0 << " 190V200 M" << titleX + 104.0 << " 190V200\"/></g>\n"
            << "<g font-family=\"Arial,sans-serif\" font-size=\"3\" fill=\"#111827\">"
            << "<text x=\"" << titleX + 2.0 << "\" y=\"176\">DESCRIPTION</text><text x=\"" << col2 + 2.0 << "\" y=\"176\">PART NUMBER</text>"
            << "<text x=\"" << col3 + 2.0 << "\" y=\"176\">REVISION</text>"
            << "<text x=\"" << titleX + 2.0 << "\" y=\"186\">" << xml(part.description.isEmpty() ? part.name : part.description) << "</text>"
            << "<text x=\"" << col2 + 2.0 << "\" y=\"186\">" << xml(part.partNumber) << "</text>"
            << "<text x=\"" << col3 + 2.0 << "\" y=\"186\">" << xml(revision) << "</text>"
            << "</g>\n";
        out << "<g font-family=\"Arial,sans-serif\" font-size=\"2.5\" fill=\"#111827\">"
            << "<text x=\"" << titleX + 2.0 << "\" y=\"193\">DRAWING NUMBER</text>"
            << "<text x=\"" << titleX + 2.0 << "\" y=\"198\">" << xml(drawingNumber) << "</text>"
            << "<text x=\"" << titleX + 38.0 << "\" y=\"193\">DRAWN BY</text>"
            << "<text x=\"" << titleX + 38.0 << "\" y=\"198\">" << xml(drawnBy) << "</text>"
            << "<text x=\"" << titleX + 72.0 << "\" y=\"193\">CHECKED BY</text>"
            << "<text x=\"" << titleX + 72.0 << "\" y=\"198\">" << xml(checkedBy) << "</text>"
            << "<text x=\"" << titleX + 106.0 << "\" y=\"193\">DATE</text>"
            << "<text x=\"" << titleX + 106.0 << "\" y=\"198\">" << xml(issueDate) << "</text></g>\n";
        if(!noteAnnotations.isEmpty()){
            double firstX=notePageWidth,firstY=notePageHeight;
            for(const DrawingNoteAnnotation& note:noteAnnotations){firstX=std::min(firstX,note.xMm/sheetScale);firstY=std::min(firstY,note.yMm/sheetScale);}
            out << "<g data-drawing-annotation=\"general-notes\" font-family=\"Arial,sans-serif\" fill=\"#111827\">"
                << "<text x=\"" << firstX << "\" y=\"" << std::max(12.0,firstY-4.0) << "\" font-size=\"3\">GENERAL NOTES</text>";
            for(const DrawingNoteAnnotation& note:noteAnnotations)
                out << "<text data-drawing-annotation=\"general-note\" data-annotation-id=\"" << xml(note.id)
                    << "\" x=\"" << QString::number(note.xMm/sheetScale,'f',3) << "\" y=\""
                    << QString::number(note.yMm/sheetScale,'f',3) << "\" font-size=\"2.7\">"
                    << xml(note.text.trimmed()) << "</text>";
            out << "</g>\n";
        }else if (!generalNotes.isEmpty()) {
            out << "<g data-drawing-annotation=\"general-notes\" font-family=\"Arial,sans-serif\" fill=\"#111827\">"
                << "<text x=\"" << notesX << "\" y=\"175\" font-size=\"3\">GENERAL NOTES</text>";
            for (int i = 0; i < generalNotes.size(); ++i)
                out << "<text x=\"" << notesX << "\" y=\"" << 180 + i * 4 << "\" font-size=\"2.7\">"
                    << xml(generalNotes[i].trimmed()) << "</text>";
            out << "</g>\n";
        }
        if(partsListAnnotation.visible){
            constexpr double tableWidth=153.0,headerHeight=7.0,rowHeight=5.5;
            const double x=partsListAnnotation.xMm/sheetScale,y=partsListAnnotation.yMm/sheetScale;
            const double tableHeight=headerHeight+rowHeight*bomRows.size();
            if(x+tableWidth>notePageWidth/sheetScale-1.0||y+tableHeight>notePageHeight/sheetScale-1.0){
                fail(error,"The linked parts list exceeds the selected sheet. Move it up/left or reduce the BOM to at most 18 rows.");return false;
            }
            out << "<g data-drawing-annotation=\"linked-parts-list\" data-annotation-id=\"" << xml(partsListAnnotation.id)
                << "\" data-source=\"drawing-source-bom\" data-link-status=\"export-snapshot\" font-family=\"Arial,sans-serif\" fill=\"#111827\">"
                << "<text x=\"" << x << "\" y=\"" << y-1.2 << "\" font-size=\"3\">PARTS LIST</text>"
                << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << tableWidth << "\" height=\"" << tableHeight << "\" fill=\"white\" stroke=\"#111827\" stroke-width=\"0.25\"/>"
                << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << tableWidth << "\" height=\"" << headerHeight << "\" fill=\"#e5e7eb\"/>";
            const std::array<double,7> columns={9.0,42.0,87.0,99.0,125.0,142.0,153.0};
            for(double column:columns)out << "<path d=\"M" << x+column << ' ' << y << "V" << y+tableHeight << "\" stroke=\"#6b7280\" stroke-width=\"0.15\"/>";
            for(int row=0;row<=bomRows.size();++row){const double lineY=y+headerHeight+rowHeight*row;out << "<path d=\"M" << x << ' ' << lineY << "H" << x+tableWidth << "\" stroke=\"#6b7280\" stroke-width=\"0.15\"/>";}
            const std::array<QString,7> headers={QStringLiteral("ITEM"),QStringLiteral("PART NO."),QStringLiteral("DESCRIPTION"),QStringLiteral("QTY"),QStringLiteral("MATERIAL"),QStringLiteral("MASS (kg)"),QStringLiteral("REV")};
            const std::array<double,7> starts={1.0,10.0,43.0,88.0,100.0,126.0,143.0};
            for(std::size_t i=0;i<headers.size();++i)out << "<text x=\"" << x+starts[i] << "\" y=\"" << y+4.8 << "\" font-size=\"2.2\" font-weight=\"bold\">" << headers[i] << "</text>";
            for(int i=0;i<bomRows.size();++i){
                const BillOfMaterialsRow& row=bomRows[i];const double baseline=y+headerHeight+rowHeight*i+3.8;
                auto cell=[&out,x,baseline](double start,double availableWidth,const QString& value){QString clipped=value.trimmed();while(!clipped.isEmpty()&&clipped.size()*1.35>availableWidth)clipped.chop(1);out << "<text x=\"" << x+start << "\" y=\"" << baseline << "\" font-size=\"2.2\">" << xml(clipped) << "</text>";};
                const QString unitMassKg=QString::number(row.unitMassKg,'g',6);
                out << "<g data-bom-item-number=\"" << row.itemNumber << "\" data-component-definition-id=\"" << xml(row.componentDefinitionId)
                    << "\" data-bom-unit-mass-kg=\"" << unitMassKg << "\">";
                cell(1.0,7.0,QString::number(row.itemNumber));cell(10.0,30.0,row.partNumber);cell(43.0,42.0,row.description);
                cell(88.0,9.0,QString::number(row.quantity));cell(100.0,24.0,row.materialName);cell(126.0,14.0,unitMassKg);cell(143.0,8.0,row.revision);out << "</g>";
            }
            out << "</g>\n";
        }
        for(const DrawingBalloonAnnotation& balloon:balloonAnnotations){
            int itemNumber=0;for(const BillOfMaterialsRow& row:bomRows)if(row.bodyIds.contains(balloon.sourceBodyId)){itemNumber=row.itemNumber;break;}
            const double bx=balloon.xMm/sheetScale,by=balloon.yMm/sheetScale;
            const double tx=frontX,ty=frontY;
            out << "<g data-drawing-annotation=\"bom-balloon\" data-annotation-id=\"" << xml(balloon.id)
                << "\" data-source-body-id=\"" << xml(balloon.sourceBodyId) << "\" data-bom-item-number=\"" << itemNumber
                << "\" data-link-status=\"export-snapshot\" fill=\"white\" stroke=\"#111827\">"
                << "<path d=\"M" << bx << ' ' << by << 'L' << tx << ' ' << ty << "\" stroke-width=\"0.25\"/>"
                << "<circle cx=\"" << bx << "\" cy=\"" << by << "\" r=\"4\" stroke-width=\"0.3\"/>"
                << "<text x=\"" << bx << "\" y=\"" << by+1.0 << "\" text-anchor=\"middle\" font-size=\"2.8\" stroke=\"none\" fill=\"#111827\">" << itemNumber << "</text></g>\n";
        }
        out << "</g>\n</svg>\n";
        out.flush();
        if (out.status() != QTextStream::Ok || bytes.isEmpty()) { fail(error, "Could not format the drawing SVG."); return false; }
        *svg = bytes;
        if (chosenScale) *chosenScale = actualScale;
        if (visiblePathCount) *visiblePathCount = visible;
        if (hiddenPathCount) *hiddenPathCount = hidden;
        return true;
    } catch (const Standard_Failure& e) {
        fail(error, QStringLiteral("Open CASCADE hidden-line projection failed: %1").arg(QString::fromUtf8(e.what())));
    } catch (const std::exception& e) {
        fail(error, QStringLiteral("Drawing generation failed: %1").arg(QString::fromUtf8(e.what())));
    }
    return false;
}

bool exportOrthographicDrawingSvg(const Part& part, const QString& path, QString* error,
                                  OrthographicProjection projection, DrawingSheetSize sheetSize,
                                  DrawingAuxiliaryView auxiliaryView, double sectionDepthFraction,
                                  const QStringList& generalNotes, const DrawingTitleBlock& titleBlock,
                                  const QMap<QString,QPointF>& viewCenters,const QMap<QString,QString>& viewIds,
                                  const QString& sheetId,const QVector<DrawingNoteAnnotation>& noteAnnotations,
                                  const QVector<DrawingDimensionAnnotation>& dimensionAnnotations,
                                  const DrawingPartsListAnnotation& partsListAnnotation,const QVector<BillOfMaterialsRow>& bomRows,
                                  const QStringList& sourceBodyIds,const QVector<DrawingBalloonAnnotation>& balloonAnnotations,
                                  const QVector<DrawingDetailAnnotation>& detailAnnotations,
                                  const QVector<DrawingSectionAnnotation>& sectionAnnotations) {
    if (path.trimmed().isEmpty()) { fail(error, "Provide an output path for the drawing SVG."); return false; }
    QByteArray bytes;
    if (!buildOrthographicDrawingSvg(part, &bytes, error, nullptr, nullptr, nullptr,
                                     projection, sheetSize, auxiliaryView, sectionDepthFraction, generalNotes, titleBlock,
                                     viewCenters,viewIds,sheetId,noteAnnotations,dimensionAnnotations,partsListAnnotation,bomRows,sourceBodyIds,balloonAnnotations,detailAnnotations,sectionAnnotations)) return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { fail(error, QStringLiteral("Could not create drawing file: %1").arg(file.errorString())); return false; }
    if (file.write(bytes) != bytes.size()) { fail(error, QStringLiteral("Could not write drawing file: %1").arg(file.errorString())); file.cancelWriting(); return false; }
    if (!file.commit()) { fail(error, QStringLiteral("Could not finalize drawing file: %1").arg(file.errorString())); return false; }
    return true;
}

bool exportOrthographicDrawingPdf(const QByteArray& svg,const QString& path,DrawingSheetSize sheetSize,QString* error) {
    if(path.trimmed().isEmpty()){fail(error,"Provide an output path for the drawing PDF.");return false;}
    if(sheetSize!=DrawingSheetSize::A4Landscape&&sheetSize!=DrawingSheetSize::A3Landscape){
        fail(error,"Choose a supported A4 or A3 landscape drawing sheet.");return false;
    }
    QSvgRenderer renderer(svg);
    if(!renderer.isValid()){fail(error,"The drawing SVG is malformed or cannot be rendered.");return false;}
    QByteArray pdfBytes;
    {
        QBuffer buffer(&pdfBytes);
        if(!buffer.open(QIODevice::WriteOnly)){fail(error,"Could not create the in-memory PDF stream.");return false;}
        {
            QPdfWriter writer(&buffer);writer.setCreator(QStringLiteral("CADWorkbench"));
            writer.setTitle(QStringLiteral("CADWorkbench orthographic drawing"));writer.setResolution(300);
            const QPageSize pageSize(sheetSize==DrawingSheetSize::A3Landscape?QPageSize::A3:QPageSize::A4);
            const QPageLayout layout(pageSize,QPageLayout::Landscape,QMarginsF(0,0,0,0),QPageLayout::Millimeter);
            if(!writer.setPageLayout(layout)){fail(error,"Could not set the drawing PDF page size.");return false;}
            QPainter painter(&writer);
            if(!painter.isActive()){fail(error,"Could not render the drawing PDF page.");return false;}
            renderer.render(&painter,QRectF(writer.pageLayout().paintRectPixels(writer.resolution())));
            painter.end();
        }
        buffer.close();
    }
    if(!pdfBytes.startsWith("%PDF-")||!pdfBytes.contains("%%EOF")){
        fail(error,"The PDF renderer did not produce a complete document.");return false;
    }
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly)){fail(error,QStringLiteral("Could not create drawing PDF: %1").arg(file.errorString()));return false;}
    if(file.write(pdfBytes)!=pdfBytes.size()){fail(error,QStringLiteral("Could not write drawing PDF: %1").arg(file.errorString()));file.cancelWriting();return false;}
    if(!file.commit()){fail(error,QStringLiteral("Could not finalize drawing PDF: %1").arg(file.errorString()));return false;}
    return true;
}

StandardsCheckReport checkDrawingSvg(const QByteArray& svg, StandardsProfile profile,const DrawingReadinessPolicy& policy) {
    StandardsCheckReport report;
    report.profile = profile;
    auto add = [&report](FindingSeverity severity, const QString& code, const QString& message) {
        report.findings.append({severity, code, {}, message});
        if (severity == FindingSeverity::Error) ++report.errors;
        else if (severity == FindingSeverity::Warning) ++report.warnings;
    };
    if (profile == StandardsProfile::BomData) {
        add(FindingSeverity::Error, "DRAWING_PROFILE_INVALID", "Choose the ISO or ASME drawing-readiness profile for an exported drawing.");
        return report;
    }
    if (svg.trimmed().isEmpty()) {
        add(FindingSeverity::Error, "DRAWING_SVG_EMPTY", "The drawing file is empty.");
        return report;
    }

    QXmlStreamReader reader(svg);
    QStringList texts;
    QString widthValue, heightValue, viewBox;
    QHash<QString, QPointF> viewLabelPositions;
    QString projectionSymbol;
    double projectionCircleX = std::numeric_limits<double>::quiet_NaN();
    double drawingScaleFactor = 1.0;
    int projectionSymbolCount = 0;
    bool insideProjectionSymbol = false;
    bool checkedRoot = false;
    bool validRoot = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (!checkedRoot && reader.isStartElement()) {
            checkedRoot = true;
            validRoot = reader.name() == QStringLiteral("svg");
            widthValue = reader.attributes().value(QStringLiteral("width")).toString();
            heightValue = reader.attributes().value(QStringLiteral("height")).toString();
            viewBox = reader.attributes().value(QStringLiteral("viewBox")).toString();
        }
        if (validRoot && reader.isStartElement()) {
            if (reader.name() == QStringLiteral("g") && reader.attributes().hasAttribute(QStringLiteral("data-projection-symbol"))) {
                ++projectionSymbolCount;
                projectionSymbol = reader.attributes().value(QStringLiteral("data-projection-symbol")).toString();
                insideProjectionSymbol = true;
            } else if (reader.name() == QStringLiteral("g")) {
                const QString transform = reader.attributes().value(QStringLiteral("transform")).toString();
                const QRegularExpression scaleTransform(QStringLiteral("^scale\\(([0-9]+(?:\\.[0-9]+)?)\\)$"));
                const auto scaleMatch = scaleTransform.match(transform);
                if (scaleMatch.hasMatch()) {
                    bool ok = false;
                    const double scale = scaleMatch.captured(1).toDouble(&ok);
                    if (ok && std::isfinite(scale) && scale > 0.0) drawingScaleFactor = scale;
                }
            } else if (insideProjectionSymbol && reader.name() == QStringLiteral("circle")) {
                bool xOk = false;
                const double x = reader.attributes().value(QStringLiteral("cx")).toDouble(&xOk);
                if (xOk && std::isfinite(x)) projectionCircleX = x;
            } else if (reader.name() == QStringLiteral("text")) {
                const QString xText = reader.attributes().value(QStringLiteral("x")).toString();
                const QString yText = reader.attributes().value(QStringLiteral("y")).toString();
                bool xOk = false, yOk = false;
                const double x = xText.toDouble(&xOk), y = yText.toDouble(&yOk);
                const QString value = reader.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
                texts.append(value);
                if (xOk && yOk && std::isfinite(x) && std::isfinite(y)
                    && (value == QStringLiteral("FRONT") || value == QStringLiteral("TOP") || value == QStringLiteral("RIGHT")))
                    viewLabelPositions.insert(value, QPointF(x, y));
            }
        } else if (validRoot && reader.isEndElement() && reader.name() == QStringLiteral("g") && insideProjectionSymbol) {
            insideProjectionSymbol = false;
        }
    }
    if (!validRoot || reader.hasError()) {
        add(FindingSeverity::Error, "DRAWING_SVG_INVALID", "The selected file is not a well-formed SVG drawing.");
        return report;
    }
    auto hasText = [&texts](const QString& expected) { return texts.contains(expected); };
    auto hasDimension = [&texts](const QString& axis) {
        const QRegularExpression pattern(QStringLiteral("^%1 ([0-9]+(?:\\.[0-9]+)?) mm$").arg(axis));
        return std::any_of(texts.cbegin(), texts.cend(), [&pattern](const QString& value) {
            const auto match = pattern.match(value);
            if (!match.hasMatch()) return false;
            bool ok = false;
            const double dimension = match.captured(1).toDouble(&ok);
            return ok && std::isfinite(dimension) && dimension > 0.0;
        });
    };
    const QRegularExpression mmValue(QStringLiteral("^([0-9]+(?:\\.[0-9]+)?)mm$"));
    const auto widthMatch = mmValue.match(widthValue), heightMatch = mmValue.match(heightValue);
    bool supportedSheet = false;
    if (widthMatch.hasMatch() && heightMatch.hasMatch()) {
        const double widthMm = widthMatch.captured(1).toDouble();
        const double heightMm = heightMatch.captured(1).toDouble();
        const QStringList boxParts = viewBox.trimmed().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        bool boxValid = false;
        if (boxParts.size() == 4) {
            bool xOk = false, yOk = false, boxWidthOk = false, boxHeightOk = false;
            const double x = boxParts[0].toDouble(&xOk), y = boxParts[1].toDouble(&yOk);
            const double boxWidth = boxParts[2].toDouble(&boxWidthOk), boxHeight = boxParts[3].toDouble(&boxHeightOk);
            boxValid = xOk && yOk && boxWidthOk && boxHeightOk && std::abs(x) < 0.01 && std::abs(y) < 0.01
                && std::abs(boxWidth - widthMm) < 0.01 && std::abs(boxHeight - heightMm) < 0.01;
        }
        supportedSheet = boxValid && ((std::abs(widthMm - 297.0) < 0.01 && std::abs(heightMm - 210.0) < 0.01)
            || (std::abs(widthMm - 420.0) < 0.01 && std::abs(heightMm - 297.0) < 0.01));
    }
    if (!supportedSheet || viewBox.trimmed().isEmpty())
        add(FindingSeverity::Warning, "DRAWING_SHEET_FORMAT", "Sheet size is missing or is not a recognized A4/A3 landscape metric sheet.");
    for (const QString& view : {QStringLiteral("FRONT"), QStringLiteral("TOP"), QStringLiteral("RIGHT")})
        if (!hasText(view)) add(FindingSeverity::Error, "DRAWING_VIEW_MISSING", QStringLiteral("Required orthographic view '%1' is missing.").arg(view));
    if (!hasText(QStringLiteral("ISOMETRIC")) && !hasText(QStringLiteral("SECTION A-A")))
        add(FindingSeverity::Warning, "DRAWING_AUXILIARY_VIEW_MISSING", "Neither an isometric nor a section auxiliary view label was found.");
    const bool projectionLabel = std::any_of(texts.cbegin(), texts.cend(), [](const QString& value) {
        return value.startsWith(QStringLiteral("FIRST-ANGLE")) || value.startsWith(QStringLiteral("THIRD-ANGLE"));
    });
    if (!projectionLabel) add(FindingSeverity::Warning, "DRAWING_PROJECTION_UNSPECIFIED", "Projection convention is not identified in the drawing heading.");
    const bool firstAngle = std::any_of(texts.cbegin(), texts.cend(), [](const QString& value) {
        return value.startsWith(QStringLiteral("FIRST-ANGLE"));
    });
    const bool thirdAngle = std::any_of(texts.cbegin(), texts.cend(), [](const QString& value) {
        return value.startsWith(QStringLiteral("THIRD-ANGLE"));
    });
    const QString expectedSymbol = firstAngle ? QStringLiteral("first-angle") : QStringLiteral("third-angle");
    if (projectionSymbolCount == 0) {
        add(FindingSeverity::Warning, "DRAWING_PROJECTION_SYMBOL_MISSING", "A schematic projection symbol is not present.");
    } else if (projectionSymbolCount != 1 || (firstAngle == thirdAngle) || projectionSymbol != expectedSymbol) {
        add(FindingSeverity::Error, "DRAWING_PROJECTION_SYMBOL_CONFLICT", "Projection heading and projection-symbol identity are missing, duplicated, or inconsistent.");
    } else if (std::isfinite(projectionCircleX)) {
        bool widthOk = false;
        const double sheetWidthMm = widthMatch.hasMatch() ? widthMatch.captured(1).toDouble(&widthOk) : 0.0;
        const double circleXOnSheet = projectionCircleX * drawingScaleFactor;
        const bool expectedSide = widthOk && (firstAngle ? circleXOnSheet > sheetWidthMm * 0.9
                                                         : circleXOnSheet < sheetWidthMm * 0.9);
        if (!expectedSide)
            add(FindingSeverity::Error, "DRAWING_PROJECTION_SYMBOL_PLACEMENT", "The projection symbol's circular view is on the wrong side for the stated convention.");
    } else {
        add(FindingSeverity::Error, "DRAWING_PROJECTION_SYMBOL_INCOMPLETE", "The projection symbol does not contain a readable circular end view.");
    }
    if (firstAngle != thirdAngle && viewLabelPositions.contains(QStringLiteral("FRONT"))
        && viewLabelPositions.contains(QStringLiteral("TOP")) && viewLabelPositions.contains(QStringLiteral("RIGHT"))) {
        const QPointF front = viewLabelPositions.value(QStringLiteral("FRONT"));
        const QPointF top = viewLabelPositions.value(QStringLiteral("TOP"));
        const QPointF right = viewLabelPositions.value(QStringLiteral("RIGHT"));
        const bool correctLayout = firstAngle ? (top.y() > front.y() && right.x() < front.x())
                                              : (top.y() < front.y() && right.x() > front.x());
        if (!correctLayout)
            add(FindingSeverity::Error, "DRAWING_PROJECTION_LAYOUT", "Orthographic view placement conflicts with the stated first-angle or third-angle convention.");
    }
    for (const QString& axis : {QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")})
        if (!hasDimension(axis)) add(FindingSeverity::Warning, "DRAWING_ENVELOPE_DIMENSION_MISSING", QStringLiteral("Overall %1 envelope dimension is missing.").arg(axis));

    const qsizetype descriptionLabel = texts.indexOf(QStringLiteral("DESCRIPTION"));
    if (descriptionLabel < 0 || descriptionLabel + 3 >= texts.size() || texts.at(descriptionLabel + 3).isEmpty())
        add(FindingSeverity::Warning, "DRAWING_DESCRIPTION_MISSING", "Title-block description is missing.");
    if (descriptionLabel < 0 || descriptionLabel + 4 >= texts.size() || texts.at(descriptionLabel + 4).isEmpty())
        add(FindingSeverity::Warning, "DRAWING_PART_NUMBER_MISSING", "Title-block part number is missing.");
    if (descriptionLabel < 0 || descriptionLabel + 5 >= texts.size() || texts.at(descriptionLabel + 5).isEmpty())
        add(FindingSeverity::Warning, "DRAWING_REVISION_MISSING", "Title-block revision is missing.");
    const qsizetype drawingNumberLabel = texts.indexOf(QStringLiteral("DRAWING NUMBER"));
    if (drawingNumberLabel < 0 || drawingNumberLabel + 1 >= texts.size() || texts.at(drawingNumberLabel + 1).isEmpty())
        add(FindingSeverity::Warning, "DRAWING_NUMBER_MISSING", "Title-block drawing number is missing.");
    const qsizetype issueDateLabel = texts.indexOf(QStringLiteral("DATE"));
    if (issueDateLabel >= 0 && issueDateLabel + 1 < texts.size() && !texts.at(issueDateLabel + 1).isEmpty()) {
        const QString dateText = texts.at(issueDateLabel + 1);
        const QDate parsedDate = QDate::fromString(dateText, Qt::ISODate);
        if (!parsedDate.isValid() || parsedDate.toString(Qt::ISODate) != dateText)
            add(FindingSeverity::Warning, "DRAWING_ISSUE_DATE_FORMAT", "Title-block issue date must use YYYY-MM-DD format.");
    }
    const bool materialField = std::any_of(texts.cbegin(), texts.cend(), [](const QString& value) {
        return value.startsWith(QStringLiteral("MATERIAL:")) && value.size() > QStringLiteral("MATERIAL:").size();
    });
    if (!materialField) add(FindingSeverity::Warning, "DRAWING_MATERIAL_MISSING", "Title-block material is missing.");
    const QHash<QString,bool> titleBlockFields={
        {QStringLiteral("description"),descriptionLabel>=0&&descriptionLabel+3<texts.size()&&!texts.at(descriptionLabel+3).isEmpty()},
        {QStringLiteral("part_number"),descriptionLabel>=0&&descriptionLabel+4<texts.size()&&!texts.at(descriptionLabel+4).isEmpty()},
        {QStringLiteral("revision"),descriptionLabel>=0&&descriptionLabel+5<texts.size()&&!texts.at(descriptionLabel+5).isEmpty()},
        {QStringLiteral("drawing_number"),drawingNumberLabel>=0&&drawingNumberLabel+1<texts.size()&&!texts.at(drawingNumberLabel+1).isEmpty()},
        {QStringLiteral("drawn_by"),texts.indexOf(QStringLiteral("DRAWN BY"))>=0&&texts.indexOf(QStringLiteral("DRAWN BY"))+1<texts.size()&&!texts.at(texts.indexOf(QStringLiteral("DRAWN BY"))+1).isEmpty()},
        {QStringLiteral("checked_by"),texts.indexOf(QStringLiteral("CHECKED BY"))>=0&&texts.indexOf(QStringLiteral("CHECKED BY"))+1<texts.size()&&!texts.at(texts.indexOf(QStringLiteral("CHECKED BY"))+1).isEmpty()},
        {QStringLiteral("issue_date"),issueDateLabel>=0&&issueDateLabel+1<texts.size()&&!texts.at(issueDateLabel+1).isEmpty()},
        {QStringLiteral("material"),materialField}
    };
    const QSet<QString> validTitleFields={QStringLiteral("description"),QStringLiteral("part_number"),QStringLiteral("revision"),
        QStringLiteral("drawing_number"),QStringLiteral("drawn_by"),QStringLiteral("checked_by"),QStringLiteral("issue_date"),QStringLiteral("material")};
    QSet<QString> seenRequiredFields;
    for(const QString& field:policy.requiredTitleBlockFields){
        if(!validTitleFields.contains(field)||seenRequiredFields.contains(field)){
            add(FindingSeverity::Error,"DRAWING_POLICY_INVALID_FIELD",QStringLiteral("Unsupported or duplicate required title-block field '%1'.").arg(field));continue;
        }
        seenRequiredFields.insert(field);
        if(!titleBlockFields.value(field))add(FindingSeverity::Error,"DRAWING_POLICY_REQUIRED_FIELD_MISSING",QStringLiteral("Project policy requires title-block field '%1'.").arg(field));
    }
    const QSet<QString> validConventions={QStringLiteral("envelope_dimensions"),QStringLiteral("projection_symbol"),
        QStringLiteral("auxiliary_view"),QStringLiteral("general_notes")};
    QSet<QString> seenConventions;
    for(const QString& convention:policy.requiredAnnotationConventions){
        if(!validConventions.contains(convention)||seenConventions.contains(convention)){
            add(FindingSeverity::Error,"DRAWING_POLICY_INVALID_ANNOTATION",QStringLiteral("Unsupported or duplicate required annotation convention '%1'.").arg(convention));continue;
        }
        seenConventions.insert(convention);
        bool present=true;
        if(convention==QStringLiteral("envelope_dimensions"))present=hasDimension(QStringLiteral("X"))&&hasDimension(QStringLiteral("Y"))&&hasDimension(QStringLiteral("Z"));
        else if(convention==QStringLiteral("projection_symbol"))present=projectionSymbolCount==1;
        else if(convention==QStringLiteral("auxiliary_view"))present=hasText(QStringLiteral("ISOMETRIC"))||hasText(QStringLiteral("SECTION A-A"));
        else if(convention==QStringLiteral("general_notes"))present=hasText(QStringLiteral("GENERAL NOTES"));
        if(!present)add(FindingSeverity::Error,"DRAWING_POLICY_REQUIRED_ANNOTATION_MISSING",QStringLiteral("Project policy requires the '%1' annotation convention.").arg(convention));
    }
    const QString profileName = profile == StandardsProfile::IsoDrawingReadiness ? QStringLiteral("ISO") : QStringLiteral("ASME");
    add(FindingSeverity::Info, "DRAWING_CHECK_SCOPE", QStringLiteral("%1 drawing content screen only: checks SVG structure, sheet labels, positive envelope-dimension values, projection label/symbol/view placement, and title-block data. It does not verify standard clauses, licensed tolerance tables, datums, or GD&T and is not standards certification.").arg(profileName));
    return report;
}

StandardsCheckReport checkDrawingSourceSnapshot(const Part& currentPart,const QByteArray& svg){
    return checkDrawingSourceSnapshot(QVector<Part>{currentPart},svg);
}

StandardsCheckReport checkDrawingSourceSnapshot(const QVector<Part>& currentParts,const QByteArray& svg){
    StandardsCheckReport report;report.profile=StandardsProfile::IsoDrawingReadiness;
    auto add=[&](FindingSeverity severity,const QString& code,const QString& message){
        report.findings.append({severity,code,currentParts.isEmpty()?QString():currentParts.first().id,message});
        if(severity==FindingSeverity::Error)++report.errors;else if(severity==FindingSeverity::Warning)++report.warnings;
    };
    if(currentParts.isEmpty()||currentParts.size()>50){add(FindingSeverity::Error,"DRAWING_SOURCE_SELECTION_INVALID","Select between one and fifty current source bodies to compare with this drawing.");return report;}
    if(svg.trimmed().isEmpty()){add(FindingSeverity::Error,"DRAWING_SOURCE_EMPTY","The drawing file is empty.");return report;}
    QXmlStreamReader reader(svg);QString sourceId,sourceToken,linkStatus,sourceIdsAttribute;bool foundRoot=false;
    while(!reader.atEnd()){
        reader.readNext();if(!reader.isStartElement()||foundRoot)continue;
        foundRoot=true;
        if(reader.name()!=QStringLiteral("svg")){add(FindingSeverity::Error,"DRAWING_SOURCE_INVALID","The selected file does not have an SVG root element.");return report;}
        const auto attributes=reader.attributes();
        sourceId=attributes.value(QStringLiteral("data-cadworkbench-source-body-id")).toString();
        sourceIdsAttribute=attributes.value(QStringLiteral("data-cadworkbench-source-body-ids")).toString();
        sourceToken=attributes.value(QStringLiteral("data-cadworkbench-source-geometry-token")).toString();
        linkStatus=attributes.value(QStringLiteral("data-cadworkbench-link-status")).toString();
    }
    if(!foundRoot||reader.hasError()){add(FindingSeverity::Error,"DRAWING_SOURCE_INVALID","The selected file is not a readable SVG snapshot.");return report;}
    if(linkStatus!=QStringLiteral("export-snapshot"))add(FindingSeverity::Warning,"DRAWING_LINK_STATUS_UNKNOWN","The drawing does not identify itself as a CADWorkbench export snapshot.");
    if(sourceToken.isEmpty()){add(FindingSeverity::Warning,"DRAWING_SOURCE_IDENTITY_MISSING","The drawing has no model revision that can be compared.");return report;}
    QStringList sourceIds=sourceIdsAttribute.split(QLatin1Char(','),Qt::SkipEmptyParts);
    if(sourceIds.isEmpty()&&!sourceId.isEmpty())sourceIds.append(sourceId);
    QStringList currentIds;for(const Part& part:currentParts)currentIds.append(part.id);
    sourceIds.removeDuplicates();currentIds.removeDuplicates();
    std::sort(sourceIds.begin(),sourceIds.end());std::sort(currentIds.begin(),currentIds.end());
    if(sourceIds.isEmpty()){add(FindingSeverity::Warning,"DRAWING_SOURCE_IDENTITY_MISSING","The drawing has no source body identity that can be compared.");return report;}
    if(sourceIds!=currentIds){add(FindingSeverity::Error,"DRAWING_SOURCE_BODY_MISMATCH","The drawing snapshot source body set differs from the selected model bodies.");return report;}
    QString currentToken;
    if(currentParts.size()==1&&sourceIdsAttribute.isEmpty())currentToken=currentParts.first().geometryToken;
    else if(currentParts.size()==1&&sourceToken==currentParts.first().geometryToken)currentToken=currentParts.first().geometryToken;
    else {Part composite;QString error;if(!buildDrawingAssemblyPart(currentParts,&composite,&error)){add(FindingSeverity::Error,"DRAWING_SOURCE_SELECTION_INVALID",error);return report;}currentToken=composite.geometryToken;}
    if(sourceToken!=currentToken){add(FindingSeverity::Warning,"DRAWING_SOURCE_GEOMETRY_STALE","One or more source bodies changed after this drawing snapshot was exported; regenerate the SVG.");return report;}
    add(FindingSeverity::Info,"DRAWING_SOURCE_CURRENT","The export snapshot matches the selected source body set and current geometry. It does not update live when the model changes.");
    return report;
}
} // namespace cad

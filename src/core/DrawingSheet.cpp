#include "DrawingSheet.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <HLRAlgo_Projector.hxx>
#include <HLRBRep_Algo.hxx>
#include <HLRBRep_HLRToShape.hxx>
#include <HLRBRep_TypeOfResultingEdge.hxx>
#include <QHash>
#include <QDate>
#include <QPointF>
#include <QSaveFile>
#include <QLocale>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QTextStream>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
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
struct DiameterCallout { Point2 center; double radius = 0.0; };
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

ViewGeometry projectSectionAtDepth(const Part& part, double sectionDepthFraction, double* sectionYOut) {
    ViewGeometry view;
    Bnd_Box bounds;
    BRepBndLib::AddOptimal(part.shape, bounds, false, false);
    if (bounds.IsVoid()) return view;
    double minX, minY, minZ, maxX, maxY, maxZ;
    bounds.Get(minX, minY, minZ, maxX, maxY, maxZ);
    const double sectionY = minY + (maxY - minY) * sectionDepthFraction;
    if (sectionYOut) *sectionYOut = sectionY;
    BRepAlgoAPI_Section section(part.shape, gp_Pln(gp_Pnt(0, sectionY, 0), gp_Dir(0, 1, 0)), false);
    section.Build();
    if (!section.IsDone() || section.HasErrors() || section.Shape().IsNull()) return view;
    collectEdges(section.Shape(), false, view, true);
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
            if (!alreadyDimensioned) view.diameters.append({center, cylinder.Radius()});
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
                double cx, double cy, double scale, int* visible, int* hidden) {
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
        out << "<g fill=\"none\" stroke=\"#1f2937\" stroke-width=\"0.18\"><path d=\"M"
            << startX << ' ' << startY << "L" << textX - 1.0 << ' ' << textY + 1.0
            << "\"/></g><text x=\"" << textX << "\" y=\"" << textY
            << "\" font-family=\"Arial,sans-serif\" font-size=\"3\" fill=\"#111827\">Ø"
            << QString::number(callout.radius * 2.0, 'f', 3) << " mm</text>\n";
    }
    out << "<text x=\"" << QString::number(cx, 'f', 2) << "\" y=\""
        << QString::number(cy + std::max(4.0, height(view) * scale * 0.5 + 23.0), 'f', 2)
        << "\" text-anchor=\"middle\" font-family=\"Arial,sans-serif\" font-size=\"3.5\">"
        << xml(label) << "</text>\n";
}

void renderHorizontalDimension(QTextStream& out, const ViewGeometry& view, double cx, double cy,
                               double scale, const QString& axis) {
    const double halfWidth = width(view) * scale * 0.5;
    const double edgeY = cy + height(view) * scale * 0.5;
    const double y = edgeY + 9.0;
    const double x1 = cx - halfWidth, x2 = cx + halfWidth;
    out << "<g fill=\"none\" stroke=\"#1f2937\" stroke-width=\"0.18\">"
        << "<path d=\"M" << x1 << ' ' << edgeY + 1.0 << "V" << y + 1.5
        << " M" << x2 << ' ' << edgeY + 1.0 << "V" << y + 1.5
        << " M" << x1 << ' ' << y << "H" << x2 << "\"/>"
        << "<path fill=\"#1f2937\" d=\"M" << x1 << ' ' << y << "l2 -0.8v1.6z M"
        << x2 << ' ' << y << "l-2 -0.8v1.6z\"/></g>"
        << "<text x=\"" << cx << "\" y=\"" << y - 1.2
        << "\" text-anchor=\"middle\" font-family=\"Arial,sans-serif\" font-size=\"3\" fill=\"#111827\">"
        << axis << " " << QString::number(width(view), 'f', 3) << " mm</text>\n";
}

void renderVerticalDimension(QTextStream& out, const ViewGeometry& view, double cx, double cy,
                             double scale, const QString& axis) {
    const double halfHeight = height(view) * scale * 0.5;
    const double edgeX = cx - width(view) * scale * 0.5;
    const double x = edgeX - 9.0;
    const double y1 = cy - halfHeight, y2 = cy + halfHeight;
    out << "<g fill=\"none\" stroke=\"#1f2937\" stroke-width=\"0.18\">"
        << "<path d=\"M" << edgeX - 1.0 << ' ' << y1 << "H" << x - 1.5
        << " M" << edgeX - 1.0 << ' ' << y2 << "H" << x - 1.5
        << " M" << x << ' ' << y1 << "V" << y2 << "\"/>"
        << "<path fill=\"#1f2937\" d=\"M" << x << ' ' << y1 << "l-0.8 2h1.6z M"
        << x << ' ' << y2 << "l-0.8 -2h1.6z\"/></g>"
        << "<text transform=\"rotate(-90 " << x - 1.2 << ' ' << cy << ")\" x=\"" << x - 1.2
        << "\" y=\"" << cy << "\" text-anchor=\"middle\" font-family=\"Arial,sans-serif\" font-size=\"3\" fill=\"#111827\">"
        << axis << " " << QString::number(height(view), 'f', 3) << " mm</text>\n";
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

bool buildOrthographicDrawingSvg(const Part& part, QByteArray* svg, QString* error,
                                 double* chosenScale, int* visiblePathCount, int* hiddenPathCount,
                                 OrthographicProjection projection, DrawingSheetSize sheetSize,
                                 DrawingAuxiliaryView auxiliaryView, double sectionDepthFraction,
                                 const QStringList& generalNotes, const DrawingTitleBlock& titleBlock) {
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
            ? projectSectionAtDepth(part, sectionDepthFraction, &sectionY) : ViewGeometry{};
        const ViewGeometry& auxiliary = auxiliaryView == DrawingAuxiliaryView::SectionA ? section : isometric;
        if (!front.hasBounds || !top.hasBounds || !right.hasBounds || !auxiliary.hasBounds) {
            fail(error, auxiliaryView == DrawingAuxiliaryView::SectionA
                ? "The section view at the body's mid-depth did not produce a closed section outline."
                : "Hidden-line projection did not produce complete front, top, right, and isometric views."); return false;
        }
        double scale = 1.0;
        auto fit = [&scale](double available, double extent) { if (extent > 1e-9) scale = std::min(scale, available / extent); };
        fit(118.0, width(front)); fit(68.0, height(front));
        fit(118.0, width(top)); fit(38.0, height(top));
        fit(78.0, width(right)); fit(68.0, height(right));
        fit(78.0, width(auxiliary)); fit(45.0, height(auxiliary));
        scale = std::clamp(scale, 1e-5, 1.0);

        // Third-angle places the top and right-side views on the corresponding sides
        // of the front view; first-angle reverses both placements.
        const bool thirdAngle = projection == OrthographicProjection::ThirdAngle;
        const double topX = thirdAngle ? 79.0 : 155.0;
        const double topY = thirdAngle ? 47.0 : 143.0;
        const double frontX = thirdAngle ? 79.0 : 155.0;
        const double frontY = thirdAngle ? 125.0 : 70.0;
        const double rightX = thirdAngle ? 204.0 : 45.0;
        const double rightY = frontY;
        const double isoX = 240.0;
        const double isoY = thirdAngle ? 47.0 : 145.0;
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
        const bool a3 = sheetSize == DrawingSheetSize::A3Landscape;
        const double sheetScale = a3 ? (420.0 / 297.0) : 1.0;
        const double sheetWidth = a3 ? 420.0 : 297.0;
        const double sheetHeight = a3 ? 297.0 : 210.0;
        const double actualScale = scale * sheetScale;
        const QString scaleLabel = actualScale >= 1.0
            ? QString("SCALE %1:1").arg(actualScale, 0, 'f', 2)
            : QString("SCALE 1:%1").arg(1.0 / actualScale, 0, 'f', 2);

        QByteArray bytes;
        QTextStream out(&bytes, QIODevice::WriteOnly);
        out.setLocale(QLocale::c());
        out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << sheetWidth << "mm\" height=\""
            << sheetHeight << "mm\" viewBox=\"0 0 " << sheetWidth << ' ' << sheetHeight << "\">\n"
            << "<rect x=\"0\" y=\"0\" width=\"" << sheetWidth << "\" height=\"" << sheetHeight << "\" fill=\"white\"/>\n"
            << "<g transform=\"scale(" << QString::number(sheetScale, 'f', 6) << ")\">\n"
            << "<rect x=\"10\" y=\"10\" width=\"277\" height=\"190\" fill=\"none\" stroke=\"#111827\" stroke-width=\"0.35\"/>\n"
            << "<text x=\"12\" y=\"17\" font-family=\"Arial,sans-serif\" font-size=\"3.5\">" << projectionName
            << " TECHNICAL DRAWING - OVERALL ENVELOPE DIMENSIONS ONLY</text>\n"
            << "<text x=\"12\" y=\"23\" font-family=\"Arial,sans-serif\" font-size=\"3\">MATERIAL: "
            << xml(part.materialName) << " | " << scaleLabel << " | UNITS: mm</text>\n";
        int visible = 0, hidden = 0;
        renderView(out, top, "TOP", topX, topY, scale, &visible, &hidden);
        renderView(out, front, "FRONT", frontX, frontY, scale, &visible, &hidden);
        renderView(out, right, "RIGHT", rightX, rightY, scale, &visible, &hidden);
        renderView(out, auxiliary, sectionSelected ? "SECTION A-A" : "ISOMETRIC", isoX, isoY,
                   scale, &visible, &hidden);
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
        renderHorizontalDimension(out, front, frontX, frontY, scale, "X");
        renderVerticalDimension(out, front, frontX, frontY, scale, "Z");
        renderVerticalDimension(out, top, topX, topY, scale, "Y");
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
        if (!generalNotes.isEmpty()) {
            out << "<g font-family=\"Arial,sans-serif\" fill=\"#111827\">"
                << "<text x=\"" << notesX << "\" y=\"175\" font-size=\"3\">GENERAL NOTES</text>";
            for (int i = 0; i < generalNotes.size(); ++i)
                out << "<text x=\"" << notesX << "\" y=\"" << 180 + i * 4 << "\" font-size=\"2.7\">"
                    << xml(generalNotes[i].trimmed()) << "</text>";
            out << "</g>\n";
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
                                  const QStringList& generalNotes, const DrawingTitleBlock& titleBlock) {
    if (path.trimmed().isEmpty()) { fail(error, "Provide an output path for the drawing SVG."); return false; }
    QByteArray bytes;
    if (!buildOrthographicDrawingSvg(part, &bytes, error, nullptr, nullptr, nullptr,
                                     projection, sheetSize, auxiliaryView, sectionDepthFraction, generalNotes, titleBlock)) return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) { fail(error, QStringLiteral("Could not create drawing file: %1").arg(file.errorString())); return false; }
    if (file.write(bytes) != bytes.size()) { fail(error, QStringLiteral("Could not write drawing file: %1").arg(file.errorString())); file.cancelWriting(); return false; }
    if (!file.commit()) { fail(error, QStringLiteral("Could not finalize drawing file: %1").arg(file.errorString())); return false; }
    return true;
}

StandardsCheckReport checkDrawingSvg(const QByteArray& svg, StandardsProfile profile) {
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
    const QString profileName = profile == StandardsProfile::IsoDrawingReadiness ? QStringLiteral("ISO") : QStringLiteral("ASME");
    add(FindingSeverity::Info, "DRAWING_CHECK_SCOPE", QStringLiteral("%1 drawing content screen only: checks SVG structure, sheet labels, positive envelope-dimension values, projection label/symbol/view placement, and title-block data. It does not verify standard clauses, licensed tolerance tables, datums, or GD&T and is not standards certification.").arg(profileName));
    return report;
}
} // namespace cad

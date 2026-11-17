#pragma once

#include <QByteArray>
#include <QString>
#include <QXmlStreamReader>
#include <cmath>

// Extract the *same* measured nominal rendered by the actual drawing-export
// pipeline, rather than inventing a second measurement or using an edge's
// bounding box. This can be used in preflight without committing revisions.
namespace cad::drawing_measurement {
inline bool extractSelectedEdgeNominal(const QByteArray& drawingSvg,
                                      const QString& annotationId,
                                      double* nominalMm,QString* error=nullptr){
    const auto fail=[&](const QString& message){if(error)*error=message;return false;};
    if(!nominalMm||annotationId.trimmed().isEmpty()||drawingSvg.isEmpty())
        return fail("The measurement requires a nonempty drawing, ID and output.");
    QXmlStreamReader xml(drawingSvg);
    double measured=0.0;
    int occurrences=0;
    while(!xml.atEnd()){
        xml.readNext();
        if(!xml.isStartElement())continue;
        const auto attributes=xml.attributes();
        if(attributes.value(QStringLiteral("data-drawing-annotation")).toString()!=
            QStringLiteral("selected-edge-dimension")||
           attributes.value(QStringLiteral("data-annotation-id")).toString()!=annotationId)continue;
        if(++occurrences>1)
            return fail("The drawing contains duplicate dimension IDs; measurement is ambiguous.");
        bool ok=false;
        const double value=attributes.value(QStringLiteral("data-value-mm")).toString().toDouble(&ok);
        if(!ok||!std::isfinite(value)||std::abs(value)>1e9)
            return fail("The current drawing dimension has an invalid nominal value.");
        measured=value;
    }
    if(xml.hasError())return fail("The drawing preview is not valid SVG/XML.");
    if(occurrences!=1)return fail("The requested dimension does not appear exactly once in the drawing preview.");
    *nominalMm=measured;
    if(error)error->clear();
    return true;
}
} // namespace cad::drawing_measurement

#pragma once

#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QColor>
#include <QPen>
#include <QPolygonF>
#include <QString>

// Original vector glyphs, deliberately independent from proprietary CAD icons.
// All commands, including the unmodified menu actions, can reuse these glyphs.
namespace cad::ui {
inline QIcon commandGlyph(const QString& text) {
    const QString name=text.toLower();
    QPixmap image(48,48);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor ink(47,91,111), accent(19,136,165);
    p.setPen(QPen(ink,2.6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    if(name.contains("box") || name.contains("solid") || name.contains("primitive") || name.contains("body")) {
        QPainterPath top;top.moveTo(8,15);top.lineTo(26,7);top.lineTo(40,16);top.lineTo(22,25);top.closeSubpath();p.drawPath(top);
        p.drawLine(QPointF(8,15),QPointF(8,35));p.drawLine(QPointF(22,25),QPointF(22,43));
        p.drawLine(QPointF(40,16),QPointF(40,35));p.drawLine(QPointF(8,35),QPointF(22,43));
        p.drawLine(QPointF(22,43),QPointF(40,35));
    } else if(name.contains("sketch") || name.contains("profile") || name.contains("polygon")) {
        p.drawRect(QRectF(8,10,31,30));
        p.setPen(QPen(accent,2.5));
        p.drawLine(10,34,32,15);p.drawEllipse(QPointF(10,34),2.5,2.5);p.drawEllipse(QPointF(32,15),2.5,2.5);
    } else if(name.contains("fillet") || name.contains("chamfer") || name.contains("blend")) {
        QPainterPath corner;corner.moveTo(8,40);corner.lineTo(8,14);corner.lineTo(32,14);
        p.drawPath(corner);p.setPen(QPen(accent,3.2));
        if(name.contains("chamfer"))p.drawLine(8,32,25,14);
        else {QPainterPath curve;curve.moveTo(8,32);curve.quadTo(8,14,26,14);p.drawPath(curve);}
    } else if(name.contains("hole") || name.contains("cylinder") || name.contains("countersink") || name.contains("counterbore")) {
        p.drawEllipse(QRectF(6,6,36,36));p.setPen(QPen(accent,3));p.drawEllipse(QRectF(17,17,14,14));
    } else if(name.contains("assembly") || name.contains("joint") || name.contains("mate") || name.contains("component")) {
        p.drawRoundedRect(QRectF(5,15,19,14),4,4);p.drawRoundedRect(QRectF(24,20,19,14),4,4);
        p.setPen(QPen(accent,3));p.drawLine(QPointF(18,25),QPointF(31,25));
    } else if(name.contains("drawing") || name.contains("dimension") || name.contains("sheet") || name.contains("export")) {
        p.drawRect(QRectF(11,5,27,38));p.drawLine(QPointF(17,16),QPointF(31,16));
        p.setPen(QPen(accent,2.5));p.drawLine(QPointF(17,27),QPointF(32,27));
        p.drawLine(QPointF(17,23),QPointF(17,31));p.drawLine(QPointF(32,23),QPointF(32,31));
    } else if(name.contains("measure") || name.contains("inspect") || name.contains("audit") || name.contains("distance")) {
        p.drawLine(QPointF(9,35),QPointF(36,12));
        p.drawLine(QPointF(6,28),QPointF(14,42));p.drawLine(QPointF(29,5),QPointF(42,20));
        p.setPen(QPen(accent,2));p.drawEllipse(QPointF(22,23),4,4);
    } else {
        p.drawRoundedRect(QRectF(8,8,32,32),7,7);
        p.setPen(QPen(accent,3));
        p.drawLine(QPointF(16,24),QPointF(32,24));p.drawLine(QPointF(24,16),QPointF(24,32));
    }
    p.end();
    return QIcon(image);
}
} // namespace cad::ui

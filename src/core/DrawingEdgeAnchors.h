#pragma once

#include <QMap>
#include <QSet>
#include <QString>
#include <cmath>

// A drawing edge reference can survive renumbering only if it carries a
// complete interior witness. Legacy, index-only annotations stay valid only
// for their original geometry token. Never silently degrade a bad witness to
// a nearest-edge or index fallback.
namespace cad {
inline bool validDrawingEdgeWitness(const QMap<QString,double>& anchor) {
    static const QSet<QString> required = {
        "edge_x","edge_y","edge_z","edge_length",
        "edge_body_x","edge_body_y","edge_body_z",
        "edge_anchor_version","edge_curve_kind",
        "edge_witness_x","edge_witness_y","edge_witness_z",
        "edge_tangent_x","edge_tangent_y","edge_tangent_z",
        "edge_curvature"
    };
    if(anchor.size()!=required.size())return false;
    for(auto it=anchor.cbegin();it!=anchor.cend();++it)
        if(!required.contains(it.key())||!std::isfinite(it.value())||std::abs(it.value())>1e9)
            return false;
    const double version=anchor.value("edge_anchor_version");
    const double kind=anchor.value("edge_curve_kind");
    const double tx=anchor.value("edge_tangent_x"),ty=anchor.value("edge_tangent_y"),tz=anchor.value("edge_tangent_z");
    const double norm=tx*tx+ty*ty+tz*tz;
    return (version==2.0||version==3.0) &&
        (kind==1.0||kind==2.0||(kind==3.0&&version==3.0)) &&
        anchor.value("edge_length")>1e-9 && anchor.value("edge_curvature")>=0.0 &&
        (kind!=1.0||anchor.value("edge_curvature")==0.0) &&
        norm>0.99 && norm<1.01;
}
} // namespace cad

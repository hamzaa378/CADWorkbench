#pragma once
#include "AssemblyMateSolver.h"
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Vec.hxx>
#include <gp_Dir.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Ax1.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <QString>
#include <cmath>

namespace cad::mates {
// Snapshot a real selected B-rep planar face as world-space mate observations.
// The face alone has three unconstrained in-plane DOFs. The guide is a
// deterministic WORLD-AXIS convenience for an EXPLICIT user-imposed twist lock;
// it is NOT a mating feature inferred from the face's topology.
struct PlanarFaceObservation {Vec3 center,normal,worldGuide;};
inline bool describeSelectedPlanarFace(const TopoDS_Face& face,
                                       PlanarFaceObservation* output,
                                       QString* error=nullptr){
    const auto fail=[&](const QString& why){if(error)*error=why;return false;};
    if(!output||face.IsNull())return fail("Select a non-null planar face.");
    const BRepAdaptor_Surface adaptor(face);
    if(adaptor.GetType()!=GeomAbs_Plane)
        return fail("Only planar faces can prefill a planar mate; cylinders require a coaxial mate.");
    GProp_GProps props;BRepGProp::SurfaceProperties(face,props);
    const gp_Pnt point=props.CentreOfMass();
    if(!std::isfinite(props.Mass())||props.Mass()<=1e-8||
       !std::isfinite(point.X())||!std::isfinite(point.Y())||!std::isfinite(point.Z()))
        return fail("The selected planar face has invalid area or center.");
    const gp_Dir axis=adaptor.Plane().Axis().Direction();
    gp_Vec normal(axis);
    if(face.Orientation()==TopAbs_REVERSED)normal.Reverse();
    const gp_Vec seed=std::abs(normal.X())<0.90?gp_Vec(1,0,0):gp_Vec(0,1,0);
    gp_Vec guide=seed-normal.Multiplied(seed.Dot(normal));
    if(guide.SquareMagnitude()<1e-12)return fail("Could not build a planar guide.");
    guide.Normalize();
    const PlanarFaceObservation prepared{{point.X(),point.Y(),point.Z()},
        {normal.X(),normal.Y(),normal.Z()},
        {guide.X(),guide.Y(),guide.Z()}};
    *output=prepared;
    if(error)error->clear();
    return true;
}
// Read a cylindrical mating surface without pretending it locks axial sliding
// or rotation about its axis. The observed point is the projection of the
// trimmed face area centroid onto the underlying cylinder axis. Axis sign is
// canonicalized because a cylindrical surface's parametric direction is not a
// mechanical orientation constraint.
struct CylindricalFaceObservation {Vec3 axisPoint,axisDirection,worldGuide;double radiusMm=0;};
inline bool describeSelectedCylindricalFace(const TopoDS_Face& face,
                                            CylindricalFaceObservation* output,
                                            QString* error=nullptr){
    const auto fail=[&](const QString& why){if(error)*error=why;return false;};
    if(!output||face.IsNull())return fail("Select a non-null cylindrical face.");
    const BRepAdaptor_Surface adaptor(face);
    if(adaptor.GetType()!=GeomAbs_Cylinder)
        return fail("Only cylindrical faces can prefill a concentric mate.");
    const gp_Cylinder cylinder=adaptor.Cylinder();
    const double radius=cylinder.Radius();
    if(!std::isfinite(radius)||radius<=1e-8||radius>1e6)
        return fail("The selected cylindrical surface has an invalid radius.");
    GProp_GProps props;BRepGProp::SurfaceProperties(face,props);
    const gp_Pnt centroid=props.CentreOfMass();
    if(!std::isfinite(props.Mass())||props.Mass()<=1e-8||
       !std::isfinite(centroid.X())||!std::isfinite(centroid.Y())||!std::isfinite(centroid.Z()))
        return fail("The selected cylindrical face has invalid area or center.");
    const gp_Ax1 support=cylinder.Axis();
    const gp_Pnt origin=support.Location();
    gp_Vec direction(support.Direction());
    // One reproducible orientation independent of REVERSED face orientation.
    const double ax=std::abs(direction.X()),ay=std::abs(direction.Y()),az=std::abs(direction.Z());
    const double dominant=ax>=ay&&ax>=az?direction.X():ay>=az?direction.Y():direction.Z();
    if(dominant<0)direction.Reverse();
    const gp_Vec offset(origin,centroid);
    const gp_Pnt point=origin.Translated(direction.Multiplied(offset.Dot(direction)));
    const gp_Vec seed=std::abs(direction.X())<0.90?gp_Vec(1,0,0):gp_Vec(0,1,0);
    gp_Vec guide=seed-direction.Multiplied(seed.Dot(direction));
    if(guide.SquareMagnitude()<1e-12)return fail("Could not build a cylindrical guide.");
    guide.Normalize();
    const CylindricalFaceObservation measured{{point.X(),point.Y(),point.Z()},
        {direction.X(),direction.Y(),direction.Z()},
        {guide.X(),guide.Y(),guide.Z()},radius};
    *output=measured; // Failed calls leave the caller's previous observation intact.
    if(error)error->clear();
    return true;
}
} // namespace cad::mates

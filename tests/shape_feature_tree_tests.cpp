#include "core/SceneDocument.h"
#include "core/ShapeFeatureTree.h"
#include "core/BooleanReferenceLineage.h"
#include "core/DocumentAudit.h"

#include <BRepBndLib.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
int checks=0;
void require(bool condition,const char* reason){
    ++checks;if(!condition)throw std::runtime_error(reason);
}
cad::ShapeFeatureNode feature(const QString& id,const QString& type,const QMap<QString,double>& params){
    return {id,type,params,false};
}
double volume(const TopoDS_Shape& shape){GProp_GProps mass;BRepGProp::VolumeProperties(shape,mass);return mass.Mass();}
double xMin(const TopoDS_Shape& shape){
    Bnd_Box bounds;BRepBndLib::AddOptimal(shape,bounds,false,false);
    double x,y,z,x2,y2,z2;bounds.Get(x,y,z,x2,y2,z2);return x;
}
}
int main(int argc,char** argv){
    QCoreApplication application(argc,argv);
    try{
        QString error;
        cad::SceneDocument doc;
        // A universal feature tree must not care about the primitive's class.
        const QVector<QPair<cad::Primitive,QVector<double>>> shapes={
            {cad::Primitive::Box,{30,20,12}},
            {cad::Primitive::Cylinder,{20,15}},
            {cad::Primitive::Sphere,{12}},
            {cad::Primitive::Cone,{15,8,18}}
        };
        for(const auto& spec:shapes){
            error.clear();
            require(doc.addPrimitive(spec.first,spec.second,&error),"Could not create primitive fixture");
            const QString id=doc.parts().last().id;
            const double before=volume(doc.find(id)->shape);
            const double xBefore=xMin(doc.find(id)->shape);
            QVector<cad::ShapeFeatureNode> nodes={
                feature("move_1","Translate",{{"x",10},{"y",3},{"z",0}}),
                feature("scale_1","Uniform scale",{{"factor",2}})
            };
            require(doc.setShapeFeatureTree(id,nodes,&error),"Failed to attach universal features");
            require(doc.shapeFeatureTreeCurrent(id),"New universal feature stack was stale");
            require(doc.find(id)->shapeFeatureNodes.size()==2,"Modifier nodes were not stored");
            require(std::abs(volume(doc.find(id)->shape)-before*8)<std::max(1e-5,before*1e-5),
                    "Universal scale did not change volume by factor cubed");
            require(std::abs(xMin(doc.find(id)->shape)-(xBefore+10)*2)<1e-4,
                    "Feature order was not applied to B-rep output");
            const QString token=doc.find(id)->geometryToken;
            nodes[1].suppressed=true;
            require(doc.setShapeFeatureTree(id,nodes,&error),"Suppressing feature failed");
            require(std::abs(volume(doc.find(id)->shape)-before)<1e-4,"Suppressed scale changed volume");
            require(doc.undo(),"Undo of feature stack failed");
            require(doc.find(id)->geometryToken==token,"Undo did not restore feature-stack token");
            require(doc.find(id)->shapeFeatureNodes[1].suppressed==false,"Undo did not restore active modifier");
            require(doc.redo(),"Redo of feature stack failed");
            nodes[1].suppressed=false;
            nodes.swapItemsAt(0,1);
            require(doc.setShapeFeatureTree(id,nodes,&error),"Reordering feature nodes failed");
            require(std::abs(xMin(doc.find(id)->shape)-(xBefore*2+10))<1e-4,
                    "Feature reordering had no effect on geometry");
            const QString safeToken=doc.find(id)->geometryToken;
            const auto safeShape=doc.find(id)->shape;
            nodes.append(feature("invalid_cut","Cylinder cut Z",{{"x",99999},{"y",99999},{"z",99999},
                {"radius",1},{"depth",10}}));
            require(!doc.setShapeFeatureTree(id,nodes,&error),"Non-intersecting cut was incorrectly accepted");
            require(doc.find(id)->geometryToken==safeToken&&
                    std::abs(volume(doc.find(id)->shape)-volume(safeShape))<1e-5,
                    "Failed feature rebuild damaged current part");
        }
        // Real workshop workflow: one editable and persisted fillet/chamfer
        // feature for an intentionally selected subset of body edges. The
        // other edges must not be modified as an accidental all-edge feature.
        for(const QString& operation:QStringList{"Fillet selected edges","Chamfer selected edges"}){
            cad::SceneDocument grouped;
            require(grouped.addPrimitive(cad::Primitive::Box,{40,30,20},&error),
                    "Group fixture stock could not be made");
            const QString bodyId=grouped.parts().last().id;
            const double original=volume(grouped.find(bodyId)->shape);
            TopTools_IndexedMapOfShape candidates;
            TopExp::MapShapes(grouped.find(bodyId)->shape,TopAbs_EDGE,candidates);
            QVector<int> vertical;
            for(int edge=1;edge<=candidates.Extent();++edge){
                BRepAdaptor_Curve line(TopoDS::Edge(candidates.FindKey(edge)));
                if(line.GetType()!=GeomAbs_Line)continue;
                const gp_Pnt a=line.Value(line.FirstParameter());
                const gp_Pnt b=line.Value(line.LastParameter());
                if(std::abs(a.X()-b.X())<1e-6&&std::abs(a.Y()-b.Y())<1e-6&&
                   std::abs(a.Z()-b.Z())>15.)vertical.append(edge);
            }
            require(vertical.size()>=2,"Box fixture needs separate upright edges");
            cad::ShapeFeatureNode group=feature("group","Fillet selected edges",{});
            group.type=operation;
            group.parameters.insert("edge_count",2);
            require(cad::supportsSurvivingModifierLineage(group),
                    "Grouped blend must be treated as a destructive edge modifier in ancestry tracking");
            const QString sizeKey=operation.startsWith("Fillet")?"radius":"distance";
            group.parameters.insert(sizeKey,1.0);
            for(int i=0;i<2;++i){
                QMap<QString,double> anchor;
                require(cad::captureStableEdgeAnchor(grouped.find(bodyId)->shape,
                    vertical[i],&anchor,&error)&&anchor.contains("edge_anchor_version"),
                    "Group edge witness capture failed");
                const QString prefix=QStringLiteral("member_%1_").arg(i);
                group.parameters.insert(prefix+"edge_index",vertical[i]);
                for(auto it=anchor.cbegin();it!=anchor.cend();++it)
                    group.parameters.insert(prefix+it.key(),it.value());
            }
            group.parameters.insert("member_0_size",0.7);
            group.parameters.insert("member_1_size",1.3);
            if(!grouped.setShapeFeatureTree(bodyId,{group},&error))
                throw std::runtime_error(("Witnessed unequal-size group failed ["+
                    operation.toStdString()+"]: "+error.toStdString()));
            ++checks;
            const double changed=volume(grouped.find(bodyId)->shape);
            require(changed<original-1e-4&&changed>original*0.90,
                    "Grouped blend should modify limited material, not entire stock");
            const QString currentToken=grouped.find(bodyId)->geometryToken;
            auto duplicate=group;
            for(const QString& key:group.parameters.keys())
                if(key.startsWith("member_0_"))
                    duplicate.parameters.insert("member_1_"+key.mid(QString("member_0_").size()),
                                                group.parameters.value(key));
            require(!grouped.setShapeFeatureTree(bodyId,{duplicate},&error),
                    "Duplicate physical edges were accepted as a group");
            require(grouped.find(bodyId)->geometryToken==currentToken,
                    "Failed group operation modified the document");
            auto badSize=group;
            badSize.parameters["member_1_size"]=-1.0;
            require(!grouped.setShapeFeatureTree(bodyId,{badSize},&error)&&
                    grouped.find(bodyId)->geometryToken==currentToken,
                    "Invalid variable edge size changed an existing solid");
            auto corrupt=group;
            corrupt.parameters.remove("member_1_edge_witness_z");
            require(!grouped.setShapeFeatureTree(bodyId,{corrupt},&error),
                    "Incomplete grouped witness was accepted");
            auto resized=group;
            resized.parameters[sizeKey]=0.5;
            resized.parameters["member_0_size"]=0.3;
            resized.parameters["member_1_size"]=0.5;
            require(grouped.setShapeFeatureTree(bodyId,{resized},&error),
                    "Resize of grouped blend did not rebuild");
            require(volume(grouped.find(bodyId)->shape)>changed,
                    "Smaller grouped blend did not remove less stock");
            require(grouped.undo()&&std::abs(volume(grouped.find(bodyId)->shape)-changed)<1e-4,
                    "Grouped blend edit did not undo");
            require(grouped.redo()&&grouped.find(bodyId)->shapeFeatureNodes.size()==1,
                    "Grouped blend edit did not redo");
            QTemporaryDir dir;
            require(dir.isValid(),"Grouped blend save test needs a directory");
            const QString saved=dir.path()+"/edge-group.cadscene";
            require(grouped.saveAs(saved,&error),"Grouped blend scene did not save");
            cad::SceneDocument reloaded;
            require(reloaded.load(saved,&error)&&reloaded.shapeFeatureTreeCurrent(bodyId)&&
                    reloaded.find(bodyId)->shapeFeatureNodes.first().parameters.contains("member_1_edge_witness_z"),
                    "Grouped blend geometry or witnesses did not survive reload");
            require(std::abs(volume(reloaded.find(bodyId)->shape)-volume(grouped.find(bodyId)->shape))<1e-4,
                    "Grouped blend changed volume across save/reload");
        }
        // Production workflow: apply one edge treatment across independently
        // owned parts; either every complete feature tree commits or NONE does.
        for(const QString& kind:QStringList{"Fillet all edges","Chamfer all edges"}){
            cad::SceneDocument job;
            require(job.addPrimitive(cad::Primitive::Box,{30,20,12},&error),
                    "Batch finishing first stock failed");
            const QString first=job.parts().last().id;
            require(job.addPrimitive(cad::Primitive::Box,{36,22,14},&error),
                    "Batch finishing second stock failed");
            const QString second=job.parts().last().id;
            const double beforeA=volume(job.find(first)->shape);
            const double beforeB=volume(job.find(second)->shape);
            const QString initialA=job.find(first)->geometryToken;
            const QString initialB=job.find(second)->geometryToken;
            const QString dimension=kind.startsWith("Fillet")?"radius":"distance";
            QMap<QString,QVector<cad::ShapeFeatureNode>> proposals;
            proposals.insert(first,{feature("finish_a",kind,{{dimension,0.5}})});
            proposals.insert(second,{feature("finish_b",kind,{{dimension,0.5}})});
            auto invalid=proposals;
            invalid[second][0].parameters[dimension]=-1;
            QString previewReport=QStringLiteral("UNCHANGED");
            require(!job.previewShapeFeatureTreesBatch(invalid,&previewReport,&error)&&
                    previewReport==QStringLiteral("UNCHANGED")&&
                    job.find(first)->geometryToken==initialA&&
                    job.find(second)->geometryToken==initialB,
                    "Failed detached feature-history preview mutated output or committed geometry");
            require(job.previewShapeFeatureTreesBatch(proposals,&previewReport,&error)&&
                    previewReport.contains("ATOMIC_FEATURE_PREVIEW")&&
                    previewReport.contains("status=READ_ONLY")&&
                    job.find(first)->geometryToken==initialA&&
                    job.find(second)->geometryToken==initialB,
                    "Valid detached preview changed actual document revisions");
            require(!job.previewShapeFeatureTreesBatch(proposals,nullptr,&error)&&
                    job.find(first)->geometryToken==initialA,
                    "Null detached preview destination modified the scene");
            require(!job.setShapeFeatureTreesBatch(invalid,&error)&&
                    job.find(first)->geometryToken==initialA&&
                    job.find(second)->geometryToken==initialB,
                    "Rejected manufacturing batch committed a partial part");
            require(job.setShapeFeatureTreesBatch(proposals,&error),
                    "Valid two-body finishing batch failed");
            require(volume(job.find(first)->shape)<beforeA&&
                    volume(job.find(second)->shape)<beforeB,
                    "Batch finishing did not modify both solid parts");
            require(job.undo(),"Single-step batch finishing undo failed");
            require(std::abs(volume(job.find(first)->shape)-beforeA)<1e-4&&
                    std::abs(volume(job.find(second)->shape)-beforeB)<1e-4,
                    "Batch finishing undo did not restore both bodies");
            require(job.redo(),"Single-step batch finishing redo failed");
            require(job.find(first)->shapeFeatureNodes.size()==1&&
                    job.find(second)->shapeFeatureNodes.size()==1,
                    "Batch finishing redo lost a part's editable feature");
            require(job.setShapeFeatureTreesBatch(proposals,&error),
                    "Idempotent batch feature application failed");
            QTemporaryDir dir;require(dir.isValid(),"Batch save needs a temp directory");
            const QString path=dir.path()+"/finished-parts.cadscene";
            require(job.saveAs(path,&error),"Production batch scene did not save");
            cad::SceneDocument reopened;
            require(reopened.load(path,&error)&&reopened.shapeFeatureTreeCurrent(first)&&
                    reopened.shapeFeatureTreeCurrent(second),
                    "Batch finish editability was not restored after project reload");
        }
        // Support arbitrary extruded 2D profiles, not only rectangle presets.
        cad::SketchModel sketch;
        sketch.points={{"a",0,0},{"b",35,0},{"c",25,15},{"d",7,23},{"e",0,10}};
        for(const auto& point:sketch.points){
            cad::SketchConstraint fix;fix.type=cad::SketchConstraintType::FixedPoint;
            fix.pointA=point.id;fix.fixedX=point.x;fix.fixedY=point.y;
            sketch.constraints.append(fix);
        }
        require(doc.addConstrainedPolygonExtrusion(sketch,{"a","b","c","d","e"},12,&error),
                "Could not make non-rectangle sketch extrusion");
        const QString sketchId=doc.parts().last().id;
        require(doc.setShapeFeatureTree(sketchId,{feature("rotate","Rotate Z",{{"degrees",30}})},&error),
                "Could not add modifier to custom sketch extrusion");
        require(doc.find(sketchId)->hasShapeFeatureTree,"Arbitrary sketch body did not get feature tree");
        // v1.70 RC5: stable feature-ID inspection notes; geometry edits are
        // intentionally NOT allowed to retain a current-review status.
        const QString inspectionBodyId=doc.parts().first().id;
        const QString inspectionToken=doc.find(inspectionBodyId)->geometryToken;
        const cad::FeatureInspectionNote qa{"QA-1","scale_1","INSPECTION",
            "Check face flatness before coating",inspectionToken};
        require(doc.inspectionFeatureIds(inspectionBodyId).contains("scale_1"),
                "RC5 feature identity catalog omitted a saved node ID");
        require(doc.setFeatureInspectionNotes(inspectionBodyId,{qa},&error),
                "RC5 feature-linked note save failed");
        require(!doc.setFeatureInspectionNotes(inspectionBodyId,
                {{"Bad","unknown","INSPECTION","Unsupported reference",inspectionToken}},&error)&&
                doc.featureInspectionNotes(inspectionBodyId).size()==1,
                "RC5 feature notes accepted an unknown new source");
        require(!doc.setFeatureInspectionNotes(inspectionBodyId,{qa,qa},&error),
                "RC5 feature notes accepted a duplicate annotation ID");
        QByteArray beforeChange;
        require(doc.buildFeatureInspectionCsv(inspectionBodyId,&beforeChange,&error)&&
                beforeChange.contains("CURRENT_REVISION")&&beforeChange.contains("scale_1"),
                "RC5 report omitted the current geometry review");
        QTemporaryDir inspectionDir;
        require(inspectionDir.isValid(),"RC5 inspection export needs a temporary directory");
        const QString inspectionPath=inspectionDir.path()+"/feature-inspection.csv";
        require(doc.exportFeatureInspectionCsv(inspectionBodyId,inspectionPath,&error),
                "RC5 atomic feature inspection CSV export failed");
        QFile inspectionFile(inspectionPath);
        require(inspectionFile.open(QIODevice::ReadOnly)&&
                inspectionFile.readAll()==beforeChange,
                "RC5 exported inspection worksheet differs from reviewed source");
        inspectionFile.close();
        require(doc.undo()&&doc.featureInspectionNotes(inspectionBodyId).isEmpty(),
                "RC5 feature annotation was not undoable");
        require(doc.redo()&&doc.featureInspectionNotes(inspectionBodyId).size()==1,
                "RC5 feature annotation was not redoable");
        const cad::FeatureInspectionNote injection{"QA-FORMULA","scale_1","PROCESS",
            "=HYPERLINK(\"http://invalid.example\",\"click\")",inspectionToken};
        require(doc.setFeatureInspectionNotes(inspectionBodyId,{qa,injection},&error),
                "RC5 safe-report fixture creation failed");
        QByteArray hardened;
        require(doc.buildFeatureInspectionCsv(inspectionBodyId,&hardened,&error)&&
                hardened.contains("\"'=HYPERLINK("),
                "RC5 export allowed a spreadsheet formula at cell start");
        require(doc.setFeatureInspectionNotes(inspectionBodyId,{qa},&error),
                "RC5 safe-report fixture cleanup failed");

        auto revised=doc.find(inspectionBodyId)->shapeFeatureNodes;
        require(revised.size()==2,"RC5 fixture lost its feature tree");
        for(auto& node:revised)if(node.id=="scale_1")node.parameters["factor"]=1.5;
        require(doc.setShapeFeatureTree(inspectionBodyId,revised,&error),
                "RC5 source feature revision failed");
        QByteArray stale;
        require(doc.buildFeatureInspectionCsv(inspectionBodyId,&stale,&error)&&
                stale.contains("REVIEW_REQUIRED")&&
                doc.featureInspectionNotes(inspectionBodyId).first().reviewedGeometryToken==inspectionToken,
                "RC5 geometry edit silently renewed an inspection review");
        auto reviewed=doc.featureInspectionNotes(inspectionBodyId);
        reviewed[0].reviewedGeometryToken=doc.find(inspectionBodyId)->geometryToken;
        require(doc.setFeatureInspectionNotes(inspectionBodyId,reviewed,&error),
                "RC5 explicit re-review of the edited feature failed");
        QByteArray current;
        require(doc.buildFeatureInspectionCsv(inspectionBodyId,&current,&error)&&
                current.contains("CURRENT_REVISION"),
                "RC5 explicit review was not retained");
        // A valid history edit can remove a feature, but the linked note must
        // remain identifiable for disposition and never appear current.
        auto withoutScale=doc.find(inspectionBodyId)->shapeFeatureNodes;
        withoutScale.erase(std::remove_if(withoutScale.begin(),withoutScale.end(),
            [](const cad::ShapeFeatureNode& node){return node.id=="scale_1";}),
            withoutScale.end());
        require(doc.setShapeFeatureTree(inspectionBodyId,withoutScale,&error),
                "RC5 removing the annotated feature failed");
        QByteArray missing;
        require(doc.buildFeatureInspectionCsv(inspectionBodyId,&missing,&error)&&
                missing.contains("MISSING_FEATURE")&&
                doc.featureInspectionNotes(inspectionBodyId).size()==1,
                "RC5 removed feature lost its orphaned engineering note");
        require(doc.undo()&&doc.inspectionFeatureIds(inspectionBodyId).contains("scale_1"),
                "RC5 annotated feature removal did not undo");
        // Restore the current annotation state for save/reopen assertions.
        require(doc.redo()&&doc.featureInspectionNotes(inspectionBodyId).size()==1,
                "RC5 orphaned note did not survive redo");
        QTemporaryDir temporary;require(temporary.isValid(),"Temporary directory unavailable");
        const QString path=temporary.path()+"/universal-features.cadscene";
        require(doc.saveAs(path,&error),"Failed to save universal feature document");
        QFile file(path);require(file.open(QIODevice::ReadOnly),"Cannot inspect saved file");
        const auto json=QJsonDocument::fromJson(file.readAll()).object();file.close();
        require(json.value("schema").toString()=="cadworkbench.scene.v24","Feature schema version not saved");
        cad::SceneDocument loaded;
        require(loaded.load(path,&error),"Could not reopen a universal feature tree document");
        require(loaded.parts().size()==doc.parts().size(),"Load lost any source body");
        require(loaded.find(sketchId)->hasShapeFeatureTree&&
                loaded.find(sketchId)->shapeFeatureNodes.size()==1,"Load lost custom sketch feature nodes");
        require(loaded.shapeFeatureTreeCurrent(sketchId),"Reloaded feature stack is stale");
        QByteArray reloadedReport;
        require(loaded.featureInspectionNotes(inspectionBodyId).size()==1&&
                loaded.buildFeatureInspectionCsv(inspectionBodyId,&reloadedReport,&error)&&
                reloadedReport.contains("MISSING_FEATURE"),
                "RC5 feature annotation did not survive project save/reopen");
        // Reject malformed saved notes without replacing the open project.
        QJsonObject noteRoot=json;
        QJsonArray noteParts=noteRoot.value("parts").toArray();
        QJsonObject notedBody=noteParts[0].toObject();
        QJsonArray persistedNotes=notedBody.value("feature_inspection_notes").toArray();
        require(persistedNotes.size()==1,"RC5 JSON feature notes were not written");
        QJsonObject malformedNote=persistedNotes[0].toObject();
        malformedNote.insert("category","UNKNOWN");persistedNotes[0]=malformedNote;
        notedBody.insert("feature_inspection_notes",persistedNotes);noteParts[0]=notedBody;
        noteRoot.insert("parts",noteParts);
        const QString invalidNotesPath=temporary.path()+"/bad-feature-notes.cadscene";
        QFile invalidNotes(invalidNotesPath);
        require(invalidNotes.open(QIODevice::WriteOnly),"RC5 could not create corrupt notes fixture");
        invalidNotes.write(QJsonDocument(noteRoot).toJson());invalidNotes.close();
        require(!loaded.load(invalidNotesPath,&error)&&
                loaded.featureInspectionNotes(inspectionBodyId).size()==1,
                "RC5 invalid feature note loaded or destroyed a valid open document");
        // Corrupt a parameter: loading must fail transactionally.
        auto tampered=json;
        QJsonArray parts=tampered.value("parts").toArray();
        QJsonObject first=parts.first().toObject();
        QJsonObject tree=first.value("shape_feature_tree").toObject();
        QJsonArray nodes=tree.value("nodes").toArray();
        QJsonObject firstNode=nodes.first().toObject();
        QJsonObject parameters=firstNode.value("parameters").toObject();
        parameters.insert("factor",0.0);firstNode.insert("parameters",parameters);
        nodes.replace(0,firstNode);tree.insert("nodes",nodes);
        first.insert("shape_feature_tree",tree);parts.replace(0,first);
        tampered.insert("parts",parts);
        const QString badPath=temporary.path()+"/corrupted.cadscene";
        QFile badFile(badPath);require(badFile.open(QIODevice::WriteOnly),"Could not write corrupt fixture");
        badFile.write(QJsonDocument(tampered).toJson());badFile.close();
        const int sizeBefore=loaded.parts().size();
        require(!loaded.load(badPath,&error),"Invalid saved feature parameters were accepted");
        require(loaded.parts().size()==sizeBefore&&loaded.find(sketchId)!=nullptr,
                "Corrupt project replaced the valid open document");
        require(loaded.translate(sketchId,3,0,0,&error),"Could not directly edit feature result");
        require(!loaded.shapeFeatureTreeCurrent(sketchId),"Direct edit failed to mark feature stack stale");
        require(!loaded.setShapeFeatureTree(sketchId,loaded.find(sketchId)->shapeFeatureNodes,&error),
                "Stale feature stack was silently overwritten");
        require(loaded.rebaseShapeFeatureTree(sketchId,&error),"Explicit feature rebase failed");
        require(loaded.shapeFeatureTreeCurrent(sketchId)&&loaded.find(sketchId)->shapeFeatureNodes.isEmpty(),
                "Rebase did not freeze the new source and clear the old stack");
        std::cout<<"PASS: "<<checks<<" universal feature tree assertions\n";
        return 0;
    }catch(const std::exception& exception){
        std::cerr<<"FAIL at check "<<checks<<": "<<exception.what()<<'\n';return 1;
    }
}

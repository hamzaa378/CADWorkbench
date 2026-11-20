#pragma once
// Revision-to-revision reconciliation for UNISSUED native CAD drawing data.
// Checks both independent snapshots, then classifies all source revisions and
// annotation identity changes. No annotation is ever auto-reviewed or issued.
#include "DrawingReferenceAudit.h"
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cad::drawing_audit {
enum class AnnotationRevisionKind { Added, Removed, Modified, ReferenceMatch };
struct AnnotationRevisionItem {
    std::string annotationId,bodyId;
    AnnotationRevisionKind kind=AnnotationRevisionKind::Modified;
    bool sourceGeometryChanged=false;
    bool sourceMetadataChanged=false;
    bool currentReferenceStale=false;
};
struct DrawingRevisionImpact {
    std::vector<AnnotationRevisionItem> annotations;
    std::vector<std::string> changedBodyIds;
    std::vector<Issue> currentIssues;
    std::size_t added=0,removed=0,modified=0,referenceMatches=0;
    bool releaseApproved=false;
};
inline bool compareDrawingRevisions(const std::vector<Source>& previousSources,
        const std::vector<Annotation>& previousAnnotations,
        const std::vector<Source>& currentSources,
        const std::vector<Annotation>& currentAnnotations,
        DrawingRevisionImpact* out,std::string* error=nullptr) {
    const auto fail=[&](const char* why){if(error)*error=why;return false;};
    if(!out)return fail("Drawing revision impact output is required.");
    DrawingReferenceReport previousCheck,currentCheck;
    if(!auditDrawingReferences(previousSources,previousAnnotations,&previousCheck,error)||
       !auditDrawingReferences(currentSources,currentAnnotations,&currentCheck,error))
        return false;
    std::map<std::string,const Source*> beforeBodies,afterBodies;
    for(const auto& s:previousSources)beforeBodies[s.bodyId]=&s;
    for(const auto& s:currentSources)afterBodies[s.bodyId]=&s;
    DrawingRevisionImpact staged;
    for(const auto& [id,previous]:beforeBodies) {
        const auto it=afterBodies.find(id);
        if(it==afterBodies.end()||previous->geometryToken!=it->second->geometryToken||
           previous->metadataToken!=it->second->metadataToken)
            staged.changedBodyIds.push_back(id);
    }
    for(const auto& [id,current]:afterBodies)
        if(beforeBodies.find(id)==beforeBodies.end())staged.changedBodyIds.push_back(id);
    std::map<std::string,const Annotation*> a,b;
    for(const auto& x:previousAnnotations)a[x.id]=&x;
    for(const auto& x:currentAnnotations)b[x.id]=&x;
    std::set<std::string> staleIds;
    for(const auto& issue:currentCheck.issues)staleIds.insert(issue.annotationId);
    for(const auto& [id,annotation]:a) {
        if(b.find(id)==b.end()) {
            staged.annotations.push_back({id,annotation->bodyId,AnnotationRevisionKind::Removed});
            ++staged.removed;
        }
    }
    for(const auto& [id,annotation]:b) {
        const auto old=a.find(id);
        const auto before=beforeBodies.find(annotation->bodyId),after=afterBodies.find(annotation->bodyId);
        const bool changedGeometry=before==beforeBodies.end()||after==afterBodies.end()||
                         before->second->geometryToken!=after->second->geometryToken;
        const bool changedMetadata=before==beforeBodies.end()||after==afterBodies.end()||
                         before->second->metadataToken!=after->second->metadataToken;
        const bool same=old!=a.end()&&old->second->bodyId==annotation->bodyId&&
                old->second->capturedGeometryToken==annotation->capturedGeometryToken&&
                old->second->capturedMetadataToken==annotation->capturedMetadataToken&&
                old->second->geometryDependent==annotation->geometryDependent&&
                old->second->metadataDependent==annotation->metadataDependent;
        const auto kind=old==a.end()?AnnotationRevisionKind::Added:
                        same?AnnotationRevisionKind::ReferenceMatch:AnnotationRevisionKind::Modified;
        staged.annotations.push_back({id,annotation->bodyId,kind,
                  changedGeometry,changedMetadata,staleIds.count(id)>0});
        if(kind==AnnotationRevisionKind::Added)++staged.added;
        else if(kind==AnnotationRevisionKind::Modified)++staged.modified;
        else ++staged.referenceMatches;
    }
    staged.currentIssues=std::move(currentCheck.issues);
    *out=std::move(staged);
    if(error)error->clear();
    return true;
}
} // namespace cad::drawing_audit

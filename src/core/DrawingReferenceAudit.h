#pragma once
// Revision/token audit for an UNISSUED engineering drawing snapshot.
// This is an independent backend planning/checking API, not a certification,
// approval workflow, nor a replacement for topology-aware dimension solving.
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace cad::drawing_audit {
struct Source {
    std::string bodyId;
    std::string geometryToken;
    std::string metadataToken;
};
struct Annotation {
    std::string id;
    std::string bodyId;
    std::string capturedGeometryToken;
    std::string capturedMetadataToken;
    bool geometryDependent=true;
    bool metadataDependent=false;
};
enum class IssueKind { MissingBody, GeometryStale, MetadataStale };
struct Issue {
    std::string annotationId;
    std::string bodyId;
    IssueKind kind=IssueKind::MissingBody;
};
struct DrawingReferenceReport {
    bool eligibleForReview=false; // false if stale; never means officially issued.
    std::size_t checkedAnnotations=0;
    std::size_t currentAnnotations=0;
    std::vector<Issue> issues;
};
inline bool auditDrawingReferences(const std::vector<Source>& sources,
        const std::vector<Annotation>& annotations,
        DrawingReferenceReport* report,std::string* error=nullptr) {
    const auto fail=[&](const char* msg){if(error)*error=msg;return false;};
    if(!report||sources.empty()||sources.size()>100000||
       annotations.empty()||annotations.size()>100000)
        return fail("A nonempty bounded set of drawing sources and annotations is required.");
    std::map<std::string,const Source*> byId;
    for(const auto& source:sources){
        if(source.bodyId.empty()||source.geometryToken.empty()||source.metadataToken.empty()||
           !byId.emplace(source.bodyId,&source).second)
            return fail("Source identifiers and revision tokens must be unique and nonempty.");
    }
    std::set<std::string> annotationIds;
    DrawingReferenceReport staged;
    staged.checkedAnnotations=annotations.size();
    for(const auto& annotation:annotations){
        if(annotation.id.empty()||annotation.bodyId.empty()||
           !annotationIds.insert(annotation.id).second||
           (annotation.geometryDependent&&annotation.capturedGeometryToken.empty())||
           (annotation.metadataDependent&&annotation.capturedMetadataToken.empty())||
           (!annotation.geometryDependent&&!annotation.metadataDependent))
            return fail("Drawing annotation has a duplicate ID, absent capture or no revision dependency.");
        const auto it=byId.find(annotation.bodyId);
        if(it==byId.end()){
            staged.issues.push_back({annotation.id,annotation.bodyId,IssueKind::MissingBody});
            continue;
        }
        bool valid=true;
        if(annotation.geometryDependent&&annotation.capturedGeometryToken!=it->second->geometryToken){
            staged.issues.push_back({annotation.id,annotation.bodyId,IssueKind::GeometryStale});
            valid=false;
        }
        if(annotation.metadataDependent&&annotation.capturedMetadataToken!=it->second->metadataToken){
            staged.issues.push_back({annotation.id,annotation.bodyId,IssueKind::MetadataStale});
            valid=false;
        }
        if(valid)++staged.currentAnnotations;
    }
    staged.eligibleForReview=staged.issues.empty();
    *report=std::move(staged);
    if(error)error->clear();
    return true;
}
} // namespace cad::drawing_audit

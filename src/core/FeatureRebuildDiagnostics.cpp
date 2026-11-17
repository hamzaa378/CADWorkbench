#include "FeatureRebuildDiagnostics.h"
#include <map>
#include <utility>

namespace cad::dependency {
namespace {
std::vector<std::string> inputsAt(const std::vector<FeatureRecord>& records, std::size_t pos){
    if(!records[pos].inputs.empty())return records[pos].inputs;
    return {pos ? records[pos-1].id : "@base"};
}
}

const char* rebuildCauseName(RebuildCause cause) noexcept {
    switch(cause){
        case RebuildCause::Reused:return "REUSED";
        case RebuildCause::NewFeature:return "NEW_FEATURE";
        case RebuildCause::MissingCachedOutput:return "MISSING_CACHE";
        case RebuildCause::DefinitionChanged:return "DEFINITION_CHANGED";
        case RebuildCause::DependencyChanged:return "DEPENDENCY_CHANGED";
        case RebuildCause::BaseGeometryChanged:return "BASE_REVISION_CHANGED";
        case RebuildCause::ExternalGeometryChanged:return "EXTERNAL_REVISION_CHANGED";
        case RebuildCause::UpstreamRecomputed:return "UPSTREAM_RECOMPUTED";
    }
    return "UNKNOWN";
}

bool explainReuseWithSources(const std::vector<FeatureRecord>& previous,
                             const std::vector<FeatureRecord>& proposed,
                             const std::vector<bool>& available,
                             const std::string& oldBase,const std::string& newBase,
                             const std::vector<SourceRevision>& oldSources,
                             const std::vector<SourceRevision>& newSources,
                             ReusePlan* plan,std::vector<RebuildDecision>* decisions,
                             std::string* error){
    if(!plan||!decisions){if(error)*error="Both plan and diagnostics destinations are required.";return false;}
    ReusePlan candidate;
    if(!planReuseWithSources(previous,proposed,available,oldBase,newBase,
                            oldSources,newSources,&candidate,error))return false;
    std::map<std::string,std::size_t> oldIndex;
    std::map<std::string,std::string> oldRevisions,newRevisions;
    for(std::size_t i=0;i<previous.size();++i)oldIndex.emplace(previous[i].id,i);
    for(const auto& src:oldSources)oldRevisions.emplace(src.name,src.geometryRevision);
    for(const auto& src:newSources)newRevisions.emplace(src.name,src.geometryRevision);
    std::map<std::string,bool> reusable;
    std::vector<RebuildDecision> result;
    result.reserve(proposed.size());
    for(std::size_t i=0;i<proposed.size();++i){
        const FeatureRecord& node=proposed[i];
        RebuildDecision decision;
        decision.featureId=node.id;
        decision.previousIndex=candidate.previousIndex[i];
        const auto old=oldIndex.find(node.id);
        if(decision.previousIndex>=0){
            decision.cause=RebuildCause::Reused;
        }else if(old==oldIndex.end()){
            decision.cause=RebuildCause::NewFeature;
            decision.trigger=node.id;
        }else{
            const auto j=old->second;
            if(!available[j]){
                decision.cause=RebuildCause::MissingCachedOutput;
                decision.trigger=node.id;
            }else if(node.type!=previous[j].type ||
                     node.parameters!=previous[j].parameters ||
                     node.suppressed!=previous[j].suppressed){
                decision.cause=RebuildCause::DefinitionChanged;
                decision.trigger=node.id;
            }else if(node.inputs!=previous[j].inputs ||
                     inputsAt(proposed,i)!=inputsAt(previous,j)){
                decision.cause=RebuildCause::DependencyChanged;
                decision.trigger=node.id;
            }else{
                bool found=false;
                for(const auto& source:inputsAt(proposed,i)){
                    if(source=="@base" && oldBase!=newBase){
                        decision.cause=RebuildCause::BaseGeometryChanged;
                        decision.trigger=source;
                        found=true;break;
                    }
                    if(source.rfind("@source:",0)==0){
                        const auto name=source.substr(8);
                        if(oldRevisions.at(name)!=newRevisions.at(name)){
                            decision.cause=RebuildCause::ExternalGeometryChanged;
                            decision.trigger=source;
                            found=true;break;
                        }
                    }else if(source!="@base" && !reusable.at(source)){
                        decision.cause=RebuildCause::UpstreamRecomputed;
                        decision.trigger=source;
                        found=true;break;
                    }
                }
                // A failing plan must never silently claim that a recomputed
                // feature was actually reusable. The legacy cache planner may
                // invalidate broader branches than the local direct cause.
                if(!found){
                    decision.cause=RebuildCause::DependencyChanged;
                    decision.trigger=node.id;
                }
            }
        }
        reusable[node.id]=decision.previousIndex>=0;
        result.push_back(std::move(decision));
    }
    *plan=std::move(candidate);
    *decisions=std::move(result);
    if(error)error->clear();
    return true;
}
}

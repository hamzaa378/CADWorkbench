#include "FeatureDependencyPlan.h"

#include <cmath>
#include <map>
#include <set>
#include <cctype>

namespace cad::dependency {
namespace {
bool validGraph(const std::vector<FeatureRecord>& records,std::string* error,
                const std::set<std::string>* knownSources=nullptr){
    std::set<std::string> seen;
    for(const auto& node:records){
        if(node.id.empty()||node.type.empty()||node.id.front()=='@'||seen.count(node.id)!=0){
            if(error)*error="Empty, reserved, or duplicated feature identity.";
            return false;
        }
        if(node.inputs.size()>2){
            if(error)*error="Too many feature inputs.";
            return false;
        }
        std::set<std::string> inputs;
        for(const auto& input:node.inputs){
            const bool external=input.rfind("@source:",0)==0;
            const bool sourceKnown=external&&knownSources&&
                knownSources->find(input.substr(8))!=knownSources->end();
            if(input.empty()||input==node.id||!inputs.insert(input).second||
               (input!="@base"&&!sourceKnown&&seen.find(input)==seen.end())){
                if(error)*error="Feature references an unknown, duplicate, or later input.";
                return false;
            }
        }
        seen.insert(node.id);
        std::string previousKey;
        for(const auto& [key,value]:node.parameters){
            // Parameters are persisted as a deterministic key-ordered map;
            // duplicate or noncanonical keys make cache identity ambiguous.
            if(key.empty()||(!previousKey.empty()&&key<=previousKey)||
               !std::isfinite(value)){
                if(error)*error="Malformed, duplicate, or unordered feature parameter.";
                return false;
            }
            previousKey=key;
        }
    }
    return true;
}
std::vector<std::string> effectiveInputs(const std::vector<FeatureRecord>& nodes,
                                         std::size_t at){
    if(!nodes[at].inputs.empty())return nodes[at].inputs;
    return {at==0?"@base":nodes[at-1].id};
}
bool sameDefinition(const FeatureRecord& a,const FeatureRecord& b){
    return a.id==b.id&&a.type==b.type&&a.parameters==b.parameters&&
           a.suppressed==b.suppressed&&a.inputs==b.inputs;
}
}
static bool planReuseImpl(const std::vector<FeatureRecord>& previous,
               const std::vector<FeatureRecord>& proposed,
               const std::vector<bool>& previousOutputAvailable,
               ReusePlan* plan,std::string* error,bool baseUnchanged){
    if(!plan||previousOutputAvailable.size()!=previous.size()){
        if(error)*error="Missing destination or inconsistent cache availability.";
        return false;
    }
    if(!validGraph(previous,error)||!validGraph(proposed,error))return false;
    std::map<std::string,std::size_t> oldIndex;
    for(std::size_t i=0;i<previous.size();++i)oldIndex.emplace(previous[i].id,i);
    ReusePlan calculated;
    calculated.previousIndex.assign(proposed.size(),-1);
    std::map<std::string,bool> reusable;
    for(std::size_t i=0;i<proposed.size();++i){
        const auto& proposedNode=proposed[i];
        const auto old=oldIndex.find(proposedNode.id);
        bool canReuse=false;
        if(old!=oldIndex.end()){
            const std::size_t j=old->second;
            canReuse=baseUnchanged&&previousOutputAvailable[j]&&
                sameDefinition(previous[j],proposedNode)&&
                effectiveInputs(previous,j)==effectiveInputs(proposed,i);
            if(canReuse){
                for(const auto& source:effectiveInputs(proposed,i)){
                    if(source!="@base"&&!reusable[source]){
                        canReuse=false;
                        break;
                    }
                }
            }
            if(canReuse)calculated.previousIndex[i]=static_cast<int>(j);
        }
        reusable[proposedNode.id]=canReuse;
        if(canReuse)++calculated.reused;
        else ++calculated.recomputed;
    }
    *plan=std::move(calculated);
    if(error)error->clear();
    return true;
}
bool planReuse(const std::vector<FeatureRecord>& previous,
               const std::vector<FeatureRecord>& proposed,
               const std::vector<bool>& previousOutputAvailable,
               ReusePlan* plan,std::string* error){
    // Legacy callers explicitly promise an unchanged base (see header).
    return planReuseImpl(previous,proposed,previousOutputAvailable,plan,error,true);
}
bool planReuseVersioned(const std::vector<FeatureRecord>& previous,
                        const std::vector<FeatureRecord>& proposed,
                        const std::vector<bool>& previousOutputAvailable,
                        const std::string& previousBaseRevision,
                        const std::string& proposedBaseRevision,
                        ReusePlan* plan,std::string* error){
    if(previousBaseRevision.empty()||proposedBaseRevision.empty()){
        if(error)*error="The previous and proposed base geometry revisions must be nonempty.";
        return false;
    }
    return planReuseImpl(previous,proposed,previousOutputAvailable,plan,error,
                         previousBaseRevision==proposedBaseRevision);
}
namespace {
bool catalog(const std::vector<SourceRevision>& sources,
             std::map<std::string,std::string>* revisions,
             std::set<std::string>* names,std::string* error){
    std::string last;
    for(const auto& source:sources){
        if(source.name.empty()||source.name.size()>64||
           source.geometryRevision.empty()||source.geometryRevision.size()>128||
           (!last.empty()&&source.name<=last)){
            if(error)*error="Source catalog must have unique ordered names and nonempty revisions.";
            return false;
        }
        for(unsigned char character:source.name){
            if(!std::isalnum(character)&&character!='_'&&character!='-'&&character!='.'){
                if(error)*error="Invalid source identifier.";
                return false;
            }
        }
        last=source.name;
        names->insert(source.name);
        revisions->emplace(source.name,source.geometryRevision);
    }
    return true;
}
}

bool planReuseWithSources(const std::vector<FeatureRecord>& previous,
                          const std::vector<FeatureRecord>& proposed,
                          const std::vector<bool>& previousOutputAvailable,
                          const std::string& previousBaseRevision,
                          const std::string& proposedBaseRevision,
                          const std::vector<SourceRevision>& previousSources,
                          const std::vector<SourceRevision>& proposedSources,
                          ReusePlan* plan,std::string* error){
    if(!plan||previousOutputAvailable.size()!=previous.size()||
       previousBaseRevision.empty()||proposedBaseRevision.empty()){
        if(error)*error="A destination, aligned cache availability and two base revisions are required.";
        return false;
    }
    std::map<std::string,std::string> oldRevisions,newRevisions;
    std::set<std::string> oldNames,newNames;
    if(!catalog(previousSources,&oldRevisions,&oldNames,error)||
       !catalog(proposedSources,&newRevisions,&newNames,error)||
       !validGraph(previous,error,&oldNames)||!validGraph(proposed,error,&newNames))return false;
    std::map<std::string,std::size_t> oldIndex;
    for(std::size_t i=0;i<previous.size();++i)oldIndex.emplace(previous[i].id,i);
    const bool baseUnchanged=previousBaseRevision==proposedBaseRevision;
    ReusePlan calculated;
    calculated.previousIndex.assign(proposed.size(),-1);
    std::map<std::string,bool> reusable;
    for(std::size_t i=0;i<proposed.size();++i){
        const auto& node=proposed[i];
        const auto at=oldIndex.find(node.id);
        bool canReuse=false;
        if(at!=oldIndex.end()){
            const std::size_t j=at->second;
            canReuse=previousOutputAvailable[j]&&sameDefinition(previous[j],node)&&
                     effectiveInputs(previous,j)==effectiveInputs(proposed,i);
            if(canReuse){
                for(const auto& input:effectiveInputs(proposed,i)){
                    if(input=="@base"){
                        if(!baseUnchanged){canReuse=false;break;}
                    }else if(input.rfind("@source:",0)==0){
                        const std::string name=input.substr(8);
                        const auto old=oldRevisions.find(name);
                        const auto now=newRevisions.find(name);
                        if(old==oldRevisions.end()||now==newRevisions.end()||
                           old->second!=now->second){canReuse=false;break;}
                    }else if(!reusable[input]){canReuse=false;break;}
                }
            }
            if(canReuse)calculated.previousIndex[i]=static_cast<int>(j);
        }
        reusable[node.id]=canReuse;
        if(canReuse)++calculated.reused;else ++calculated.recomputed;
    }
    *plan=std::move(calculated);
    if(error)error->clear();
    return true;
}

} // namespace cad::dependency

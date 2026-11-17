#include "core/FeatureDependencyPlan.h"
#include "core/FeatureRebuildDiagnostics.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using cad::dependency::FeatureRecord;
using cad::dependency::ReusePlan;
int checks=0;
void check(bool condition,const char* message){
    ++checks;if(!condition)throw std::runtime_error(message);
}
FeatureRecord make(const char* id,const char* type,std::vector<std::string> sources={},
                   double parameter=1.0,bool suppressed=false){
    return FeatureRecord{id,type,{{"length",parameter}},suppressed,std::move(sources)};
}
ReusePlan plan(const std::vector<FeatureRecord>& old,const std::vector<FeatureRecord>& next,
               std::vector<bool> available={}){
    if(available.empty())available.assign(old.size(),true);
    ReusePlan output;std::string error;
    check(cad::dependency::planReuse(old,next,available,&output,&error),"Valid graph was rejected");
    check(error.empty(),"Successful plan retained an error");
    check(output.previousIndex.size()==next.size(),"Wrong plan size");
    check(output.reused+output.recomputed==static_cast<int>(next.size()),"Wrong accounting");
    return output;
}
void expect(const ReusePlan& result,const std::vector<int>& expected){
    check(result.previousIndex==expected,"Incorrect dependency-aware cache mask");
}
void validity(){
    const auto a=make("a","Translate");
    const auto b=make("b","Translate",{"a"});
    const auto base=make("c","Translate",{"@base"});
    std::string error;ReusePlan out;
    check(!cad::dependency::planReuse({a},{a},{},&out,&error),"Mismatched cache accepted");
    check(!cad::dependency::planReuse({a},{a},{true},nullptr,&error),"Null destination accepted");
    check(!cad::dependency::planReuse({a},{a,b,b},{true},&out,&error),"Duplicate new ID accepted");
    check(!cad::dependency::planReuse({a},{b,a},{true},&out,&error),"Forward reference accepted");
    check(!cad::dependency::planReuse({a},{make("self","Translate",{"self"})},{true},&out,&error),"Self link accepted");
    check(!cad::dependency::planReuse({a},{make("empty","Translate",{""})},{true},&out,&error),"Empty input accepted");
    check(!cad::dependency::planReuse({a},{make("same","Translate",{"@base","@base"})},{true},&out,&error),"Duplicate source accepted");
    check(!cad::dependency::planReuse({a,a},{base},{true,true},&out,&error),"Duplicate old ID accepted");
    check(!cad::dependency::planReuse({a},{make("@base","Translate")},{true},&out,&error),"Reserved ID accepted");
    auto badType=a;badType.type.clear();
    check(!cad::dependency::planReuse({a},{badType},{true},&out,&error),"Empty operation type accepted");
    auto badParameters=a;badParameters.parameters={{"a",1.0},{"a",2.0}};
    check(!cad::dependency::planReuse({a},{badParameters},{true},&out,&error),"Duplicate parameter name accepted");
    badParameters.parameters={{"z",1.0},{"a",2.0}};
    check(!cad::dependency::planReuse({a},{badParameters},{true},&out,&error),"Unordered parameter map accepted");
    check(!cad::dependency::planReuse({a},{make("over","Translate",{"@base","a","b"})},{true},&out,&error),"Three inputs accepted");
    check(out.previousIndex.empty(),"Rejected plan clobbered result");
}
void branches(){
    std::vector<FeatureRecord> original={
        make("left","Translate",{"@base"},3),
        make("right","Translate",{"@base"},5),
        make("leftNext","Translate",{"left"},7),
        make("rightNext","Translate",{"right"},9),
        make("union","Fuse",{"leftNext","rightNext"},1),
        make("finish","Fillet",{},2)
    };
    expect(plan(original,original),{0,1,2,3,4,5});
    auto next=original;
    next[0].parameters[0].second=4;
    expect(plan(original,next),{-1,1,-1,3,-1,-1});
    next=original;next[1].parameters[0].second=6;
    expect(plan(original,next),{0,-1,2,-1,-1,-1});
    next=original;next[4].suppressed=true;
    expect(plan(original,next),{0,1,2,3,-1,-1});
    next=original;next[5].parameters[0].second=20;
    expect(plan(original,next),{0,1,2,3,4,-1});
    next=original;
    std::swap(next[0],next[1]);
    expect(plan(original,next),{1,0,2,3,4,5});
    next=original;
    std::swap(next[2],next[3]);
    expect(plan(original,next),{0,1,3,2,4,5});
    next=original;next.insert(next.begin()+2,make("insert","Translate",{"@base"},13));
    expect(plan(original,next),{0,1,-1,2,3,4,5});
    next=original;next.insert(next.begin()+2,make("insert","Translate",{},13));
    expect(plan(original,next),{0,1,-1,2,3,4,5});
    next=original;
    next[2].inputs.clear(); // now follows right instead of left
    expect(plan(original,next),{0,1,-1,3,-1,-1});
    next=original;
    next.erase(next.begin()+1); // right becomes unknown to the following branch
    std::string error;ReusePlan result;
    check(!cad::dependency::planReuse(original,next,std::vector<bool>(original.size(),true),&result,&error),"Broken dependency accepted");
    check(!error.empty(),"Broken graph has no diagnostic");
    next=original;next[4].inputs={"leftNext","rightNext"};
    expect(plan(original,next,{true,true,false,true,true,true}),{0,1,-1,3,-1,-1});
    next=original;next[0].id="renamed";next[2].inputs={"renamed"};
    expect(plan(original,next),{-1,1,-1,3,-1,-1});
    // Appending a sequential feature changes none of the earlier cached outputs.
    next=original;next.push_back(make("new","Translate",{},11));
    expect(plan(original,next),{0,1,2,3,4,5,-1});
    // A newly inserted implicit feature invalidates a later implicit child.
    const std::vector<FeatureRecord> linear={make("x","Translate"),make("y","Translate"),make("z","Translate")};
    auto altered=linear;altered.insert(altered.begin()+1,make("new","Translate"));
    expect(plan(linear,altered),{0,-1,-1,-1}); // z still depends on y; y changed predecessor
}
void sourceRevisions(){
    const std::vector<FeatureRecord> featureGraph={
        make("baseA","Extrude",{"@base"}),make("baseB","Revolve",{"@base"}),
        make("fuse","Fuse",{"baseA","baseB"}),make("finish","Fillet",{"fuse"})
    };
    cad::dependency::ReusePlan report;std::string error;
    const std::vector<bool> available(featureGraph.size(),true);
    check(cad::dependency::planReuseVersioned(featureGraph,featureGraph,available,"rev1","rev1",&report,&error),
          "Identical base revision should allow reuse planning");
    expect(report,{0,1,2,3});
    check(cad::dependency::planReuseVersioned(featureGraph,featureGraph,available,"rev1","rev2",&report,&error),
          "Changed base revision must be supported");
    expect(report,{-1,-1,-1,-1});
    const auto last=report.previousIndex;
    check(!cad::dependency::planReuseVersioned(featureGraph,featureGraph,available,"","rev2",&report,&error),
          "Unspecified source revision must be rejected");
    check(report.previousIndex==last,"Rejected revision plan must not mutate output");
    check(!cad::dependency::planReuseVersioned(featureGraph,featureGraph,available,"rev1","",&report,&error),
          "Missing target revision must be rejected");
    auto changed=featureGraph;changed[0].parameters[0].second+=1;
    check(cad::dependency::planReuseVersioned(featureGraph,changed,available,"rev1","rev1",&report,&error),
          "Node edits with same base should retain independent branches");
    expect(report,{-1,1,-1,-1});
}
void externalSources(){
    using cad::dependency::SourceRevision;
    const std::vector<FeatureRecord> graph={
        make("bodyA","SketchBoss",{"@source:sketchA"}),
        make("bodyB","SketchBoss",{"@source:sketchB"}),
        make("cap","Loft",{"@base"}),
        make("cutA","Pocket",{"bodyA"}),
        make("cutB","Pocket",{"bodyB"}),
        make("join","Fuse",{"cutA","cutB"}),
        make("finish","Fillet",{"join"})};
    const std::vector<SourceRevision> old={{"sketchA","rev-a1"},{"sketchB","rev-b1"}};
    const std::vector<bool> available(graph.size(),true);
    ReusePlan result;std::string err;
    auto compare=[&](const std::string& oldBase,const std::string& newBase,
                     const std::vector<SourceRevision>& after,const std::vector<int>& expected){
        check(cad::dependency::planReuseWithSources(graph,graph,available,oldBase,newBase,
                      old,after,&result,&err),"Valid external source graph rejected");
        expect(result,expected);
    };
    compare("base1","base1",old,{0,1,2,3,4,5,6});
    compare("base1","base2",old,{0,1,-1,3,4,5,6});
    compare("base1","base1",{{"sketchA","rev-a2"},{"sketchB","rev-b1"}},
             {-1,1,2,-1,4,-1,-1});
    compare("base1","base1",{{"sketchA","rev-a1"},{"sketchB","rev-b2"}},
             {0,-1,2,3,-1,-1,-1});
    compare("base1","base2",{{"sketchA","rev-a2"},{"sketchB","rev-b1"}},
             {-1,1,-1,-1,4,-1,-1});
    const auto prior=result.previousIndex;
    const std::vector<SourceRevision> unsorted={{"sketchB","x"},{"sketchA","y"}};
    check(!cad::dependency::planReuseWithSources(graph,graph,available,"a","a",
          old,unsorted,&result,&err),"Unsorted source catalog accepted");
    check(result.previousIndex==prior,"Bad catalog changed old result");
    check(!cad::dependency::planReuseWithSources(graph,graph,available,"a","a",
          old,{{"sketchA","a"},{"sketchA","b"}},&result,&err),
          "Duplicate source identifier accepted");
    check(!cad::dependency::planReuseWithSources(graph,graph,available,"a","a",
          old,{{"sketchA","a"}},&result,&err),"Unknown referenced source accepted");
    check(!cad::dependency::planReuseWithSources(graph,graph,available,"a","a",
          old,{{"sketchA",""},{"sketchB","b"}},&result,&err),
          "Empty source revision accepted");
    check(!cad::dependency::planReuseWithSources(graph,graph,available,"a","a",
          old,{{"sketchA","a"},{"sketch B","b"}},&result,&err),
          "Malformed source ID accepted");
    check(!cad::dependency::planReuse(graph,graph,available,&result,&err),
          "Legacy planner silently accepted external sources");
    const auto modified=std::vector<FeatureRecord>{make("only","Extrude",{"@source:sketchC"})};
    check(!cad::dependency::planReuseWithSources(graph,modified,available,"a","a",
          old,old,&result,&err),"Unknown source in proposed graph accepted");
}
void stress(){
    // Exercise 128 independent branches, changed roots, and reorderings.
    std::vector<FeatureRecord> nodes;
    for(int i=0;i<128;++i)
        nodes.push_back(make(("branch"+std::to_string(i)).c_str(),"Translate",{"@base"},i+1.));
    auto changed=nodes;
    for(int i=0;i<128;i+=5)changed[i].parameters[0].second+=1.;
    auto result=plan(nodes,changed);
    for(int i=0;i<128;++i)check(result.previousIndex[i]==(i%5==0?-1:i),"Independent branch invalidation leaked");
    auto reordered=nodes;
    std::reverse(reordered.begin(),reordered.end());
    result=plan(nodes,reordered);
    for(int i=0;i<128;++i)check(result.previousIndex[i]==127-i,"Explicitly rooted reorder was not reused");
}
}

void causalRebuildDiagnostics(){
    using namespace cad::dependency;
    const std::vector<FeatureRecord> before={
        make("left","Extrude",{"@source:sketchA"}),
        make("right","Extrude",{"@source:sketchB"}),
        make("cut","Pocket",{"left"}),
        make("join","Fuse",{"cut","right"})};
    std::vector<FeatureRecord> after=before;
    ReusePlan plan;std::vector<RebuildDecision> decision;std::string err;
    const std::vector<SourceRevision> sources={{"sketchA","v1"},{"sketchB","v1"}};
    const auto afterSources=std::vector<SourceRevision>{{"sketchA","v2"},{"sketchB","v1"}};
    check(explainReuseWithSources(before,after,{true,true,true,true},"b1","b1",sources,afterSources,&plan,&decision,&err),
          "Change-impact explanation rejected a valid source edit");
    expect(plan,{-1,1,-1,-1});
    check(decision.size()==4,"Missing change-impact decisions");
    check(decision[0].cause==RebuildCause::ExternalGeometryChanged&&decision[0].trigger=="@source:sketchA",
          "Missing originating external-source explanation");
    check(decision[1].cause==RebuildCause::Reused,"Independent cached branch must retain reuse");
    check(decision[2].cause==RebuildCause::UpstreamRecomputed&&decision[2].trigger=="left",
          "Downstream feature missing cause");
    check(decision[3].cause==RebuildCause::UpstreamRecomputed&&decision[3].trigger=="cut",
          "Join must explain the first invalid input");
    const auto retained=decision;
    const auto indices=plan.previousIndex;
    check(!explainReuseWithSources(before,after,{true},"b1","b1",sources,afterSources,&plan,&decision,&err),
          "Invalid cache vector accepted");
    check(plan.previousIndex==indices&&decision.size()==retained.size(),
          "Failed change-impact analysis modified outputs");
    after=before;
    after[0].parameters[0].second+=1;
    check(explainReuseWithSources(before,after,{true,true,true,true},"b1","b1",sources,sources,&plan,&decision,&err),
          "Local dimension edit rejected");
    check(decision[0].cause==RebuildCause::DefinitionChanged,"Wrong local edit cause");
    after=before;
    check(explainReuseWithSources(before,after,{true,true,true,true},"b1","b2",sources,sources,&plan,&decision,&err),
          "Independent branches should survive base source revision");
    check(plan.reused==4,"Source-only features should not be invalidated by unrelated base revision");
}

int main(){
    try{validity();branches();sourceRevisions();externalSources();stress();causalRebuildDiagnostics();std::cout<<"PASS: "<<checks<<" sparse graph planning checks\n";return 0;}
    catch(const std::exception& e){std::cerr<<"FAIL check "<<checks<<": "<<e.what()<<'\n';return 1;}
}

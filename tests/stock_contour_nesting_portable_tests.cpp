#include "core/StockContourNesting.h"
#include <cassert>
#include <cmath>
#include <random>
using namespace cad::contour_nesting;
int main(){
    std::string issue;Layout result;
    assert(build({{"a",100,100,{{60,60,40,40}}},
                  {"b",42,42,{{0,0,3,3}}}},150,130,1,true,&result,&issue));
    assert(result.items.size()==2);
    // Two rectangular envelopes may overlap safely where one blank has a
    // void: physical material regions, not envelopes, determine collisions.
    Layout interlocked;
    assert(build({{"L",60,60,{{20,20,40,40}}},{"S",35,35,{}}},
                 90,90,1,false,&interlocked,&issue));
    assert(interlocked.plan.sheets==1);
    const auto& left=interlocked.items[0].placement;
    const auto& nested=interlocked.items[1].placement;
    assert(left.xMm+left.lengthMm>nested.xMm &&
           nested.xMm+nested.lengthMm>left.xMm &&
           left.yMm+left.widthMm>nested.yMm &&
           nested.yMm+nested.widthMm>left.yMm);
    for(const auto& x:interlocked.items[0].material)
        for(const auto& y:interlocked.items[1].material)
            assert(!intersect(x,y,1,1e-7));
    for(const auto& a:result.items)for(const auto& b:result.items){
        if(a.placement.id==b.placement.id||a.placement.sheet!=b.placement.sheet)continue;
        for(const auto& x:a.material)for(const auto& y:b.material)
            assert(!intersect(x,y,1,1e-7));
    }
    auto original=result;
    assert(!build({{"a",10,10,{{9,9,5,5}}}},100,100,1,true,&result,&issue));
    assert(result.items.size()==original.items.size());
    assert(!build({{"a",10,10,{}},{"a",10,10,{}}},100,100,1,false,&result,&issue));
    assert(!build({{"bad",10,10,{{0,0,10,10}}}},100,100,1,false,&result,&issue));
    assert(build({{"one",25,12,{}},{"two",30,14,{}}},100,50,2,false,&result,&issue));
    assert(result.plan.totalBlankAreaMm2==25*12+30*14);
    std::mt19937 rng(1707);
    for(int trial=0;trial<120;++trial){
        std::vector<Blank> blanks;
        for(int n=0;n<10;++n){
            const double w=10+rng()%32,h=10+rng()%28;
            Blank b{std::to_string(n),w,h,{}};
            if(trial%3!=0)b.notches.push_back({w/3,0,w/3,h/4});
            blanks.push_back(b);
        }
        assert(build(blanks,100,80,1,trial%2==0,&result,&issue));
        for(std::size_t i=0;i<result.items.size();++i){
            const auto& a=result.items[i];
            assert(a.placement.xMm>=1&&a.placement.yMm>=1);
            assert(a.placement.xMm+a.placement.lengthMm+1<=100+1e-6);
            assert(a.placement.yMm+a.placement.widthMm+1<=80+1e-6);
            for(std::size_t j=i+1;j<result.items.size();++j){
                const auto& b=result.items[j];if(a.placement.sheet!=b.placement.sheet)continue;
                for(const auto& x:a.material)for(const auto& y:b.material)
                    assert(!intersect(x,y,1,1e-7));
            }
        }
    }
}

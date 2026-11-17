#include "core/StockSheetLayout.h"
#include <cassert>
#include <cmath>
#include <string>
#include <random>
#include <vector>
using namespace cad::stock_layout;
int main(){
    Plan result;std::string err;
    assert(build({{"a",42,20},{"b",50,30},{"c",30,20}},100,65,2,false,&result,&err));
    assert(result.placements.size()==3);
    assert(result.sheets>=1);
    for(auto& a:result.placements){
        assert(a.xMm>=2 && a.yMm>=2);
        assert(a.xMm+a.lengthMm+2<=100+1e-7);
        assert(a.yMm+a.widthMm+2<=65+1e-7);
        for(auto& b:result.placements){
            if(a.id==b.id||a.sheet!=b.sheet)continue;
            assert(a.xMm+a.lengthMm+2<=b.xMm+1e-7||b.xMm+b.lengthMm+2<=a.xMm+1e-7||
                   a.yMm+a.widthMm+2<=b.yMm+1e-7||b.yMm+b.widthMm+2<=a.yMm+1e-7);
        }
    }
    Plan before=result;
    assert(!build({{"a",42,20},{"a",50,30}},100,65,2,false,&result,&err));
    assert(result.placements.size()==before.placements.size());
    assert(!build({{"a",100,100}},100,65,2,true,&result,&err));
    assert(result.placements.size()==before.placements.size());
    assert(build({{"wide",55,80}},100,65,2,true,&result,&err));
    assert(result.placements.size()==1 && result.placements[0].rotated);
    assert(!build({{"wide",55,80}},100,65,2,false,&result,&err));
    assert(build({{"a",90,55},{"b",90,55}},100,65,2,false,&result,&err));
    assert(result.sheets==2 && result.placements[1].sheet==2);
    assert(result.utilization>0 && result.utilization<1);
    assert(!build({{"n",NAN,3}},100,65,2,false,&result,&err));
    assert(!build({{"n",1,3}},100,65,-2,false,&result,&err));
    assert(!build({},100,65,2,false,&result,&err));
    // A tall left-hand item leaves two right-hand slots. A shelf-only
    // planner wastes the second slot on another stock sheet.
    assert(build({{"tall",50,58},{"right_top",35,15},{"right_bottom",35,15}},
                 100,80,2,false,&result,&err));
    assert(result.sheets==1 && result.placements.size()==3);
    assert(result.placements[2].sheet==1);
    // Deterministic stress regression: no envelope may overlap a different
    // blank on the same sheet, including across shelf and sheet transitions.
    std::mt19937 rng(7170);
    for(int caseNo=0;caseNo<350;++caseNo){
        std::vector<Blank> input;
        for(int i=0;i<24;++i){
            const double length=5+static_cast<double>(rng()%58);
            const double width=4+static_cast<double>(rng()%45);
            input.push_back({std::to_string(i),length,width});
        }
        assert(build(input,100,80,2,caseNo%2==0,&result,&err));
        assert(result.placements.size()==input.size());
        assert(result.sheets>=1);
        for(std::size_t i=0;i<result.placements.size();++i){
            const auto& a=result.placements[i];
            assert(a.sheet>=1&&a.sheet<=result.sheets);
            assert(a.xMm>=2&&a.yMm>=2);
            assert(a.xMm+a.lengthMm+2<=100+1e-7);
            assert(a.yMm+a.widthMm+2<=80+1e-7);
            for(std::size_t j=i+1;j<result.placements.size();++j){
                const auto& b=result.placements[j];
                if(a.sheet!=b.sheet)continue;
                assert(a.xMm+a.lengthMm+2<=b.xMm+1e-7||
                       b.xMm+b.lengthMm+2<=a.xMm+1e-7||
                       a.yMm+a.widthMm+2<=b.yMm+1e-7||
                       b.yMm+b.widthMm+2<=a.yMm+1e-7);
            }
        }
    }
}

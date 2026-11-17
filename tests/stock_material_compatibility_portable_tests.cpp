#include "core/StockMaterialCompatibility.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <string>
#include <vector>
using namespace cad::stock_material;
int main(){
    Result result;std::string issue;
    assert(validate({{"one","STEEL",1.5},{"two","STEEL",1.5}},&result,&issue));
    assert(result.count==2&&result.materialAssigned&&result.material=="STEEL");
    assert(std::abs(result.thicknessMm-1.5)<1e-10);
    const auto old=result;
    assert(!validate({{"one","STEEL",1.5},{"two","ALUMINUM",1.5}},&result,&issue));
    assert(result.material==old.material);
    assert(!validate({{"one","STEEL",1.5},{"two","STEEL",1.7}},&result,&issue));
    assert(!validate({{"one","STEEL",1.5},{"two","",1.5}},&result,&issue));
    assert(validate({{"one","",1.5},{"two","",1.5}},&result,&issue));
    assert(!result.materialAssigned);
    assert(!validate({{"one","",1.5},{"two","",std::numeric_limits<double>::quiet_NaN()}},&result,&issue));
    assert(!validate({{"one","",1.5},{"one","",1.5}},&result,&issue));
    assert(!validate({{"one","",1.5},{"one","",1.5}},nullptr,&issue));
    assert(!validate({},&result,&issue));
    assert(!validate({{"","STEEL",1.5}},&result,&issue));
}

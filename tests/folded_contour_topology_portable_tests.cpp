#include "../src/core/FoldedContourTopology.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <array>
#include <utility>

int main(){
    using cad::detail::ContourPoint;
    using cad::detail::simpleClosedContour;
    auto valid=[](std::initializer_list<ContourPoint> v){return simpleClosedContour(std::vector<ContourPoint>(v));};
    assert(valid({{0,0},{10,0},{10,1},{0,1}}));
    assert(valid({{0,0},{20,0},{20,1},{4,1},{4,8},{3,8},{3,1},{0,1}}));
    assert(!valid({{0,0},{5,5},{0,5},{5,0}})); // bow tie
    assert(!valid({{0,0},{5,0},{2,0},{4,5},{0,5}})); // adjacent reversal
    assert(!valid({{0,0},{10,0},{10,10},{0,10},{5,0}})); // nonadjacent T-junction
    assert(!valid({{0,0},{10,0},{10,10},{0,10},{0,0}})); // repeated closing vertex
    assert(!valid({{0,0},{0,0},{1,1}}));
    assert(!valid({{0,0},{1,1}}));
    assert(valid({{0,0},{1000000,0},{1000000,0.00002},{0,0.00002}})); // long thin flange
    // Two disjoint near-parallel outlines do not collide by relative tolerance.
    assert(valid({{0,0},{30,0},{30,0.00001},{1,0.00001},{1,10},{0,10}}));
    std::pair<std::size_t,std::size_t> bad{999,999};
    assert(!simpleClosedContour(std::vector<ContourPoint>{{0,0},{5,5},{0,5},{5,0}},&bad));
    assert(bad.first!=999&&bad.second!=999);
    std::cout<<"PASS folded contour topology portable checks\n";
}

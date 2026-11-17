#include "core/SimultaneousProximity.h"
#include <cassert>
#include <cmath>
#include <string>
using namespace cad::proximity;
int main(){
    Tracker tracker;std::string error;
    const Bounds a{0,0,0,10,10,10};
    const Bounds distant{20,0,0,30,10,10};
    const Bounds touching{10,0,0,15,10,10};
    // Time 0: well separated, time 1: simultaneously contacting.
    assert(tracker.observe({{"a",a},{"b",distant}},&error));
    assert(tracker.observe({{"a",a},{"b",touching}},&error));
    assert(tracker.sampleCount()==2&&tracker.pairs().size()==1);
    assert(tracker.pairs()[0].simultaneousOverlapSamples==1);
    assert(tracker.pairs()[0].closestSample==1);
    assert(tracker.pairs()[0].minimumAabbGapMm==0.0);
    // Invalid/ambiguous observations must not change the evidence.
    assert(!tracker.observe({{"a",a},{"a",touching}},&error));
    assert(!tracker.observe({{"b",touching},{"a",a}},&error));
    assert(!tracker.observe({{"a",a},{"b",{NAN,0,0,1,1,1}}},&error));
    assert(tracker.sampleCount()==2&&tracker.pairs()[0].simultaneousOverlapSamples==1);
    Tracker disjointAtAllTimes;
    assert(disjointAtAllTimes.observe({{"a",{0,0,0,1,1,1}},
                                      {"b",{10,0,0,11,1,1}}},&error));
    assert(disjointAtAllTimes.observe({{"a",{10,0,0,11,1,1}},
                                      {"b",{0,0,0,1,1,1}}},&error));
    // Both bodies share their UNION workspace, but never at the same time.
    assert(disjointAtAllTimes.pairs()[0].simultaneousOverlapSamples==0);
    assert(std::abs(disjointAtAllTimes.pairs()[0].minimumAabbGapMm-9.0)<1e-12);
    OrderInvariantTracker imported;
    assert(imported.observe({{"fixture",a},{"slider",distant}},&error));
    assert(imported.observe({{"slider",touching},{"fixture",a}},&error));
    assert(imported.sampleCount()==2&&imported.pairs().size()==1);
    assert(imported.pairs()[0].simultaneousOverlapSamples==1);
    assert(imported.pairs()[0].closestSample==1);
    const auto saved=imported.pairs()[0];
    assert(!imported.observe({{"slider",touching},{"slider",a}},&error));
    assert(!imported.observe({{"fixture",a},{"slider",{NAN,0,0,1,1,1}}},&error));
    assert(!imported.observe({{"fixture",a},{"unknown",distant}},&error));
    assert(imported.sampleCount()==2&&imported.pairs()[0].closestSample==saved.closestSample&&
           imported.pairs()[0].simultaneousOverlapSamples==saved.simultaneousOverlapSamples);
    Tracker singular;
    assert(singular.observe({{"sole",a}},&error));
    assert(singular.sampleCount()==1&&singular.pairs().empty());
}

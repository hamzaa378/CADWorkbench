#pragma once
#include <string>
#include <vector>

namespace cad {
struct StudyInput {
    std::string key, label, unit;
    double example=0, minimum=0, maximum=1e12;
};
struct GuidedStudy {
    std::string id, name, group, assumptions;
    std::vector<StudyInput> inputs;
};
struct StudyMetric {
    std::string name, unit;
    double value=0;
};
struct GuidedStudyResult {
    bool valid=false;
    std::vector<StudyMetric> metrics;
    std::vector<std::string> warnings;
    std::string error;
};
const std::vector<GuidedStudy>& guidedEngineeringStudyCatalog();
GuidedStudyResult evaluateGuidedEngineeringStudy(const std::string& id,const std::vector<double>& inputs);
}

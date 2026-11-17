#pragma once

#include <string>
#include <vector>

// Machine-neutral DESIGN REVIEW ONLY. No executable toolpaths or G-code.
// All linear coordinates are in millimetres in one common work coordinate
// system. A CAD hole is not evidence of machinability, fixture clearance, or
// an approved drilling process.
namespace cad::drilling {
struct Hole {
    std::string label;
    double x=0,y=0,diameter=0,topZ=0,bottomZ=0;
    bool through=false;
};
struct Tool {
    std::string id;
    double diameter=0;
    double usableLength=0;
};
struct Setup {
    std::string bodyId;
    std::string geometryRevision;
    double stockTopZ=0;
    double stockBottomZ=0;
    double clearanceZ=0;
    double homeX=0,homeY=0;
    double diameterTolerance=0.05;
    // Through operations always require separate fixture/exit clearance review.
    // This flag permits listing them as CONDITIONAL, never as approved motion.
    bool fixtureExitReviewed=false;
};
enum class Decision {EligibleForReview,ConditionalFixtureReview,Rejected};
struct Row {
    Hole hole;
    std::string toolId;
    double penetration=0;
    double approachZ=0;
    Decision decision=Decision::Rejected;
    std::string reason;
    int visitOrder=0; // 0 for rejected; nearest-next planning order otherwise
};
struct Plan {
    std::string bodyId;
    std::string geometryRevision;
    double clearanceZ=0;
    std::vector<Row> rows;
    int eligible=0,conditional=0,rejected=0;
    double estimatedXYTravel=0;
};
// Returns false for a malformed input and never changes *out on failure.
// Individual design-review issues are reported as Rejected rows on success.
bool plan(const Setup& setup,const std::vector<Hole>& holes,
          const std::vector<Tool>& tools,Plan* out,std::string* error=nullptr);
// RFC-4180-style quoted text, formula-injection hardened. This is a snapshot
// with the exact source revision and conspicuous non-production disclaimer.
bool exportReviewCsv(const Plan& plan,std::string* csv,std::string* error=nullptr);
} // namespace cad::drilling

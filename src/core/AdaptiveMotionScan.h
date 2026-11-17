#pragma once

#include <functional>
#include <string>
#include <vector>

// A deterministic, bounded refinement scheduler for motion-study samples.
// It does NOT certify continuous collision freedom: a narrow collision can
// still lie between samples. All source keyframes must be in initialTimes.
namespace cad::motion {
struct Observation {
    double timeSeconds = 0;
    double clearanceMm = 0;
    double overlapVolumeMm3 = 0;
};
struct Settings {
    int maximumSamples = 128;            // Including keyframes/endpoints.
    double safetyClearanceMm = 1.0;       // Extra refinement near this gap.
    double coarseStepSeconds = 0.25;     // Maximum desired quiet interval.
    double fineStepSeconds = 0.015625;    // Minimum interval worth subdividing.
    // Optional independent engineering bound on the maximum relative boundary
    // speed of BOTH solids (mm/s) throughout the scan. -1 disables the proof.
    // The caller must establish this bound; samples cannot infer it safely.
    double provenRelativeSpeedBoundMmPerSecond = -1.0;
};
// Each final sampling interval receives a reviewable status. A proof is
// conditional on the independently established surface-speed bound.
struct IntervalAssessment {
    double startSeconds = 0.0;
    double endSeconds = 0.0;
    double minimumEndpointClearanceMm = 0.0;
    // Raw Lipschitz lower bound, possibly negative; 0 when no bound is given.
    double clearanceLowerBoundMm = 0.0;
    bool speedBoundAvailable = false;
    bool sampledOverlap = false;
    bool provenAboveMargin = false;
    bool refinementNeeded = false;
};
struct Result {
    std::vector<Observation> observations; // Strictly increasing timestamps.
    std::vector<IntervalAssessment> intervals; // One per adjacent sample pair.
    bool budgetExhausted = false;        // Unresolved intervals remain.
    int intervalsNeedingRefinement = 0;
    // Count of intervals NOT proven to remain above safetyClearanceMm.
    // If no independent speed bound was provided, every interval is unproven.
    int intervalsWithoutClearanceProof = 0;
    // Conditional on a correct, externally justified speed bound. This is
    // NOT a formal certificate unless that bound has itself been validated.
    bool clearanceBoundSatisfied = false;
    double conservativeClearanceLowerBoundMm = 0.0;
};
using Evaluate = std::function<bool(double,Observation*,std::string*)>;
// Never modifies result on validation or evaluation failure.
// Intermediate samples are evaluated exactly once, with no geometry mutation
// required from the caller. The callback must be deterministic for a timestamp.
bool scanAdaptive(const std::vector<double>& initialTimes,const Settings& settings,
                  const Evaluate& evaluate,Result* result,std::string* error=nullptr);
} // namespace cad::motion

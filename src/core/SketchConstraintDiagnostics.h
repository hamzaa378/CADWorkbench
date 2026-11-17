#pragma once

#include <cmath>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

// Standalone numerical-rank advisory used by the Qt sketch solver. It never
// edits a sketch or deletes constraints; it identifies whole constraint blocks
// whose Jacobian equations add no independent local information. Such a block
// may be harmless redundancy, not a conflicting constraint. The input rows
// describe the converged configuration; a failed/nonconverged sketch must be
// diagnosed separately using residuals.
namespace cad::sketchdiagnostics {
inline bool findRedundantConstraints(const std::vector<std::vector<double>>& jacobian,
                                     const std::vector<int>& equationsPerConstraint,
                                     std::vector<int>* redundant,
                                     std::string* error=nullptr,
                                     std::vector<int>* locallyMobileCoordinates=nullptr){
    const auto fail=[&](const char* message){if(error)*error=message;return false;};
    if(!redundant||equationsPerConstraint.empty()||jacobian.empty())
        return fail("Jacobian, constraint equation counts and destination are required.");
    const std::size_t variables=jacobian.front().size();
    if(variables==0||variables>512||jacobian.size()>1024||
       equationsPerConstraint.size()>512)
        return fail("Unsupported Jacobian dimensions.");
    std::size_t rows=0;
    for(int count:equationsPerConstraint){
        if(count<1||count>4)return fail("Invalid number of equations for a constraint.");
        rows+=static_cast<std::size_t>(count);
    }
    if(rows!=jacobian.size())return fail("Constraint equations do not match Jacobian rows.");
    for(const auto& row:jacobian){
        if(row.size()!=variables)return fail("Jagged sketch Jacobian.");
        for(double x:row)if(!std::isfinite(x))return fail("Non-finite sketch Jacobian.");
    }
    std::vector<std::vector<double>> independentRows;
    independentRows.reserve(std::min(jacobian.size(),variables));
    std::vector<int> next;
    std::size_t rowIndex=0;
    for(std::size_t index=0;index<equationsPerConstraint.size();++index){
        bool independent=false;
        for(int rowCount=0;rowCount<equationsPerConstraint[index];++rowCount){
            std::vector<double> row=jacobian[rowIndex++];
            double norm=0.0;
            for(double x:row)norm=std::hypot(norm,x);
            if(!std::isfinite(norm))return fail("Jacobian row norm overflowed.");
            if(norm<1e-12)continue;
            for(double& x:row)x/=norm;
            // Twice-reorthogonalized modified Gram-Schmidt; local rank only.
            for(int pass=0;pass<2;++pass){
                for(const auto& basis:independentRows){
                    double projection=0.0;
                    for(std::size_t column=0;column<variables;++column)
                        projection+=row[column]*basis[column];
                    for(std::size_t column=0;column<variables;++column)
                        row[column]-=projection*basis[column];
                }
            }
            double remainder=0.0;
            for(double x:row)remainder=std::hypot(remainder,x);
            if(!std::isfinite(remainder))return fail("Jacobian orthogonalization overflowed.");
            if(remainder>1e-7){
                for(double& x:row)x/=remainder;
                independentRows.push_back(std::move(row));
                independent=true;
            }
        }
        if(!independent)next.push_back(static_cast<int>(index));
    }
    // The orthonormal row space is the locally constrained subspace.
    // A coordinate with a nonzero projection onto its orthogonal complement
    // participates in at least one local motion. Several marked coordinates
    // may represent the SAME degree of freedom: these are diagnostic hints,
    // not an independently movable-axis list or a guaranteed finite motion.
    std::vector<int> mobile;
    if(locallyMobileCoordinates){
        for(std::size_t column=0;column<variables;++column){
            double constrainedProjectionSquared=0.0;
            for(const auto& basis:independentRows)
                constrainedProjectionSquared+=basis[column]*basis[column];
            if(constrainedProjectionSquared<1.0-1e-7)
                mobile.push_back(static_cast<int>(column));
        }
    }
    *redundant=std::move(next);
    if(locallyMobileCoordinates)*locallyMobileCoordinates=std::move(mobile);
    if(error)error->clear();
    return true;
}
} // namespace cad::sketchdiagnostics

#ifndef OBLO_INCLUDE_SURFELS_REDUCTION
#define OBLO_INCLUDE_SURFELS_REDUCTION

uint find_lowest_within_subgroup(in float value)
{
    const float lowestCoverage = subgroupMin(value);
    const bool isBestCandidate = lowestCoverage == value;
    const uvec4 bestCandidateBallot = subgroupBallot(isBestCandidate);
    return subgroupBallotFindLSB(bestCandidateBallot);
}

#endif
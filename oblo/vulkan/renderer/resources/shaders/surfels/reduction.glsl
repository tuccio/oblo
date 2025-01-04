#ifndef OBLO_INCLUDE_SURFELS_REDUCTION
#define OBLO_INCLUDE_SURFELS_REDUCTION

uint find_lowest_within_subgroup(in float value)
{
    const float lowestValue = subgroupMin(value);
    const bool isBestCandidate = lowestValue == value;
    const uvec4 bestCandidateBallot = subgroupBallot(isBestCandidate);
    return subgroupBallotFindLSB(bestCandidateBallot);
}

#endif
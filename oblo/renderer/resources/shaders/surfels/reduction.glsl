#ifndef OBLO_INCLUDE_SURFELS_REDUCTION
#define OBLO_INCLUDE_SURFELS_REDUCTION

#include <renderer/random/random>

/// @brief Looks for the minimum within a subgroup.
/// @remarks When the minimum is not unique, results will be biased towards the lowest active thread.
/// @params value The value to do perform the reduction on.
/// @return An index in the range [0, gl_SubgroupSize), representing the subgroup invocation holding the minimum.
uint find_lowest_within_subgroup(in float value)
{
    const float lowestValue = subgroupMin(value);
    const bool isBestCandidate = lowestValue == value;
    const uvec4 bestCandidateBallot = subgroupBallot(isBestCandidate);
    return subgroupBallotFindLSB(bestCandidateBallot);
}

/// @brief Looks for the minimum within a subgroup, adding some randomness to eliminate bias towards lower value.
/// @params value The value to do perform the reduction on.
/// @params subgroupSeed A random seed, required to be uniform across all invocations of the subgroup.
/// @return An index in the range [0, gl_SubgroupSize), representing the subgroup invocation holding the minimum.
uint find_lowest_within_subgroup_rand(in float value, inout uint subgroupSeed)
{
    const float lowestValue = subgroupMin(value);
    const bool isBestCandidate = lowestValue == value;

    const uvec4 bestCandidateBallot = subgroupBallot(isBestCandidate);
    const uint first = subgroupBallotFindLSB(bestCandidateBallot);
    const uint last = subgroupBallotFindMSB(bestCandidateBallot);
    const uint count = subgroupBallotBitCount(bestCandidateBallot);

    const uint choice = hash_pcg(subgroupSeed) % count;

    uint index = first;

    for (uint relative = 0; relative < choice; ++relative)
    {
        // Remove the indices we processed so far from the ballot
        const uvec4 maskedBallot = subgroupBallot(gl_SubgroupInvocationID > index ? isBestCandidate : false);
        index = subgroupBallotFindLSB(maskedBallot);
    }

    return index;
}

#endif
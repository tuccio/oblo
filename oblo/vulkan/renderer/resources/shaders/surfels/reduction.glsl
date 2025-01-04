#ifndef OBLO_INCLUDE_SURFELS_REDUCTION
#define OBLO_INCLUDE_SURFELS_REDUCTION

#include <renderer/random/random>

uint find_lowest_within_subgroup(in float value)
{
    const float lowestValue = subgroupMin(value);
    const bool isBestCandidate = lowestValue == value;
    const uvec4 bestCandidateBallot = subgroupBallot(isBestCandidate);
    return subgroupBallotFindLSB(bestCandidateBallot);
}

uint find_lowest_within_subgroup_rand(in float value, inout uint subgroupSeed)
{
    const float lowestValue = subgroupMin(value);
    const bool isBestCandidate = lowestValue == value;

    const uvec4 bestCandidateBallot = subgroupBallot(isBestCandidate);
    const uint first = subgroupBallotFindLSB(bestCandidateBallot);
    const uint last = subgroupBallotFindMSB(bestCandidateBallot);
    const uint count = subgroupBallotBitCount(bestCandidateBallot);

    // debug_assert(subgroupAllEqual(count));
    const uint choice = hash_pcg(subgroupSeed) % count;
    // const uint choice = uint(min(count, random_uniform_1d(subgroupSeed) * (count + 1)));

    uint index = first;

    for (uint relative = 0; relative < choice; ++index, ++relative)
    {
        const bool isMin = subgroupShuffle(isBestCandidate, index);
        relative += uint(isMin);
    }

    return index;

    // const uint delta = hash_pcg(subgroupSeed) % 16;

    // const bool shuffleIsBestCandidate = subgroupShuffleDown(isBestCandidate, delta);
    // const uint shuffleIndex = subgroupShuffleDown(gl_SubgroupInvocationID, delta);

    // const uvec4 shuffleBestCandidateBallot = subgroupBallot(shuffleIsBestCandidate);
    // const uint ballotIndex = subgroupBallotFindLSB(shuffleBestCandidateBallot);

    // return subgroupBroadcast(shuffleIndex, ballotIndex);
}

#endif
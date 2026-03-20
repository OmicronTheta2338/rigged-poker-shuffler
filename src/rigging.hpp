#pragma once

#include "card.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

// DP that helps fill the remainder of an interleaving with minimal additional
// “adjacent same-half picks” cost.
struct MinCostCompletionDP {
  static constexpr int MAX = 26;

  // dp[a_rem][b_rem][last] where last: 0->A, 1->B meaning previous placed card's source.
  // cost counts adjacent same-half transitions within the completed sequence.
  int dp[MAX + 1][MAX + 1][2];
  bool vis[MAX + 1][MAX + 1][2];

  MinCostCompletionDP();

  int minCostFromState(int a_rem, int b_rem, int last);

  // Returns the source sequence for the completion segment
  // where each element is 0->A, 1->B.
  std::vector<int> buildMinSources(int a_rem, int b_rem, int last);

private:
  int solve(int a_rem, int b_rem, int last);
};

// Search for an interleaving prefix (within search limits) that makes the chosen
// player have the best Hold'em hand while keeping the interleaving as close as
// possible to alternating picks.
std::optional<std::vector<int>> findBestPrefixRig(
    int winnerPlayer, int N, const std::vector<Card>& A_rev, const std::vector<Card>& B_rev,
    MinCostCompletionDP& dp, uint32_t maxExpandedNodes, bool tieAllowed);


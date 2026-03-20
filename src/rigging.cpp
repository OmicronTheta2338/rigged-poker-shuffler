#include "rigging.hpp"

#include "poker_hand.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>

MinCostCompletionDP::MinCostCompletionDP() {
  for (int a = 0; a <= MAX; a++) {
    for (int b = 0; b <= MAX; b++) {
      for (int last = 0; last < 2; last++) {
        dp[a][b][last] = 0;
        vis[a][b][last] = false;
      }
    }
  }
}

int MinCostCompletionDP::solve(int a_rem, int b_rem, int last) {
  if (a_rem < 0 || b_rem < 0) return std::numeric_limits<int>::max() / 4;
  if (a_rem == 0 && b_rem == 0) return 0;
  if (vis[a_rem][b_rem][last]) return dp[a_rem][b_rem][last];
  vis[a_rem][b_rem][last] = true;

  int best = std::numeric_limits<int>::max() / 4;

  // last==0 means previous card came from A
  if (a_rem > 0) {
    int costEdge = (last == 0) ? 1 : 0; // choosing A repeats last source
    best = std::min(best, costEdge + solve(a_rem - 1, b_rem, 0));
  }
  if (b_rem > 0) {
    int costEdge = (last == 1) ? 1 : 0; // choosing B repeats last source
    best = std::min(best, costEdge + solve(a_rem, b_rem - 1, 1));
  }

  dp[a_rem][b_rem][last] = best;
  return best;
}

int MinCostCompletionDP::minCostFromState(int a_rem, int b_rem, int last) {
  return solve(a_rem, b_rem, last);
}

std::vector<int> MinCostCompletionDP::buildMinSources(int a_rem, int b_rem, int last) {
  // Returns sources for the completion segment, where each element is 0->A, 1->B.
  std::vector<int> out;
  out.reserve(a_rem + b_rem);

  while (a_rem > 0 || b_rem > 0) {
    int bestCost = std::numeric_limits<int>::max() / 4;
    int bestChoice = -1;

    if (a_rem > 0) {
      int costEdge = (last == 0) ? 1 : 0;
      int cand = costEdge + solve(a_rem - 1, b_rem, 0);
      if (cand < bestCost) {
        bestCost = cand;
        bestChoice = 0;
      }
    }
    if (b_rem > 0) {
      int costEdge = (last == 1) ? 1 : 0;
      int cand = costEdge + solve(a_rem, b_rem - 1, 1);
      if (cand < bestCost) {
        bestCost = cand;
        bestChoice = 1;
      }
    }

    if (bestChoice == -1) break;

    // Ties: prefer choosing the opposite of `last` to keep alternation (lower "human surprise").
    if (bestChoice != 0 && bestChoice != 1) break;

    if (bestChoice == 0) {
      if (a_rem <= 0) break;
      out.push_back(0);
      a_rem--;
      last = 0;
    } else {
      if (b_rem <= 0) break;
      out.push_back(1);
      b_rem--;
      last = 1;
    }
  }
  return out;
}

struct Node {
  uint8_t pos; // number of cards placed so far (>=1)
  uint8_t a_used;
  uint8_t last; // 0->A, 1->B
  uint32_t cost;
  int parent;
  uint64_t f; // priority key
};

struct PQItem {
  uint64_t f;
  int nodeIndex;
};

struct PQCmp {
  bool operator()(const PQItem& x, const PQItem& y) const {
    return x.f > y.f; // min-heap by f
  }
};

std::optional<std::vector<int>> findBestPrefixRig(
    int winnerPlayer, int N, const std::vector<Card>& A_rev, const std::vector<Card>& B_rev,
    MinCostCompletionDP& dp, uint32_t maxExpandedNodes, bool tieAllowed) {
  const int L = 2 * N + 8; // uses hole cards + burns + 5 community cards
  if (L <= 0 || L > 52) return std::nullopt;
  assert((int)A_rev.size() == 26 && (int)B_rev.size() == 26);

  // Enforce the default first choice: A.
  // State at pos=1 means we've placed cards[0] and last source is A.
  std::vector<Node> nodes;
  nodes.reserve(1 << 20);

  auto computeF = [&](int pos, int a_used, int last, uint32_t cost_so_far) -> uint64_t {
    int b_used = pos - a_used;
    int a_rem = 26 - a_used;
    int b_rem = 26 - b_used;
    assert(a_rem >= 0 && b_rem >= 0);
    int h = dp.minCostFromState(a_rem, b_rem, last);
    return (uint64_t)cost_so_far + (uint64_t)h;
  };

  Node root{};
  root.pos = 1;
  root.a_used = 1;
  root.last = 0; // A
  root.cost = 0;
  root.parent = -1;
  root.f = computeF(1, 1, 0, 0);
  nodes.push_back(root);

  std::priority_queue<PQItem, std::vector<PQItem>, PQCmp> pq;
  pq.push({root.f, 0});

  uint32_t expanded = 0;

  while (!pq.empty()) {
    PQItem item = pq.top();
    pq.pop();

    int idx = item.nodeIndex;
    const Node& cur = nodes[idx];

    if (expanded++ > maxExpandedNodes) break;

    int i = cur.pos;
    int a_used = cur.a_used;
    int b_used = i - a_used;

    if (i == L) {
      // Reconstruct the sources for the prefix.
      std::vector<int> sources(L, -1);
      int curId = idx;
      for (int t = L - 1; t >= 0; --t) {
        sources[t] = (int)nodes[curId].last; // last source at that depth
        curId = nodes[curId].parent;
      }
      assert((int)sources.size() == L);

      // Build output prefix cards from the source pattern.
      std::vector<Card> output(L);
      int ai = 0, bi = 0;
      for (int t = 0; t < L; t++) {
        if (sources[t] == 0) {
          output[t] = A_rev[ai++];
        } else {
          output[t] = B_rev[bi++];
        }
      }

      // Extract community (5 cards) and hole cards for each player.
      std::array<Card, 5> community = {
          output[2 * N + 1],  // 2N+2
          output[2 * N + 2],  // 2N+3
          output[2 * N + 3],  // 2N+4
          output[2 * N + 5],  // 2N+6
          output[2 * N + 7]   // 2N+8
      };

      std::optional<HandValue> targetValue;
      HandValue bestOther{};
      bestOther.category = -1;
      bestOther.kickers = {0, 0, 0, 0, 0};

      for (int p = 1; p <= N; p++) {
        // Hole cards:
        // D+p : p, N+p (1-based deck positions)
        // -> 0-based array indices: hole1 at (p-1), hole2 at (N+p-1)
        const Card hole1 = output[p - 1];
        const Card hole2 = output[N + p - 1];

        std::array<Card, 7> seven = {hole1, hole2, community[0], community[1], community[2], community[3], community[4]};
        HandValue hv = eval7(seven);

        if (p == winnerPlayer) {
          targetValue = hv;
        } else {
          if (bestOther < hv) bestOther = hv;
        }
      }

      if (!targetValue.has_value()) continue;

      bool ok = false;
      if (tieAllowed) {
        ok = !(*targetValue < bestOther); // target >= bestOther
      } else {
        ok = bestOther < *targetValue; // target > bestOther
      }

      if (ok) {
        return sources; // minimal-cost because we pop by f
      }
      continue;
    }

    // Expand to pos+1
    // Option 1: pick from A (source 0)
    if (a_used < 26) {
      int nextPos = i + 1;
      int nextA = a_used + 1;
      uint32_t delta = (cur.last == 0) ? 1u : 0u; // same as last => adjacent-equal
      uint32_t nextCost = cur.cost + delta;
      int nextLast = 0;

      Node child{};
      child.pos = (uint8_t)nextPos;
      child.a_used = (uint8_t)nextA;
      child.last = (uint8_t)nextLast;
      child.cost = nextCost;
      child.parent = idx;
      child.f = computeF(nextPos, nextA, nextLast, nextCost);

      int childIdx = (int)nodes.size();
      nodes.push_back(child);
      pq.push({child.f, childIdx});
    }

    // Option 2: pick from B (source 1)
    if (b_used < 26) {
      int nextPos = i + 1;
      int nextA = a_used;
      uint32_t delta = (cur.last == 1) ? 1u : 0u;
      uint32_t nextCost = cur.cost + delta;
      int nextLast = 1;

      Node child{};
      child.pos = (uint8_t)nextPos;
      child.a_used = (uint8_t)nextA;
      child.last = (uint8_t)nextLast;
      child.cost = nextCost;
      child.parent = idx;
      child.f = computeF(nextPos, nextA, nextLast, nextCost);

      int childIdx = (int)nodes.size();
      nodes.push_back(child);
      pq.push({child.f, childIdx});
    }
  }

  return std::nullopt;
}


#include "poker_hand.hpp"

#include <algorithm>
#include <array>
#include <vector>

static bool isStraightWithHigh(const std::array<int, 5>& ranksAsc, int& straightHigh) {
  // ranksAsc must be sorted unique ascending by rank index (2..14).
  // Special case: A-2-3-4-5 ("wheel") has straight high = 5.
  if (ranksAsc[0] == 2 && ranksAsc[1] == 3 && ranksAsc[2] == 4 && ranksAsc[3] == 5 && ranksAsc[4] == 14) {
    straightHigh = 5;
    return true;
  }
  for (int i = 0; i < 4; i++) {
    if (ranksAsc[i + 1] != ranksAsc[i] + 1) return false;
  }
  straightHigh = ranksAsc[4];
  return true;
}

static HandValue eval5(const std::array<Card, 5>& cards) {
  std::array<int, 5> ranks{};
  std::array<int, 5> suits{};
  for (int i = 0; i < 5; i++) {
    ranks[i] = cards[i].rank;
    suits[i] = cards[i].suit;
  }

  bool flush = true;
  for (int i = 1; i < 5; i++) {
    if (suits[i] != suits[0]) {
      flush = false;
      break;
    }
  }

  // Count ranks (2..14 inclusive).
  std::array<int, 15> cnt{}; // indices 0..14 used
  for (int r : ranks) cnt[r]++;

  std::vector<int> uniqueRanks;
  uniqueRanks.reserve(5);
  for (int r = 2; r <= 14; r++) {
    if (cnt[r] > 0) uniqueRanks.push_back(r);
  }

  bool straight = false;
  int straightHigh = 0;
  if (uniqueRanks.size() == 5) {
    std::array<int, 5> ranksAsc{};
    std::sort(ranks.begin(), ranks.end());
    ranksAsc = ranks;
    straight = isStraightWithHigh(ranksAsc, straightHigh);
  }

  // Rank groups for duplicates; sort by (count desc, rank desc).
  std::vector<std::pair<int, int>> groups; // (count, rank)
  groups.reserve(5);
  for (int r = 2; r <= 14; r++) {
    if (cnt[r] > 0) groups.push_back({cnt[r], r});
  }
  std::sort(groups.begin(), groups.end(), [](auto a, auto b) {
    if (a.first != b.first) return a.first > b.first;
    return a.second > b.second;
  });

  HandValue hv{};
  hv.kickers = {0, 0, 0, 0, 0};

  if (straight && flush) {
    hv.category = static_cast<int>(HandCategory::StraightFlush);
    hv.kickers = {straightHigh, 0, 0, 0, 0};
    return hv;
  }

  if (groups[0].first == 4) {
    hv.category = static_cast<int>(HandCategory::Quads);
    int quadRank = groups[0].second;
    int kicker = 0;
    for (int r = 14; r >= 2; r--) {
      if (cnt[r] == 1) {
        kicker = r;
        break;
      }
    }
    hv.kickers = {quadRank, kicker, 0, 0, 0};
    return hv;
  }

  if (groups[0].first == 3 && groups[1].first == 2) {
    hv.category = static_cast<int>(HandCategory::FullHouse);
    int tripsRank = groups[0].second;
    int pairRank = groups[1].second;
    hv.kickers = {tripsRank, pairRank, 0, 0, 0};
    return hv;
  }

  if (flush) {
    hv.category = static_cast<int>(HandCategory::Flush);
    std::sort(ranks.begin(), ranks.end(), std::greater<int>());
    hv.kickers = {ranks[0], ranks[1], ranks[2], ranks[3], ranks[4]};
    return hv;
  }

  if (straight) {
    hv.category = static_cast<int>(HandCategory::Straight);
    hv.kickers = {straightHigh, 0, 0, 0, 0};
    return hv;
  }

  if (groups[0].first == 3) {
    hv.category = static_cast<int>(HandCategory::Trips);
    int tripsRank = groups[0].second;
    std::vector<int> kick;
    kick.reserve(2);
    for (auto [c, r] : groups) {
      if (c == 1) kick.push_back(r);
    }
    std::sort(kick.begin(), kick.end(), std::greater<int>());
    hv.kickers = {tripsRank, kick[0], kick[1], 0, 0};
    return hv;
  }

  if (groups[0].first == 2 && groups[1].first == 2) {
    hv.category = static_cast<int>(HandCategory::TwoPair);
    int highPair = std::max(groups[0].second, groups[1].second);
    int lowPair = std::min(groups[0].second, groups[1].second);
    int kicker = 0;
    for (int r = 14; r >= 2; r--) {
      if (cnt[r] == 1) {
        kicker = r;
        break;
      }
    }
    hv.kickers = {highPair, lowPair, kicker, 0, 0};
    return hv;
  }

  if (groups[0].first == 2) {
    hv.category = static_cast<int>(HandCategory::OnePair);
    int pairRank = groups[0].second;
    std::vector<int> kick;
    kick.reserve(3);
    for (auto [c, r] : groups) {
      if (c == 1) kick.push_back(r);
    }
    std::sort(kick.begin(), kick.end(), std::greater<int>());
    hv.kickers = {pairRank, kick[0], kick[1], kick[2], 0};
    return hv;
  }

  hv.category = static_cast<int>(HandCategory::HighCard);
  std::sort(ranks.begin(), ranks.end(), std::greater<int>());
  hv.kickers = {ranks[0], ranks[1], ranks[2], ranks[3], ranks[4]};
  return hv;
}

HandValue eval7(const std::array<Card, 7>& cards) {
  HandValue best{};
  best.category = -1;
  best.kickers = {0, 0, 0, 0, 0};

  // Brute-force all 21 possible 5-card subsets.
  for (int a = 0; a < 3; a++) {
    for (int b = a + 1; b < 4; b++) {
      for (int c = b + 1; c < 5; c++) {
        for (int d = c + 1; d < 6; d++) {
          for (int e = d + 1; e < 7; e++) {
            std::array<Card, 5> c5 = {cards[a], cards[b], cards[c], cards[d], cards[e]};
            HandValue hv = eval5(c5);
            if (best < hv) best = hv;
          }
        }
      }
    }
  }

  return best;
}

const char* handCategoryName(const HandValue& hv) {
  switch (static_cast<HandCategory>(hv.category)) {
    case HandCategory::HighCard:
      return "High";
    case HandCategory::OnePair:
      return "Pair";
    case HandCategory::TwoPair:
      return "2Pair";
    case HandCategory::Trips:
      return "Trip";
    case HandCategory::Straight:
      return "Str";
    case HandCategory::Flush:
      return "Flush";
    case HandCategory::FullHouse:
      return "Full";
    case HandCategory::Quads:
      return "Quad";
    case HandCategory::StraightFlush:
      return "StrFlush";
    default:
      return "Unknown";
  }
}


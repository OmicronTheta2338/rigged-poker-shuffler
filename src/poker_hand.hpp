#pragma once

#include "card.hpp"

#include <array>

enum class HandCategory : int {
  HighCard = 0,
  OnePair = 1,
  TwoPair = 2,
  Trips = 3,
  Straight = 4,
  Flush = 5,
  FullHouse = 6,
  Quads = 7,
  StraightFlush = 8,
};

struct HandValue {
  int category;              // higher is better
  std::array<int, 5> kickers; // lexicographic compare

  bool operator<(const HandValue& other) const {
    if (category != other.category) return category < other.category;
    return kickers < other.kickers;
  }
};

// Evaluate best 5-card poker hand out of 7 cards (Texas Hold'em).
HandValue eval7(const std::array<Card, 7>& cards);

// Human-readable category abbreviation for CLI output.
const char* handCategoryName(const HandValue& hv);


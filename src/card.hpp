#pragma once

#include <string>

struct Card {
  // rank: 2..14 (14 = Ace)
  int rank;
  int suit; // 0..3
};

std::string cardToString(const Card& c);


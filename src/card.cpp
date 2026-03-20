#include "card.hpp"

#include <string>

std::string cardToString(const Card& c) {
  static const char* SUITS = "CDHS";
  static const char* RANKS = "23456789TJQKA";
  if (c.rank < 2 || c.rank > 14 || c.suit < 0 || c.suit > 3) return "??";
  char r = RANKS[c.rank - 2];
  char s = SUITS[c.suit];
  std::string out;
  out.push_back(r);
  out.push_back(s);
  return out;
}


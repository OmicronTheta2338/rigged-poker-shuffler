#include "card.hpp"
#include "poker_hand.hpp"
#include "rigging.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

static void printUsage(const char* argv0) {
  std::cerr
      << "Usage:\n"
      << "  " << argv0 << " <winner_player> <num_players> [seed] [--verbose|--concise]\n\n"
      << "Notes:\n"
      << "  - Players are 1..N in dealing order as described in notes.txt.\n"
      << "  - If `seed` is omitted, you can pass `--verbose`/`--concise` as the 3rd arg.\n"
      << "  - This searches for an interleaving of the two 26-card halves that makes the\n"
      << "    chosen player have the best Texas Hold'em hand (tie allowed).\n";
}

static bool isDigits(const std::string& s) {
  if (s.empty()) return false;
  for (char ch : s) {
    if (ch < '0' || ch > '9') return false;
  }
  return true;
}

int main(int argc, char** argv) {
  if (argc < 3 || argc > 5) {
    printUsage(argv[0]);
    return 2;
  }

  int winnerPlayer = std::stoi(argv[1]);
  int N = std::stoi(argv[2]);
  std::optional<uint64_t> seed;

  // Default to concise output (minimal: community + player hole cards + hand + final_cost).
  bool verbose = false;

  if (argc >= 4) {
    std::string a3 = argv[3];
    if (isDigits(a3)) {
      seed = (uint64_t)std::stoull(a3);
    } else {
      if (a3 == "--verbose" || a3 == "-v") verbose = true;
      if (a3 == "--concise" || a3 == "-c") verbose = false;
    }
  }
  if (argc == 5) {
    std::string a4 = argv[4];
    if (a4 == "--verbose" || a4 == "-v") verbose = true;
    if (a4 == "--concise" || a4 == "-c") verbose = false;
    // Otherwise ignore unknown extra args to keep CLI forgiving.
  }

  if (N < 1 || N > 22) {
    std::cerr << "num_players must be in [1, 22] so that 2N+8 <= 52.\n";
    return 2;
  }
  if (winnerPlayer < 1 || winnerPlayer > N) {
    std::cerr << "winner_player must be in [1, num_players].\n";
    return 2;
  }

  uint64_t actualSeed = 0;
  std::mt19937_64 rng;
  if (seed.has_value()) {
    actualSeed = *seed;
    rng.seed(actualSeed);
  } else {
    std::random_device rd;
    actualSeed = (((uint64_t)rd()) << 32) ^ (uint64_t)rd();
    rng.seed(actualSeed);
  }

  // Generate a random 52-card deck.
  std::vector<Card> deck;
  deck.reserve(52);
  for (int suit = 0; suit < 4; suit++) {
    for (int rank = 2; rank <= 14; rank++) {
      deck.push_back(Card{rank, suit});
    }
  }
  assert(deck.size() == 52);
  std::shuffle(deck.begin(), deck.end(), rng);

  // Cut into two 26-card halves (top and bottom halves as they come from shuffle).
  std::vector<Card> A(deck.begin(), deck.begin() + 26);
  std::vector<Card> B(deck.begin() + 26, deck.end());

  // Picks take from the "bottom card" of each half; reversing makes "front" be the next pick.
  std::vector<Card> A_rev(A.rbegin(), A.rend());
  std::vector<Card> B_rev(B.rbegin(), B.rend());

  const int L = 2 * N + 8; // uses hole cards + burns + 5 community cards

  MinCostCompletionDP dp;
  const uint32_t maxExpandedNodes = 5'000'000; // safety bound
  const bool tieAllowed = true;

  auto t0 = std::chrono::steady_clock::now();
  auto sourcesPrefixOpt = findBestPrefixRig(winnerPlayer, N, A_rev, B_rev, dp, maxExpandedNodes, tieAllowed);
  auto t1 = std::chrono::steady_clock::now();
  auto searchMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

  if (!sourcesPrefixOpt.has_value()) {
    std::cerr << "No rigging pattern found within search limits.\n";
    std::cerr << "  winner_player=" << winnerPlayer << " num_players=" << N << " seed=" << actualSeed << "\n";
    return 1;
  }

  std::vector<int> sourcesPrefix = *sourcesPrefixOpt;
  assert((int)sourcesPrefix.size() == L);

  int a_used = 0;
  int b_used = 0;
  for (int s : sourcesPrefix) {
    if (s == 0) a_used++;
    else b_used++;
  }
  assert(a_used <= 26 && b_used <= 26);

  // Finish the remainder of the 52-card deal using the DP completion.
  int last = sourcesPrefix.back(); // previous pick source for completion
  int a_rem = 26 - a_used;
  int b_rem = 26 - b_used;

  std::vector<int> completionSources = dp.buildMinSources(a_rem, b_rem, last);
  if ((int)completionSources.size() != 52 - L) {
    std::cerr << "Internal error: completionSources length mismatch.\n";
    return 1;
  }

  std::vector<int> sources;
  sources.reserve(52);
  sources.insert(sources.end(), sourcesPrefix.begin(), sourcesPrefix.end());
  sources.insert(sources.end(), completionSources.begin(), completionSources.end());
  assert((int)sources.size() == 52);

  // Reconstruct the full output deck from source sequence.
  std::vector<Card> output;
  output.reserve(52);
  int ai = 0, bi = 0;
  for (int s : sources) {
    if (s == 0) output.push_back(A_rev[ai++]);
    else output.push_back(B_rev[bi++]);
  }
  assert(ai == 26 && bi == 26);

  // Compute total cost = number of adjacent equal sources.
  uint32_t totalCost = 0;
  for (int i = 1; i < 52; i++) {
    if (sources[i] == sources[i - 1]) totalCost++;
  }

  // Extract the actual community cards (Texas Hold'em).
  std::array<Card, 5> community = {
      output[2 * N + 1],  // 2N+2
      output[2 * N + 2],  // 2N+3
      output[2 * N + 3],  // 2N+4
      output[2 * N + 5],  // 2N+6
      output[2 * N + 7]   // 2N+8
  };

  // Evaluate each player's best 7-card hand.
  std::vector<HandValue> playerValues(N + 1);
  for (int p = 1; p <= N; p++) {
    const Card hole1 = output[p - 1];
    const Card hole2 = output[N + p - 1];
    std::array<Card, 7> seven = {hole1, hole2, community[0], community[1], community[2], community[3], community[4]};
    playerValues[p] = eval7(seven);
  }

  HandValue best = playerValues[1];
  for (int p = 2; p <= N; p++) best = (best < playerValues[p] ? playerValues[p] : best);
  bool targetOk = !(playerValues[winnerPlayer] < best);

  if (verbose) {
    std::cout << "rigged-poker-shuffler\n";
    std::cout << "seed=" << actualSeed << "\n";
    std::cout << "winner_player=" << winnerPlayer << " num_players=" << N << "\n";
    std::cout << "L(cards used for deal)=" << L << "\n";
    std::cout << "minimize_cost=adjacent_same_picks\n";
    std::cout << "final_cost=" << totalCost << "\n";
    std::cout << "target_has_best_hand=" << (targetOk ? "true" : "false") << "\n";
    std::cout << "search_time_ms=" << searchMs << "\n\n";
  }

  std::cout << "Community: ";
  for (int i = 0; i < 5; i++) {
    if (i) std::cout << ' ';
    std::cout << cardToString(community[i]);
  }
  std::cout << "\n";

  for (int p = 1; p <= N; p++) {
    const Card hole1 = output[p - 1];
    const Card hole2 = output[N + p - 1];
    const HandValue& hv = playerValues[p];
    if (!verbose) {
      std::cout << "Player " << p << (p == winnerPlayer ? " (WINNER)" : "") << ": "
                << cardToString(hole1) << ' ' << cardToString(hole2)
                << "  hand=" << handCategoryName(hv) << "\n";
    } else {
      std::cout << "Player " << p << (p == winnerPlayer ? " (WINNER)" : "") << ": "
                << cardToString(hole1) << ' ' << cardToString(hole2)
                << "  hand=" << handCategoryName(hv) << "\n";
    }
  }

  if (verbose) {
    std::cout << "\nHalf decks and pick sequence\n";

    std::cout << "A half (top->bottom order as in the shuffled cut): ";
    for (int i = 0; i < 26; i++) {
      std::cout << cardToString(A[i]) << (i + 1 == 26 ? '\n' : ' ');
    }
    std::cout << "B half (top->bottom order as in the shuffled cut): ";
    for (int i = 0; i < 26; i++) {
      std::cout << cardToString(B[i]) << (i + 1 == 26 ? '\n' : ' ');
    }

    std::cout << "Pick order A_rev (next picked card is at index 0): ";
    for (int i = 0; i < 26; i++) {
      std::cout << cardToString(A_rev[i]) << (i + 1 == 26 ? '\n' : ' ');
    }
    std::cout << "Pick order B_rev (next picked card is at index 0): ";
    for (int i = 0; i < 26; i++) {
      std::cout << cardToString(B_rev[i]) << (i + 1 == 26 ? '\n' : ' ');
    }

    std::cout << "Sources (52 picks): ";
    for (int i = 0; i < 52; i++) {
      std::cout << (sources[i] == 0 ? 'A' : 'B') << (i + 1 == 52 ? '\n' : ' ');
    }

    std::cout << "Output deck order (52):\n";
    for (int i = 0; i < 52; i++) {
      std::cout << cardToString(output[i]) << (i + 1 == 52 ? '\n' : ' ');
    }
  } else {
    std::cout << "\nfinal_cost=" << totalCost << "\n";
  }

  return 0;
}


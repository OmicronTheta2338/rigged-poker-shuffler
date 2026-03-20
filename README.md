# rigged-poker-shuffler

CLI C++ program that generates a random 52-card deck, cuts it into two 26-card halves,
and builds an output deck by repeatedly taking the bottom card from either half.

It searches for an interleaving that makes a chosen player have the best Texas Hold'em
hand of the table, while minimizing how often it takes consecutive cards from the same
half (i.e. “how little you stray” from perfect A-B-A-B alternation).

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

If you don't have `cmake`, you can also compile directly:

```bash
g++ -std=c++20 -O3 -Wall -Wextra -pedantic \
  src/main.cpp src/card.cpp src/poker_hand.cpp src/rigging.cpp \
  -o rigged-poker-shuffler
```

## Run

```bash
./rigged-poker-shuffler <winner_player> <num_players> [seed]
```

Example:
```bash
./rigged-poker-shuffler 1 4 12345
```

`winner_player` and `num_players` are 1-based / in `[1..N]` and follow the dealing order
implied by `notes.txt`.

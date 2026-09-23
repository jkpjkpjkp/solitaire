#include <array>
#include <cstdint>
#include <iostream>
#include <random>

import solitaire;
import solitaire_strategy;

int main() {
    constexpr int games = 1000;
    constexpr int limit = 10000;
    std::mt19937_64 seeds(0x5eedd03d12346748ULL);
    int wins = 0;
    int stalled = 0;
    std::uint64_t first_failure = 0;
    std::uint64_t first_cap = 0;
    std::array<int, 4> first_failure_towers{};
    std::size_t first_failure_stock = 0;
    std::size_t first_failure_board = 0;
    long long total_steps = 0;
    int max_steps = 0;

    for (int game_number = 0; game_number < games; ++game_number) {
        const std::uint64_t seed = seeds();
        Solitaire game(seed);
        if(greedy_solve(game)) {
            ++wins;
        }
    }

    std::cout << "games=" << games
              << " wins=" << wins  
              << std::endl;
}

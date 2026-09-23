#include <array>
#include <cstdint>
#include <iostream>
#include <random>

import solitaire;
import solitaire_strategy;

int main() {
    constexpr int games = 1000;
    constexpr int limit = 10000;
    std::mt19937_64 seeds(0x5eedd00d12345678ULL);
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
        Solitaire game;
        game.init(seed);

        int steps = 0;
        for (; steps < limit && !game.complete(); ++steps) {
            if (!greedy_step(game)) {
                ++stalled;
                break;
            }
        }
        total_steps += steps;
        if (steps > max_steps) {
            max_steps = steps;
        }
        if (game.complete()) {
            ++wins;
        } else if (first_failure == 0) {
            first_failure = seed;
            first_failure_towers = game.towers();
            first_failure_stock = game.deck().cards().size();
            for (const auto& column : game.board()) {
                first_failure_board += column.size();
            }
        }
        if (!game.complete() && steps == limit && first_cap == 0) {
            first_cap = seed;
        }
    }

    std::cout << "games=" << games
              << " wins=" << wins
              << " losses=" << games - wins
              << " stalled=" << stalled
              << " average_steps=" << (total_steps / static_cast<double>(games))
              << " max_steps=" << max_steps
              << " first_failure_seed=" << first_failure
              << " first_failure_towers=" << first_failure_towers[0] << ','
              << first_failure_towers[1] << ',' << first_failure_towers[2] << ','
              << first_failure_towers[3]
              << " first_failure_stock=" << first_failure_stock
              << " first_failure_board=" << first_failure_board
              << " first_cap_seed=" << first_cap << '\n';
}

#include <charconv>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string_view>
#include <system_error>
#include <vector>

import solitaire;
import solitaire_strategy;

int main(int argc, char* argv[]) {
    const auto usage = [](std::ostream& out) {
        out << "Usage: random_check [--deal N] [--visualize-failures N] [--help]\n"
            << "  --deal N                Draw N cards (positive integer; default: compare 1 and 3).\n"
            << "  --visualize-failures N   Show ASCII boards for at most N failed games (default: 0).\n"
            << "Runs 1,000 games per deal number with reproducible seeds.\n"
            << "Example: make run ARGS=\"--deal 1 --visualize-failures 3\"\n";
    };
    std::vector<int> deals{1, 3};
    int failure_limit = 0;
    for (int arg = 1; arg < argc; ++arg) {
        const std::string_view option(argv[arg]);
        if (option == "--help" || option == "-h") {
            usage(std::cout);
            return 0;
        }
        if (option != "--deal" && option != "--visualize-failures") {
            std::cerr << "Unknown option: " << option << '\n';
            usage(std::cerr);
            return 1;
        }
        if (++arg == argc) {
            std::cerr << "Missing value for " << option << '\n';
            usage(std::cerr);
            return 1;
        }
        const std::string_view value(argv[arg]);
        int number = 0;
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), number);
        if (error != std::errc{} || end != value.data() + value.size()
            || number < (option == "--deal" ? 1 : 0)) {
            std::cerr << "Invalid value for " << option << ": " << value << '\n';
            usage(std::cerr);
            return 1;
        }
        if (option == "--deal") {
            deals = {number};
        } else {
            failure_limit = number;
        }
    }

    constexpr int games = 1000;
    constexpr int limit = 10000;
    std::mt19937_64 seeds(0x5eedd03d12346748ULL);
    std::vector<int> wins(deals.size());
    int failures_shown = 0;

    for (int game_number = 0; game_number < games; ++game_number) {
        const std::uint64_t seed = seeds();
        for (std::size_t mode = 0; mode < deals.size(); ++mode) {
            Solitaire game(seed, deals[mode]);
            if (greedy_solve(game, limit, false)) {
                ++wins[mode];
            } else if (failures_shown < failure_limit) {
                ++failures_shown;
                std::cout << "\nFailed game=" << game_number + 1
                          << " deal=" << deals[mode] << " seed=" << seed << '\n';
                game.visualize();
                std::cout << '\n';
            }
        }
    }

    const auto report = [&](int deal_number, int wins) {
        std::cout << "deal=" << deal_number << " games=" << games
                  << " wins=" << wins
                  << " success_rate=" << std::fixed << std::setprecision(2)
                  << 100.0 * wins / games << "%\n";
    };
    for (std::size_t mode = 0; mode < deals.size(); ++mode) {
        report(deals[mode], wins[mode]);
    }
}

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

import solitaire;
import solitaire_strategy;
import solitaire_uct;

int main(int argc, char* argv[]) {
    const auto usage = [](std::ostream& out) {
        out << "Usage: random_check [options]\n"
            << "  --strategy greedy|uct    Solver (default: greedy).\n"
            << "  --deal N                Draw N cards (positive integer; default: compare 1 and 3).\n"
            << "  --games N               Games per deal number (default: 1000).\n"
            << "  --max-parallel N        Maximum concurrent games (default: available CPUs).\n"
            << "  --visualize-failures N   Show ASCII boards for at most N failed games (default: 0).\n"
            << "  --trajectories N        UCT rollouts per tree and decision (default: 100).\n"
            << "  --rollout-limit N       Maximum moves per rollout (default: 1000).\n"
            << "  --sampling-width N      Sparse UCT width; 0 is unlimited (default: 0).\n"
            << "  --trees N               UCT ensemble size (default: 1).\n"
            << "  --seed N                Seed for the reproducible sequence of deals.\n"
            << "Example: make run ARGS=\"--strategy uct --deal 3 --games 10\"\n";
    };
    std::vector<int> deals{1, 3};
    int failure_limit = 0;
    int games = 1000;
    unsigned max_parallel = std::max(1u, std::thread::hardware_concurrency());
    bool use_uct = false;
    UctOptions uct;
    std::uint64_t deal_seed = 0x5eedd03d12346748ULL;
    for (int arg = 1; arg < argc; ++arg) {
        const std::string_view option(argv[arg]);
        if (option == "--help" || option == "-h") {
            usage(std::cout);
            return 0;
        }
        if (option != "--deal" && option != "--visualize-failures"
            && option != "--strategy" && option != "--games" && option != "--seed"
            && option != "--trajectories" && option != "--rollout-limit"
            && option != "--sampling-width" && option != "--trees"
            && option != "--max-parallel") {
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
        if (option == "--strategy") {
            if (value != "greedy" && value != "uct") {
                std::cerr << "Invalid strategy: " << value << '\n';
                return 1;
            }
            use_uct = value == "uct";
            continue;
        }
        if (option == "--seed") {
            const auto [end, error] = std::from_chars(
                value.data(), value.data() + value.size(), deal_seed);
            if (error != std::errc{} || end != value.data() + value.size()) {
                std::cerr << "Invalid seed: " << value << '\n';
                return 1;
            }
            continue;
        }
        int number = 0;
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), number);
        const int minimum = option == "--visualize-failures" || option == "--sampling-width" ? 0 : 1;
        if (error != std::errc{} || end != value.data() + value.size()
            || number < minimum) {
            std::cerr << "Invalid value for " << option << ": " << value << '\n';
            usage(std::cerr);
            return 1;
        }
        if (option == "--deal") {
            deals = {number};
        } else if (option == "--visualize-failures") {
            failure_limit = number;
        } else if (option == "--games") {
            games = number;
        } else if (option == "--max-parallel") {
            max_parallel = number;
        } else if (option == "--trajectories") {
            uct.trajectories = number;
        } else if (option == "--rollout-limit") {
            uct.rollout_limit = number;
        } else if (option == "--sampling-width") {
            uct.sampling_width = number;
        } else if (option == "--trees") {
            uct.trees = number;
        }
    }

    constexpr int limit = 10000;
    std::mt19937_64 seeds(deal_seed);
    std::vector<std::uint64_t> game_seeds(games);
    for (auto& seed : game_seeds) seed = seeds();
    std::vector<int> wins(deals.size());
    std::map<std::pair<int, std::size_t>, std::string> failures;
    std::mutex results_mutex;
    std::atomic<std::size_t> next_game{0};

    const auto worker = [&] {
        auto options = uct;
        while (true) {
            const auto game_number = next_game.fetch_add(1, std::memory_order_relaxed);
            if (game_number >= game_seeds.size()) return;
            const auto seed = game_seeds[game_number];
            for (std::size_t mode = 0; mode < deals.size(); ++mode) {
                UctSolitaire game(seed, deals[mode]);
                options.seed = seed ^ 0x756374ULL;
                const bool won = use_uct ? uct_solve(game, options, limit, false)
                                         : greedy_solve(game, limit, false);
                std::lock_guard lock(results_mutex);
                if (won) {
                    ++wins[mode];
                } else if (failure_limit > 0) {
                    const auto key = std::pair{static_cast<int>(game_number), mode};
                    // Retain only the earliest failures, regardless of completion order.
                    if (failures.size() < static_cast<std::size_t>(failure_limit)
                        || key < failures.rbegin()->first) {
                        std::ostringstream out;
                        out << "\nFailed game=" << game_number + 1
                            << " deal=" << deals[mode] << " seed=" << seed << '\n';
                        game.visualize(out);
                        out << '\n';
                        failures.emplace(key, out.str());
                        if (failures.size() > static_cast<std::size_t>(failure_limit))
                            failures.erase(std::prev(failures.end()));
                    }
                }
            }
        }
    };
    {
        std::vector<std::jthread> workers;
        const auto count = std::min(max_parallel, static_cast<unsigned>(games));
        workers.reserve(count);
        for (unsigned i = 0; i < count; ++i) workers.emplace_back(worker);
    }
    for (const auto& [key, board] : failures) std::cout << board;

    const auto report = [&](int deal_number, int wins) {
        std::cout << "deal=" << deal_number << " games=" << games
                  << " wins=" << wins
                  << " success_rate=" << std::fixed << std::setprecision(2)
                  << 100.0 * wins / games << '%';
        if (use_uct) {
            std::cout << " strategy=uct trajectories=" << uct.trajectories
                      << " trees=" << uct.trees << " sampling_width=" << uct.sampling_width;
        }
        std::cout << '\n';
    };
    for (std::size_t mode = 0; mode < deals.size(); ++mode) {
        report(deals[mode], wins[mode]);
    }
}

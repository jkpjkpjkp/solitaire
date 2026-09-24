module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

export module solitaire_uct;

import solitaire;

// UCT-specific rules and observation/sampling helpers live outside the base game.
export class UctSolitaire : public Solitaire {
public:
    using Solitaire::Solitaire;
    explicit UctSolitaire(const Solitaire& game) : Solitaire(game) {}

    struct Move {
        Card from;
        int to;
        bool operator==(const Move&) const = default;
    };

    bool fully_revealed() const noexcept {
        return std::all_of(board_.begin(), board_.end(),
                           [](const Column& column) { return column.hidden() == 0; });
    }

    std::vector<Move> legal_moves() const;
    // Hidden positions are excluded; search can also ignore column labels.
    std::string state_key(bool interchangeable_columns = false) const;
    UctSolitaire sample_action(Move move, std::mt19937_64& generator) const;

    void action(Card card, int destination) {
        if (card.number() < 1 || card.number() > 13 || card.suit() < 0 || card.suit() > 3) {
            throw std::invalid_argument("invalid card");
        }
        if (destination >= 0 && destination < 7
            && towers_[card.suit()] == card.number()) {
            if (!fits(board_[destination], card)) {
                throw std::invalid_argument("invalid tableau move");
            }
            board_[destination].push_back(card);
            --towers_[card.suit()];
            return;
        }
        std::optional<Card> foundation;
        if (destination >= 0 && destination < 7) {
            int kind = 0, source = 0, row = 0;
            if (locate(card, kind, source, row) && kind == 0
                && static_cast<std::size_t>(row) > board_[source].hidden()) {
                const Card next = board_[source][row - 1];
                if (towers_[next.suit()] != next.number() - 1) {
                    throw std::invalid_argument("partial move must uncover a foundation move");
                }
                foundation = next;
            }
        }
        Solitaire::action(card, destination);
        // A partial tableau transfer and the newly exposed card's foundation
        // transfer form one UCT action, including any resulting hidden reveal.
        if (foundation) {
            Solitaire::action(*foundation, 10 + foundation->suit());
        }
    }
};

std::vector<UctSolitaire::Move> UctSolitaire::legal_moves() const {
    std::vector<Move> moves;
    const auto to_tableau = [&](Card card, int source) {
        for (int destination = 0; destination < 7; ++destination) {
            if (destination != source && fits(board_[destination], card)) {
                moves.push_back({card, destination});
            }
        }
    };
    const auto to_foundation = [&](Card card) {
        if (towers_[card.suit()] == card.number() - 1) {
            moves.push_back({card, 10 + card.suit()});
        }
    };
    for (int source = 0; source < 7; ++source) {
        const Column& column = board_[source];
        for (std::size_t row = column.hidden(); row < column.size(); ++row) {
            if (column.valid_run(row)
                && (row == column.hidden()
                    || towers_[column[row - 1].suit()] == column[row - 1].number() - 1)) {
                to_tableau(column[row], source);
            }
            if (row + 1 == column.size()) {
                to_foundation(column[row]);
            }
        }
    }
    for (std::size_t position = 0; position < deck_.cards().size(); ++position) {
        if (deck_.usable(position)) {
            to_tableau(deck_.cards()[position], -1);
            to_foundation(deck_.cards()[position]);
        }
    }
    for (int suit = 0; suit < 4; ++suit) {
        if (towers_[suit] > 0) {
            to_tableau(Card{towers_[suit], suit}, -1);
        }
    }
    return moves;
}

std::string UctSolitaire::state_key(bool interchangeable_columns) const {
    std::string key = std::to_string(deal_number_) + ':';
    const auto append = [&](std::size_t value) { key.push_back(static_cast<char>(value)); };
    const auto card = [&](Card value) { append(value.suit() * 13 + value.number()); };
    for (int count : towers_) {
        append(count);
    }
    append(deck_.cursor() == SIZE_MAX ? 0 : deck_.cursor() + 1);
    append(deck_.cards().size());
    for (Card value : deck_.cards()) {
        card(value);
    }
    std::array<std::string, 7> columns;
    for (std::size_t index = 0; index < board_.size(); ++index) {
        const Column& column = board_[index];
        std::string& encoded = columns[index];
        encoded.push_back(static_cast<char>(column.hidden()));
        encoded.push_back(static_cast<char>(column.size()));
        for (std::size_t row = column.hidden(); row < column.size(); ++row) {
            encoded.push_back(static_cast<char>(column[row].suit() * 13 + column[row].number()));
        }
    }
    if (interchangeable_columns) {
        std::sort(columns.begin(), columns.end());
    }
    for (const auto& column : columns) {
        key += column;
    }
    return key;
}

UctSolitaire UctSolitaire::sample_action(Move move, std::mt19937_64& generator) const {
    UctSolitaire next = *this;
    next.action(move.from, move.to);
    for (int source = 0; source < 7; ++source) {
        if (next.board_[source].hidden() == board_[source].hidden()) {
            continue;
        }
        // Only the unseen set matters, never its actual arrangement. Sorting
        // makes sampling independent of that arrangement even for a fixed seed.
        std::vector<Card> unseen;
        for (const Column& column : board_) {
            const auto cards = column.all();
            unseen.insert(unseen.end(), cards.begin(), cards.begin() + column.hidden());
        }
        std::sort(unseen.begin(), unseen.end(), [](Card a, Card b) {
            return a.suit() * 13 + a.number() < b.suit() * 13 + b.number();
        });
        const Card sampled = unseen[std::uniform_int_distribution<std::size_t>(
            0, unseen.size() - 1)(generator)];
        auto revealed_column = next.board_[source].all();
        Card& revealed = revealed_column[next.board_[source].hidden()];
        if (revealed == sampled) {
            return next;
        }
        for (int index = 0; index < 7; ++index) {
            auto cards = next.board_[index].all();
            for (std::size_t row = 0; row < next.board_[index].hidden(); ++row) {
                if (cards[row] != sampled) {
                    continue;
                }
                if (index == source) {
                    std::swap(revealed, revealed_column[row]);
                } else {
                    std::swap(revealed, cards[row]);
                    next.board_[index] = Column(std::move(cards), next.board_[index].hidden());
                }
                next.board_[source] = Column(
                    std::move(revealed_column), next.board_[source].hidden());
                return next;
            }
        }
    }
    return next;
}

export struct UctOptions {
    int trajectories = 100;
    int rollout_limit = 1000;
    double exploration = 1.0;
    // 0: ordinary UCT; w > 0: Sparse UCT; w = 1: a HOP-UCT tree.
    int sampling_width = 0;
    int trees = 1;
    std::uint64_t seed = 0;
    // Optional diagnostics for root estimates and executed moves.
    std::ostream* trace = nullptr;
};

export struct UctActionValue {
    UctSolitaire::Move move;
    std::uint64_t visits = 0;
    double value = 0.0;
    // Sum of distinct immediate outcomes observed across the ensemble.
    std::size_t outcomes = 0;
};

export struct UctResult {
    std::optional<UctSolitaire::Move> move;
    std::vector<UctActionValue> actions;
};

namespace {

using Seen = std::unordered_set<std::string>;

void print_move(std::ostream& out, UctSolitaire::Move move) {
    constexpr const char* ranks[] = {
        "", "A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"
    };
    constexpr char suits[] = "CDSH";
    out << ranks[move.from.number()] << suits[move.from.suit()] << " -> ";
    if (move.to >= 10) {
        out << "foundation " << suits[move.to - 10];
    } else {
        out << "column " << move.to;
    }
}

void execute(UctSolitaire& game, UctSolitaire::Move move,
             const UctOptions& options, bool greedy = false) {
    game.action(move.from, move.to);
    if (options.trace) {
        *options.trace << (greedy ? "Greedy finish: " : "Play: ");
        print_move(*options.trace, move);
        *options.trace << '\n';
        game.visualize(*options.trace);
    }
}

std::string observation(const UctSolitaire& game) {
    // Renaming tableau columns cannot change the value of a belief state.
    return game.state_key(true);
}

void validate(const UctOptions& options) {
    if (options.trajectories <= 0 || options.rollout_limit <= 0
        || options.sampling_width < 0 || options.trees <= 0
        || !std::isfinite(options.exploration) || options.exploration < 0) {
        throw std::invalid_argument("invalid UCT options");
    }
}

bool reveals_card(const UctSolitaire& game, UctSolitaire::Move move) {
    for (const Column& column : game.board()) {
        if (column.hidden() > 0 && column.hidden() < column.size()) {
            if (column[column.hidden()] == move.from) {
                return true;
            }
            if (move.to >= 0 && move.to < 7
                && column.hidden() + 1 < column.size()
                && column[column.hidden() + 1] == move.from) {
                // Legal partial moves immediately send the preceding card to
                // foundation, revealing a hidden card when it is the first.
                return true;
            }
        }
    }
    return false;
}

int priority(const UctSolitaire& game, UctSolitaire::Move move) {
    const bool reveals = reveals_card(game, move);
    if (move.to >= 10) {
        return reveals ? 0 : 1;
    }
    if (reveals) {
        return 2;
    }
    std::size_t position = 0;
    if (game.deck().contains(move.from, position)) {
        return 3;
    }
    if (game.towers()[move.from.suit()] == move.from.number()) {
        return 4;
    }
    return 5;
}

std::vector<UctSolitaire::Move> ordered_moves(const UctSolitaire& game) {
    auto moves = game.legal_moves();
    std::stable_sort(moves.begin(), moves.end(), [&](auto a, auto b) {
        return priority(game, a) < priority(game, b);
    });
    return moves;
}

// The paper's six-level greedy policy, used only once every card is visible.
// Work on a copy: a failed attempt must not commit any moves to the real game.
std::optional<std::vector<UctSolitaire::Move>> greedy_finish(
    UctSolitaire game, Seen seen, int limit) {
    std::vector<UctSolitaire::Move> path;
    seen.insert(observation(game));
    for (int step = 0; step < limit && !game.complete(); ++step) {
        bool moved = false;
        for (const auto move : ordered_moves(game)) {
            UctSolitaire next = game;
            next.action(move.from, move.to);
            if (!seen.insert(observation(next)).second) {
                continue;
            }
            game = std::move(next);
            path.push_back(move);
            moved = true;
            break;
        }
        if (!moved) {
            return std::nullopt;
        }
    }
    return game.complete() ? std::optional(std::move(path)) : std::nullopt;
}

struct Edge {
    UctSolitaire::Move move;
    bool reveals;
    std::string next_key;
    std::uint64_t visits = 0;
    std::uint64_t wins = 0;
    std::vector<std::size_t> children;
    // Samples retain multiplicity: duplicate outcomes count toward width w.
    std::vector<std::size_t> samples;
};

struct Node {
    UctSolitaire game;
    std::string key;
    std::uint64_t visits = 0;
    std::vector<Edge> edges;
    std::optional<bool> greedy_win;

    explicit Node(UctSolitaire state) : game(std::move(state)), key(observation(game)) {
        if (game.complete()) {
            return;
        }
        for (auto move : ordered_moves(game)) {
            const bool reveals = reveals_card(game, move);
            std::string next_key;
            if (!reveals) {
                UctSolitaire next = game;
                next.action(move.from, move.to);
                next_key = observation(next);
            }
            edges.push_back(Edge{move, reveals, std::move(next_key), 0, 0, {}, {}});
        }
    }
};

std::size_t random_index(std::size_t size, std::mt19937_64& generator) {
    return std::uniform_int_distribution<std::size_t>(0, size - 1)(generator);
}

class Tree {
    const UctOptions& options_;
    std::mt19937_64& generator_;
    // An arena avoids recursive destruction of potentially deep trajectories.
    std::vector<std::unique_ptr<Node>> nodes_;

    std::optional<std::size_t> select(const Node& node, const Seen& seen) {
        std::vector<std::size_t> unvisited;
        std::optional<std::size_t> best;
        double best_score = -std::numeric_limits<double>::infinity();
        for (std::size_t index = 0; index < node.edges.size(); ++index) {
            const Edge& edge = node.edges[index];
            // A reveal strictly decreases the hidden count and cannot repeat a
            // previous observation. Other moves are checked before selection.
            if (!edge.reveals && seen.contains(edge.next_key)) {
                continue;
            }
            if (edge.visits == 0) {
                unvisited.push_back(index);
                continue;
            }
            const double score = static_cast<double>(edge.wins) / edge.visits
                + options_.exploration * std::sqrt(
                    std::log(static_cast<double>(node.visits)) / edge.visits);
            if (score > best_score) {
                best_score = score;
                best = index;
            }
        }
        if (!unvisited.empty()) {
            return unvisited[random_index(unvisited.size(), generator_)];
        }
        return best;
    }

    std::size_t transition(Node& node, Edge& edge) {
        if (!edge.reveals && !edge.children.empty()) {
            return edge.children.front();
        }
        if (options_.sampling_width > 0
            && edge.samples.size() == static_cast<std::size_t>(options_.sampling_width)) {
            return edge.samples[random_index(edge.samples.size(), generator_)];
        }

        UctSolitaire next = node.game.sample_action(edge.move, generator_);
        const std::string key = observation(next);
        std::size_t child = nodes_.size();
        for (std::size_t existing : edge.children) {
            if (nodes_[existing]->key == key) {
                child = existing;
                break;
            }
        }
        if (child == nodes_.size()) {
            nodes_.push_back(std::make_unique<Node>(std::move(next)));
            edge.children.push_back(child);
        }
        if (options_.sampling_width > 0) {
            edge.samples.push_back(child);
        }
        return child;
    }

public:
    Tree(const UctSolitaire& game, const UctOptions& options, std::mt19937_64& generator)
        : options_(options), generator_(generator) {
        nodes_.push_back(std::make_unique<Node>(game));
    }

    const Node& root() const { return *nodes_.front(); }

    void rollout(const Seen& history) {
        Seen seen = history;
        std::vector<std::pair<Node*, std::size_t>> path;
        Node* node = nodes_.front().get();
        bool won = false;
        for (int depth = 0; ; ++depth) {
            if (node->game.complete()) {
                won = true;
                break;
            }
            if (depth == options_.rollout_limit) {
                break;
            }
            seen.insert(node->key);
            if (depth > 0 && node->game.fully_revealed()) {
                if (!node->greedy_win) {
                    node->greedy_win = greedy_finish(
                        node->game, seen, options_.rollout_limit - depth).has_value();
                }
                if (*node->greedy_win) {
                    won = true;
                    break;
                }
            }
            const auto selected = select(*node, seen);
            if (!selected) {
                break;
            }
            path.emplace_back(node, *selected);
            const std::size_t child = transition(*node, node->edges[*selected]);
            node = nodes_[child].get();
        }
        ++node->visits;
        for (auto [parent, index] : path) {
            ++parent->visits;
            Edge& edge = parent->edges[index];
            ++edge.visits;
            edge.wins += won ? 1 : 0;
        }
    }
};

UctResult search(const UctSolitaire& game, const UctOptions& options,
                 std::mt19937_64& generator, const Seen& history) {
    UctResult result;
    if (game.complete()) {
        return result;
    }
    for (int number = 0; number < options.trees; ++number) {
        Tree tree(game, options, generator);
        if (number == 0) {
            for (const Edge& edge : tree.root().edges) {
                result.actions.push_back({edge.move});
            }
        }
        for (int trajectory = 0; trajectory < options.trajectories; ++trajectory) {
            tree.rollout(history);
        }
        for (std::size_t index = 0; index < result.actions.size(); ++index) {
            const Edge& edge = tree.root().edges[index];
            auto& value = result.actions[index];
            value.visits += edge.visits;
            value.outcomes += edge.children.size();
            if (edge.visits > 0) {
                value.value += static_cast<double>(edge.wins) / edge.visits / options.trees;
            }
        }
    }
    double best_value = -1.0;
    // Equal estimates use the paper's default action preference. Never select
    // an unvisited (possibly cycle-disqualified) root action.
    for (const auto& value : result.actions) {
        if (value.visits > 0 && value.value > best_value) {
            best_value = value.value;
            result.move = value.move;
        }
    }
    if (options.trace) {
        *options.trace << "UCT search (move limit " << options.rollout_limit << "):\n";
        for (const auto& action : result.actions) {
            *options.trace << "  ";
            print_move(*options.trace, action.move);
            *options.trace << " visits=" << action.visits << " value=" << action.value
                           << " outcomes=" << action.outcomes;
            if (result.move == action.move) {
                *options.trace << " selected";
            }
            *options.trace << '\n';
        }
    }
    return result;
}

} // namespace

// Search does not mutate the supplied game. Reusing a seed is reproducible.
export UctResult uct_search(const UctSolitaire& game, const UctOptions& options = {}) {
    validate(options);
    std::mt19937_64 generator(options.seed);
    return search(game, options, generator, {});
}

export bool uct_step(UctSolitaire& game, const UctOptions& options = {}) {
    const auto result = uct_search(game, options);
    if (!result.move) {
        return false;
    }
    execute(game, *result.move, options);
    return true;
}

export bool uct_solve(UctSolitaire& game, const UctOptions& options = {},
                      int limit = 10000, bool verbose = true) {
    validate(options);
    if (limit < 0) {
        return false;
    }
    std::mt19937_64 generator(options.seed);
    Seen history;
    for (int step = 0; step < limit && !game.complete(); ++step) {
        // Simulated wins must fit within the moves still available to the
        // actual solver, otherwise a detour can outrank an immediate finish.
        UctOptions remaining = options;
        remaining.rollout_limit = std::min(options.rollout_limit, limit - step);
        history.insert(observation(game));
        if (game.fully_revealed()) {
            if (const auto finish = greedy_finish(
                    game, history, remaining.rollout_limit)) {
                for (auto move : *finish) {
                    execute(game, move, options, true);
                }
                return true;
            }
        }
        const auto result = search(game, remaining, generator, history);
        if (!result.move) {
            if (verbose) {
                std::cout << "UCT solver stuck (no non-repeating move possible).\n";
                game.visualize();
            }
            return false;
        }
        // Execute against the real deal; only simulations sample hidden cards.
        execute(game, *result.move, options);
    }
    return game.complete();
}

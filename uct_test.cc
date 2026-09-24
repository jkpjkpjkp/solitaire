#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string_view>
#include <vector>

import solitaire;
import solitaire_uct;

namespace {

template<class Function>
void rejects(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

UctSolitaire last_king() {
    std::vector<Card> cards;
    for (int suit = 0; suit < 4; ++suit) {
        for (int rank = 1; rank <= 13; ++rank) {
            cards.emplace_back(rank, suit);
        }
    }
    UctSolitaire game(std::array<Column, 7>{}, cards, 1);
    for (Card card : cards) {
        if (card != Card{13, 3}) {
            game.action(card, 10 + card.suit());
        }
    }
    return game;
}

UctSolitaire two_hidden(bool swapped = false) {
    std::array<Column, 7> board{};
    board[0] = Column({swapped ? Card{8, 1} : Card{9, 0}, Card{1, 0}}, 1);
    board[1] = Column({swapped ? Card{9, 0} : Card{8, 1}, Card{7, 0}}, 1);
    return UctSolitaire(board);
}

UctSolitaire six_cards_left() {
    std::array<Column, 7> board{};
    board[0] = Column({Card{13, 3}, Card{12, 0}}, 1);
    board[1] = Column({Card{13, 1}, Card{12, 2}}, 1);
    std::vector<Card> stock;
    for (int suit = 0; suit < 4; ++suit) {
        for (int rank = 1; rank <= 13; ++rank) {
            if (rank != (suit % 2 == 0 ? 12 : 13)) {
                stock.emplace_back(rank, suit);
            }
        }
    }
    UctSolitaire game(board, stock, 1);
    for (Card card : stock) {
        if (card.number() <= (card.color() == 0 ? 11 : 12)) {
            game.action(card, 10 + card.suit());
        }
    }
    return game;
}

void check_moves(const UctSolitaire& game) {
    const auto moves = game.legal_moves();
    for (std::size_t index = 0; index < moves.size(); ++index) {
        assert(std::find(moves.begin(), moves.begin() + index, moves[index]) == moves.begin() + index);
        UctSolitaire next = game;
        next.action(moves[index].from, moves[index].to);
        assert(next.state_key() != game.state_key());
    }
    // Independently enumerate the move API, including illegal cards/destinations.
    // The generator must neither miss legal actions nor advertise illegal ones.
    for (int suit = 0; suit < 4; ++suit) {
        for (int rank = 1; rank <= 13; ++rank) {
            for (int destination : {0, 1, 2, 3, 4, 5, 6, 10, 11, 12, 13}) {
                const UctSolitaire::Move move{Card{rank, suit}, destination};
                bool legal = true;
                try {
                    UctSolitaire next = game;
                    next.action(move.from, move.to);
                } catch (const std::invalid_argument&) {
                    legal = false;
                }
                assert(legal == (std::find(moves.begin(), moves.end(), move) != moves.end()));
            }
        }
    }
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--trace") {
        auto game = six_cards_left();
        game.visualize();
        UctOptions options;
        options.seed = 2;
        options.rollout_limit = 100;
        options.trace = &std::cout;
        const bool won = uct_solve(game, options, 6, true);
        std::cout << "Six-move finish: " << (won ? "won" : "failed") << '\n';
        return won ? 0 : 1;
    }
    {
        auto game = last_king();
        const auto before = game.state_key();
        Solitaire base = game;
        rejects([&] { base.action(Card{13, 0}, 0); });
        assert(UctSolitaire(base).state_key() == before);
        rejects([&] { game.action(Card{12, 0}, 0); }); // Buried foundation card.
        rejects([&] { game.action(Card{0, 0}, 0); });
        rejects([&] { game.action(Card{14, 0}, 0); });
        assert(game.state_key() == before);
        game.action(Card{13, 0}, 0);
        assert(game.towers()[0] == 12);
        assert((game.board()[0].last() == Card{13, 0}));
        game.action(Card{13, 0}, 10);
        assert(game.state_key() == before);
        check_moves(game);
    }
    {
        std::array<Column, 7> board{};
        board[0] = Column({Card{9, 0}, Card{8, 1}, Card{7, 0}});
        board[1] = Column({Card{9, 2}});
        board[2] = Column({Card{10, 3}});
        UctSolitaire game(board);
        const auto moves = game.legal_moves();
        assert(std::find(moves.begin(), moves.end(), UctSolitaire::Move{Card{8, 1}, 1}) == moves.end());
        const auto before = game.state_key();
        rejects([&] { game.action(Card{8, 1}, 1); });
        assert(game.state_key() == before);
        check_moves(game);
        game.action(Card{9, 0}, 2); // The whole revealed run is still movable.
        assert(game.board()[0].empty() && game.board()[2].size() == 4);
    }
    {
        // Partial transfers must immediately move the preceding card to its
        // foundation, whether or not that uncovers a hidden card.
        for (bool hidden : {false, true}) {
            std::array<Column, 7> board{};
            board[0] = Column({Card{13, 3}, Card{9, 0}, Card{8, 1}, Card{7, 2}}, hidden ? 1 : 0);
            board[1] = Column({Card{9, 2}});
            board[2] = Column({Card{12, 1}, Card{11, 0}}, 1);
            std::vector<Card> stock;
            for (int rank = 1; rank <= 8; ++rank) {
                stock.emplace_back(rank, 0);
            }
            UctSolitaire game(board, stock, 1);
            for (Card card : stock) {
                game.action(card, 10);
            }
            check_moves(game);
            const UctSolitaire::Move move{Card{8, 1}, 1};
            const auto moves = game.legal_moves();
            assert(std::find(moves.begin(), moves.end(), move) != moves.end());
            auto actual = game;
            actual.action(move.from, move.to);
            assert(actual.towers()[0] == 9);
            assert(actual.board()[0].size() == 1 && actual.board()[0].hidden() == 0);
            assert((actual.board()[0].last() == Card{13, 3}));
            assert(actual.board()[1].size() == 3);
            std::mt19937_64 generator(123);
            int kings = 0;
            for (int sample = 0; sample < 100; ++sample) {
                const auto next = game.sample_action(move, generator);
                assert(next.towers()[0] == 9 && next.board()[0].hidden() == 0);
                kings += next.board()[0].last() == Card{13, 3};
                if (!hidden) {
                    assert(next.state_key() == actual.state_key());
                }
            }
            if (hidden) {
                assert(kings > 30 && kings < 70);
                UctOptions options;
                options.trajectories = 200;
                options.rollout_limit = 1;
                options.seed = 123;
                const auto result = uct_search(game, options);
                const auto edge = std::find_if(result.actions.begin(), result.actions.end(),
                    [&](const auto& action) { return action.move == move; });
                assert(edge != result.actions.end() && edge->outcomes == 2);
            }
        }
    }
    {
        std::mt19937_64 generator(7);
        for (int deal : {1, 3}) {
            UctSolitaire game(42, deal);
            for (int step = 0; step < 24; ++step) {
                check_moves(game);
                const auto moves = game.legal_moves();
                if (moves.empty()) {
                    break;
                }
                auto move = moves[generator() % moves.size()];
                game.action(move.from, move.to);
            }
        }
    }
    {
        const auto game = two_hidden();
        const auto swapped = two_hidden(true);
        const auto original = game.state_key();
        assert(original == swapped.state_key());
        std::mt19937_64 first(123), second(123);
        int nines = 0;
        for (int sample = 0; sample < 1000; ++sample) {
            auto next = game.sample_action({Card{1, 0}, 10}, first);
            auto other = swapped.sample_action({Card{1, 0}, 10}, second);
            assert(next.state_key() == other.state_key());
            assert(next.board()[0].hidden() == 0);
            assert(next.board()[1].hidden() == 1);
            assert(next.towers()[0] == 1);
            const auto revealed = next.board()[0].last();
            assert((revealed == Card{9, 0} || revealed == Card{8, 1}));
            nines += revealed == Card{9, 0};
            if (revealed == Card{8, 1}) {
                next = next.sample_action({Card{7, 0}, 0}, first);
                other = other.sample_action({Card{7, 0}, 0}, second);
                assert((next.board()[1].last() == Card{9, 0}));
                assert(next.state_key() == other.state_key());
            }
        }
        assert(nines > 400 && nines < 600);
        assert(game.state_key() == original);
        UctSolitaire actual = game;
        actual.action(Card{1, 0}, 10);
        assert((actual.board()[0].last() == Card{9, 0}));
    }
    {
        // Sampling can exchange two hidden identities within the same column.
        std::array<Column, 7> board{};
        board[0] = Column({Card{9, 0}, Card{8, 1}, Card{1, 0}}, 2);
        const UctSolitaire game(board);
        std::mt19937_64 generator(123);
        int nines = 0;
        for (int sample = 0; sample < 100; ++sample) {
            const auto next = game.sample_action({Card{1, 0}, 10}, generator);
            assert(next.board()[0].hidden() == 1);
            const auto cards = next.board()[0].all();
            assert(cards.size() == 2);
            assert((cards[0] == Card{9, 0} && cards[1] == Card{8, 1})
                || (cards[0] == Card{8, 1} && cards[1] == Card{9, 0}));
            nines += next.board()[0].last() == Card{9, 0};
        }
        assert(nines > 30 && nines < 70);
        assert(game.board()[0].hidden() == 2);
        assert((game.board()[0].last() == Card{1, 0}));
    }
    {
        const auto game = two_hidden();
        for (int width : {0, 1, 2}) {
            UctOptions options;
            options.trajectories = 80;
            options.rollout_limit = 1;
            options.sampling_width = width;
            options.seed = 123;
            const auto result = uct_search(game, options);
            const auto swapped = uct_search(two_hidden(true), options);
            assert(result.actions.size() == 1);
            assert(result.move == swapped.move);
            assert(result.actions[0].visits == 80);
            assert(result.actions[0].value == 0);
            assert(result.actions[0].outcomes == swapped.actions[0].outcomes);
            if (width == 0) {
                assert(result.actions[0].outcomes == 2);
            } else {
                assert(result.actions[0].outcomes <= static_cast<std::size_t>(width));
            }
        }
    }
    {
        auto game = last_king();
        const auto before = game.state_key();
        UctOptions options;
        options.trajectories = 100;
        options.rollout_limit = 1;
        options.trees = 2;
        const auto result = uct_search(game, options);
        assert((result.move == UctSolitaire::Move{Card{13, 3}, 13}));
        const auto winning = std::find_if(result.actions.begin(), result.actions.end(),
            [&](const auto& action) { return action.move == *result.move; });
        std::uint64_t visits = 0;
        for (const auto& action : result.actions) {
            visits += action.visits;
            assert(action.visits > 0);
            assert(action.outcomes == 2); // One deterministic outcome per tree.
            assert(action.value == (action.move == *result.move ? 1.0 : 0.0));
            if (action.move != *result.move) {
                assert(winning->visits > action.visits);
            }
        }
        assert(visits == 200);
        assert(game.state_key() == before);
        const auto repeat = uct_search(game, options);
        for (std::size_t i = 0; i < result.actions.size(); ++i) {
            assert(result.actions[i].visits == repeat.actions[i].visits);
            assert(result.actions[i].value == repeat.actions[i].value);
        }
        assert(uct_step(game, options));
        assert(game.complete());
        assert(!uct_step(game, options));
        assert(uct_solve(game, options, 0, false));
    }
    {
        // Moving an ace between tableau and foundation must not cause a cycle.
        std::array<Column, 7> board{};
        board[0] = Column({Card{2, 1}});
        UctSolitaire game(board, {Card{1, 0}}, 1);
        UctOptions options;
        options.trajectories = 8;
        options.rollout_limit = 20;
        assert(!uct_solve(game, options, 100, false));
        UctSolitaire dead(std::array<Column, 7>{});
        assert(!uct_search(dead, options).move);
        assert(!uct_solve(dead, options, 100, false));
        auto nearly_done = last_king();
        const auto before = nearly_done.state_key();
        assert(!uct_solve(nearly_done, options, 0, false));
        assert(nearly_done.state_key() == before);
        assert(uct_solve(nearly_done, options, 1, false));
    }
    {
        // Shuffling a king between empty columns is the same search position.
        std::array<Column, 7> board{};
        board[0] = Column({Card{13, 0}});
        UctSolitaire game(board);
        auto renamed = game;
        renamed.action(Card{13, 0}, 1);
        assert(game.state_key() != renamed.state_key());
        assert(game.state_key(true) == renamed.state_key(true));
        const auto result = uct_search(game);
        assert(!result.move);
        for (const auto& action : result.actions) {
            assert(action.visits == 0);
        }
    }
    {
        // Two uncertain reveals on a complete 52-card position, followed by a
        // deterministic finish. The solver must execute the actual deal's cards.
        auto game = six_cards_left();
        UctOptions options;
        options.trajectories = 80;
        options.rollout_limit = 12;
        options.seed = 3;
        const auto before = game.state_key();
        const auto result = uct_search(game, options);
        assert(result.move);
        assert(game.state_key() == before);
        assert(uct_solve(game, options, 6, false));
        assert(game.complete());
    }
    {
        // With six cards left, a six-move budget leaves no room for detours.
        for (int width : {0, 1, 2}) {
            for (int trees : {1, 3}) {
                auto game = six_cards_left();
                UctOptions options;
                options.seed = 2;
                options.rollout_limit = 100;
                options.sampling_width = width;
                options.trees = trees;
                assert(uct_solve(game, options, 6, false));
                assert(game.complete());
            }
        }
    }
    {
        // A full, exposed deck is solved by the paper's deterministic finish.
        std::array<Column, 7> board{};
        for (int suit = 0; suit < 4; ++suit) {
            std::vector<Card> cards;
            for (int rank = 13; rank > 0; --rank) {
                cards.emplace_back(rank, suit);
            }
            board[suit] = Column(cards);
        }
        UctSolitaire game(board);
        assert(uct_solve(game, {}, 52, false));
        assert(game.complete());
    }
    {
        const auto game = two_hidden();
        for (int field = 0; field < 7; ++field) {
            UctOptions options;
            switch (field) {
                case 0: options.trajectories = 0; break;
                case 1: options.rollout_limit = 0; break;
                case 2: options.sampling_width = -1; break;
                case 3: options.trees = 0; break;
                case 4: options.exploration = -1; break;
                case 5: options.exploration = std::numeric_limits<double>::infinity(); break;
                case 6: options.exploration = std::numeric_limits<double>::quiet_NaN(); break;
            }
            rejects([&] { uct_search(game, options); });
        }
    }
    std::cout << "UCT checks ok\n";
}

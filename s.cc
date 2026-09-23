module;

#include <optional>
#include <set>
#include <vector>

export module solitaire_strategy;

import solitaire;

namespace {

bool fits(const Column& destination, Card card) {
    return destination.empty()
        ? card.number == 13
        : destination.last().number == card.number + 1
            && destination.last().color() != card.color();
}

std::optional<Solitaire::Move> board_reveal(const Solitaire& game) {
    const auto& board = game.board();
    const auto& towers = game.towers();

    for (int source = 0; source < 7; ++source) {
        const Column& column = board[source];
        if (column.unrevealed() == 0 || column.empty()) {
            continue;
        }

        const std::size_t row = static_cast<std::size_t>(column.unrevealed());
        const Card card = column[row];
        if (!column.valid_run(row)) {
            continue;
        }

        // A foundation move also counts when it turns over a hidden card.
        if (row + 1 == column.size() && towers[card.suit] == card.number - 1) {
            return Solitaire::Move{card, 10 + card.suit};
        }

        for (int destination = 0; destination < 7; ++destination) {
            if (destination != source && fits(board[destination], card)) {
                return Solitaire::Move{card, destination};
            }
        }
    }
    return std::nullopt;
}

std::optional<Solitaire::Move> bottom_stock_move(const Solitaire& game) {
    const auto& board = game.board();
    const auto& towers = game.towers();
    const auto& cards = game.deck().cards();

    // The end of the stock vector is the bottom of the usable pile. Scan
    // backwards so the first playable card is the requested bottom-most one.
    for (std::size_t position = cards.size(); position-- > 0;) {
        if (!game.deck().usable(position)) {
            continue;
        }

        const Card card = cards[position];
        if (card.suit >= 0 && card.suit < 4 && towers[card.suit] == card.number - 1) {
            return Solitaire::Move{card, 10 + card.suit};
        }
        for (int destination = 0; destination < 7; ++destination) {
            if (fits(board[destination], card)) {
                return Solitaire::Move{card, destination};
            }
        }
    }
    return std::nullopt;
}

std::optional<Solitaire::Move> empty_column_move(const Solitaire& game) {
    const auto& board = game.board();
    for (int source = 0; source < 7; ++source) {
        const Column& column = board[source];
        if (column.empty() || column.unrevealed() != 0 || !column.valid_run(0)) {
            continue;
        }
        const Card card = column[0];
        // Emptying a fully exposed column is useful only with a non-king;
        // moving a king between empty columns creates a reversible cycle.
        if (card.number == 13) {
            continue;
        }
        for (int destination = 0; destination < 7; ++destination) {
            if (destination != source && !board[destination].empty() && fits(board[destination], card)) {
                return Solitaire::Move{card, destination};
            }
        }
    }
    return std::nullopt;
}

std::optional<Solitaire::Move> board_foundation_move(const Solitaire& game) {
    const auto& board = game.board();
    const auto& towers = game.towers();
    for (int source = 0; source < 7; ++source) {
        if (board[source].empty()) {
            continue;
        }
        const Card card = board[source].last();
        if (card.suit >= 0 && card.suit < 4 && towers[card.suit] == card.number - 1) {
            return Solitaire::Move{card, 10 + card.suit};
        }
    }
    return std::nullopt;
}

std::vector<int> state_key(const Solitaire& game) {
    std::vector<int> key;
    for (int count : game.towers()) {
        key.push_back(count);
    }
    key.push_back(static_cast<int>(game.deck().cursor()));
    key.push_back(static_cast<int>(game.deck().cards().size()));
    for (const Card& card : game.deck().cards()) {
        key.push_back(card.number);
        key.push_back(card.suit);
    }
    key.push_back(-1);
    for (const Column& column : game.board()) {
        key.push_back(column.unrevealed());
        key.push_back(static_cast<int>(column.size()));
        for (std::size_t i = 0; i < column.size(); ++i) {
            key.push_back(column[i].number);
            key.push_back(column[i].suit);
        }
        key.push_back(-1);
    }
    return key;
}

} // namespace

export bool greedy_step(Solitaire& game) {
    if (const auto move = board_reveal(game)) {
        game.action(move->from, move->to);
        return true;
    }

    if (const auto move = board_foundation_move(game)) {
        game.action(move->from, move->to);
        return true;
    }

    if (const auto move = empty_column_move(game)) {
        game.action(move->from, move->to);
        return true;
    }

    if (const auto move = bottom_stock_move(game)) {
        game.action(move->from, move->to);
        return true;
    }

    if (game.deck().empty()) {
        return false;
    }
    return game.draw();
}

export bool greedy_solve(Solitaire& game, int limit = 10000) {
    if (limit < 0) {
        return false;
    }
    std::set<std::vector<int>> seen;
    for (int step = 0; step < limit && !game.complete(); ++step) {
        if (!seen.insert(state_key(game)).second) {
            return false;
        }
        if (!greedy_step(game)) {
            return false;
        }
    }
    return game.complete();
}

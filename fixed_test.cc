#include <array>
#include <cassert>
#include <iostream>
#include <vector>

import solitaire;
import solitaire_strategy;

int main() {
    {
        std::array<Column, 7> board{};
        Column first;
        first.add(Card{9, 0});
        first.add(Card{5, 0});
        board[0] = Column(first.all(), 1);
        Column second;
        second.add(Card{6, 1});
        board[1] = second;
        Solitaire game(board);
        assert(greedy_step(game));
        assert(game.board()[0].size() == 1);
        assert(game.board()[0].unrevealed() == 0);
        assert((game.board()[0].last() == Card{9, 0}));
        assert(game.board()[1].size() == 2);
        assert((game.board()[1].last() == Card{5, 0}));
    }

    {
        std::array<Column, 7> board{};
        Column first;
        first.add(Card{7, 1});
        board[0] = first;
        Solitaire game(board, {
            Card{2, 0}, Card{3, 1}, Card{4, 0}, Card{5, 1},
            Card{8, 0}, Card{6, 0}, Card{13, 0},
        });
        assert(greedy_step(game));
        assert(game.board()[1].size() == 1);
        assert((game.board()[1].last() == Card{13, 0}));
        assert(game.deck().cards().size() == 6);
    }

    {
        std::array<Column, 7> board{};
        board[0] = Column({Card{7, 0}, Card{6, 1}});
        board[1] = Column({Card{7, 2}});
        Solitaire game(board);
        assert(!greedy_step(game));
        assert(game.board()[0].size() == 2);
        assert(game.board()[1].size() == 1);
    }

    {
        Solitaire game;
        game.init(42);
        std::size_t board_cards = 0;
        for (int column = 0; column < 7; ++column) {
            assert(game.board()[column].unrevealed() == column);
            board_cards += game.board()[column].size();
        }
        assert(board_cards == 28);
        assert(game.deck().cards().size() == 24);
    }

    {
        std::array<Column, 7> board{};
        for (int suit = 0; suit < 4; ++suit) {
            std::vector<Card> pile;
            for (int number = 13; number >= 1; --number) {
                pile.emplace_back(number, suit);
            }
            board[suit] = Column(std::move(pile), 12);
        }

        Solitaire game(board);
        assert(greedy_solve(game, 1000));
        assert(game.complete());
    }

    std::cout << "fixed deal ok\n";
}

#include <array>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

import solitaire;
import solitaire_strategy;

int main() {
    {
        std::array<Column, 7> board{};
        board[0] = Column({Card{9, 0}, Card{5, 0}}, 1);
        board[1] = Column({Card{6, 1}});
        Solitaire game(board);
        assert(greedy_step(game));
        assert(game.board()[0].size() == 1);
        assert(game.board()[0].hidden() == 0);
        assert((game.board()[0].last() == Card{9, 0}));
        assert(game.board()[1].size() == 2);
        assert((game.board()[1].last() == Card{5, 0}));
    }

    {
        std::array<Column, 7> board{};
        board[0] = Column({Card{7, 1}});
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
        Solitaire game(42);
        assert(game.deal_number() == 3);
        std::size_t board_cards = 0;
        for (int column = 0; column < 7; ++column) {
            assert(game.board()[column].hidden() == static_cast<std::size_t>(column));
            board_cards += game.board()[column].size();
        }
        assert(board_cards == 28);
        assert(game.deck().cards().size() == 24);
    }

    {
        Solitaire game(42, 1);
        assert(game.deal_number() == 1);
        Deck stock = game.deck();
        while (!stock.empty()) {
            for (std::size_t position = 0; position < stock.cards().size(); ++position) {
                assert(stock.usable(position));
                assert(stock.surface(position));
            }
            assert(!stock.usable(stock.cards().size()));
            assert(stock.remove(stock.cards().size() / 2));
        }
        assert(!stock.usable(0));
        assert(!stock.remove(0));
    }

    {
        const std::vector<Card> cards = {
            Card{1, 0}, Card{8, 1}, Card{9, 0}, Card{10, 1},
            Card{11, 0}, Card{12, 1}, Card{13, 0},
        };
        Deck stock(cards);
        for (std::size_t position = 0; position < cards.size(); ++position) {
            assert(stock.usable(position) == (position == 2 || position == 5 || position == 6));
        }
        assert(!stock.remove(0));
        assert(stock.remove(2));
        assert(stock.usable(1));
        assert(stock.usable(4));
        assert(!stock.usable(3));

        Deck deal2(cards, 2);
        assert(!deal2.usable(0));
        assert(deal2.usable(1));
        assert(!deal2.usable(2));
        assert(deal2.usable(6)); // A final partial draw is usable.

        std::array<Column, 7> board{};
        Solitaire deal1(board, cards, 1);
        deal1.action(Card{1, 0}, 10);
        assert(deal1.towers()[0] == 1);
        assert(deal1.deck().cards().size() == 6);

        Solitaire deal3(board, cards);
        bool rejected = false;
        try {
            deal3.action(Card{1, 0}, 10);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
        assert(deal3.towers()[0] == 0);
        assert(deal3.deck().cards().size() == 7);
    }

    for (int invalid : {0, -1}) {
        bool rejected = false;
        try {
            Solitaire game(42, invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
        rejected = false;
        try {
            Solitaire game(std::array<Column, 7>{}, {}, invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
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

module;

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

export module solitaire;

export class Card {
private:
    int number_, suit_;
public:
    Card(int number, int suit) : number_(number), suit_(suit) {}

    int number() const noexcept { return number_; }
    int suit() const noexcept { return suit_; }
    int color() const noexcept { return suit_ & 1; }
    bool operator ==(const Card & other) const = default;
};

export class Column {
    friend class Solitaire;

    std::vector<Card> cards_;
    std::size_t hidden_ = 0;

    void reveal() noexcept {
        if (hidden_ > 0) {
            --hidden_;
        }
    }
    std::vector<Card> all() const { return cards_; }

public:
    Column() = default;
    explicit Column(std::vector<Card> cards, std::size_t hidden = 0)
        : cards_(std::move(cards)), hidden_(hidden) {
        if (hidden_ > cards_.size()) {
            throw std::invalid_argument("hidden count exceeds column size");
        }
    }

    std::size_t hidden() const noexcept { return hidden_; }
    bool empty() const noexcept { return cards_.empty(); }
    std::size_t size() const noexcept { return cards_.size(); }

    const Card& operator[](std::size_t index) const {
        if (index < hidden_) {
            throw std::out_of_range("card hidden");
        }
        return cards_.at(index);
    }

    const Card& last() const {
        if (cards_.empty()) {
            throw std::out_of_range("empty column");
        }
        return cards_.back();
    }

    std::vector<Card> revealed() const {
        return {cards_.begin() + static_cast<std::ptrdiff_t>(hidden_), cards_.end()};
    }

    bool valid_run(std::size_t position) const noexcept {
        if (position < hidden_ || position >= cards_.size()) {
            return false;
        }
        for (std::size_t i = position + 1; i < cards_.size(); ++i) {
            if (cards_[i - 1].number() != cards_[i].number() + 1
                || cards_[i - 1].color() == cards_[i].color()) {
                return false;
            }
        }
        return true;
    }

    void push_back(Card card) { cards_.push_back(card); }

    void extend(const std::vector<Card>& cards) {
        cards_.insert(cards_.end(), cards.begin(), cards.end());
    }

    void erase_from(std::size_t position) {
        if (position >= cards_.size()) {
            throw std::out_of_range("column erase position");
        }
        cards_.erase(cards_.begin() + static_cast<std::ptrdiff_t>(position), cards_.end());
    }

};

export class Deck {
    std::vector<Card> cards_;
    std::size_t cursor_ = SIZE_MAX;
    std::size_t draws_ = 0;

public:
    Deck() = default;
    explicit Deck(std::vector<Card> cards) : cards_(std::move(cards)) {}

    const std::vector<Card>& cards() const noexcept { return cards_; }
    const std::vector<Card>& deck() const noexcept { return cards_; }
    operator const std::vector<Card>&() const noexcept { return cards_; }

    std::size_t cursor() const noexcept { return cursor_; }
    bool empty() const noexcept { return cards_.empty(); }

    bool usable(std::size_t position) const noexcept {
        if (position >= cards_.size()) {
            return false;
        }
        if (position >= cursor_ && (position - cursor_) % 3 == 0 ) {
            return true;
        }
        return position % 3 == 2 || position + 1 == cards_.size();
    }

    bool surface(std::size_t position) const noexcept { return usable(position); }

    bool contains(Card card, std::size_t& position) const noexcept {
        for (std::size_t i = 0; i < cards_.size(); ++i) {
            if (cards_[i] == card) {
                position = i;
                return true;
            }
        }
        return false;
    }

    bool remove(std::size_t position) {
        if (!usable(position)) {
            return false;
        }
        cards_.erase(cards_.begin() + static_cast<std::ptrdiff_t>(position));
        if (position) {
            cursor_ = position - 1;
        } else {
            cursor_ = SIZE_MAX;
        }
        return true;
    }
};

export class Solitaire {
public:
    struct Move {
        Card from;
        int to;
    };

private:
    std::array<Column, 7> board_{};
    std::array<int, 4> towers_{};
    Deck deck_;

    bool locate(Card card, int& kind, int& index, int& row) const noexcept {
        for (int column = 0; column < 7; ++column) {
            for (std::size_t position = board_[column].hidden();
                 position < board_[column].size(); ++position) {
                if (board_[column][position] == card) {
                    kind = 0;
                    index = column;
                    row = static_cast<int>(position);
                    return true;
                }
            }
        }

        std::size_t position = 0;
        if (deck_.contains(card, position)) {
            kind = 1;
            index = 0;
            row = static_cast<int>(position);
            return true;
        }

        if (card.suit() >= 0 && card.suit() < 4 && towers_[card.suit()] >= card.number()) {
            kind = 2;
            index = card.suit();
            row = card.number();
            return true;
        }
        return false;
    }

    static bool fits(const Column& destination, Card card) noexcept {
        return destination.empty()
            ? card.number() == 13
            : destination.last().number() == card.number() + 1
                && destination.last().color() != card.color();
    }

    void reveal_after(Column& column) noexcept {
        // Removing the only exposed card leaves hidden_ equal to size().
        if (column.hidden() > 0
            && column.size() == column.hidden()) {
            column.reveal();
        }
    }

public:
    Solitaire(std::uint64_t seed = std::random_device{}()) {
        std::vector<Card> cards;
        cards.reserve(52);
        for (int suit = 0; suit < 4; ++suit) {
            for (int number = 1; number <= 13; ++number) {
                cards.emplace_back(number, suit);
            }
        }

        std::mt19937_64 generator(seed);
        std::shuffle(cards.begin(), cards.end(), generator);

        board_ = {};
        towers_ = {};
        std::size_t position = 0;
        for (int column = 0; column < 7; ++column) {
            std::vector<Card> pile;
            pile.reserve(static_cast<std::size_t>(column + 1));
            for (int row = 0; row <= column; ++row) {
                pile.push_back(cards[position++]);
            }
            board_[column] = Column(std::move(pile), static_cast<std::size_t>(column));
        }
        deck_ = Deck({cards.begin() + static_cast<std::ptrdiff_t>(position), cards.end()});
    }

    Solitaire(std::array<Column, 7> board, std::vector<Card> deck = {})
        : board_(std::move(board)), deck_(std::move(deck)) {}

    const std::array<int, 4>& towers() const noexcept { return towers_; }
    const Deck& deck() const noexcept { return deck_; }
    const std::array<Column, 7>& board() const noexcept { return board_; }

    bool complete() const noexcept {
        return std::all_of(towers_.begin(), towers_.end(), [](int count) { return count == 13; });
    }

    void action(Card card, int destination);
};

void Solitaire::action(Card card, int destination) {
    int kind = 0;
    int index = 0;
    int row = 0;
    if (!locate(card, kind, index, row)) {
        throw std::invalid_argument("unavailable card");
    }

    if (kind == 0 && !board_[index].valid_run(static_cast<std::size_t>(row))) {
        throw std::invalid_argument("unavailable card");
    }
    if (kind == 1 && !deck_.usable(static_cast<std::size_t>(row))) {
        throw std::invalid_argument("unavailable card");
    }

    if (destination >= 0 && destination < 7) {
        if (kind == 2 || (kind == 0 && index == destination) || !fits(board_[destination], card)) {
            throw std::invalid_argument("invalid tableau move");
        }

        if (kind == 0) {
            const std::vector<Card> cards = board_[index].all();
            board_[destination].extend(
                {cards.begin() + static_cast<std::ptrdiff_t>(row), cards.end()});
            board_[index].erase_from(static_cast<std::size_t>(row));
            reveal_after(board_[index]);
        } else if (!deck_.remove(static_cast<std::size_t>(row))) {
            throw std::invalid_argument("unavailable card");
        } else {
            board_[destination].push_back(card);
        }
        return;
    }

    if (destination < 10 || destination > 13 || card.suit() != destination - 10
        || card.number() < 1 || card.number() > 13
        || towers_[card.suit()] != card.number() - 1) {
        throw std::invalid_argument("invalid foundation move");
    }
    if (kind == 0 && row != static_cast<int>(board_[index].size()) - 1) {
        throw std::invalid_argument("only exposed card can enter foundation");
    }
    if (kind == 2) {
        throw std::invalid_argument("invalid foundation move");
    }

    if (kind == 0) {
        board_[index].erase_from(static_cast<std::size_t>(row));
        reveal_after(board_[index]);
    } else if (!deck_.remove(static_cast<std::size_t>(row))) {
        throw std::invalid_argument("unavailable card");
    }
    ++towers_[card.suit()];
}

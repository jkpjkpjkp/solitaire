# Klondike solvers

```sh
make
make test
make run ARGS="--strategy uct --deal 3 --games 10 --trajectories 100"
```

The existing greedy solver remains the default. `--deal 1` selects draw-one;
omitting `--deal` compares draw-one and draw-three on the same deals.
`build/random_check --help` lists the options. Use a small `--games` count for
UCT: its default of 100 trajectories is spent at **each decision**. Deals and
searches use reproducible seeds; `--seed N` changes the sequence of deals.

## UCT

`uct.cc` implements the UCT algorithm from Bjarnason, Fern, and Tadepalli,
*Lower Bounding Klondike Solitaire with Monte-Carlo Planning* (`klondike.pdf`).

- Unvisited actions are selected uniformly at random. Otherwise selection uses
  `Q(s,a) + c * sqrt(log(n(s)) / n(s,a))`, with `c = 1` by default.
- Each trajectory grows the tree to a win, dead end, or configured move limit.
  Every traversed action receives reward 1 for a win and 0 otherwise. The real
  move maximizes the mean reward, rather than the exploration bonus.
- Reveal outcomes are sampled uniformly from the remaining unseen identities,
  independently whenever a transition is simulated. The search never uses their
  actual positions. The stock is visible, with draw actions absorbed into macro
  moves, as in the paper and the existing `Deck::usable` representation.
- Legal moves include partial tableau runs and withdrawals of the top foundation
  card. Actions repeating observations in the current trajectory or actual game
  history are excluded before selection. Column permutations are treated as the
  same search position, avoiding redundant king moves between empty columns.
- Fully revealed positions get a greedy completion attempt with the paper's six
  action preferences. Failed attempts are discarded and UCT continues. Equal
  root values use that same preference order.
- `uct_solve` caps each search at the remaining real move budget, so paths that
  would finish after the solver's deadline cannot count as wins.

`--sampling-width W` enables Sparse UCT: the first W transition samples for each
state/action are retained (including duplicate outcomes), then reused uniformly.
Width 0 means ordinary UCT; width 1 yields a HOP-UCT tree. `--trees N` averages
root action values over N independent trees, enabling Ensemble-UCT and
Ensemble-Sparse-UCT. Each tree receives `--trajectories` rollouts. Actions left
unvisited by a small budget contribute zero to the ensemble mean and are not
selected unless visited in another tree.

```sh
make run ARGS="--strategy uct --deal 3 --games 10 --trajectories 1000 --sampling-width 5"
make run ARGS="--strategy uct --deal 3 --games 10 --trajectories 100 --sampling-width 1 --trees 20"
```

Unlike the paper's unbounded trajectories, this implementation defaults to a
1,000-move safety limit (`--rollout-limit`). A cutoff receives reward 0; increasing
the limit allows longer searches. Failure means the policy did not solve the game
within its budget, not that the deal is unsolvable. The paper's published win
rates have not been reproduced by this implementation.

## C++ interface

Import `solitaire` and `solitaire_uct`:

```cpp
UctSolitaire game(42, 3);
UctOptions options;
options.trajectories = 1000;
options.seed = 123;

auto result = uct_search(game, options); // Non-mutating move and root statistics.
bool moved = uct_step(game, options);   // Executes one selected move.
bool won = uct_solve(game, options, 10000, false);
```

`UctOptions` also exposes `exploration`, `rollout_limit`, `sampling_width`, and
`trees`. `uct_solve` retains the random generator and visited game observations
between decisions; repeated independent calls to `uct_step` do not retain that
history. All search budgets must be positive, sampling width nonnegative, and
exploration finite and nonnegative.

`UctSolitaire` derives from `Solitaire` in `uct.cc`. It owns legal-move generation,
observation keys, reveal sampling, and foundation withdrawals. The base game
keeps its original move rules. Use `UctSolitaire(existing_game)` to copy an
existing `Solitaire` into the UCT game type.

To observe a trivial six-card finish, including each root action's visits,
estimated win rate, sampled outcomes, and the actual moves and boards:

```sh
make build/uct_test
./build/uct_test --trace
```

The fixture has two hidden kings under exposed queens and two kings in the stock.
With seed 2 and a six-move solve budget, the old search preferred a stock-to-tableau
detour that required a seventh move. The regression now finishes in six moves.
Set `options.trace = &std::cout` to trace other searches or games; tracing is off
by default.

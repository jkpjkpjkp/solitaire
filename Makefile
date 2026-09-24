# Requires Clang 18 or newer with C++ modules support.
# make       builds build/random_check
# make run   builds and runs the random checks
# make run ARGS="--deal 1 --visualize-failures 3" selects a deal and shows up to 3 failures
# make run ARGS="--strategy uct --deal 3 --games 10" runs the UCT solver
# make run ARGS="--max-parallel 4" limits concurrent random games to 4
# make test  builds and runs the fixed checks
# make clean removes generated build files
CXX = clang++
CXXFLAGS ?= -O2
MODULE_FLAGS = -std=c++23 -fprebuilt-module-path=build -pthread

.PHONY: all random_check run test clean
.DELETE_ON_ERROR:

all: random_check

random_check: build/random_check

run: build/random_check
	./build/random_check $(ARGS)

test: build/fixed_test build/uct_test
	./build/fixed_test
	./build/uct_test

build:
	mkdir -p build

# Build module interfaces before compiling their importers.
build/solitaire.pcm: solitaire.cc Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(MODULE_FLAGS) -x c++-module --precompile $< -o $@

build/solitaire_strategy.pcm: greedy.cc build/solitaire.pcm Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(MODULE_FLAGS) -x c++-module --precompile $< -o $@

build/solitaire_uct.pcm: uct.cc build/solitaire.pcm Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(MODULE_FLAGS) -x c++-module --precompile $< -o $@

build/solitaire.o: build/solitaire.pcm
	$(CXX) $(CXXFLAGS) $(MODULE_FLAGS) -c $< -o $@

build/greedy.o: build/solitaire_strategy.pcm
	$(CXX) $(CXXFLAGS) $(MODULE_FLAGS) -c $< -o $@

build/uct.o: build/solitaire_uct.pcm
	$(CXX) $(CXXFLAGS) $(MODULE_FLAGS) -c $< -o $@

build/random_check.o: random_check.cc build/solitaire.pcm build/solitaire_strategy.pcm build/solitaire_uct.pcm Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(MODULE_FLAGS) -c $< -o $@

build/random_check: build/random_check.o build/solitaire.o build/greedy.o build/uct.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS) -pthread -o $@

build/fixed_test.o: fixed_test.cc build/solitaire.pcm build/solitaire_strategy.pcm Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(MODULE_FLAGS) -c $< -o $@

build/fixed_test: build/fixed_test.o build/solitaire.o build/greedy.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

build/uct_test.o: uct_test.cc build/solitaire.pcm build/solitaire_uct.pcm Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(MODULE_FLAGS) -c $< -o $@

build/uct_test: build/uct_test.o build/solitaire.o build/uct.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(RM) -r build

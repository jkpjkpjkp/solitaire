# Requires Clang 18 or newer with C++ modules support.
# make       builds build/random_check
# make run   builds and runs the random checks
# make clean removes generated build files
CXX = clang++-18
CXXFLAGS ?= -O2
MODULE_FLAGS = -std=c++23 -fprebuilt-module-path=build

.PHONY: all random_check run clean
.DELETE_ON_ERROR:

all: random_check

random_check: build/random_check

run: build/random_check
	./build/random_check

build:
	mkdir -p build

# Build module interfaces before compiling their importers.
build/solitaire.pcm: solitaire.cc Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(MODULE_FLAGS) -x c++-module --precompile $< -o $@

build/solitaire_strategy.pcm: s.cc build/solitaire.pcm Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(MODULE_FLAGS) -x c++-module --precompile $< -o $@

build/solitaire.o: build/solitaire.pcm
	$(CXX) $(CXXFLAGS) $(MODULE_FLAGS) -c $< -o $@

build/s.o: build/solitaire_strategy.pcm
	$(CXX) $(CXXFLAGS) $(MODULE_FLAGS) -c $< -o $@

build/random_check.o: random_check.cc build/solitaire.pcm build/solitaire_strategy.pcm Makefile | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(MODULE_FLAGS) -c $< -o $@

build/random_check: build/random_check.o build/solitaire.o build/s.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(RM) -r build

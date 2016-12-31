CXXFLAGS=-g -Wall
BINDIR=../bin
TESTBIN=$(BINDIR)/test_shitbrix
SRC_DIR=../src

# The googletest directory is not included in the shitbrix archive
# and must be provided manually. https://github.com/google/googletest
GTEST_DIR=googletest/include

CPPFLAGS+=-I$(SRC_DIR) -isystem $(GTEST_DIR)
CXXFLAGS+=-pthread

# Unfortunately, even the non-gfx game code depends on SDL
# for functions like SDL_assert().
LDLIBS+=-lpthread $$(sdl2-config --libs)

TEST_O=test_block.o test_pit.o test_replay.o test_blockdirector.o test_gameevent.o

.all: $(TESTBIN)
.PHONY: $(SRC_DIR)/game.a clean run

# gtest_main.a is the compiled archive of googletest functions
# including a main() definition. It must be provided manually.
$(TESTBIN): $(TEST_O) game.a gtest_main.a | $(BINDIR)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BINDIR):
	mkdir $@

$(SRC_DIR)/game.a:
	$(MAKE) -C $(SRC_DIR) game.a

# game.a is the main game project compiled into an archive.
# This job is done by the main makefile found in SRC_DIR.
# SRC_DIR must be set correctly.
# Once the symlink exists, it is not touched again.
game.a: $(SRC_DIR)/game.a
	ln -s $(SRC_DIR)/game.a 2>/dev/null || true

test_block.o: test_block.cpp mock.hpp $(wildcard $(SRC_DIR)/{stage.{c,h}pp,context.hpp,globals.hpp})
test_pit.o: test_pit.cpp $(wildcard $(SRC_DIR)/{stage.{c,h}pp,context.hpp,globals.hpp})
test_replay.o: test_replay.cpp
test_blockdirector.o: test_blockdirector.cpp $(wildcard $(SRC_DIR)/{director.{c,h}pp,stage.hpp,context.hpp,globals.hpp})
test_gameevent.o: test_gameevent.cpp $(wildcard $(SRC_DIR)/{director.{c,h}pp,stage.hpp,context.hpp,globals.hpp,gameevent.hpp})

clean:
	rm -f $(TESTBIN) $(TEST_O) game.a

run:
	(cd .. && tests/$(TESTBIN))

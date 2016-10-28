TESTBIN=../bin/test_shitbrix
SRC_DIR=../src

# The googletest directory is not included in the shitbrix archive
# and must be provided manually. https://github.com/google/googletest
GTEST_DIR=googletest/include

CPPFLAGS+=-I$(SRC_DIR) -isystem $(GTEST_DIR)
CXXFLAGS+=-pthread

# Unfortunately, even the non-gfx game code depends on SDL
# for functions like SDL_assert().
LDLIBS+=-lpthread $$(sdl2-config --libs)

TEST_O=test_block.o test_replay.o mock.o

.all: $(TESTBIN)
.PHONY: game.a clean run

# gtest_main.a is the compiled archive of googletest functions
# including a main() definition. It must be provided manually.
$(TESTBIN): $(TEST_O) game.a gtest_main.a
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# game.a is the main game project compiled into an archive.
# This job is done by the main makefile found in SRC_DIR.
# SRC_DIR must be set correctly.
# Once the symlink exists, it is not touched again.
game.a:
	$(MAKE) -C $(SRC_DIR) game.a
	ln -s $(SRC_DIR)/game.a 2>/dev/null || true

test_block.o: test_block.cpp mock.hpp
test_replay.o: test_replay.cpp
mock.o: mock.cpp mock.hpp

clean:
	rm -f $(TESTBIN) $(TEST_O) game.a

run:
	(cd .. && tests/$(TESTBIN))

CXXFLAGS=-g -Wall
BINDIR=../bin
VISUAL_BIN=$(BINDIR)/visualdemo
SRC_DIR=../src

CPPFLAGS+=-I$(SRC_DIR)
CXXFLAGS+=-pthread

# Unfortunately, even the non-gfx game code depends on SDL
# for functions like SDL_assert().
LDLIBS+=-lpthread $$(sdl2-config --libs) -lSDL2_image

VISUAL_O=visualdemo.o

.all: $(VISUAL_BIN)
.PHONY: $(SRC_DIR)/game.a clean run

# gtest_main.a is the compiled archive of googletest functions
# including a main() definition. It must be provided manually.
$(VISUAL_BIN): $(VISUAL_O) game.a | $(BINDIR)
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

visualdemo.o: visualdemo.cpp $(wildcard $(SRC_DIR)/*.?pp)

clean:
	rm -f $(VISUAL_BIN) $(VISUAL_O) game.a

run:
	(cd .. && visualdemo/$(VISUAL_BIN))

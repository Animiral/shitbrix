CXXFLAGS=-g -Wall
LDLIBS+=$$(sdl2-config --libs) -lSDL2_image
GAME_O=globals.o sdl_helper.o replay.o input.o stage.o draw.o director.o screen.o
BINDIR=../bin
BIN=$(BINDIR)/shitbrix
GAME_A=game.a
CXXFLAGS+=-std=c++1z

.all: $(BIN)

# order-only prerequisite, requires GNU make 3.80 or higher!
$(BIN): main.o $(GAME_A) | $(BINDIR)
	$(CXX) $(LDFLAGS) -o $@ main.o $(GAME_A) $(LDLIBS)

$(BINDIR):
	mkdir $@

# This archive of all game implementation code except main
# is created for testing purposes. See the separate test project.
$(GAME_A): $(GAME_O)
	ar rcs $@ $^

globals.o: globals.cpp globals.hpp
sdl_helper.o: sdl_helper.cpp sdl_helper.hpp globals.hpp
replay.o: replay.cpp replay.hpp globals.hpp
input.o: input.cpp input.hpp globals.hpp replay.hpp
stage.o: stage.cpp stage.hpp context.hpp globals.hpp
draw.o: draw.cpp draw.hpp context.hpp stage.hpp globals.hpp gameevent.hpp
director.o: director.cpp director.hpp globals.hpp stage.hpp context.hpp gameevent.hpp
screen.o: screen.cpp screen.hpp director.hpp globals.hpp input.hpp stage.hpp context.hpp replay.hpp draw.hpp gameevent.hpp
main.o: main.cpp globals.hpp stage.hpp context.hpp sdl_context.hpp asset.hpp sdl_helper.hpp director.hpp screen.hpp draw.hpp gameevent.hpp
# main.o needs less?

clean:
	rm -f $(BIN) $(GAME_A) $(GAME_O) main.o

run:
	(cd .. && src/$(BIN))

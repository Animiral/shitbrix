CXXFLAGS=-g -Wall
LDLIBS+=$$(sdl2-config --libs) -lSDL2_image
GAME_O=globals.o sdl_helper.o replay.o input.o context.o block.o director.o screen.o
BIN=../bin/shitbrix
GAME_A=game.a
CXXFLAGS+=-std=c++1z

.all: $(BIN) $(GAME_A)

$(BIN): $(GAME_O) main.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# This archive of all game implementation code except main
# is created for testing purposes. See the separate test project.
$(GAME_A): $(GAME_O)
	ar rcs $@ $^

globals.o: globals.cpp globals.hpp
sdl_helper.o: sdl_helper.cpp sdl_helper.hpp globals.hpp
replay.o: replay.cpp replay.hpp globals.hpp
input.o: input.cpp input.hpp globals.hpp replay.hpp
context.o: context.cpp context.hpp globals.hpp
block.o: block.cpp block.hpp context.hpp globals.hpp
director.o: director.cpp director.hpp globals.hpp block.hpp context.hpp
screen.o: screen.cpp screen.hpp director.hpp globals.hpp input.hpp block.hpp context.hpp replay.hpp
main.o: main.cpp globals.hpp block.hpp context.hpp asset.hpp sdl_helper.hpp director.hpp screen.hpp
# main.o needs less?

clean:
	rm -f $(BIN) $(GAME_O) main.o

run:
	(cd .. && src/$(BIN))

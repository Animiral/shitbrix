CXXFLAGS+=-g -Wall -std=c++1z
LDFLAGS+=$$(sdl2-config --libs) -lSDL2_image
BIN=../bin/shitbrix

.all: $(BIN)

$(BIN): globals.o block.o director.o main.o
	$(CXX) $(LDFLAGS) -o $@ $^

globals.o: globals.cpp globals.hpp
block.o: block.cpp block.hpp context.hpp globals.hpp
director.o: director.cpp director.hpp globals.hpp block.hpp context.hpp
main.o: main.cpp globals.hpp block.hpp context.hpp asset.hpp sdl_helper.hpp director.hpp

clean:
	rm -f $(BIN) globals.o block.o director.o main.o

run:
	(cd .. && src/$(BIN))

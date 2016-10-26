CXXFLAGS+=-g -Wall -std=c++1z
LDLIBS+=$$(sdl2-config --libs) -lSDL2_image
BIN=../bin/shitbrix

.all: $(BIN)

$(BIN): globals.o sdl_helper.o context.o block.o director.o screen.o main.o
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

globals.o: globals.cpp globals.hpp
sdl_helper.o: sdl_helper.cpp sdl_helper.hpp globals.hpp
context.o: context.cpp context.hpp globals.hpp
block.o: block.cpp block.hpp context.hpp globals.hpp
director.o: director.cpp director.hpp globals.hpp block.hpp context.hpp
screen.o: screen.cpp screen.hpp director.hpp globals.hpp block.hpp context.hpp
main.o: main.cpp globals.hpp block.hpp context.hpp asset.hpp sdl_helper.hpp director.hpp screen.hpp
# main.o needs less?

clean:
	rm -f $(BIN) globals.o sdl_helper.o context.o block.o director.o screen.o main.o

run:
	(cd .. && src/$(BIN))

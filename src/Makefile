CXXFLAGS+=-g -Wall -std=c++1z
LDFLAGS+=$$(sdl2-config --libs) -lSDL2_image
BIN=../bin/shitbrix

.all: $(BIN)

$(BIN): main.o
	$(CXX) $(LDFLAGS) -o $@ $<

main.o: main.cpp shitbrix.hpp block.hpp context.hpp asset.hpp sdl_helper.hpp
	$(CXX) -c $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(BIN) main.o

run:
	(cd .. && src/$(BIN))

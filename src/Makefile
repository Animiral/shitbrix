CXXFLAGS+=-g -Wall
LDFLAGS+=$$(sdl2-config --libs) -lSDL2_image

.all: shitbrix

shitbrix: main.o
	g++ $(LDFLAGS) -o $@ $<

main.o: main.cpp shitbrix.hpp block.hpp context.hpp asset.hpp sdl_helper.hpp
	g++ -c $(CXXFLAGS) -o $@ $<

clean:
	rm -f shitbrix main.o

run:
	(cd .. && bin/shitbrix)

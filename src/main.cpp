#include "game_loop.hpp"

int main(int argc, const char* argv[])
{
	Options options(argc, argv);
	GameLoop loop(options);
	loop.game_loop();
	return 0;
}

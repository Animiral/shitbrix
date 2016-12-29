#include "screen.hpp"
#include "sdl_context.hpp"
#include <iostream>
#include <algorithm>
#include <ctime>
#include <chrono>
#include <iomanip>

class Options
{

public:
	Options(int argc, const char* argv[])
		: m_replay_file(str_option(argc, argv, "--replay"))
	{
	}

	const char* replay_file() const { return m_replay_file; }

private:
	const char* m_replay_file;

	// Minimalistic opts parsing from http://stackoverflow.com/questions/865668/how-to-parse-command-line-arguments-in-c
	const char* str_option(int argc, const char* argv[], const std::string& option)
	{
		auto end = argv + argc;
	    const char** itr = std::find(argv, end, option);
	    if (itr != end && ++itr != end)
	    {
	        return *itr;
	    }
	    return nullptr;
	}

	bool bool_option(int argc, const char* argv[], const std::string& option)
	{
		auto end = argv + argc;
	    return std::find(argv, end, option) != end;
	}

};

namespace
{

/**
 * Returns the default name of the file where the replay of this session will
 * be stored. This default name is built from the current date and time.
 */
std::string make_journal_file()
{
	using clock = std::chrono::system_clock;
	auto now = clock::now();
	std::time_t time_now = clock::to_time_t(now);
	auto ltime = std::localtime(&time_now);
	std::ostringstream stream;
	stream << std::put_time(ltime, "replay/%Y-%m-%d_%H-%M.txt");
	return stream.str();
}

}

/**
 * Top-level class which owns general application resources such as the initialized SDL library
 * and offers the main loop function.
 */
class Main
{

public:

	Main(Options options)
		: m_options(std::move(options)),
		  context(),
		  game_screen(m_options.replay_file(), make_journal_file().c_str(), context),
		  keyboard(game_screen)
	{
	}

	/**
	 * Main loop.
	 * Design goals are:
	 *  - Renders as many frames as possible
	 *  - Does not fall behind on game logic
	 *  - Handles inputs and events fast
	 *  - Frequently yields CPU to other programs in need
	 * 
	 * Speed is controlled by FPS (frames per second) and TPS (logic ticks per second) in shitbrix.hpp.
	 */
	void game_loop()
	{
		Uint64 t0 = SDL_GetPerformanceCounter(); // start of game time
		Uint64 freq = SDL_GetPerformanceFrequency();
		long tick = 0; // current logic tick counter
		Uint64 next_logic = t0 + freq / TPS; // time for next logic update

		while(!game_screen.done()) {
			// draw frames as long as logic is up to date
			Uint64 now = SDL_GetPerformanceCounter();
			while(now < next_logic) {
				float fraction = 1.0f - static_cast<float>((next_logic-now) * TPS) / freq;
				SDL_assert((fraction >= 0) && (fraction <= 1));

				game_screen.draw(fraction);
				context.render();
				now = SDL_GetPerformanceCounter();
			}

			keyboard.poll();

			// run one frame of local logic
			game_screen.update();

			tick++;
			next_logic = t0 + (tick+1) * freq / TPS;
		}
	}

private:

	Options m_options;
	SdlContext context;
	GameScreen game_screen;
	Keyboard keyboard;

};

int main(int argc, const char* argv[])
{
	Options options(argc, argv);
	Main main(options);
	main.game_loop();
	return 0;
}

#pragma once

#include "screen.hpp"
#include "draw.hpp"
#include "audio.hpp"
#include <string>

/**
 * Parses command-line options.
 */
class Options
{

public:

	Options(int argc, const char* argv[]);
	const char* replay_file() const;

private:

	const char* m_replay_file;

	const char* str_option(int argc, const char* argv[], const std::string& option);
	bool bool_option(int argc, const char* argv[], const std::string& option);

};

/**
 * Top-level class which owns general application resources such as the initialized SDL library
 * and offers the main loop function.
 */
class GameLoop
{

public:

	GameLoop(Options options);

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
	void game_loop();

private:

	Options m_options;
	SdlFactory m_factory;
	Assets m_assets;
	DrawGame m_draw;
	Audio m_sound;
	GameScreen m_screen;
	Keyboard m_keyboard;

};

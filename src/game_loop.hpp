#pragma once

#include "screen.hpp"
#include "draw.hpp"
#include "audio.hpp"
#include "options.hpp"

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
	SdlFactory m_sdl_factory;
	Assets m_assets;
	DrawGame m_draw;
	Audio m_audio;
	ScreenFactory m_screen_factory;
	std::unique_ptr<IScreen> m_screen;
	Keyboard m_keyboard;

};

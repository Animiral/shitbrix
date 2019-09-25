#pragma once

#include "screen.hpp"
#include "draw.hpp"

/**
 * Top-level class which owns general application resources such as the initialized SDL library
 * and offers the main loop function.
 */
class GameLoop
{

public:

	GameLoop();

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

	// resources
	InputDevices m_input_devices;

	// network host
	std::unique_ptr<ServerThread> m_server;

	// screens (instance if active, nullptr otherwise)
	ScreenFactory m_screen_factory;
	IScreen* m_screen; //!< currently active screen

};

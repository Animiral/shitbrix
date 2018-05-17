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

	// resources
	Options m_options;
	Assets m_assets;
	Audio m_audio;
	Keyboard m_keyboard;

	// network hosts
	//std::unique_ptr<ServerThread> m_server;
	std::unique_ptr<ENetClient> m_client;

	// screens (instance if active, nullptr otherwise)
	ScreenFactory m_screen_factory;
	IScreen* m_screen; //!< currently active screen
	std::unique_ptr<IScreen> m_server_screen;
	std::unique_ptr<IScreen> m_menu_screen;
	std::unique_ptr<IScreen> m_game_screen;
	std::unique_ptr<IScreen> m_transition_screen;

	// debug screens
	std::unique_ptr<IScreen> m_pink_screen; //!< pink screen when active, nullptr otherwise
	std::unique_ptr<IScreen> m_creme_screen; //!< creme screen when active, nullptr otherwise

	/**
	 * Switch the screen object to a successor. Destroy the previous one.
	 */
	void next_screen();

};

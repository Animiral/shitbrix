#include "game_loop.hpp"
#include "replay.hpp"
#include "arbiter.hpp"
#include "game.hpp"
#include "error.hpp"
#include "context.hpp"
#include "configuration.hpp"
#include <fstream>
#include <random> // DEBUG

GameLoop::GameLoop()
: m_input_devices(),
  m_screen_factory(the_context),
  m_screen(m_screen_factory.create_default())
{
	const Configuration& configuration = *the_context.configuration;

	// configure player control
	if(configuration.player_number.has_value()) {
		const int player_number = *configuration.player_number;
		if(2 <= player_number) {
			throw GameException("Cannot control player "
				+ std::to_string(player_number)
				+ ". More than two players are currently not yet supported.");
		}
		m_input_devices.set_player_number(player_number);
	}

	// attach joystick input
	if(configuration.joystick_number.has_value()) {
		const int joystick_number = *configuration.joystick_number;
		const int joysticks_count = SDL_NumJoysticks();
		if(joystick_number < 0 || joystick_number >= joysticks_count) {
			throw GameException("Joystick "
				+ std::to_string(joystick_number)
				+ " not found. There are "
				+ std::to_string(joysticks_count) + " joysticks.");
		}
		JoystickPtr joystick(SDL_JoystickOpen(joystick_number));
		sdlok(joystick.get());
		m_input_devices.set_joystick(std::move(joystick));
	}
}

void GameLoop::game_loop()
{
	Uint64 t0 = SDL_GetPerformanceCounter(); // start of game time
	Uint64 freq = SDL_GetPerformanceFrequency();
	long tick = 0; // current logic tick counter
	Uint64 next_logic = t0 + freq / TPS; // time for next logic update

	while (m_screen) {
		// draw frames as long as logic is up to date
		Uint64 now = SDL_GetPerformanceCounter();
		while (now < next_logic) {
			float fraction = 1.0f - static_cast<float>((next_logic - now) * TPS) / freq;
			assert(fraction >= 0);
			assert(fraction <= 1);

			m_screen->draw(fraction);
			now = SDL_GetPerformanceCounter();

			// yield CPU if we have the time
			if(now < next_logic) {
				Uint64 wait = (next_logic - now) * 1000L / freq; // in ms
				assert(wait <= std::numeric_limits<Uint32>::max());
				SDL_Delay(static_cast<Uint32>(wait));
				now = SDL_GetPerformanceCounter();
			}
		}

		// get different sources of input
		const auto inputs = m_input_devices.poll();
		for(auto i : inputs) {
			// Debug functionality: take control of a certain player.
			// F2 key: take control of player 0
			// F3 key: take control of player 1
			if(Button::DEBUG2 == i.button && ButtonAction::DOWN == i.action)
				m_input_devices.set_player_number(0);
			else if(Button::DEBUG3 == i.button && ButtonAction::DOWN == i.action)
				m_input_devices.set_player_number(1);

			m_screen->input(i);
		}

		// run one frame of local logic
		m_screen->update();

		if(m_screen->done()) {
			m_screen = m_screen_factory.create_next(*m_screen);
			t0 = SDL_GetPerformanceCounter();
			tick = 0;
			next_logic = t0 + freq / TPS;
		}
		else {
			tick++;
			next_logic = t0 + (tick + 1) * freq / TPS;
		}
	}

	Log::info("Game exit.");
}

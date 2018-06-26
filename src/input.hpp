/**
 * input.hpp
 * Functions for converting user actions into game actions.
 */
#pragma once

#include "globals.hpp"
#include "sdl_helper.hpp"
#include <optional>
#include <vector>

/**
 * The @InputDevices read inputs from the available devices: keyboard and joysticks.
 *
 * By default, this keyboard collects the following inputs:
 *  * \[RETURN]: reset key
 *  * \[ESC]: quit key
 *  * Player 1: arrow keys + [Z]
 *  * Player 2: numpad 8456 + [0]
 *  * [D], [H]: toggle debug view on pits
 *
 * The keys can currently not be remapped.
 * BUG: This implementation reads all SDL events, when it can only handle
 *      those which are inputs. Might need refactoring.
 */
class InputDevices
{

public:

	void set_player_number(std::optional<int> player_number) { m_player_number = player_number; }
	void set_joystick(JoystickPtr joystick) { m_joystick = std::move(joystick); }

	/**
	 * Read inputs from the keyboard buffer and return them in order.
	 */
	std::vector<ControllerInput> poll();

private:

	std::optional<int> m_player_number;
	JoystickPtr m_joystick; //!< Optional SDL joystick object
	Uint8 m_joy_hat = SDL_HAT_CENTERED; //!< last known joystick hat position

};

/**
 * Translate a controller input to a game input.
 * Not all controller inputs are game inputs.
 * If the controller input cannot be mapped, return an empty optional.
 */
std::optional<GameInput> controller_to_game(ControllerInput input) noexcept;

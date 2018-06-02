/**
 * Definitions for input classes.
 */

#include "input.hpp"
#include "error.hpp"
#include <SDL2/SDL.h>

namespace
{

ControllerInput key_to_controller(SDL_Keycode key, Uint8 state, std::optional<int> default_player)
{
	// default assignments for the left- and right-hand key sets
	const int player0 = default_player.has_value() ? *default_player : 0;
	const int player1 = default_player.has_value() ? *default_player : 1;

	int player = NOONE;
	Button button = Button::NONE;

	switch(key) {
		// player 0 default keys
		case SDLK_LEFT:  player = player0; button = Button::LEFT;  break;
		case SDLK_RIGHT: player = player0; button = Button::RIGHT; break;
		case SDLK_UP:    player = player0; button = Button::UP;    break;
		case SDLK_DOWN:  player = player0; button = Button::DOWN;  break;
		case SDLK_z:     player = player0; button = Button::A;     break;
		case SDLK_x:     player = player0; button = Button::B;     break;

		// player 1 default keys
		case SDLK_KP_4: case SDLK_j: player = player1; button = Button::LEFT;  break;
		case SDLK_KP_6: case SDLK_l: player = player1; button = Button::RIGHT; break;
		case SDLK_KP_8: case SDLK_i: player = player1; button = Button::UP;    break;
		case SDLK_KP_5: case SDLK_k: player = player1; button = Button::DOWN;  break;
		case SDLK_KP_0: case SDLK_g: player = player1; button = Button::A;     break;
		case SDLK_KP_1: case SDLK_h: player = player1; button = Button::B;     break;

		// debug keys
		case SDLK_F1:     button = Button::DEBUG1; break;
		case SDLK_F2:     button = Button::DEBUG2; break;
		case SDLK_F3:     button = Button::DEBUG3; break;
		case SDLK_F4:     button = Button::DEBUG4; break;
		case SDLK_F5:     button = Button::DEBUG5; break;

		// control keys
		case SDLK_RETURN: button = Button::RESET;  break;
		case SDLK_SPACE:  button = Button::PAUSE;  break;
		case SDLK_ESCAPE: button = Button::QUIT;   break;
	}

	ButtonAction action = state == SDL_RELEASED ? ButtonAction::UP : ButtonAction::DOWN;

	return ControllerInput { player, button, action };
}

}


std::vector<ControllerInput> InputDevices::poll()
{
	// default player for input if we do not have anyone assigned
	const int player0 = m_player_number.has_value() ? *m_player_number : 0;

	std::vector<ControllerInput> buffer;
	SDL_Event event;

	while(SDL_PollEvent(&event)) {
		switch(event.type) {

		case SDL_QUIT: // overrides all other inputs
			return {ControllerInput{NOONE, Button::QUIT, ButtonAction::DOWN}};

		case SDL_KEYUP:
		case SDL_KEYDOWN:
			if(!event.key.repeat) {
				ControllerInput input = key_to_controller(event.key.keysym.sym, event.key.state, m_player_number);

				// with function keys, we only care about press, not release
				if(NOONE == input.player && ButtonAction::UP == input.action)
					break;

				if(input.button != Button::NONE)
					buffer.push_back(input);
			}
			break;

		case SDL_JOYHATMOTION:
		{
			// TODO: find the mapping from the joystick to the player number
			if(SDL_JoystickInstanceID(m_joystick.get()) != event.jhat.which)
				break;

			const Uint8 hat_up = m_joy_hat & ~event.jhat.value;
			if(hat_up & SDL_HAT_LEFT)  buffer.push_back({player0, Button::LEFT,  ButtonAction::UP});
			if(hat_up & SDL_HAT_RIGHT) buffer.push_back({player0, Button::RIGHT, ButtonAction::UP});
			if(hat_up & SDL_HAT_UP)    buffer.push_back({player0, Button::UP,    ButtonAction::UP});
			if(hat_up & SDL_HAT_DOWN)  buffer.push_back({player0, Button::DOWN,  ButtonAction::UP});

			const Uint8 hat_down = event.jhat.value & ~m_joy_hat;
			if(hat_down & SDL_HAT_LEFT)  buffer.push_back({player0, Button::LEFT,  ButtonAction::DOWN});
			if(hat_down & SDL_HAT_RIGHT) buffer.push_back({player0, Button::RIGHT, ButtonAction::DOWN});
			if(hat_down & SDL_HAT_UP)    buffer.push_back({player0, Button::UP,    ButtonAction::DOWN});
			if(hat_down & SDL_HAT_DOWN)  buffer.push_back({player0, Button::DOWN,  ButtonAction::DOWN});
		}
			break;

		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
		{
			const Button button{static_cast<Button>(static_cast<int>(Button::A) + event.jbutton.button)};
			const ButtonAction action = event.type == SDL_JOYBUTTONDOWN ? ButtonAction::DOWN : ButtonAction::UP;
			buffer.push_back({player0, button, action});
		}
			break;

		}
	}

	return buffer;
}


std::optional<GameInput> controller_to_game(ControllerInput input) noexcept
{
	switch(input.button) {
		case Button::LEFT:
		case Button::RIGHT:
		case Button::UP:
		case Button::DOWN:
		case Button::A:
		case Button::B:
			GameInput ginput;
			ginput.game_time = GameInput::TIME_ASAP;
			ginput.player = input.player; // TODO: properly map dev to player
			ginput.button = static_cast<GameButton>(input.button);
			ginput.action = input.action;
			return ginput;

		default:
			return {};
	}
}

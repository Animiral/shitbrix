/**
 * Definitions for input classes.
 */

#include "input.hpp"
#include <SDL2/SDL.h>

namespace
{

ControllerInput key_to_controller(SDL_Keycode key, Uint8 state)
{
	int player = NOONE;
	Button button;

	switch(key) {
		// player 0 default keys
		case SDLK_LEFT:  player = 0; button = Button::LEFT;  break;
		case SDLK_RIGHT: player = 0; button = Button::RIGHT; break;
		case SDLK_UP:    player = 0; button = Button::UP;    break;
		case SDLK_DOWN:  player = 0; button = Button::DOWN;  break;
		case SDLK_z:     player = 0; button = Button::A;     break;
		case SDLK_x:     player = 0; button = Button::B;     break;

		// player 1 default keys
		case SDLK_KP_4: case SDLK_j: player = 1; button = Button::LEFT;  break;
		case SDLK_KP_6: case SDLK_l: player = 1; button = Button::RIGHT; break;
		case SDLK_KP_8: case SDLK_i: player = 1; button = Button::UP;    break;
		case SDLK_KP_5: case SDLK_k: player = 1; button = Button::DOWN;  break;
		case SDLK_KP_0: case SDLK_g: player = 1; button = Button::A;     break;
		case SDLK_KP_1: case SDLK_h: player = 1; button = Button::B;     break;

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
		default:          button = Button::NONE;   break;
	}

	ButtonAction action = state == SDL_RELEASED ? ButtonAction::UP : ButtonAction::DOWN;

	return ControllerInput { player, button, action };
}

}

void Keyboard::poll()
{
	SDL_assert(m_sink);

	SDL_Event event;

	while(SDL_PollEvent(&event)) {
		switch(event.type) {

		case SDL_QUIT:
			m_sink->input(ControllerInput{NOONE, Button::QUIT, ButtonAction::DOWN});
			break;

		case SDL_KEYUP:
		case SDL_KEYDOWN:
			if(!event.key.repeat) {
				ControllerInput input = key_to_controller(event.key.keysym.sym, event.key.state);

				// with function keys, we only care about press, not release
				if(NOONE == input.player && ButtonAction::UP == input.action)
					break;

				if(input.button != Button::NONE)
					m_sink->input(input);
			}
			break;
		}
	}
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

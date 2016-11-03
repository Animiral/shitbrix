/**
 * Definitions for input classes.
 */

#include "input.hpp"
#include <SDL2/SDL.h>

namespace
{

ControllerInput key_to_controller(SDL_Keycode key)
{
	int player = NOONE;
	Button button;

	switch(key) {
		case SDLK_LEFT:  player = 0; button = Button::LEFT;  break;
		case SDLK_RIGHT: player = 0; button = Button::RIGHT; break;
		case SDLK_UP:    player = 0; button = Button::UP;    break;
		case SDLK_DOWN:  player = 0; button = Button::DOWN;  break;
		case SDLK_z:     player = 0; button = Button::A;     break;
		case SDLK_KP_4:  player = 1; button = Button::LEFT;  break;
		case SDLK_KP_6:  player = 1; button = Button::RIGHT; break;
		case SDLK_KP_8:  player = 1; button = Button::UP;    break;
		case SDLK_KP_5:  player = 1; button = Button::DOWN;  break;
		case SDLK_KP_0:  player = 1; button = Button::A;     break;
		case SDLK_d:      button = Button::DEBUG1; break;
		case SDLK_h:      button = Button::DEBUG2; break;
		case SDLK_RETURN: button = Button::RESET;  break;
		case SDLK_ESCAPE: button = Button::QUIT;   break;
		default:          button = Button::NONE;   break;
	}

	return ControllerInput { player, button };
}

}

void Keyboard::poll()
{
	SDL_Event event;

	while(SDL_PollEvent(&event)) {
		switch(event.type) {

		case SDL_QUIT:
			m_sink.input(ControllerInput{NOONE, Button::QUIT});
			break;

		case SDL_KEYDOWN:
			if(!event.key.repeat) {
				ControllerInput input = key_to_controller(event.key.keysym.sym);

				if(input.button != Button::NONE)
					m_sink.input(input);
			}
			break;
		}
	}
}

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
		case SDLK_F1:     button = Button::DEBUG1; break;
		case SDLK_F2:     button = Button::DEBUG2; break;
		case SDLK_F3:     button = Button::DEBUG3; break;
		case SDLK_F4:     button = Button::DEBUG4; break;
		case SDLK_F5:     button = Button::DEBUG5; break;
		case SDLK_RETURN: button = Button::RESET;  break;
		case SDLK_SPACE:  button = Button::PAUSE;  break;
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


GameInputMixer::GameInputMixer(IReplaySink& replay_sink, const char* replay_file)
: m_game_sink(nullptr), m_replay_sink(replay_sink)
{
	if(replay_file) {
		replay_stream.open(replay_file);
		replay = std::make_unique<Replay>(replay_stream);
		*replay >> next_event;
	}
}

void GameInputMixer::input(ControllerInput input)
{
	if(replay) return; // do not accept regular inputs during replay
	if(NOONE == input.player) return; // do not accept non-player inputs
	if(!m_game_sink) return; // no recipient registered

	switch(input.button) {
		case Button::LEFT:
		case Button::RIGHT:
		case Button::UP:
		case Button::DOWN:
		case Button::A:
		case Button::B:
			GameInput ginput;
			ginput.player = input.player; // TODO: properly map dev to player
			ginput.button = static_cast<GameButton>(input.button);
			m_game_sink->input(ginput);
			break;

		// These buttons should not be used by any actual player nr
		case Button::PAUSE:
		case Button::RESET:
		case Button::QUIT:
		case Button::DEBUG1:
		case Button::DEBUG2:
		case Button::NONE:
		default:
			SDL_assert_paranoid(false);
	}
}

void GameInputMixer::update(long game_time)
{
	if(replay) {
		while(*replay &&                        // replay stream is intact
		      !replay->eof() &&                 // more input is available
		      game_time >= next_event.time()) { // it is time to handle this
			if(m_game_sink && ReplayEvent::Type::INPUT == next_event.type()) {
				m_game_sink->input(next_event.input());
			}

			m_replay_sink.handle(next_event);

			if(ReplayEvent::Type::START == next_event.type()) {
				game_time = 0;
			}

			*replay >> next_event;
		}
	}
}

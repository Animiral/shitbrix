/**
 * Definitions for input classes.
 */

#include <sstream>
#include <string>
#include <cassert>
#include "input.hpp"
#include "error.hpp"
#include <SDL.h>

namespace
{

ControllerAction key_to_controller(SDL_Keycode key, Uint8 state, std::optional<int> default_player)
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

	return ControllerAction { player, button, action };
}

}


std::string PlayerInput::to_string() const
{
	std::ostringstream ss;
	ss << game_time << " " << player << " "
	   << game_button_to_string(button) << " "
	   << button_action_to_string(action);
	return ss.str();
}

PlayerInput PlayerInput::from_string(std::string input_string)
{
	std::istringstream tokenizer(input_string);
	long game_time;
	int player;
	std::string button_str;
	std::string action_str;

	tokenizer >> game_time >> player >> button_str >> action_str;
	if(!tokenizer)
		throwx<GameException>("Invalid PlayerInput string: \"%s\"", input_string.c_str());

	GameButton button = string_to_game_button(button_str);
	ButtonAction action = string_to_button_action(action_str);

	return PlayerInput{game_time, player, button, action};
}

std::string SpawnBlockInput::to_string() const
{
	std::ostringstream ss;
	ss << game_time << " " << player << " " << row;

	for(int i = 0; i < PIT_COLS; i++)
		ss << " " << color_to_string(colors[i]);

	return ss.str();
}

SpawnBlockInput SpawnBlockInput::from_string(std::string input_string)
{
	std::istringstream tokenizer(input_string);
	SpawnBlockInput result;
	std::string color_source;

	tokenizer >> result.game_time >> result.player >> result.row;

	for(auto it = result.colors.begin(); result.colors.end() != it; ++it) {
		tokenizer >> color_source;
		*it = string_to_color(color_source);
	}

	if(!tokenizer)
		throwx<GameException>("Invalid SpawnBlockInput string: \"%s\"", input_string.c_str());

	return result;
}

std::string SpawnGarbageInput::to_string() const
{
	std::ostringstream ss;
	ss << game_time << " " << player << " " << rows << " " << columns;

	for(int i = 0; i < loot.size(); i++)
		ss << " " << color_to_string(loot[i]);

	return ss.str();
}

SpawnGarbageInput SpawnGarbageInput::from_string(std::string input_string)
{
	std::istringstream tokenizer(input_string);
	SpawnGarbageInput result;
	std::string color_source;

	tokenizer >> result.game_time >> result.player >> result.rows >> result.columns;

	if(result.columns <= 0 || result.columns > PIT_COLS || result.rows <= 0)
		throwx<GameException>("Invalid SpawnGarbageInput size: \"%dr * %dc\"",
			result.rows, result.columns);

	result.loot.resize((size_t)result.rows * (size_t)result.columns);
	for(int i = 0; i < result.rows * result.columns; i++) {
		tokenizer >> color_source;
		result.loot[i] = string_to_color(color_source);
	}

	if(!tokenizer)
		throwx<GameException>("Invalid SpawnGarbageInput string: \"%s\"", input_string.c_str());

	return result;
}


Input::Input(PlayerInput input) noexcept : m_impl(std::move(input)) {}
Input::Input(SpawnBlockInput input) noexcept : m_impl(std::move(input)) {}
Input::Input(SpawnGarbageInput input) noexcept : m_impl(std::move(input)) {}

Input::Input(std::string source)
{
	// The source string starts with a prefix describing the type of input.
	std::istringstream tokenizer(source);
	std::string type_name;
	tokenizer >> type_name >> std::ws;

	// What remains is the type-specific data of the particular input.
	std::string input_string;
	std::getline(tokenizer, input_string);

	if("PlayerInput" == type_name)
		m_impl = PlayerInput::from_string(input_string);
	else if("SpawnBlockInput" == type_name)
		m_impl = SpawnBlockInput::from_string(input_string);
	else if("SpawnGarbageInput" == type_name)
		m_impl = SpawnGarbageInput::from_string(input_string);
	else
		throwx<GameException>("Invalid Input string: \"%s\"", source.c_str());
}

bool Input::operator==(const Input& rhs) const noexcept
{
	if(rhs.m_impl.index() != m_impl.index())
		return false;

	if(const PlayerInput* pi = std::get_if<PlayerInput>(&m_impl)) {
		const PlayerInput& rpi = std::get<PlayerInput>(rhs.m_impl);
		return rpi.player == pi->player &&
			rpi.game_time == pi->game_time &&
			rpi.button == pi->button &&
			rpi.action == pi->action;
	}
	else if(const SpawnBlockInput* bi = std::get_if<SpawnBlockInput>(&m_impl)) {
		const SpawnBlockInput& rbi = std::get<SpawnBlockInput>(rhs.m_impl);
		return rbi.player == bi->player &&
			rbi.game_time == bi->game_time &&
			rbi.row == bi->row &&
			rbi.colors == bi->colors;
	}
	else if(const SpawnGarbageInput* gi = std::get_if<SpawnGarbageInput>(&m_impl)) {
		const SpawnGarbageInput& rgi = std::get<SpawnGarbageInput>(rhs.m_impl);
		return rgi.player == gi->player &&
			rgi.game_time == gi->game_time &&
			rgi.rows == gi->rows &&
			rgi.columns == gi->columns &&
			rgi.loot == gi->loot;
	}
	else
		return assert(false), false;
}

Input::operator std::string() const
{
	if(const PlayerInput* pi = std::get_if<PlayerInput>(&m_impl))
		return "PlayerInput " + pi->to_string();
	else if(const SpawnBlockInput* bi = std::get_if<SpawnBlockInput>(&m_impl))
		return "SpawnBlockInput " + bi->to_string();
	else if(const SpawnGarbageInput* gi = std::get_if<SpawnGarbageInput>(&m_impl))
		return "SpawnGarbageInput " + gi->to_string();
	else
		return assert(false), "";
}

long Input::game_time() const
{
	const auto get_time = [](auto&& i) -> long { return i.game_time; };
	return std::visit(get_time, m_impl);
}


std::vector<ControllerAction> InputDevices::poll()
{
	// default player for input if we do not have anyone assigned
	const int player1 = m_player_number.has_value() ? *m_player_number : 1;

	std::vector<ControllerAction> buffer;
	SDL_Event event;

	while(SDL_PollEvent(&event)) {
		switch(event.type) {

		case SDL_QUIT: // overrides all other inputs
			return {ControllerAction{NOONE, Button::QUIT, ButtonAction::DOWN}};

		case SDL_KEYUP:
		case SDL_KEYDOWN:
			if(!event.key.repeat) {
				ControllerAction input = key_to_controller(event.key.keysym.sym, event.key.state, m_player_number);

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
			if(hat_up & SDL_HAT_LEFT)  buffer.push_back({player1, Button::LEFT,  ButtonAction::UP});
			if(hat_up & SDL_HAT_RIGHT) buffer.push_back({player1, Button::RIGHT, ButtonAction::UP});
			if(hat_up & SDL_HAT_UP)    buffer.push_back({player1, Button::UP,    ButtonAction::UP});
			if(hat_up & SDL_HAT_DOWN)  buffer.push_back({player1, Button::DOWN,  ButtonAction::UP});

			const Uint8 hat_down = event.jhat.value & ~m_joy_hat;
			if(hat_down & SDL_HAT_LEFT)  buffer.push_back({player1, Button::LEFT,  ButtonAction::DOWN});
			if(hat_down & SDL_HAT_RIGHT) buffer.push_back({player1, Button::RIGHT, ButtonAction::DOWN});
			if(hat_down & SDL_HAT_UP)    buffer.push_back({player1, Button::UP,    ButtonAction::DOWN});
			if(hat_down & SDL_HAT_DOWN)  buffer.push_back({player1, Button::DOWN,  ButtonAction::DOWN});
		}
			break;

		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
		{
			const Button button{static_cast<Button>(static_cast<int>(Button::A) + event.jbutton.button)};
			const ButtonAction action = event.type == SDL_JOYBUTTONDOWN ? ButtonAction::DOWN : ButtonAction::UP;
			buffer.push_back({player1, button, action});
		}
			break;

		}
	}

	return buffer;
}


std::optional<PlayerInput> controller_to_input(ControllerAction input) noexcept
{
	switch(input.button) {
		case Button::LEFT:
		case Button::RIGHT:
		case Button::UP:
		case Button::DOWN:
		case Button::A:
		case Button::B:
			PlayerInput ginput;
			ginput.game_time = PlayerInput::TIME_ASAP;
			ginput.player = input.player; // TODO: properly map dev to player
			ginput.button = static_cast<GameButton>(input.button);
			ginput.action = input.action;
			return ginput;

		default:
			return {};
	}
}

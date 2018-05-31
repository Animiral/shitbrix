#include "globals.hpp"
#include <fstream>
#include <sstream>
#include <cassert>
#include "error.hpp"

Gfx operator+(Gfx gfx, int delta)
{
	return static_cast<Gfx>(static_cast<int>(gfx) + delta);
}

BlockFrame& operator++(BlockFrame& frame)
{
	return frame = static_cast<BlockFrame>(static_cast<size_t>(frame) + 1);
}

namespace
{

const char* gamebutton_string[] =
{"none", "left", "right", "up", "down", "swap", "raise"};

}

const char* game_button_to_string(GameButton button) noexcept
{
	const size_t button_index = static_cast<size_t>(button);
	assert(button_index < std::size(gamebutton_string));
	return gamebutton_string[button_index];
}

GameButton string_to_game_button(const std::string& button_string)
{
	const auto button_found = std::find(gamebutton_string, std::end(gamebutton_string), button_string);
	const size_t button_index = std::distance(gamebutton_string, button_found);

	if(std::size(gamebutton_string) <= button_index)
		throw GameException("Invalid game button string");

	return static_cast<GameButton>(button_index);
}

const char* button_action_to_string(ButtonAction action) noexcept
{
	switch(action) {
		case ButtonAction::UP: return "release";
		case ButtonAction::DOWN: return "press";
		default: assert(false); return nullptr;
	}
}

ButtonAction string_to_button_action(const std::string& str)
{
	if("release" == str) return ButtonAction::UP;
	else if("press" == str) return ButtonAction::DOWN;
	else throw GameException("Invalid button action string");
}

std::string GameInput::to_string() const
{
	std::ostringstream ss;
	ss << game_time << " " << player << " "
	   << game_button_to_string(button) << " "
	   << button_action_to_string(action);
	return ss.str();
}

GameInput GameInput::from_string(std::string str)
{
	std::istringstream tokenizer(str);
	int game_time;
	int player;
	std::string button_str;
	std::string action_str;

	tokenizer >> game_time >> player >> button_str >> action_str;
	if(!tokenizer)
		throw GameException("Invalid GameInput string");

	GameButton button = string_to_game_button(button_str);
	ButtonAction action = string_to_button_action(action_str);

	return GameInput{game_time, player, button, action};
}

std::string GameMeta::to_string() const
{
	std::ostringstream ss;
	ss << players << " " << seed << " " << winner;
	return ss.str();
}

GameMeta GameMeta::from_string(std::string str)
{
	std::istringstream tokenizer(str);
	int players;
	unsigned seed;
	int winner;

	tokenizer >> players >> seed >> winner;
	if(!tokenizer)
		throw GameException("Invalid GameMeta string");

	return GameMeta{players, seed, winner};
}

Point from_rc(RowCol rc)
{
	return Point{static_cast<float>(rc.c*BLOCK_W), static_cast<float>(rc.r*BLOCK_H)};
}

std::ostream& operator<<(std::ostream& stream, RowCol rc)
{
	return stream << "{r" << rc.r << "c" << rc.c << "}";
}

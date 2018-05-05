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

const char* game_button_to_string(GameButton button) noexcept
{
	switch(button) {
		case GameButton::NONE: return "none";
		case GameButton::LEFT: return "left";
		case GameButton::RIGHT: return "right";
		case GameButton::UP: return "up";
		case GameButton::DOWN: return "down";
		case GameButton::SWAP: return "swap";
		case GameButton::RAISE: return "raise";
		default: assert(false); return nullptr;
	}
}

GameButton string_to_game_button(const std::string& str)
{
	if("left" == str) return GameButton::LEFT;
	else if("right" == str) return GameButton::RIGHT;
	else if("up" == str) return GameButton::UP;
	else if("down" == str) return GameButton::DOWN;
	else if("swap" == str) return GameButton::SWAP;
	else if("raise" == str) return GameButton::RAISE;
	else throw GameException("Invalid game button string");
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
	if(tokenizer.bad())
		throw GameException("Invalid GameInput string");

	GameButton button = string_to_game_button(button_str);
	ButtonAction action = string_to_button_action(action_str);

	return GameInput{game_time, player, button, action};
}

Point from_rc(RowCol rc)
{
	return Point{static_cast<float>(rc.c*BLOCK_W), static_cast<float>(rc.r*BLOCK_H)};
}

std::ostream& operator<<(std::ostream& stream, RowCol rc)
{
	return stream << "{r" << rc.r << "c" << rc.c << "}";
}

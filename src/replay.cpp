/**
 * Definitions for replay facilities.
 */

#include "replay.hpp"
#include <sstream>
#include <cctype>
#include <SDL2/SDL_assert.h>

ReplayEvent ReplayEvent::make_set(std::string name, std::string value) noexcept
{
	ReplayEvent event;
	event.type = Type::SET;
	event.set_name = move(name);
	event.set_value = value;
	return event;
}

ReplayEvent ReplayEvent::make_start() noexcept
{
	ReplayEvent event;
	event.type = Type::START;
	return event;
}

ReplayEvent ReplayEvent::make_input(GameInput input) noexcept
{
	ReplayEvent event;
	event.type = Type::INPUT;
	event.input = input;
	return event;
}

ReplayEvent ReplayEvent::make_end() noexcept
{
	ReplayEvent event;
	event.type = Type::END;
	return event;
}

namespace
{

const char* replay_event_type_string(ReplayEvent::Type type) noexcept
{
	switch(type) {
		case ReplayEvent::Type::SET: return "set";
		case ReplayEvent::Type::START: return "start";
		case ReplayEvent::Type::INPUT: return "input";
		case ReplayEvent::Type::END: return "end";
		default: SDL_assert_paranoid(false); return nullptr;
	}
}

const char* game_button_string(GameButton button) noexcept
{
	switch(button) {
		case GameButton::NONE: return "none";
		case GameButton::LEFT: return "left";
		case GameButton::RIGHT: return "right";
		case GameButton::UP: return "up";
		case GameButton::DOWN: return "down";
		case GameButton::SWAP: return "swap";
		case GameButton::RAISE: return "raise";
		default: SDL_assert_paranoid(false); return nullptr;
	}
}

const char* button_action_string(ButtonAction action) noexcept
{
	switch(action) {
		case ButtonAction::UP: return "up";
		case ButtonAction::DOWN: return "down";
		default: SDL_assert_paranoid(false); return nullptr;
	}
}

ReplayEvent::Type string_to_replay_event_type(const std::string& str)
{
	if("set" == str) return ReplayEvent::Type::SET;
	else if("start" == str) return ReplayEvent::Type::START;
	else if("input" == str) return ReplayEvent::Type::INPUT;
	else if("end" == str) return ReplayEvent::Type::END;
	else throw GameException("Invalid event type string.");
}

GameButton string_to_game_button(const std::string& str)
{
	if("left" == str) return GameButton::LEFT;
	else if("right" == str) return GameButton::RIGHT;
	else if("up" == str) return GameButton::UP;
	else if("down" == str) return GameButton::DOWN;
	else if("swap" == str) return GameButton::SWAP;
	else if("raise" == str) return GameButton::RAISE;
	else throw GameException("Invalid game button string.");
}

ButtonAction string_to_button_action(const std::string& str)
{
	if("up" == str) return ButtonAction::UP;
	else if("down" == str) return ButtonAction::DOWN;
	else throw GameException("Invalid button action string.");
}

}

Journal& Journal::operator<<(const ReplayEvent event)
{
	const ReplayEvent::Type event_type = event.type;

	m_stream << replay_event_type_string(event_type);

	switch(event_type) {
		case ReplayEvent::Type::SET:
			m_stream << " " << event.set_name << " " << event.set_value;
			break;

		case ReplayEvent::Type::INPUT:
			{
				m_stream << " " << event.input.game_time
				         << " " << event.input.player
				         << " " << game_button_string(event.input.button)
				         << " " << button_action_string(event.input.action);
			}
			break;

		case ReplayEvent::Type::START:
		case ReplayEvent::Type::END:
			break; // no parameters
	}

	m_stream << "\n";
	return *this;
}

void replay_read(std::istream& stream, IReplaySink& sink)
{
	while(stream && (stream.eof() || stream.peek(), !stream.eof())) {
		std::string line;

		if(!std::getline(stream, line)) {
			throw GameException("Failed to read from replay.");
		}

		ReplayEvent event;
		std::string type_str;
		std::istringstream tokenizer(line);
		tokenizer >> type_str;

		ReplayEvent::Type type = string_to_replay_event_type(type_str);

		switch(type) {

		case ReplayEvent::Type::SET:
			{
				std::string set_name;
				std::string set_value;
				tokenizer >> set_name;

				while(std::isspace(tokenizer.peek()))
					tokenizer.ignore();

				std::getline(tokenizer, set_value);
				event = ReplayEvent::make_set(set_name, set_value);
			}
			break;

		case ReplayEvent::Type::START:
			event = ReplayEvent::make_start();
			break;

		case ReplayEvent::Type::INPUT:
			{
				int game_time;
				int player;
				std::string button_str;
				std::string action_str;

				tokenizer >> game_time >> player >> button_str >> action_str;
				GameButton button = string_to_game_button(button_str);
				ButtonAction action = string_to_button_action(action_str);
				GameInput input{game_time, player, button, action};

				event = ReplayEvent::make_input(input);
			}
			break;

		case ReplayEvent::Type::END:
			event = ReplayEvent::make_end();
			break;

		default:
			SDL_assert_paranoid(false);

		}

		sink.handle(event);
	}

	if(stream.bad())
		throw GameException("Something went wrong in reading from replay.");
}

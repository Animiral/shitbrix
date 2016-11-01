/**
 * Definitions for replay facilities.
 */

#include "replay.hpp"
#include <sstream>
#include <cctype>
#include <SDL2/SDL_assert.h>

ReplayEvent ReplayEvent::make_set(std::string name, std::string value)
{
	ReplayEvent event;
	event.m_time = 0;
	event.m_type = Type::SET;
	event.m_set_name = name;
	event.m_set_value = value;

	return event;
}

ReplayEvent ReplayEvent::make_start()
{
	ReplayEvent event;
	event.m_time = 0;
	event.m_type = Type::START;
	
	return event;
}

ReplayEvent ReplayEvent::make_input(int time, GameInput input)
{
	ReplayEvent event;
	event.m_time = time;
	event.m_type = Type::INPUT;
	event.m_input = input;
	
	return event;
}

ReplayEvent ReplayEvent::make_end(int time)
{
	ReplayEvent event;
	event.m_time = time;
	event.m_type = Type::END;
	
	return event;
}

const char* replay_event_type_string(ReplayEvent::Type type)
{
	switch(type) {
		case ReplayEvent::Type::SET: return "set";
		case ReplayEvent::Type::START: return "start";
		case ReplayEvent::Type::INPUT: return "input";
		case ReplayEvent::Type::END: return "end";
		default: SDL_assert_paranoid(false); return nullptr;
	}
}

const char* game_button_string(GameButton button)
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

namespace
{

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

}

Journal& Journal::operator<<(ReplayEvent event)
{
	ReplayEvent::Type event_type = event.type();

	m_stream << event.time() << " " << replay_event_type_string(event_type);

	switch(event_type) {
		case ReplayEvent::Type::SET:
			m_stream << " " << event.set_name() << " " << event.set_value();
			break;

		case ReplayEvent::Type::INPUT:
			{
				int player = event.input().player;
				GameButton button = event.input().button;
				m_stream << " " << player << " " << game_button_string(button);
			}
			break;

		case ReplayEvent::Type::START:
		case ReplayEvent::Type::END:
			break; // no parameters
	}

	m_stream << "\n";
	return *this;
}

Replay& Replay::operator>>(ReplayEvent& event)
{
	if(m_stream) {
		std::string line;
		std::getline(m_stream, line);

		int time;
		std::string type_str;
		std::istringstream tokenizer(line);
		tokenizer >> time >> type_str;

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
				int player;
				std::string button_str;
				tokenizer >> player >> button_str;
				GameButton button = string_to_game_button(button_str);
				GameInput input{player, button};
				event = ReplayEvent::make_input(time, input);
			}
			break;

		case ReplayEvent::Type::END:
			event = ReplayEvent::make_end(time);
			break;

		default:
			SDL_assert_paranoid(false);

		}
	}
	else {
		throw GameException("Failed to read from replay.");
	}

	return *this;
}

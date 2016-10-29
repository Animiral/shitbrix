/**
 * Definitions for replay facilities.
 */

#include "replay.hpp"
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

ReplayEvent ReplayEvent::make_input(int time, int player, PlayerInput input)
{
	ReplayEvent event;
	event.m_time = time;
	event.m_type = Type::INPUT;
	event.m_player = player;
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

const char* player_input_string(PlayerInput input)
{
	switch(input) {
		case PlayerInput::NONE: return "none";
		case PlayerInput::LEFT: return "left";
		case PlayerInput::RIGHT: return "right";
		case PlayerInput::UP: return "up";
		case PlayerInput::DOWN: return "down";
		case PlayerInput::SWAP: return "swap";
		case PlayerInput::RAISE: return "raise";
		default: SDL_assert_paranoid(false); return nullptr;
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
			m_stream << " " << event.player() << " " << player_input_string(event.input());
			break;

		case ReplayEvent::Type::START:
		case ReplayEvent::Type::END:
			break; // no parameters
	}

	m_stream << "\n";
	return *this;
}

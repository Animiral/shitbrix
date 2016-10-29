/**
 * replay.hpp
 * This module provides facilities for writing and reading replays.
 * A replay is a text file that describes the course of a game round.
 * Replays consist of a series of ReplayEvents.
 * Replay files are written by Journal objects.
 */
#pragma once

#include "globals.hpp"
#include <string>
#include <ostream>

class ReplayEvent
{

public:

	enum class Type { SET, START, INPUT, END };

	int time() const { return m_time; }
	Type type() const { return m_type; }
	std::string set_name() const { return m_set_name; }
	std::string set_value() const { return m_set_value; }
	int player() const { return m_player; }
	PlayerInput input() const { return m_input; }

	static ReplayEvent make_set(std::string name, std::string value);
	static ReplayEvent make_start();
	static ReplayEvent make_input(int time, int player, PlayerInput input);
	static ReplayEvent make_end(int time);

private:

	int m_time;              //!< Event time in game ticks
	Type m_type;
	std::string m_set_name;  //!< name of set target
	std::string m_set_value; //!< value to set to
	int m_player;            //!< Input from this player
	PlayerInput m_input;     //!< Input key

};

const char* replay_event_type_string(ReplayEvent::Type type);
const char* player_input_string(PlayerInput input);

class Journal
{

public:

	Journal(std::ostream& stream) : m_stream(stream) {}
	Journal& operator<<(ReplayEvent event);

private:

	std::ostream& m_stream;

};

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
	GameInput input() const { return m_input; }

	static ReplayEvent make_set(std::string name, std::string value);
	static ReplayEvent make_start();
	static ReplayEvent make_input(int time, GameInput input);
	static ReplayEvent make_end(int time);

private:

	int m_time;              //!< Event time in game ticks
	Type m_type;
	std::string m_set_name;  //!< name of set target
	std::string m_set_value; //!< value to set to
	GameInput m_input;       //!< Input player and key

};

const char* replay_event_type_string(ReplayEvent::Type type);
const char* game_button_string(GameButton button);

/**
 * Writes a replay file event by event.
 */
class Journal
{

public:

	Journal(std::ostream& stream) : m_stream(stream) {}
	Journal& operator<<(ReplayEvent event);

private:

	std::ostream& m_stream;

};

/**
 * Reads through a replay file event by event.
 */
class Replay
{

public:

	Replay(std::istream& stream) : m_stream(stream), m_bad(false) {}
	Replay& operator>>(ReplayEvent& event);
	bool bad() const { return m_bad; }
	operator bool() const { return static_cast<bool>(m_stream); }

private:

	std::istream& m_stream;
	bool m_bad;

};
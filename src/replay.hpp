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

struct ReplayEvent
{
	enum class Type { SET, START, INPUT, END };

	Type type;
	std::string set_name;  //!< name of set target
	std::string set_value; //!< value to set to
	GameInput input;       //!< Input player and key

	static ReplayEvent make_set(std::string name, std::string value) noexcept;
	static ReplayEvent make_start() noexcept;
	static ReplayEvent make_input(GameInput input) noexcept;
	static ReplayEvent make_end() noexcept;

};

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
 * A ReplaySink can handle replay events.
 */
class IReplaySink
{
	public: virtual void handle(const ReplayEvent& event) =0;
};

/**
 * Reads a replay file and sends event by event to the sink.
 */
void replay_read(std::istream& stream, IReplaySink& sink);

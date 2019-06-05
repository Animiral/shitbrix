/**
 * This module provides facilities for journals and replays.
 *
 * A journal is the record of a game round held in memory.
 * It can seek to the game state at any point in game time using checkpoints
 * and recorded events.
 *
 * A replay is a text file that describes the course of a game round.
 * Replays consist of a series of ReplayRecords.
 * Journals can be written to and read from replays.
 */
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <ostream>
#include "globals.hpp"
#include "state.hpp"
#include "input.hpp"

struct InputDiscovered { Input input; bool discovered; };
using Inputs = std::vector<InputDiscovered>;
using InputSpan = std::pair<Inputs::const_iterator, Inputs::const_iterator>;

/**
 * Represents one piece of information in the replay file.
 */
struct ReplayRecord
{
	enum class Type { START, META, INPUT };

	Type type;
	GameMeta meta; //!< Meta information
	std::optional<Input> input; //!< Input information

	/**
	 * Meta information specification.
	 */
	static ReplayRecord make_meta(GameMeta meta) noexcept;

	/**
	 * Start of one game replay.
	 * Replays continue until the next start or until end of file.
	 */
	static ReplayRecord make_start() noexcept;

	/**
	 * All kinds of input.
	 */
	static ReplayRecord make_input(Input input) noexcept;
};

/**
 * Keeps the game record.
 */
class Journal
{

public:

	explicit Journal(GameMeta meta, GameState state0);

	GameMeta meta() const noexcept { return m_meta; }

	//! Time value of “earliest undiscovered input” if there are no new inputs.
	static const long NO_UNDISCOVERED = std::numeric_limits<long>::max();

	/**
	 * Return the earliest time at which an input happens that has not yet been discovered
	 * through @c discover_inputs.
	 * If all inputs have been discovered, returns @c NO_UNDISCOVERED.
	 */
	long earliest_undiscovered() const noexcept { return m_earliest_undiscovered; }

	/**
	 * Return all inputs with time >= start_time and <= end_time.
	 * The inputs will be marked as discovered.
	 */
	InputSpan discover_inputs(long start_time, long end_time) noexcept;

	/**
	 * Simply return the list of inputs and do not discover anything.
	 */
	const Inputs& inputs() const noexcept { return m_inputs; }

	/**
	 * Add an input into the queue and mark it as undiscovered.
	 * All checkpoints made at or after the time of the input become obsolete.
	 */
	void add_input(Input input);

	/**
	 * Update the winner in the meta information.
	 */
	void set_winner(int winner) noexcept;

	/**
	 * Enter a checkpoint into the journal.
	 */
	void add_checkpoint(GameState&& checkpoint);

	/**
	 * Return the latest checkpoint state before the given time.
	 */
	const GameState& checkpoint_before(long game_time) const;

private:

	GameMeta m_meta;
	Inputs m_inputs; //!< all inputs ordered by time
	long m_earliest_undiscovered;
	std::vector<GameState> m_checkpoint; //!< checkpoints ordered by time

};

/**
 * Write a text representation of the journal to the stream.
 */
void replay_stream(std::ostream& stream, const Journal& journal);

/**
 * Write the journal to an automatically generated file name in the replay directory.
 * If the replay directory does not exist, do nothing.
 * This name is built from the current date and time.
 */
void replay_write(const Journal& journal);

/**
 * Reads a replay file and sends event by event to the sink.
 */
Journal replay_read(std::istream& stream);

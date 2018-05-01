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
#include <ostream>
#include "globals.hpp"
#include "stage.hpp"
#include "input.hpp"

struct ReplayRecord
{
	enum class Type { START, META, INPUT };

	Type type;
	GameMeta meta;   //!< Meta information
	GameInput input; //!< Input player and key

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
	 * Player input.
	 */
	static ReplayRecord make_input(GameInput input) noexcept;
};

struct InputDiscovered { GameInput input; bool discovered; };
using GameInputs = std::vector<InputDiscovered>;
using GameInputSpan = std::pair<GameInputs::const_iterator, GameInputs::const_iterator>;

/**
 * Keeps the game record.
 */
class Journal
{

public:

	explicit Journal(GameMeta meta, GameState&& state0);

	/**
	 * Return the game state as it was or will be at the specified time.
	 * Start by assigning the preceding checkpoint to the Pit and follow the
	 * inputs since then by feeding them into the m_sink, which must be set up
	 * to manipulate the passed pit.
	 * In the regular @c CHECKPOINT_INTERVAL, store the game state as a
	 * checkpoint. Invalidate all later checkpoints.
	 */
	//void reproduce(long target_time, GameState& state);

	GameMeta meta() const noexcept { return m_meta; }

	/**
	 * Return the earliest time at which an input happens that has not yet been discovered
	 * through @c discover_inputs.
	 * If all inputs have been discovered, returns the maximum time of any input + 1.
	 */
	long earliest_undiscovered() const noexcept { return m_earliest_undiscovered; }

	/**
	 * Return all inputs with time >= start_time and <= end_time.
	 * The inputs will be marked as discovered.
	 */
	GameInputSpan discover_inputs(long start_time, long end_time) noexcept;

	/**
	 * Simply return the list of inputs and do not discover anything.
	 */
	const GameInputs& inputs() const noexcept { return m_inputs; }

	/**
	 * Add an input into the queue and mark it as undiscovered.
	 */
	void add_input(GameInput input);

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

	static const long CHECKPOINT_INTERVAL = 5 * TPS;

	GameMeta m_meta;
	GameInputs m_inputs; //!< player inputs ordered by time
	long m_earliest_undiscovered;
	std::vector<GameState> m_checkpoint; //!< checkpoints ordered by time

};

/**
 * Write a replay file from a Journal.
 */
void replay_write(std::ostream& stream, const Journal& journal);

/**
 * Reads a replay file and sends event by event to the sink.
 */
Journal replay_read(std::istream& stream);

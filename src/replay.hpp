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

/**
 * A ReplaySink can handle replay events.
 */
class IReplaySink
{
	public: virtual void do_event(const ReplayRecord& event) = 0;
};

struct InputDiscovered { GameInput input; bool discovered; };
using GameInputs = std::vector<InputDiscovered>;

/**
 * Keeps the game record.
 */
class Journal : public IReplaySink
{

public:

	explicit Journal(GameMeta meta, IReplaySink* sink = nullptr);
	explicit Journal(GameMeta meta, GameState&& state0);

	/**
	 * Return the game state as it was or will be at the specified time.
	 * Start by assigning the preceding checkpoint to the Pit and follow the
	 * inputs since then by feeding them into the m_sink, which must be set up
	 * to manipulate the passed pit.
	 * In the regular @c CHECKPOINT_INTERVAL, store the game state as a
	 * checkpoint. Invalidate all later checkpoints.
	 */
	void reproduce(long target_time, GameState& state);

	void set_sink(IReplaySink* sink) noexcept { m_sink = sink; }

	GameMeta meta() const noexcept { return m_meta; }

	/**
	 * Activate all stored inputs at the specified time and replay them into
	 * the m_sink.
	 */
	void poll(long target_time) const;

	const GameInputs& inputs() const noexcept { return m_inputs; }

	const std::vector<ReplayRecord>& events() const noexcept { return m_events; }

	virtual void do_event(const ReplayRecord& event) override { m_events.push_back(event); }

	/**
	 * Add an input into the queue and mark it as undiscovered.
	 */
	void add_input(GameInput input);

	/**
	 * Update the winner in the meta information.
	 */
	void set_winner(int winner) noexcept;

private:

	static const long CHECKPOINT_INTERVAL = 5 * TPS;

	std::vector<ReplayRecord> m_events;
	std::vector<GameState> m_checkpoint;
	GameMeta m_meta;
	GameInputs m_inputs; //!< player inputs ordered by time
	IReplaySink* m_sink; //!< output for events on @poll, optional

};

/**
 * Write a replay file from a Journal.
 */
void replay_write(std::ostream& stream, const Journal& journal);

/**
 * Reads a replay file and sends event by event to the sink.
 */
Journal replay_read(std::istream& stream);

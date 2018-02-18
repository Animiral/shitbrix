/**
 * director.hpp
 */
#pragma once

#include "stage.hpp"
#include "gameevent.hpp"
#include "logic.hpp"
#include <SDL2/SDL_assert.h>
#include <algorithm>

/**
 * Maintains a sequence of block colors spawned deterministically out of an
 * initial seed. This allows us to see what color blocks to introduce next,
 * as well as reconstruct the whole history of spawned block colors for
 * replay and netplay purposes.
 */
class BlocksQueue
{

public:

	/**
	 * Construct the queue with the given seed, which deterministically produces
	 * the same block colors every time.
	 */
	explicit BlocksQueue(unsigned seed);

	BlocksQueue(const BlocksQueue& ) = default;
	BlocksQueue(BlocksQueue&& ) = default;

	/**
	 * Return the next color of a block coming out on the stack from below.
	 * The @c game_time will be recorded for reference.
	 */
	Block::Color next() noexcept;

	/**
	 * Start reading block colors from the specified @c index forward.
	 */
	void backtrack(size_t index) noexcept;

private:

	std::vector<Block::Color> m_record;
	std::minstd_rand m_generator;
	size_t m_index; //!< Current queue index (for backtracking)

};

/**
 * Spawns and removes stuff to and from the stage.
 * The BlockDirector implements game-logical interactions between objects which these
 * objects cannot handle on their own.
 * Exmaples are spawning and reaping, block collisions and making blocks fall when they lose support.
 * The BlockDirector does /not/ concern itself with pixel coordinates - it only thinks in block rows and columns.
 */
class BlockDirector
{

public:

	BlockDirector(Pit& pit, Logic& logic, BlocksQueue grow_queue);

	/**
	 * Set the handler for game events from this director.
	 */
	void set_handler(evt::IGameEvent& handler) { m_handler = &handler; }

	float panic() const noexcept { return static_cast<float>(m_panic) / PANIC_TIME; }
	float recovery() const noexcept { return static_cast<float>(m_recovery) / RECOVERY_TIME; }
	bool over() const { return m_over; }

	/**
	 * Run one tick of game logic over the game state.
	 */
	void update();

	/**
	 * Attempt to set the block or space at lrc to swap with the one to the right of it.
	 *
	 * The following conditions must be met for success:
	 *  - Both blocks must be in a swappable state. These are REST, SWAP, FALL, LAND.
	 *  - A block can swap with a space, but two spaces cannot be swapped.
	 *
	 * Returns true if the swap was successful, false otherwise.
	 */
	bool swap(RowCol lrc);

	/**
	 * Set the block raise mode. If raise mode is on, the pit will scroll upwards
	 * at an accelerated speed, revealing more block material in a short time.
	 * Raise mode will always last until the next whole row of blocks turns from
	 * preview to normal, even after just a short tap of the raise button.
	 */
	void set_raise(bool raise);

	void debug_spawn_garbage(int columns, int rows); // spawn some stuff to demo garbage

	bool debug_no_gameover = false;

private:

	Pit& pit;
	Logic& m_logic;
	evt::IGameEvent* m_handler;

	// TODO: All this is game state and belongs in a separate class to facilitate syncs and rollbacks.
	//       For simplicity, I can probably burden the Pit with them.
	int m_chain; //!< chain counter
	int m_recovery; //!< recover time pool; scrolling stops after a quality match
	int m_panic; //!< panic time pool; the player has this many ticks left until game over
	bool m_over; // whether the game is over (the player with this Director loses)
	bool m_raise; //!< whether the pit should scroll in new blocks as fast as possible
	BlocksQueue m_grow_queue; //< generator for blocks spawning from below

};

/**
 * An interface for user input during the game.
 * Applies directions to the cursor.
 */
class CursorDirector
{

public:

	CursorDirector(Pit& pit, Cursor& cursor)
	: pit(pit), m_cursor(cursor), m_handler(nullptr)
	{}

	/**
	 * Set the handler for game events from this director.
	 */
	void set_handler(evt::IGameEvent& handler) { m_handler = &handler; }

	Cursor& cursor() const { return m_cursor; }
	RowCol rc() const { return m_cursor.rc; }

	/**
	 * Moves the cursor one column in the specified direction, if possible.
	 * Dir::NONE is a special direction that prevents the cursor from
	 * scrolling out of bounds.
	 */
	void move(Dir dir);

private:

	Pit& pit;
	Cursor& m_cursor;
	evt::IGameEvent* m_handler;

};

/**
 * A game event handler that converts combos and chains into garbage spawns.
 * It is attached to a source BlockDirector as event handler. Every
 * combo and chain event that the director raises is converted into
 * garbage block dimensions. The garbage is then spawned / dropped
 * in a target pit (the pit of the other player).
 */
class GarbageThrow : public evt::IGameEvent
{

public:

	GarbageThrow(Pit& pit, BlocksQueue emerge_queue)
	: m_pit(pit), m_emerge_queue(std::move(emerge_queue))
	{}

	virtual void fire(evt::Match event) override;
	virtual void fire(evt::Chain event) override;

private:

	Pit& m_pit;
	BlocksQueue m_emerge_queue; //< generator for blocks spawning from dissolved garbage

	void spawn(int columns, int rows, bool right_side);

};

/**
 * This glue class connects combo and chain events reported by the director (logic)
 * with the BonusIndicator display class.
 */
class BonusThrow : public evt::IGameEvent
{

public:

	BonusThrow(BonusIndicator& indicator) : m_indicator(indicator) {}

	virtual void fire(evt::Match event) override;
	virtual void fire(evt::Chain event) override;

private:

	BonusIndicator& m_indicator;

};

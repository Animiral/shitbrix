/**
 * director.hpp
 */
#pragma once

#include "stage.hpp"
#include "gameevent.hpp"
#include <SDL2/SDL_assert.h>
#include <algorithm>
#include <set>

using RndGen = std::shared_ptr<std::mt19937>;

/**
 * Examines the pit for matching blocks from a sequence of “hot” blocks
 * which have just been moved or landed. They are passed to the MatchBuilder via ignite(). 
 * Returns all detected matching blocks (3 or more in a row from a hot block) in result().
 * The combo() specifies the number of blocks resolved at the same time.
 */
class MatchBuilder
{

private:

	/**
	 * Helper struct to enable a std::set of blocks and garbage
	 */
	struct PhysicalLess
	{
		bool operator()(const Physical& lhs, const Physical& rhs) const { return lhs.rc() < rhs.rc(); }
	};

public:

	using BlockSet = std::set<std::reference_wrapper<Block>, PhysicalLess>;
	using GarbageSet = std::set<std::reference_wrapper<Garbage>, PhysicalLess>;

	MatchBuilder(const Pit& pit) : pit(pit), m_chaining(false) {}

	void ignite(Block& block);
	const BlockSet& result() { return m_result; }

	/**
	 * For each block in the result set of the MatchBuilder,
	 * examine the adjacent locations for Garbage blocks.
	 * The set of garbage bricks found in this manner is marked
	 * and available via @ref touched_garbage.
	 */
	void find_touch_garbage();
	const GarbageSet& touched_garbage() const noexcept { return m_touched_garbage; }

	int combo() { return static_cast<int>(m_result.size()); }
	bool chaining() { return m_chaining; }

private:

	const Pit& pit;
	BlockSet m_result;
	bool m_chaining;
	GarbageSet m_touched_garbage;

	bool match_at(RowCol rc, Block::Color color);
	void insert(RowCol rc);

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

	BlockDirector(Pit& pit, RndGen rndgen)
	: pit(pit), m_handler(nullptr), m_chain(0), m_over(false), m_raise(false), rndgen(rndgen) {}

	/**
	 * Set the handler for game events from this director.
	 */
	void set_handler(evt::IGameEvent& handler) { m_handler = &handler; }

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
	evt::IGameEvent* m_handler;
	int m_chain; //!< chain counter
	bool m_over; // whether the game is over (the player with this Director loses)
	bool m_raise; //!< whether the pit should scroll in new blocks as fast as possible
	RndGen rndgen;     // block colors are generated randomly

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

	GarbageThrow(Pit& pit) : m_pit(pit) {}

	virtual void fire(evt::Match event) override;
	virtual void fire(evt::Chain event) override;

private:

	Pit& m_pit;

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

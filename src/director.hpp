/**
 * director.hpp
 */
#pragma once

#include "stage.hpp"
#include "gameevent.hpp"
#include "logic.hpp"
#include <algorithm>

/**
 * The BlockDirector implements game-logical interactions between objects which these
 * objects cannot handle on their own.
 * Examples are spawning and reaping, block collisions and making blocks fall when they lose support.
 * The BlockDirector does /not/ concern itself with pixel coordinates - it only thinks in block rows and columns.
 */
class BlockDirector
{

public:

	explicit BlockDirector(GameState& state);

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
	 * Attempt to initiate a swapping action at the player's current cursor coordinates.
	 *
	 * The following conditions must be met for success:
	 *  - Both blocks must be in a swappable state. These are REST, SWAP, FALL, LAND.
	 *  - A block can swap with a space, but two spaces cannot be swapped.
	 *
	 * Returns true if the swap was successful, false otherwise.
	 */
	bool swap(int player);

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

	/**
	 * Run one tick of game logic over one pit.
	 */
	void update_single(Pit& pit);

	std::reference_wrapper<GameState> m_state;
	evt::IGameEvent* m_handler;
	bool m_raise; //!< whether the pit should scroll in new blocks as fast as possible
	bool m_over;  //!< whether the game is over (the player with this Director loses)

};

/**
 * An interface for user input during the game.
 * Applies directions to the cursor.
 */
class CursorDirector
{

public:

	CursorDirector(Pit& pit)
	: m_pit(pit), m_handler(nullptr)
	{}

	/**
	 * Set the handler for game events from this director.
	 */
	void set_handler(evt::IGameEvent& handler) { m_handler = &handler; }

	RowCol rc() const { return m_pit.cursor().rc; }

	/**
	 * Moves the cursor one column in the specified direction, if possible.
	 * Dir::NONE is a special direction that prevents the cursor from
	 * scrolling out of bounds.
	 */
	void move(Dir dir);

private:

	Pit& m_pit;
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

	GarbageThrow(Pit& pit)
	: m_pit(pit)
	{}

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

class Journal;

///**
// * Assorted objects that are required on this screen once per player.
// */
//struct PlayerObjects
//{
//	PlayerObjects(Pit& pit, Pit& other_pit, BonusIndicator& bonus)
//	: logic(pit), block_director(pit, logic), cursor_director(pit),
//		event_hub(), garbage_throw(other_pit), bonus_throw(bonus)
//	{
//		block_director.set_handler(event_hub);
//		event_hub.subscribe(garbage_throw);
//		event_hub.subscribe(bonus_throw);
//	}
//
//	// default move would leave dangling references!
//	PlayerObjects(const PlayerObjects& ) = delete;
//	PlayerObjects(PlayerObjects&& ) = default;
//	PlayerObjects& operator=(const PlayerObjects& ) = delete;
//	PlayerObjects& operator=(PlayerObjects&& ) = delete;
//
//	Logic logic;
//	BlockDirector block_director;
//	CursorDirector cursor_director;
//	evt::GameEventHub event_hub;
//	GarbageThrow garbage_throw; // event handler for generating garbage bricks
//	BonusThrow bonus_throw; // event handler for displaying stars
//};

/*
 * The rules contain all the implementation objects
 * for advancing a game state for all players.
 */
struct Rules
{
	BlockDirector block_director;
	//CursorDirector cursor_director;
	//evt::GameEventHub event_hub;
	//GarbageThrow garbage_throw; // event handler for generating garbage bricks
	//BonusThrow bonus_throw; // event handler for displaying stars
};

void apply_input(Rules& rules, GameInput ginput);

/**
 * Bring the game state to the given @c target_time using the
 * @c journal for history and @director for logic.
 */
void synchronurse(GameState& state, long target_time, Journal& journal, Rules& rules);

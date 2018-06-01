/**
 * director.hpp
 */
#pragma once

#include "stage.hpp"
#include "gameevent.hpp"
#include "logic.hpp"

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

	int winner() const noexcept { return m_winner; }
	bool over() const noexcept { return NOONE != m_winner; }

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

	void debug_spawn_garbage(int columns, int rows); // spawn some stuff to demo garbage

	bool debug_no_gameover = false;

private:

	/**
	 * Run one tick of game logic over one player's pit.
	 */
	void update_single(int player);

	GameState* m_state;
	evt::IGameEvent* m_handler;
	int m_winner = NOONE; //!< number of the player who wins the game

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

/**
 * Bring the game state to the given @c target_time using the
 * @c journal for history and @director for logic.
 */
void synchronurse(GameState& state, long target_time, Journal& journal, Rules& rules);

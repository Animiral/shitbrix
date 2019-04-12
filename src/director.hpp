/**
 * director.hpp
 */
#pragma once

#include "stage.hpp"
#include "event.hpp"
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

	explicit BlockDirector();

	/**
	 * Set the game state handled by this director.
	 */
	void set_state(GameState& state) { m_state = &state; }

	/**
	 * Set the handler for game events from this director.
	 */
	void set_handler(evt::IEventObserver& handler) { m_handler = &handler; }

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

	/**
	 * Given the number of one player in the game, returns the target opponent.
	 * If the given player loses, the opponent wins.
	 * If the given player produces a combo or chain, the opponent receives garbage.
	 */
	int opponent(int player) const noexcept;

	GameState* m_state;
	evt::IEventObserver* m_handler;
	int m_winner = NOONE; //!< number of the player who wins the game

};

class Journal;

/*
 * The rules contain all the implementation objects
 * for advancing a game state for all players.
 */
struct Rules
{
	BlockDirector block_director; //!< game rules implementation
	evt::GameEventHub event_hub; //!< subscription service for game events
};

/**
 * Bring the game state to the @c target_time by calculation from the game
 * journal and the game rules.
 */
void synchronurse(GameState& state, long target_time, Journal& journal, Rules& rules);

/**
 * director.hpp
 */
#pragma once

#include "event.hpp"
#include "input.hpp"

class GameState;
class Input;
struct PlayerInput;
struct SpawnBlockInput;
struct SpawnGarbageInput;
class Journal;

/**
 * The Director implements high-level game-logical interactions between
 * objects which these objects cannot handle on their own.
 * Examples are spawning and reaping, block collisions and making blocks
 * fall when they lose support.
 *
 * These high-level interactions are implemented using lower-level pit
 * manipulation routines defined in the separate Logic class.
 *
 * The Director does /not/ concern itself with pixel coordinates.
 * It only thinks in block rows and columns and state timeouts.
 *
 * The Director only implements game rules as a consequence of various inputs.
 * It does not by itself take decisions based on random rolls or other external
 * influences. These decisions are made by the separate Arbiter class and
 * feed into the Director in the form of yet more inputs.
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
	 * Change the game state according to the given input.
	 *
	 * This function is used by our recalculation logic @c synchronurse to
	 * advance the game state to the current game time continuously as well as
	 * from a past checkpoint on-demand.
	 */
	void apply_input(const Input& input);

	void debug_spawn_garbage(int columns, int rows); // spawn some stuff to demo garbage

	bool debug_no_gameover = false;

private:

	/**
	 * Run one tick of game logic over one player's pit.
	 */
	void update_single(int player);

	/**
	 * Change the game state according to the given player input.
	 * For example, we move the cursor or change some blocks to the swapping state.
	 */
	void apply_input(const PlayerInput& ginput);

	/**
	 * Change the game state according to the given description of a preview
	 * block to be spawned.
	 * This kind of input arises out of a game situation in which the arbiter
	 * (e.g. server) decides that new blocks should be placed in the pit.
	 */
	void apply_input(const SpawnBlockInput& spinput);

	/**
	 * Change the game state according to the given description of a garbage
	 * brick to be spawned.
	 * This kind of input arises out of a game situation in which the arbiter
	 * (e.g. server) decides that a new garbage should be placed in the pit.
	 */
	void apply_input(const SpawnGarbageInput& spinput);

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

	GameState* m_state;
	evt::IEventObserver* m_handler;
	int m_winner = NOONE; //!< number of the player who wins the game

};


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

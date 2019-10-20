/**
 * Routines for an AI agent which plays the game by taking over the virtual
 * controls normally available to a human player.
 */
#pragma once

#include <vector>
#include "input.hpp"

// forward declarations
class Pit;
class GameState;

/**
 * A model of intent for the agent to perform a series of actions towards
 * a goal as laid out in the plan.
 *
 * The whole plan is just a list of more specific plans, each of which
 * addresses the need to get a specific block somewhere.
 */
class Plan
{

public:

	/**
	 * Models the intent to move a certain block, identified by its location
	 * and color, towards a goal coordinate.
	 */
	struct BlockPlan
	{
		RowCol block_rc; //!< coordinates of block to be moved
		Color block_color; //!< required block color
		RowCol goal; //!< coordinates to move the block to
	};

	using Steps = std::vector<RowCol>; //!< each step means to swap at the given coordinate

	/**
	 * Add the given plan for a single block to the overall plan.
	 */
	void add(BlockPlan plan);

	/**
	 * Find the next swap coordinate that will lead to executing all block plans.
	 *
	 * Among the available sub-goals, this function will choose one that is
	 * estimated to cost less distance and fewer actions to complete everything.
	 *
	 * @param cursor the current cursor position
	 */
	RowCol next_step(RowCol cursor) const;

	/**
	 * Update the Plan with the knowledge that blocks have been swapped at the
	 * specified coordinates. The Plan must then expect the blocks at the new
	 * coordinates.
	 */
	void notify_swapped(RowCol rc);

	/**
	 * Returns whether the Plan is sensible in the given pit.
	 *
	 * This means that all expectations about blocks are met. The blocks which
	 * are part of the plan must be found at the given coordinates and have
	 * their expected color.
	 */
	bool is_sensible(const Pit& pit) const noexcept;

	/**
	 * Returns whether the Plan is finished.
	 *
	 * Finished means that all blocks have arrived at their intended destinations
	 * and the sub-plans are left empty.
	 */
	bool is_finished() const noexcept;

private:

	std::vector<BlockPlan> m_blocks;

};

/**
 * The Agent continuously examines the game state for opportunities to achieve
 * winning moves.
 *
 * The implementation should be assumed to be planning. It is therefore
 * important to query the agent regularly (~1/tick) on a game state that is
 * roughly developing forward in time so that once it has set its plans,
 * it has a chance to execute them.
 *
 * The difficulty level of the agent can be configured via its *delay*, which
 * is the number of ticks that the agent will wait after making one move until
 * it makes another move.
 * Regardless of delay value, the agent is limited to one cursor movement per
 * tick and can only use any one button once per tick, either press or release.
 */
class Agent
{

public:

	/**
	 * Construct the agent to play on the specified settings.
	 *
	 * @param state game state object to base decisions on
	 * @param pit number of the pit under control of the agent
	 * @param delay to weaken the agent, it will only be permitted to move every N ticks
	 */
	explicit Agent(const GameState& state, int pit, int delay);

	std::vector<PlayerInput> move();

private:

	const GameState* m_state; //!< game state object to base decisions on
	int m_pit; //!< pit under control of the agent
	int m_delay; //!< enforced wait time between moves
	long m_last_time; //!< game state time of last generated move
	Plan m_plan; //!< current tactical aim of the agent's movement

	/**
	 * Examine the current pit state and find out some way to proceed.
	 */
	Plan make_plan();

	const int RAISE_BUFFER = 2; //!< number of rows left free when raising

};

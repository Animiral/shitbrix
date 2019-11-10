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
	 * Merge all steps from the other plan into this plan.
	 */
	void join(const Plan& rhs);

	/**
	 * Return the set of all block movements in this plan.
	 */
	const std::vector<BlockPlan>& block_plan() const noexcept;

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
 * This class gathers information about the blocks in the pit and which colors
 * it is possible to move to certain coordinates.
 *
 * Since blocks can only be moved horizontally and not through garbage bricks,
 * every space in the pit draws from a limited *pool* of possible colors.
 *
 * The pools are mutable to make it easier for the agent to consider questions
 * such as: “if I pick a green block from this pool to fill a target square,
 * is there another green block available from the same pool for the next
 * square?”
 *
 * This examination takes into account the limits of the cursor placement in
 * the pit (only what is on screen).
 *
 * On top of all that, we consider the possibility that blocks may fall down
 * in the future. As a result, the pool of a coordinate at or above a block
 * that is currently dissolving is given by the location further up, where
 * a falling block would have to be to land at the given coordinate.
 */
class MovePossiblity
{

public:

	/**
	 * Specifies that a specific color block is to be found at the given location.
	 */
	struct ColorCoord
	{
		Color color;
		RowCol rc;
	};

	/**
	 * Construct the object from the information in the target pit.
	 */
	explicit MovePossiblity(const Pit& pit);

	/**
	 * Return true if the given color can be sourced from the predicted
	 * pool associated with the given coordinate.
	 */
	bool is_available(RowCol where, Color color) const noexcept;

	/**
	 * Remove one available block of the given color from the predicted pool
	 * associated with the coordinate and return it.
	 *
	 * @return the location and color of the picked block
	 * @throw GameException if the color is not available.
	 */
	ColorCoord pick(RowCol where, Color color);

	/**
	 * Add one available block color/coord entry to the predicted pool
	 * associated with the coordinate.
	 */
	void put(RowCol where, ColorCoord entry);

private:

	using Pool = std::vector<ColorCoord>;

	/**
	 * Given a location in the pit, this function returns the location of the
	 * block that will fall in its place after currently dissolving blocks
	 * have disappeared.
	 *
	 * This is a requirement for accurately judging the
	 * available resources for a given location.
	 */
	RowCol prediction(const Pit& pit, RowCol where) const noexcept;

	/**
	 * Group together all blocks which can be moved among the same spaces into
	 * pools.
	 *
	 * Write the results to the @c m_pool field.
	 *
	 * @return v such that m_pool[v[translate_rc(rc)]] contains the available colors
	 */
	std::vector<size_t> make_pools(const Pit& pit);

	/**
	 * Find the index of every source @c m_pool for every reachable location.
	 *
	 * Write the results to the @c m_pool_at field.
	 */
	void map_pools(const Pit& pit, const std::vector<size_t>& pool_source);

	/**
	 * Return the index into the @c m_pool_at lookup vector for the specified
	 * coordinate.
	 *
	 * The length of the vector exactly covers the reachable pit, and each
	 * coordinate within the reachable area translates to one specific index.
	 */
	size_t translate_rc(RowCol rc) const;

	int m_top; //!< top reachable row in the pit
	int m_bottom; //!< bottom reachable row in the pit
	std::vector<Pool> m_pool; //!< all my pools, unsorted
	std::vector<size_t> m_pool_at; //!< index of the pool at the given translated rc

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

	/**
	 * Attempt to make a plan in which 3 blocks of the given color match
	 * at the given coordinates, under the given move possibilities.
	 *
	 * Out of bounds coordinates are tolerated but lead to no plan.
	 *
	 * @return a Plan if one exists or an empty optional otherwise
	 */
	std::optional<Plan> make_plan_match(MovePossiblity& moves, std::array<RowCol, 3> coords, Color color);

	/**
	 * Return the estimated value of executing the given plan.
	 *
	 * This includes a small cost for every block to move and a bonus for adjacent
	 * garbage cleared.
	 *
	 * @param plan set of moves in the plan execution
	 * @param coords location where the matching blocks should go
	 */
	int evaluate_plan(const Plan& plan, const std::array<RowCol, 3>& coords);

	const int RAISE_BUFFER = 2; //!< number of rows left free when raising

};

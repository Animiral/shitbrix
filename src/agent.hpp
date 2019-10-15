/**
 * Routines for an AI agent which plays the game by taking over the virtual
 * controls normally available to a human player.
 */
#pragma once

#include <vector>
#include "input.hpp"

// forward declarations
class GameState;

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

	std::vector<PlayerInput> move() const;

private:

	const GameState* m_state; //!< game state object to base decisions on
	int m_pit; //!< pit under control of the agent
	int m_delay; //!< enforced wait time between moves
	int m_cooldown; //!< remaining wait time between moves

};

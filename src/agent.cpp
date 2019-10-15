#include "agent.hpp"

Agent::Agent(const GameState& state, int pit, int delay)
	: m_state(&state), m_pit(pit), m_delay(delay), m_cooldown(0)
{}

std::vector<PlayerInput> Agent::move() const
{
	return {};
}

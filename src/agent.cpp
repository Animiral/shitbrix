#include "agent.hpp"
#include "state.hpp"
#include "error.hpp"

Agent::Agent(const GameState& state, int pit, int delay)
	: m_state(&state), m_pit(pit), m_delay(delay), m_last_time(-delay - 1)
{
	enforce(pit >= 0);
	enforce(pit < state.pit().size());
	enforce(delay >= 0);
}

std::vector<PlayerInput> Agent::move()
{
	if(m_state->game_time() <= m_last_time + m_delay)
		return {};

	const Pit& pit = *m_state->pit()[m_pit];
	const int time = m_state->game_time() + 1; // produce inputs for this time
	std::vector<PlayerInput> inputs;

	// control raise
	if(pit.want_raise()) {
		// stop raising?
		if(pit.peak() <= pit.top() + RAISE_BUFFER)
			inputs.push_back(PlayerInput{ time, m_pit, GameButton::RAISE, ButtonAction::UP});
	}
	else {
		if(pit.peak() > pit.top() + RAISE_BUFFER)
			inputs.push_back(PlayerInput{ time, m_pit, GameButton::RAISE, ButtonAction::DOWN });
	}

	// set up input delay
	if(!inputs.empty()) {
		m_last_time = m_state->game_time();
	}

	return inputs;
}

#include "agent.hpp"
#include "state.hpp"
#include "error.hpp"

namespace
{

/**
 * Return the next required swap location for the given @c BlockPlan.
 */
RowCol next_step_blockplan(const Plan::BlockPlan& b);

}

void Plan::add(const BlockPlan plan)
{
	enforce(plan.block_rc.r == plan.goal.r);
	enforce(Color::FAKE != plan.block_color);

	m_blocks.push_back(plan);
}

RowCol Plan::next_step(const RowCol cursor) const
{
	if(m_blocks.empty())
		throwx<GameException>("There is no next step in a finished plan.");

	const auto cost = [cursor](const BlockPlan& b)
	{
		const RowCol step = next_step_blockplan(b);
		return std::abs(cursor.r - step.r) + std::abs(cursor.c - step.c);
	};

	const auto less_cost = [&cost](const BlockPlan& a, const BlockPlan& b) { return cost(a) < cost(b); };

	const auto cheapest = std::min_element(m_blocks.begin(), m_blocks.end(), less_cost);
	return next_step_blockplan(*cheapest);
}

void Plan::notify_swapped(const RowCol rc)
{
	// update all plans which are affected by the swap
	for(BlockPlan& b : m_blocks) {
		if(b.block_rc.r == rc.r) {
			if(b.block_rc.c == rc.c) {
				b.block_rc.c++; // swap right
			}
			else if(b.block_rc.c + 1 == rc.c) {
				b.block_rc.c--; // swap left
			}
		}
	}

	// resolve finished plans
	const auto is_bp_finished = [](const BlockPlan& b) { return b.block_rc == b.goal; };
	const auto end = std::remove_if(m_blocks.begin(), m_blocks.end(), is_bp_finished);
	m_blocks.erase(end, m_blocks.end());
}

bool Plan::is_sensible(const Pit& pit) const noexcept
{
	const auto is_bp_sensible = [&pit](const BlockPlan& b)
	{
		const Block* block = pit.block_at(b.block_rc);
		return block && block->col == b.block_color;
	};

	return std::all_of(m_blocks.begin(), m_blocks.end(), is_bp_sensible);
}

bool Plan::is_finished() const noexcept
{
	return m_blocks.empty();
}


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

	// need a plan?
	if(m_plan.is_finished())
		m_plan = make_plan();

	// out of plans?
	if(m_plan.is_finished())
		return inputs; // wait for more

	// follow the plan
	const RowCol cursor = pit.cursor().rc;
	const RowCol next_step = m_plan.next_step(cursor);

	if(next_step.r < cursor.r) {
		inputs.push_back(PlayerInput{ time, m_pit, GameButton::UP, ButtonAction::DOWN });
	}
	else if(next_step.r > cursor.r) {
		inputs.push_back(PlayerInput{ time, m_pit, GameButton::DOWN, ButtonAction::DOWN });
	}
	else if(next_step.c < cursor.c) {
		inputs.push_back(PlayerInput{ time, m_pit, GameButton::LEFT, ButtonAction::DOWN });
	}
	else if(next_step.c > cursor.c) {
		inputs.push_back(PlayerInput{ time, m_pit, GameButton::RIGHT, ButtonAction::DOWN });
	}
	else {
		inputs.push_back(PlayerInput{ time, m_pit, GameButton::SWAP, ButtonAction::DOWN });
	}

	return inputs;
}

Plan Agent::make_plan()
{
	Plan plan;
	const Pit& pit = *m_state->pit()[m_pit];

	// rebalancing
	{
		std::array<int, PIT_COLS> peaks;
		peaks.fill(pit.top() - 1);

		// find the highest stack in every column
		for(int c = 0; c < PIT_COLS; c++) {
			for(int r = pit.bottom(); r >= pit.top(); r--) {
				if(!pit.at({ r, c })) {
					peaks[c] = r;
					break;
				}
			}
		}

		// rebalance all stacks which are off by more than 1 compared to neighbor
		for(int c = 0; c < PIT_COLS - 1; c++) {
			if(peaks[c + 1] - peaks[c] > 1) { // left stack is higher
				// rebalance lowest block to the right
				if(Block* block = pit.block_at({ peaks[c + 1], c })) {
					const RowCol block_rc = block->rc();
					const RowCol goal{ block_rc.r, block_rc.c + 1 };
					plan.add({ block_rc, block->col, goal });
				}
			}
			else if(peaks[c] - peaks[c + 1] > 1) { // right stack is higher
				// rebalance lowest block to the left
				if(Block* block = pit.block_at({ peaks[c], c + 1 })) {
					const RowCol block_rc = block->rc();
					const RowCol goal{ block_rc.r, block_rc.c - 1 };
					plan.add({ block_rc, block->col, goal });
				}
			}
		}
	}

	return plan;
}


namespace
{

RowCol next_step_blockplan(const Plan::BlockPlan& b)
{
	if(b.block_rc.c < b.goal.c) {
		return b.block_rc; // swap right
	}
	else {
		return RowCol{ b.block_rc.r, b.block_rc.c - 1 }; // swap left
	}
}

}

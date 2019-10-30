#include "agent.hpp"
#include "state.hpp"
#include "error.hpp"
#include <cassert>

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


MovePossiblity::MovePossiblity(const Pit& pit)
	: m_top(pit.top()), m_bottom(pit.bottom())
{
	m_pool.push_back({}); // empty pool at index 0

	// scout out the pool information
	auto pool_source = make_pools(pit);
	map_pools(pit, pool_source);
}

bool MovePossiblity::is_available(const RowCol where, const Color color) const noexcept
{
	const size_t index = translate_rc(where);
	const Pool& pool = m_pool[m_pool_at[index]];
	return pool.end() != std::find(pool.begin(), pool.end(), color);
}

void MovePossiblity::pick(const RowCol where, const Color color)
{
	const size_t index = translate_rc(where);
	Pool& pool = m_pool[m_pool_at[index]];
	const auto it = std::find(pool.begin(), pool.end(), color);

	if(pool.end() == it)
		throwx<GameException>("Cannot pick %s block from row around r%d c%d.", color_to_string(color).c_str(), where.r, where.c);

	pool.erase(it);
}

void MovePossiblity::put(const RowCol where, const Color color)
{
	const size_t index = translate_rc(where);
	Pool& pool = m_pool[m_pool_at[index]];
	pool.push_back(color);
}

RowCol MovePossiblity::prediction(const Pit& pit, const RowCol where) const noexcept
{
	// count breaking and empty blocks down for fall distance
	int fall = 0;

	for(RowCol scoop{ where.r + 1, where.c }; scoop.r <= pit.bottom(); scoop.r++) {
		const Physical* physical = pit.at(scoop);
		if(!physical || Physical::State::BREAK == physical->physical_state())
			fall++;

		const Garbage* garbage = dynamic_cast<const Garbage*>(physical);
		if(garbage && Physical::State::REST == garbage->physical_state())
			break; // we assume that resting garbage will not fall
	}

	// find enough blocks upward to fill the hole
	RowCol scoop;
	for(scoop = where; scoop.r >= pit.top() && fall > 0; scoop.r--) {
		if(const Physical* physical = pit.at(scoop)) {
			if(Physical::State::BREAK == physical->physical_state())
				continue; // skip this
			else
				fall--;
		}

		// NOTE: special treatment for falling garbage? nah
	}

	// not enough blocks to fill the hole?
	if(fall > 0)
		return { m_top - 1, where.c };

	// skip upwards past all currently breaking physicals
	while(scoop.r >= pit.top()) {
		const Physical* physical = pit.at(scoop);

		if(physical && Physical::State::BREAK == physical->physical_state())
			scoop.r--;
		else
			break;
	}

	return scoop;
}

std::vector<size_t> MovePossiblity::make_pools(const Pit& pit)
{
	const size_t pit_spaces = (m_bottom - m_top + 1) * PIT_COLS;
	std::vector<size_t> pool_source(pit_spaces, 0); // which pool can be immediately tapped from rc
	size_t current_pool = 0;
	std::vector<Pool>::iterator pool_it;

	for(int r = m_top; r <= m_bottom; r++) {
		for(int c = 0; c < PIT_COLS; c++) {
			const RowCol rc{ r,c };
			const Physical* physical = pit.at(rc);
			const Block* block = dynamic_cast<const Block*>(physical);

			// scoop all blocks from the row into the current pool as far as we can
			if(block && Block::State::BREAK != block->block_state()) {
				if(0 == current_pool) {
					current_pool = m_pool.size();
					pool_it = m_pool.insert(m_pool.end(), Pool{}); // new empty pool
				}

				pool_it->push_back(block->col);
			}
			else if(physical) {
				current_pool = 0; // space obstructed
			}

			pool_source[translate_rc(rc)] = current_pool;
		}

		current_pool = 0;
	}

	return pool_source;
}

void MovePossiblity::map_pools(const Pit& pit, const std::vector<size_t>& pool_source)
{
	const size_t pit_spaces = (m_bottom - m_top + 1) * PIT_COLS;
	m_pool_at.resize(pit_spaces, 0); // all pools default empty

	for(int r = m_top; r <= m_bottom; r++) {
		for(int c = 0; c < PIT_COLS; c++) {
			const RowCol rc{ r,c };
			const RowCol predicted = prediction(pit, rc);
			const size_t at_index = translate_rc(rc); // for pool entry

			if(predicted.r < m_top) {
				// cannot reach this tile with any blocks
				m_pool_at[at_index] = 0;
				continue;
			}

			const size_t predicted_index = translate_rc(predicted);
			m_pool_at[at_index] = pool_source[predicted_index];
		}
	}
}

size_t MovePossiblity::translate_rc(const RowCol rc) const
{
	assert(rc.r >= m_top);
	assert(rc.r <= m_bottom);
	assert(rc.c >= 0);
	assert(rc.c < PIT_COLS);

	return ((size_t)rc.r - (size_t)m_top) * PIT_COLS + (size_t)rc.c;
}


Agent::Agent(const GameState& state, const int pit, const int delay)
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
			if(peaks[c + 1ull] - peaks[c] > 1) { // left stack is higher
				// rebalance lowest block to the right
				if(Block* block = pit.block_at({ peaks[c + 1ull], c })) {
					const RowCol block_rc = block->rc();
					const RowCol goal{ block_rc.r, block_rc.c + 1 };
					plan.add({ block_rc, block->col, goal });
				}
			}
			else if(peaks[c] - peaks[c + 1ull] > 1) { // right stack is higher
				// rebalance lowest block to the left
				if(Block* block = pit.block_at({ peaks[c], c + 1 })) {
					const RowCol block_rc = block->rc();
					const RowCol goal{ block_rc.r, block_rc.c - 1 };
					plan.add({ block_rc, block->col, goal });
				}
			}
		}
	}

	//// matching (make only one plan for this at a time)
	//{
	//	std::vector<int> pool; // available
	//	const auto color_option = [&pit](const RowCol rc, const Color color)
	//	{
	//		for(const Block* block = pit.block_at(rc); block && Block::State::REST == block->block_state)
	//	};

	//	for(int c = 0; c < PIT_COLS; c++) {
	//		for(int r = pit.bottom(); r >= pit.top(); r++) {

	//		}
	//	}
	//}

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

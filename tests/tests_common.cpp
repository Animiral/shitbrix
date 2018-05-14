/**
 * Shared helper functions for unit tests.
 */

#include "tests_common.hpp"

Block::Color RainbowBlocksQueue::next() noexcept
{
	Block::Color previous = m_color;
	m_color = static_cast<Block::Color>((static_cast<int>(m_color) + 1 - 1) % 6 + 1);
	return previous;
}

void RainbowBlocksQueue::backtrack(size_t index) noexcept
{
	m_color = static_cast<Block::Color>(index % 6 + 1);
}

bool swap_at(Pit& pit, BlockDirector& director, RowCol rc)
{
	const_cast<Cursor&>(pit.cursor()).rc = rc;
	return director.swap(0);
}

/**
 * tests_common.hpp
 * Definitions for shared helpers for unit tests.
 */
#pragma once

#include "stage.hpp"
#include "director.hpp"

/**
 * Set the global context to use stub implementations for our test environment.
 */
void configure_context_for_testing();

/**
 * A block-spawning queue for testing that simply rotates through colors.
 */
class RainbowBlocksQueue : public IBlocksQueue
{

public:

	/**
	 * Return the next color of a block coming out on the stack from below.
	 */
	virtual Block::Color next() noexcept override;

	/**
	 * Start reading block colors from the specified @c index forward.
	 */
	virtual void backtrack(size_t index) noexcept override;

	virtual std::unique_ptr<IBlocksQueue> clone() const override { return std::make_unique<RainbowBlocksQueue>(*this); }

private:

	Block::Color m_color;

};

/**
 * Swap the blocks at the specified location, regardless of the current cursor position.
 * This is not normally allowed in the game (the cursor does not give random access).
 * @return success of the swapping, just like @c BlockDirector::swap.
 */
bool swap_at(Pit& pit, BlockDirector& director, RowCol rc);

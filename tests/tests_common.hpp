/**
 * tests_common.hpp
 * Definitions for shared helpers for unit tests.
 */
#pragma once

#include "stage.hpp"

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

/**
 * Tests for game-object behavior
 */

#include "stage.hpp"
#include "gtest/gtest.h"

/**
 * Tests whether a falling block correctly updates.
 */
TEST(BlockTest, Fall)
{
	// setup
	Block block(Block::Color::BLUE, RowCol{3,3}, Block::State::REST);
	block.set_state(Block::State::FALL, ROW_HEIGHT, FALL_SPEED);

	const int TICKS = 3; // block updates in this test

	for(int i = 0; i < TICKS; i++)
	{
		block.update();
	}

	EXPECT_FLOAT_EQ(float(ROW_HEIGHT) / FALL_SPEED - TICKS, block.eta());
}

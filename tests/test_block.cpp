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
	Block block(Block::Color::BLUE, RowCol{3,3}, Block::State::FALL);

	Point loc0 = block.loc(); // current location
	const int TICKS = 3; // block updates in this test

	for(int i = 0; i < TICKS; i++)
	{
		block.update();
	}

	EXPECT_EQ(loc0.y + TICKS * ROW_H * FALL_SPEED / ROW_HEIGHT, block.loc().y);
}

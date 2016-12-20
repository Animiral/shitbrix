/**
 * Tests for game-object behavior
 */

#include "stage.hpp"
#include "mock.hpp"
#include "gtest/gtest.h"

/**
 * Tests whether a falling block correctly updates.
 */
TEST(BlockTest, Fall)
{
	// setup
	auto context = MockContext();
	Block block(Block::Color::BLUE, RowCol{3,3}, Block::State::FALL);

	Point loc0 = block.loc(); // current location
	const int TICKS = 3; // block updates in this test

	for(int i = 0; i < TICKS; i++)
	{
		block.update(context);
	}

	EXPECT_EQ(loc0.y + TICKS * FALL_SPEED, block.loc().y);
}

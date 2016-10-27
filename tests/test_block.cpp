/**
 * Tests for game-object behavior
 */

#include "block.hpp"
#include "mock.hpp"
#include "gtest/gtest.h"

/**
 * Tests whether a falling block correctly updates.
 */
TEST(BlockTest, Fall)
{
	// setup
	auto context = MockContext();
	auto transform = std::make_shared<MockTransform>();
	BlockImpl block(BlockCol::BLUE, RowCol{3,3}, transform);
	block.set_state(BlockState::FALL);

	Point loc0 = block.loc(); // current location
	const int TICKS = 3; // block updates in this test

	for(int i = 0; i < TICKS; i++)
	{
		block.update(context);
	}

	EXPECT_EQ(loc0.y + TICKS * FALL_SPEED, block.loc().y);
}

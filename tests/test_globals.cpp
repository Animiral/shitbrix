/**
 * Tests for global utility functions
 */

#include "globals.hpp"
#include "gtest/gtest.h"

bool operator==(GameInput g, GameInput h) noexcept;


/**
 * Tests parsing of a GameInput from a string.
 */
TEST(GlobalsTest, ParseGameInput)
{
	std::string input_string = "2 0 swap release";

	GameInput expected{2, 0, GameButton::SWAP, ButtonAction::UP};
	GameInput actual = GameInput::from_string(input_string);

	EXPECT_EQ(expected, actual);
}


bool operator==(GameInput g, GameInput h) noexcept
{
	return g.game_time == h.game_time &&
	       g.player == h.player &&
	       g.button == h.button &&
	       g.action == h.action;
}

/**
 * Tests for input utility functions
 */

#include "input.hpp"
#include "tests_common.hpp"

/**
 * Tests parsing of some Inputs from strings.
 */
TEST(InputTest, ParseInput)
{
	std::string input_string = "PlayerInput 2 0 swap release";
	Input expected{PlayerInput{2, 0, GameButton::SWAP, ButtonAction::UP}};
	Input actual(input_string);
	EXPECT_EQ(expected, actual);

	input_string = "SpawnBlockInput 3 0 5 blue red red red red red";
	expected = Input{SpawnBlockInput{3, 0, 5, {Color::BLUE, Color::RED, Color::RED, Color::RED, Color::RED, Color::RED}}};
	actual = Input(input_string);
	EXPECT_EQ(expected, actual);

	input_string = "SpawnGarbageInput 4 0 1 2 blue red";
	expected = Input{SpawnGarbageInput{4, 0, 1, 2, {Color::BLUE, Color::RED}}};
	actual = Input(input_string);
	EXPECT_EQ(expected, actual);
}

/**
 * Tests converting of some Inputs to strings.
 */
TEST(InputTest, InputToString)
{
	Input source{PlayerInput{2, 0, GameButton::SWAP, ButtonAction::UP}};
	std::string expected = "PlayerInput 2 0 swap release";
	std::string actual = std::string(source);
	EXPECT_EQ(expected, actual);

	source = Input{SpawnBlockInput{3, 0, 5, {Color::BLUE, Color::RED, Color::RED, Color::RED, Color::RED, Color::RED}}};
	expected = "SpawnBlockInput 3 0 5 blue red red red red red";
	actual = std::string(source);
	EXPECT_EQ(expected, actual);

	source = Input{SpawnGarbageInput{4, 0, 1, 2, {Color::BLUE, Color::RED}}};
	expected = "SpawnGarbageInput 4 0 1 2 blue red";
	actual = std::string(source);
	EXPECT_EQ(expected, actual);
}

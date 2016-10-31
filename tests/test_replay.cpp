/**
 * Tests for replay facilities
 */

#include "replay.hpp"
#include "gtest/gtest.h"
#include <string>
#include <sstream>

/**
 * Tests basic replay output via Journal.
 */
TEST(ReplayTest, WriteJournal)
{
	std::ostringstream stream;
	auto journal = Journal(stream);

	ReplayEvent events[] =
	{
		ReplayEvent::make_set("rng_seed", "4711"),
		ReplayEvent::make_set("winner", "1"),
		ReplayEvent::make_start(),
		ReplayEvent::make_input(3, GameInput{0, GameButton::LEFT}),
		ReplayEvent::make_input(5, GameInput{1, GameButton::UP}),
		ReplayEvent::make_input(8, GameInput{0, GameButton::RAISE}),
		ReplayEvent::make_input(10, GameInput{0, GameButton::LEFT}),
		ReplayEvent::make_input(10, GameInput{1, GameButton::SWAP}),
		ReplayEvent::make_end(20)
	};

	for(auto& event : events)
	{
		journal << event;
	}

	std::string expected =
R"(0 set rng_seed 4711
0 set winner 1
0 start
3 input 0 left
5 input 1 up
8 input 0 raise
10 input 0 left
10 input 1 swap
20 end
)";

	EXPECT_EQ(expected, stream.str());
}

/**
 * Test basic replay parsing
 */
TEST(ReplayTest, ReadBasic)
{
	std::string replay_str =
R"(0 set rng_seed 4711
0 start
10 input 1 swap
20 end
)";
	std::istringstream stream(replay_str);
	ReplayEvent set_event, start_event, input_event, end_event;

	Replay replay(stream);
	replay >> set_event >> start_event >> input_event >> end_event;

	ASSERT_TRUE(replay);
	EXPECT_EQ(ReplayEvent::Type::SET, set_event.type());
	EXPECT_EQ("rng_seed", set_event.set_name());
	EXPECT_EQ("4711", set_event.set_value());
	EXPECT_EQ(ReplayEvent::Type::START, start_event.type());
	EXPECT_EQ(ReplayEvent::Type::INPUT, input_event.type());
	EXPECT_EQ(10, input_event.time());
	EXPECT_EQ(1, input_event.input().player);
	EXPECT_EQ(GameButton::SWAP, input_event.input().button);
	EXPECT_EQ(ReplayEvent::Type::END, end_event.type());
	EXPECT_EQ(20, end_event.time());
}

/**
 * Test replay error (input)
 */
TEST(ReplayTest, ReadErrorInput)
{
	std::string replay_str = "10 input 1\n20 end\n";
	std::istringstream stream(replay_str);
	ReplayEvent input_event;

	Replay replay(stream);
	EXPECT_THROW(replay >> input_event, GameException);
}

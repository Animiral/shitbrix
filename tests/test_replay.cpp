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
		ReplayEvent::make_input(3, 0, PlayerInput::LEFT),
		ReplayEvent::make_input(5, 1, PlayerInput::UP),
		ReplayEvent::make_input(8, 0, PlayerInput::RAISE),
		ReplayEvent::make_input(10, 0, PlayerInput::LEFT),
		ReplayEvent::make_input(10, 1, PlayerInput::SWAP),
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

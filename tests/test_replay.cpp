/**
 * Tests for replay facilities
 */

#include "replay.hpp"
#include "gtest/gtest.h"
#include <string>
#include <sstream>

/**
 * Accepts and simply stores replay data.
 */
class FakeSink : public IReplaySink
{
public:
	std::vector<ReplayEvent> m_events;
	virtual void handle(const ReplayEvent& event) override { m_events.push_back(event); }
};

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
		ReplayEvent::make_input(GameInput{3, 0, GameButton::LEFT, ButtonAction::DOWN}),
		ReplayEvent::make_input(GameInput{5, 1, GameButton::UP, ButtonAction::DOWN}),
		ReplayEvent::make_input(GameInput{8, 0, GameButton::RAISE, ButtonAction::DOWN}),
		ReplayEvent::make_input(GameInput{10, 0, GameButton::LEFT, ButtonAction::DOWN}),
		ReplayEvent::make_input(GameInput{10, 1, GameButton::SWAP, ButtonAction::DOWN}),
		ReplayEvent::make_end()
	};

	for(auto& event : events)
	{
		journal << event;
	}

	std::string expected =
R"(set rng_seed 4711
set winner 1
start
input 3 0 left down
input 5 1 up down
input 8 0 raise down
input 10 0 left down
input 10 1 swap down
end
)";

	EXPECT_EQ(expected, stream.str());
}

/**
 * Test basic replay parsing
 */
TEST(ReplayTest, ReadBasic)
{
	std::string replay_str =
R"(set rng_seed 4711
start
input 10 1 swap down
end
)";
	std::istringstream stream(replay_str);
	FakeSink sink;
	replay_read(stream, sink);

	ASSERT_EQ(4, sink.m_events.size());

	// set event
	EXPECT_EQ(ReplayEvent::Type::SET, sink.m_events[0].type);
	EXPECT_EQ("rng_seed", sink.m_events[0].set_name);
	EXPECT_EQ("4711", sink.m_events[0].set_value);

	// start event
	EXPECT_EQ(ReplayEvent::Type::START, sink.m_events[1].type);

	// input event
	EXPECT_EQ(ReplayEvent::Type::INPUT, sink.m_events[2].type);
	EXPECT_EQ(10, sink.m_events[2].input.game_time);
	EXPECT_EQ(1, sink.m_events[2].input.player);
	EXPECT_EQ(GameButton::SWAP, sink.m_events[2].input.button);

	// end event
	EXPECT_EQ(ReplayEvent::Type::END, sink.m_events[3].type);
}

/**
 * Test replay error (input)
 */
TEST(ReplayTest, ReadErrorInput)
{
	std::string replay_str = "input 10 1\nend\n";
	std::istringstream stream(replay_str);
	FakeSink sink;

	EXPECT_THROW(replay_read(stream, sink), GameException);
}

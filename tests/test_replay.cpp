/**
 * Tests for replay facilities
 */

#include "replay.hpp"
#include "error.hpp"
#include "gtest/gtest.h"
#include <string>
#include <sstream>

/**
 * Tests basic replay output via Journal.
 */
TEST(ReplayTest, WriteJournal)
{
	std::ostringstream stream;
	const GameMeta meta{2, 4711, 1}; // { nr.players, seed, winner }
	Journal journal{meta, GameState{meta}};

	//ReplayRecord events[] =
	journal.add_input(GameInput{3, 0, GameButton::LEFT, ButtonAction::DOWN});
	journal.add_input(GameInput{5, 1, GameButton::UP, ButtonAction::DOWN});
	journal.add_input(GameInput{8, 0, GameButton::RAISE, ButtonAction::DOWN});
	journal.add_input(GameInput{10, 0, GameButton::LEFT, ButtonAction::DOWN});
	journal.add_input(GameInput{10, 1, GameButton::SWAP, ButtonAction::DOWN});

	replay_write(stream, journal);

	std::string expected =
R"(start
meta 2 4711 1
input 3 0 left down
input 5 1 up down
input 8 0 raise down
input 10 0 left down
input 10 1 swap down
)";

	EXPECT_EQ(expected, stream.str());
}

/**
 * Test basic replay parsing
 */
TEST(ReplayTest, ReadBasic)
{
	std::string replay_str =
R"(start
meta 2 4711 1
input 10 1 swap down
)";
	std::istringstream stream(replay_str);
	Journal journal = replay_read(stream);

	GameMeta meta = journal.meta();
	EXPECT_EQ(2, meta.players);
	EXPECT_EQ(4711, meta.seed);
	EXPECT_EQ(1, meta.winner);

	GameInputs inputs = journal.inputs();
	EXPECT_EQ(1, inputs.size());
	EXPECT_EQ(10, inputs[0].input.game_time);
	EXPECT_EQ(1, inputs[0].input.player);
	EXPECT_EQ(GameButton::SWAP, inputs[0].input.button);
	EXPECT_EQ(ButtonAction::DOWN, inputs[0].input.action);
}

/**
 * Test replay error (input)
 */
TEST(ReplayTest, ReadErrorInput)
{
	std::string replay_str = "input 10 1\nend\n";
	std::istringstream stream(replay_str);

	EXPECT_THROW(replay_read(stream), ReplayException);
}

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
input 3 0 left press
input 5 1 up press
input 8 0 raise press
input 10 0 left press
input 10 1 swap press
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
input 10 1 swap press
)";
	std::istringstream stream(replay_str);
	Journal journal = replay_read(stream);

	GameMeta meta = journal.meta();
	EXPECT_EQ(2, meta.players);
	EXPECT_EQ(4711, meta.seed);
	EXPECT_EQ(1, meta.winner);

	GameInputs inputs = journal.inputs();
	ASSERT_EQ(1, inputs.size());
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

/**
 * Test Journal checkpoints
 */
TEST(ReplayTest, Checkpoint)
{
	GameMeta meta{2 /* players */, 0 /* seed */};
	GameState state0{meta};
	Journal journal{meta, GameState(state0)};

	GameState state1 = state0;
	state1.update();
	state1.update();
	state1.update();
	journal.add_checkpoint(std::move(state1));

	EXPECT_EQ(0, journal.checkpoint_before(3).game_time());
	EXPECT_EQ(3, journal.checkpoint_before(4).game_time());
}

/**
 * Test rewinding the Journal to the last checkpoint
 */
TEST(ReplayTest, Rewind)
{
	// Pre-test required setup.
	// We use ASSERT because this is not the topic of this test.
	GameMeta meta{2 /* players */, 0 /* seed */};
	GameState state{meta};
	RowCol block_coords = state.pit()[0]->cursor().rc; // ready for swap later
	state.pit()[0]->spawn_block(Block::Color::BLUE, block_coords, Block::State::REST); // object with 2 paths
	Journal journal{meta, std::move(state)};

	// get the working copy of the state from the journal
	state = journal.checkpoint_before(1);

	ASSERT_EQ(0, state.game_time());
	state.update(); // time = 1
	state.update(); // time = 2
	ASSERT_EQ(2, state.game_time());
	ASSERT_TRUE(state.pit()[0]->block_at(block_coords)); // block has not moved

	// Test 1: Journal must order new inputs
	journal.add_input(GameInput{1 /* int time */, 0, GameButton::SWAP, ButtonAction::DOWN});
	long earliest = journal.earliest_undiscovered();
	EXPECT_EQ(1, earliest);

	// Test 2: Journal must properly discover inputs
	state = journal.checkpoint_before(earliest); // reset to time 0
	EXPECT_EQ(0, state.game_time());
	const long target_time = 3;
	GameInputSpan inputs = journal.discover_inputs(state.game_time(), target_time);
	EXPECT_EQ(1, std::distance(inputs.first, inputs.second)); // we have 1 new input
}

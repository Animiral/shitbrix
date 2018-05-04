/**
 * Tests for replay facilities
 */

#include "replay.hpp"
#include "error.hpp"
#include "gtest/gtest.h"
#include <string>
#include <sstream>

class ReplayTest : public ::testing::Test
{

protected:

	virtual void SetUp()
	{
		meta = GameMeta{2 /* players */, 4711 /* seed */};
		state = std::make_unique<GameState>(meta);
		journal = std::make_unique<Journal>(meta, GameState(*state));
	}

	// virtual void TearDown() {}

	GameMeta meta;
	std::unique_ptr<GameState> state;
	std::unique_ptr<Journal> journal;

};

/**
 * Tests basic replay output via Journal.
 */
TEST_F(ReplayTest, WriteJournal)
{
	std::ostringstream stream;
	journal->set_winner(1);

	//ReplayRecord events[] =
	journal->add_input(GameInput{3, 0, GameButton::LEFT, ButtonAction::DOWN});
	journal->add_input(GameInput{5, 1, GameButton::UP, ButtonAction::DOWN});
	journal->add_input(GameInput{8, 0, GameButton::RAISE, ButtonAction::DOWN});
	journal->add_input(GameInput{10, 0, GameButton::LEFT, ButtonAction::DOWN});
	journal->add_input(GameInput{10, 1, GameButton::SWAP, ButtonAction::DOWN});

	replay_write(stream, *journal);

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
TEST_F(ReplayTest, ReadBasic)
{
	std::string replay_str =
R"(start
meta 2 4711 1
input 10 1 swap press
)";
	std::istringstream stream(replay_str);
	journal.reset(new Journal(replay_read(stream)));

	meta = journal->meta();
	EXPECT_EQ(2, meta.players);
	EXPECT_EQ(4711, meta.seed);
	EXPECT_EQ(1, meta.winner);

	GameInputs inputs = journal->inputs();
	ASSERT_EQ(1, inputs.size());
	EXPECT_EQ(10, inputs[0].input.game_time);
	EXPECT_EQ(1, inputs[0].input.player);
	EXPECT_EQ(GameButton::SWAP, inputs[0].input.button);
	EXPECT_EQ(ButtonAction::DOWN, inputs[0].input.action);
}

/**
 * Test replay error (input)
 */
TEST_F(ReplayTest, ReadErrorInput)
{
	std::string replay_str = "input 10 1\nend\n";
	std::istringstream stream(replay_str);

	EXPECT_THROW(replay_read(stream), ReplayException);
}

/**
 * Test Journal checkpoints
 */
TEST_F(ReplayTest, Checkpoint)
{
	state->update();
	state->update();
	state->update();
	journal->add_checkpoint(std::move(*state));

	EXPECT_EQ(0, journal->checkpoint_before(3).game_time());
	EXPECT_EQ(3, journal->checkpoint_before(4).game_time());
}

/**
 * Test that the Journal properly discovers inputs
 */
TEST_F(ReplayTest, DiscoverInputs)
{
	const GameInput input1 = GameInput{1, 0, GameButton::SWAP, ButtonAction::DOWN};
	const GameInput input2 = GameInput{2, 0, GameButton::SWAP, ButtonAction::DOWN};
	const GameInput input3 = GameInput{3, 0, GameButton::SWAP, ButtonAction::DOWN};
	const GameInput input4 = GameInput{4, 0, GameButton::SWAP, ButtonAction::DOWN};

	// Test 1: Journal must order new inputs
	journal->add_input(input1);
	journal->add_input(input3);
	long earliest = journal->earliest_undiscovered();
	EXPECT_EQ(1, earliest);

	// Test 2: Journal must properly discover inputs
	GameInputSpan span = journal->discover_inputs(earliest, 4);
	ASSERT_EQ(2, std::distance(span.first, span.second));
	EXPECT_EQ(1, span.first->input.game_time);
	EXPECT_EQ(3, (span.first+1)->input.game_time);

	// Test 3: insert inputs in the past
	journal->add_input(input2);
	journal->add_input(input4);
	earliest = journal->earliest_undiscovered();
	EXPECT_EQ(2, earliest);

	// Test 4: Journal must discover only recent inputs
	span = journal->discover_inputs(earliest, 4);
	ASSERT_EQ(3, std::distance(span.first, span.second));
	EXPECT_EQ(2, span.first->input.game_time);
	EXPECT_EQ(3, (span.first+1)->input.game_time);
	EXPECT_EQ(4, (span.first+2)->input.game_time);
}

/**
 * Test that the Journal reports when there are no more inputs to discover.
 */
TEST_F(ReplayTest, EarliestUndiscovered)
{
	const GameInput input = GameInput{5, 0, GameButton::SWAP, ButtonAction::DOWN};

	// Test 1: Initially, there are no inputs to discover.
	long earliest = journal->earliest_undiscovered();
	EXPECT_EQ(Journal::NO_UNDISCOVERED, earliest);

	// Test 2: When we add an input, we report its earliest time.
	journal->add_input(input);
	earliest = journal->earliest_undiscovered();
	EXPECT_EQ(5, earliest);

	// Test 3: After input discovery, the time again indicates no new inputs.
	journal->discover_inputs(5, 5);
	earliest = journal->earliest_undiscovered();
	EXPECT_EQ(Journal::NO_UNDISCOVERED, earliest);
}

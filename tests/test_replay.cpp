/**
 * Tests for replay facilities
 */

#include "tests_common.hpp"
#include "replay.hpp"
#include "error.hpp"
#include <string>
#include <sstream>

class ReplayTest : public ::testing::Test
{

public:

	explicit ReplayTest()
	{
		meta = GameMeta{2 /* players */, 4711 /* seed */, false /* replay */};
		// TODO: Add an Arbiter to the game data where the color supplier used to be
		// ColorSupplierFactory color_factory = [](int) { return std::make_unique<RainbowColorSupplier>(); };
		state = std::make_unique<GameState>(meta);
		journal = std::make_unique<Journal>(meta, GameState(*state));
	}

protected:

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
	journal->add_input(Input{PlayerInput{3, 0, GameButton::LEFT, ButtonAction::DOWN}});
	journal->add_input(Input{PlayerInput{5, 1, GameButton::UP, ButtonAction::DOWN}});
	journal->add_input(Input{PlayerInput{8, 0, GameButton::RAISE, ButtonAction::DOWN}});
	journal->add_input(Input{PlayerInput{10, 0, GameButton::LEFT, ButtonAction::DOWN}});
	journal->add_input(Input{PlayerInput{10, 1, GameButton::SWAP, ButtonAction::DOWN}});

	replay_stream(stream, *journal);

	std::string expected =
R"(start
meta 2 4711 false 1
input PlayerInput 3 0 left press
input PlayerInput 5 1 up press
input PlayerInput 8 0 raise press
input PlayerInput 10 0 left press
input PlayerInput 10 1 swap press
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
meta 2 4711 false 1
input PlayerInput 10 1 swap press
)";
	std::istringstream stream(replay_str);
	journal.reset(new Journal(replay_read(stream)));

	meta = journal->meta();
	EXPECT_EQ(2, meta.players);
	EXPECT_EQ(4711, meta.seed);
	EXPECT_EQ(false, meta.replay);
	EXPECT_EQ(1, meta.winner);

	Inputs inputs = journal->inputs();
	ASSERT_EQ(1, inputs.size());
	const PlayerInput input = inputs[0].get<PlayerInput>();
	EXPECT_EQ(10, input.game_time);
	EXPECT_EQ(1, input.player);
	EXPECT_EQ(GameButton::SWAP, input.button);
	EXPECT_EQ(ButtonAction::DOWN, input.action);
}

/**
 * Test replay error (input)
 */
TEST_F(ReplayTest, ReadErrorInput)
{
	std::string replay_str = "game_input 10 1\nend\n";
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
	const Input input1{PlayerInput{1, 0, GameButton::SWAP, ButtonAction::DOWN}};
	const Input input2{PlayerInput{2, 0, GameButton::SWAP, ButtonAction::DOWN}};
	const Input input3{PlayerInput{3, 0, GameButton::SWAP, ButtonAction::DOWN}};
	const Input input4{PlayerInput{4, 0, GameButton::SWAP, ButtonAction::DOWN}};

	// Test 1: Journal must order new inputs
	journal->add_input(input1);
	journal->add_input(input3);
	long earliest = journal->earliest_undiscovered();
	EXPECT_EQ(1, earliest);

	// Test 2: Journal must properly discover inputs
	InputSpan span = journal->get_inputs(1);
	ASSERT_EQ(1, std::distance(span.first, span.second));
	EXPECT_EQ(1, span.first->get<PlayerInput>().game_time);

	// Test 3: insert inputs in the past
	journal->discover_inputs(4); // we declare all existing inputs seen
	journal->add_input(input2);
	journal->add_input(input4);
	earliest = journal->earliest_undiscovered();
	EXPECT_EQ(2, earliest);
}

/**
 * Test that the Journal retracts the correct kinds of inputs.
 */
TEST_F(ReplayTest, Retract)
{
	std::array<Color, PIT_COLS> colors; // preparation for SpawnBlockInputs
	colors.fill(Color::BLUE);

	auto inputs = {
		Input{PlayerInput{1, 0, GameButton::SWAP, ButtonAction::DOWN}}, // early input - not retracted
		Input{SpawnBlockInput{1, 0, 1, colors}}, // early input - not retracted
		Input{PlayerInput{2, 0, GameButton::SWAP, ButtonAction::DOWN}}, // player input - not retracted
		Input{SpawnBlockInput{2, 0, 2, colors}}, // to be retracted
		Input{SpawnGarbageInput{2, 0, 1, PIT_COLS, {-9, 0}, {colors.begin(), colors.end()}}} // to be retracted
	};

	for(auto input : inputs)
		journal->add_input(input);

	journal->discover_inputs(3);
	ASSERT_EQ(5, journal->inputs().size());
	ASSERT_EQ(3, journal->earliest_undiscovered());

	journal->retract(1); // e.g. when a new input at time 1 becomes known
	EXPECT_EQ(3, journal->inputs().size());
	EXPECT_EQ(2, journal->earliest_undiscovered());
}

/**
 * Test that the Journal reports when there are no more inputs to discover.
 */
TEST_F(ReplayTest, EarliestUndiscovered)
{
	const Input input{PlayerInput{5, 0, GameButton::SWAP, ButtonAction::DOWN}};

	// Test 1: Initially, there are no inputs to discover.
	long earliest = journal->earliest_undiscovered();
	EXPECT_EQ(Journal::NO_UNDISCOVERED, earliest);

	// Test 2: When we add an input, we report its earliest time.
	journal->add_input(input);
	earliest = journal->earliest_undiscovered();
	EXPECT_EQ(5, earliest);

	// Test 3: After input discovery, the earliest time is set accordingly.
	journal->discover_inputs(6);
	earliest = journal->earliest_undiscovered();
	EXPECT_EQ(6, earliest);
}

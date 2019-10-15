/**
 * Tests for the AI agent.
 */

#include "agent.hpp"
#include "state.hpp"
#include "tests_common.hpp"

using testing::Truly;

class AgentTest : public ::testing::Test
{

protected:

	GameMeta meta{ 2, 0 };
	GameState state{ meta };

};

/**
 * When the Pit is empty and has lots of space, the agent should press the
 * raise button.
 */
TEST_F(AgentTest, WantRaise)
{
	Agent agent(state, 0, 0);
	auto inputs = agent.move();

	ASSERT_EQ(1, inputs.size());
	EXPECT_EQ(1, inputs[0].game_time);
	EXPECT_EQ(0, inputs[0].player);
	EXPECT_EQ(GameButton::RAISE, inputs[0].button);
	EXPECT_EQ(ButtonAction::DOWN, inputs[0].action);
}

/**
 * When the Pit is filling up with blocks and still raising, the agent should
 * release the raise button.
 */
TEST_F(AgentTest, StopRaise)
{
	Pit& pit = *state.pit().at(0).get();

	// fill the pit almost to the top.
	const int top = pit.top() + 2;
	const int bottom = pit.bottom();
	pit.set_floor(bottom + 1);

	for(int c = 0; c < PIT_COLS; c++)
		for(int r = top; r <= bottom; r++) {
			Color color = (c + r) % 2 ? Color::PURPLE : Color::ORANGE;
			pit.spawn_block(color, { r, c }, Block::State::REST);
		}

	pit.set_raise(true); // according to the pit, we want to raise the blocks

	Agent agent(state, 0, 0);
	auto inputs = agent.move();
	auto unraise_input = std::find_if(inputs.begin(), inputs.end(), [](const PlayerInput i) { return GameButton::RAISE == i.button; });

	ASSERT_NE(unraise_input, inputs.end());
	EXPECT_EQ(1, unraise_input->game_time);
	EXPECT_EQ(0, unraise_input->player);
	EXPECT_EQ(ButtonAction::UP, unraise_input->action);
}

/**
 * When there is a column of movable blocks in the pit where blocks can be
 * pushed down a good bit lower, the agent should plan to do that.
 */
TEST_F(AgentTest, Rebalance)
{
}

/**
 * When the agent has incentive to swap a block (in this case for rebalancing),
 * but the cursor is not in the right position, the agent must move towards it.
 */
TEST_F(AgentTest, MoveTowardsBlock)
{
}

/**
 * After the agent has made a non-empty move, it is blocked from producing any
 * other input until the specified delay has run out.
 */
TEST_F(AgentTest, DelayBlocksMoves)
{
}

/**
 * If a match is possible, the agent must perform it.
 */
TEST_F(AgentTest, PerformMatch)
{
}

/**
 * If a match is located deeper in the pit, the agent must prefer it.
 */
TEST_F(AgentTest, PreferLowMatch)
{
}

/**
 * If a match can dissolve a garbage block, the agent must prefer it even more.
 */
TEST_F(AgentTest, PreferDissolveMatch)
{
}

/**
 * If a match is possible by arranging blocks to fall from others currently
 * dissolving, the agent must do it.
 */
TEST_F(AgentTest, PerformChainFromAbove)
{
}

/**
 * If a match is possible by arranging blocks to join with others falling
 * from dissolving blocks, the agent must do it.
 */
TEST_F(AgentTest, PerformChainFromBelow)
{
}

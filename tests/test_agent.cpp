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
 * A BlockPlan must have the block and goal in the same row for the cursor to reach.
 */
TEST_F(AgentTest, PlanAddFailsInDifferentRows)
{
	const RowCol block_rc{ 0, 0 };
	const RowCol goal{ 1, 0 };
	const Plan::BlockPlan block_plan{ block_rc, Color::BLUE, goal };
	Plan plan;

	EXPECT_THROW(plan.add(block_plan), EnforceException);
}

/**
 * A BlockPlan must aim for proper colors only, not `Color::FAKE`.
 */
TEST_F(AgentTest, PlanAddFailsOnFake)
{
	const Plan::BlockPlan block_plan{ { 0, 0 }, Color::FAKE, { 0, 1 } };
	Plan plan;

	EXPECT_THROW(plan.add(block_plan), EnforceException);
}

/**
 * The next step of a BlockPlan must be constructive, i.e. move the block
 * closer to the goal.
 */
TEST_F(AgentTest, PlanNextStep)
{
	const Plan::BlockPlan block_plan{ { 0, 1 }, Color::GREEN, { 0, 3 } };
	Plan plan;
	plan.add(block_plan);

	const RowCol cursor{ 3, 3 };
	const RowCol actual = plan.next_step(cursor);
	EXPECT_EQ(0, actual.r);
	EXPECT_EQ(1, actual.c);
}

/**
 * The next step of a BlockPlan does not exist for a finished plan.
 */
TEST_F(AgentTest, PlanNextStepExceptFinished)
{
	Plan plan;

	const RowCol cursor{ 3, 3 };
	EXPECT_THROW(plan.next_step(cursor), GameException);
}

/**
 * Once we inform the Plan about the necessary swaps, it must update its
 * internal bookkeeping accordingly. In this test, the plan finishes.
 */
TEST_F(AgentTest, PlanNotifySwapped)
{
	const Plan::BlockPlan block_plan{ { 0, 1 }, Color::GREEN, { 0, 3 } };
	Plan plan;
	plan.add(block_plan);

	EXPECT_FALSE(plan.is_finished());
	plan.notify_swapped({ 0, 1 });
	plan.notify_swapped({ 0, 2 });
	EXPECT_TRUE(plan.is_finished()); // green block has arrived
}

/**
 * A plan is sensible when it finds its blocks still at the expected positions
 * with the expected colors.
 */
TEST_F(AgentTest, PlanSensible)
{
	const Color color = Color::GREEN;
	const RowCol block_rc{ 0, 1 };
	const RowCol goal{ 0, 3 };

	Pit pit{ {0.f, 0.f} };
	pit.set_floor(1);
	Block& green_block = pit.spawn_block(color, block_rc, Block::State::REST);
	Block& fake_block = pit.spawn_block(Color::FAKE, { 0, 2 }, Block::State::REST);

	const Plan::BlockPlan block_plan{ block_rc, color, goal };
	Plan plan;
	plan.add(block_plan);

	EXPECT_TRUE(plan.is_sensible(pit));
	pit.swap(green_block, fake_block);
	EXPECT_FALSE(plan.is_sensible(pit));
	plan.notify_swapped(block_rc);
	EXPECT_TRUE(plan.is_sensible(pit));
}

/**
 * When the Pit is empty and has lots of space, the agent should press the
 * raise button.
 */
TEST_F(AgentTest, WantRaise)
{
	Agent agent(state, 0, 0);
	auto inputs = agent.move();
	auto raise_input = std::find_if(inputs.begin(), inputs.end(), [](const PlayerInput i) { return GameButton::RAISE == i.button; });

	ASSERT_NE(raise_input, inputs.end());
	EXPECT_EQ(1, raise_input->game_time);
	EXPECT_EQ(0, raise_input->player);
	EXPECT_EQ(ButtonAction::DOWN, raise_input->action);
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
	Pit& pit = *state.pit().at(0).get();

	// create a column of blocks that give incentive to rebalance
	const int top = pit.bottom() - 4;
	const int bottom = pit.bottom();
	pit.set_floor(bottom + 1);

	// floor blocks at the bottom
	for(int c = 0; c < PIT_COLS; c++) {
		Color color = c % 2 ? Color::PURPLE : Color::ORANGE;
		pit.spawn_block(color, { bottom, c }, Block::State::REST);
	}

	// pillar to the left
	for(int r = top; r < bottom; r++) {
		Color color = (bottom - r) % 2 ? Color::PURPLE : Color::ORANGE;
		pit.spawn_block(color, { r, 0 }, Block::State::REST);
	}

	// place cursor over the top block on the pillar
	cursor_to(pit, RowCol{ bottom - 1, 0 });

	// now the agent should want to swap, since that throws down a block
	Agent agent(state, 0, 0);
	const auto inputs = agent.move();
	const auto swap_input = std::find_if(inputs.begin(), inputs.end(), [](const PlayerInput i) { return GameButton::SWAP == i.button; });

	ASSERT_NE(swap_input, inputs.end());
	EXPECT_EQ(1, swap_input->game_time);
	EXPECT_EQ(0, swap_input->player);
	EXPECT_EQ(ButtonAction::DOWN, swap_input->action);
}

/**
 * When the agent has incentive to swap a block (in this case for rebalancing),
 * but the cursor is not in the right position, the agent must move towards it.
 */
TEST_F(AgentTest, MoveTowardsBlock)
{
	Pit& pit = *state.pit().at(0).get();

	// create a column of blocks that give incentive to rebalance
	const int top = pit.bottom() - 4;
	const int bottom = pit.bottom();
	pit.set_floor(bottom + 1);

	// floor blocks at the bottom
	for(int c = 0; c < PIT_COLS; c++) {
		Color color = c % 2 ? Color::PURPLE : Color::ORANGE;
		pit.spawn_block(color, { bottom, c }, Block::State::REST);
	}

	// pillar to the left
	for(int r = top; r < bottom; r++) {
		Color color = (bottom - r) % 2 ? Color::PURPLE : Color::ORANGE;
		pit.spawn_block(color, { r, 0 }, Block::State::REST);
	}

	// place cursor next to the top block on the pillar
	cursor_to(pit, RowCol{ top, 1 });

	// now the agent should want to move left, since there is a block there to throw down
	Agent agent(state, 0, 0);
	const auto inputs = agent.move();
	const auto left_input = std::find_if(inputs.begin(), inputs.end(), [](const PlayerInput i) { return GameButton::LEFT == i.button; });

	ASSERT_NE(left_input, inputs.end());
	EXPECT_EQ(1, left_input->game_time);
	EXPECT_EQ(0, left_input->player);
	EXPECT_EQ(ButtonAction::DOWN, left_input->action);
}

/**
 * After the agent has made a non-empty move, it is blocked from producing any
 * other input until the specified delay has run out.
 */
TEST_F(AgentTest, DelayBlocksMoves)
{
	Pit& pit = *state.pit().at(0).get();

	// create a column of blocks that give incentive to rebalance
	const int top = pit.bottom() - 4;
	const int bottom = pit.bottom();
	pit.set_floor(bottom + 1);

	// floor blocks at the bottom
	for(int c = 0; c < PIT_COLS; c++) {
		Color color = c % 2 ? Color::PURPLE : Color::ORANGE;
		pit.spawn_block(color, { bottom, c }, Block::State::REST);
	}

	// pillar to the left
	for(int r = top; r < bottom; r++) {
		Color color = (bottom - r) % 2 ? Color::PURPLE : Color::ORANGE;
		pit.spawn_block(color, { r, 0 }, Block::State::REST);
	}

	// place cursor two spaces from the top block on the pillar
	cursor_to(pit, RowCol{ top, 2 });

	// now the agent should want to move left twice
	const int delay = 3;
	Agent agent(state, 0, delay);
	auto inputs = agent.move();
	EXPECT_FALSE(inputs.empty());

	// advance to one frame before the agent is allowed to move again
	for(int i = 0; i < delay; i++)
		state.update();

	inputs = agent.move();
	EXPECT_TRUE(inputs.empty());

	state.update();
	inputs = agent.move();
	EXPECT_FALSE(inputs.empty());
}

/**
 * If a match is possible, the agent must perform it.
 */
TEST_F(AgentTest, PerformMatch)
{
	Pit& pit = *state.pit().at(0).get();
	const int bottom = pit.bottom();
	pit.set_floor(bottom + 1);

	// matchable blocks at the bottom
	pit.spawn_block(Color::PURPLE, { bottom, 0 }, Block::State::REST);
	pit.spawn_block(Color::PURPLE, { bottom, 1 }, Block::State::REST);
	pit.spawn_block(Color::PURPLE, { bottom, 3 }, Block::State::REST);

	cursor_to(pit, RowCol{ bottom, 2 });

	// now the agent should want to perform the match
	Agent agent(state, 0, 0);
	const auto inputs = agent.move();
	const auto swap_input = std::find_if(inputs.begin(), inputs.end(), [](const PlayerInput i) { return GameButton::SWAP == i.button; });

	ASSERT_NE(swap_input, inputs.end());
	EXPECT_EQ(1, swap_input->game_time);
	EXPECT_EQ(0, swap_input->player);
	EXPECT_EQ(ButtonAction::DOWN, swap_input->action);
}

/**
 * If a match is located deeper in the pit, the agent must prefer it.
 */
TEST_F(AgentTest, PreferLowMatch)
{
	Pit& pit = *state.pit().at(0).get();
	const int bottom = pit.bottom();
	pit.set_floor(bottom + 1);

	// matchable blocks (2 rows)
	pit.spawn_block(Color::PURPLE, { bottom, 0 }, Block::State::REST);
	pit.spawn_block(Color::PURPLE, { bottom, 1 }, Block::State::REST);
	pit.spawn_block(Color::ORANGE, { bottom, 2 }, Block::State::REST);
	pit.spawn_block(Color::PURPLE, { bottom, 3 }, Block::State::REST);
	pit.spawn_block(Color::GREEN, { bottom - 1, 0 }, Block::State::REST);
	pit.spawn_block(Color::GREEN, { bottom - 1, 1 }, Block::State::REST);
	pit.spawn_block(Color::GREEN, { bottom - 1, 3 }, Block::State::REST);

	cursor_to(pit, RowCol{ bottom - 1, 2 });

	// now the agent should refuse to match at the current position and
	// instead move towards the more rewarding match down
	Agent agent(state, 0, 0);
	const auto inputs = agent.move();
	const auto swap_input = std::find_if(inputs.begin(), inputs.end(), [](const PlayerInput i) { return GameButton::SWAP == i.button; });
	EXPECT_EQ(swap_input, inputs.end());

	const auto move_input = std::find_if(inputs.begin(), inputs.end(), [](const PlayerInput i) { return GameButton::DOWN == i.button; });

	ASSERT_NE(move_input, inputs.end());
	EXPECT_EQ(1, move_input->game_time);
	EXPECT_EQ(0, move_input->player);
	EXPECT_EQ(ButtonAction::DOWN, move_input->action);
}

/**
 * If a match can dissolve a garbage block, the agent must prefer it even more.
 */
TEST_F(AgentTest, PreferDissolveMatch)
{
	Pit& pit = *state.pit().at(0).get();
	const int bottom = pit.bottom();
	pit.set_floor(bottom + 1);

	// matchable blocks (2 rows)
	pit.spawn_block(Color::PURPLE, { bottom, 0 }, Block::State::REST);
	pit.spawn_block(Color::PURPLE, { bottom, 1 }, Block::State::REST);
	pit.spawn_block(Color::ORANGE, { bottom, 2 }, Block::State::REST);
	pit.spawn_block(Color::PURPLE, { bottom, 3 }, Block::State::REST);
	pit.spawn_block(Color::GREEN, { bottom - 1, 0 }, Block::State::REST);
	pit.spawn_block(Color::GREEN, { bottom - 1, 1 }, Block::State::REST);
	pit.spawn_block(Color::GREEN, { bottom - 1, 3 }, Block::State::REST);

	// garbage
	pit.spawn_garbage({ bottom - 2, 0 }, PIT_COLS, 1, rainbow_loot(PIT_COLS));

	cursor_to(pit, RowCol{ bottom, 2 });

	// now the agent should refuse to match at the current position and
	// instead move towards the more rewarding match up (which can dissolve the garbage)
	Agent agent(state, 0, 0);
	const auto inputs = agent.move();
	const auto swap_input = std::find_if(inputs.begin(), inputs.end(), [](const PlayerInput i) { return GameButton::SWAP == i.button; });
	EXPECT_EQ(swap_input, inputs.end());

	const auto move_input = std::find_if(inputs.begin(), inputs.end(), [](const PlayerInput i) { return GameButton::UP == i.button; });

	ASSERT_NE(move_input, inputs.end());
	EXPECT_EQ(1, move_input->game_time);
	EXPECT_EQ(0, move_input->player);
	EXPECT_EQ(ButtonAction::DOWN, move_input->action);
}

/**
 * If a match is possible by arranging blocks to fall from others currently
 * dissolving, the agent must do it.
 */
TEST_F(AgentTest, PerformChainFromAbove)
{
	Pit& pit = *state.pit().at(0).get();
	const int bottom = pit.bottom();
	pit.set_floor(bottom + 1);

	// matchable blocks (2 rows)
	pit.spawn_block(Color::PURPLE, { bottom, 0 }, Block::State::BREAK);
	pit.spawn_block(Color::PURPLE, { bottom, 1 }, Block::State::BREAK);
	pit.spawn_block(Color::PURPLE, { bottom, 2 }, Block::State::BREAK);
	pit.spawn_block(Color::ORANGE, { bottom - 1, 1 }, Block::State::REST);
	pit.spawn_block(Color::ORANGE, { bottom, 3 }, Block::State::REST);
	pit.spawn_block(Color::ORANGE, { bottom, 4 }, Block::State::REST);

	cursor_to(pit, RowCol{ bottom - 1, 1 });

	// now the agent should want to prepare the upper block for the match
	Agent agent(state, 0, 0);
	const auto inputs = agent.move();
	const auto swap_input = std::find_if(inputs.begin(), inputs.end(), [](const PlayerInput i) { return GameButton::SWAP == i.button; });

	ASSERT_NE(swap_input, inputs.end());
	EXPECT_EQ(1, swap_input->game_time);
	EXPECT_EQ(0, swap_input->player);
	EXPECT_EQ(ButtonAction::DOWN, swap_input->action);
}

/**
 * If a match is possible by arranging blocks to join with others falling
 * from dissolving blocks, the agent must do it.
 */
TEST_F(AgentTest, PerformChainFromBelow)
{
	Pit& pit = *state.pit().at(0).get();
	const int bottom = pit.bottom();
	pit.set_floor(bottom + 1);

	// matchable blocks (2 rows)
	pit.spawn_block(Color::PURPLE, { bottom, 0 }, Block::State::BREAK);
	pit.spawn_block(Color::PURPLE, { bottom, 1 }, Block::State::BREAK);
	pit.spawn_block(Color::PURPLE, { bottom, 2 }, Block::State::BREAK);
	pit.spawn_block(Color::ORANGE, { bottom - 1, 1 }, Block::State::REST);
	pit.spawn_block(Color::ORANGE, { bottom - 1, 2 }, Block::State::REST);
	pit.spawn_block(Color::ORANGE, { bottom, 4 }, Block::State::REST);

	cursor_to(pit, RowCol{ bottom, 3 });

	// now the agent should want to prepare the lower block for the match
	Agent agent(state, 0, 0);
	const auto inputs = agent.move();
	const auto swap_input = std::find_if(inputs.begin(), inputs.end(), [](const PlayerInput i) { return GameButton::SWAP == i.button; });

	ASSERT_NE(swap_input, inputs.end());
	EXPECT_EQ(1, swap_input->game_time);
	EXPECT_EQ(0, swap_input->player);
	EXPECT_EQ(ButtonAction::DOWN, swap_input->action);
}

/**
 * Tests for the game logic implementation in BlockDirector.
 */

#include "stage.hpp"
#include "director.hpp"
#include "mock.hpp"
#include "gtest/gtest.h"

class BlockDirectorTest : public ::testing::Test
{

protected:

	virtual void SetUp()
	{
		pit = std::make_unique<Pit>(Point{0,0});

		// 1 preview row, 2 normal rows, 1 half row, match-ready
		pit->spawn_block(Block::Color::BLUE, RowCol{0, 0}, Block::State::REST);
		pit->spawn_block(Block::Color::RED, RowCol{0, 1}, Block::State::REST);
		pit->spawn_block(Block::Color::YELLOW, RowCol{0, 2}, Block::State::REST);
		pit->spawn_block(Block::Color::GREEN, RowCol{0, 3}, Block::State::REST);
		pit->spawn_block(Block::Color::PURPLE, RowCol{0, 4}, Block::State::REST);
		pit->spawn_block(Block::Color::ORANGE, RowCol{0, 5}, Block::State::REST);

		pit->spawn_block(Block::Color::ORANGE, RowCol{-1, 0}, Block::State::REST);
		pit->spawn_block(Block::Color::BLUE, RowCol{-1, 1}, Block::State::REST);
		pit->spawn_block(Block::Color::RED, RowCol{-1, 2}, Block::State::REST);
		pit->spawn_block(Block::Color::YELLOW, RowCol{-1, 3}, Block::State::REST);
		pit->spawn_block(Block::Color::GREEN, RowCol{-1, 4}, Block::State::REST);
		pit->spawn_block(Block::Color::PURPLE, RowCol{-1, 5}, Block::State::REST);

		pit->spawn_block(Block::Color::BLUE, RowCol{-2, 0}, Block::State::REST);
		pit->spawn_block(Block::Color::RED, RowCol{-2, 1}, Block::State::REST);
		pit->spawn_block(Block::Color::YELLOW, RowCol{-2, 2}, Block::State::REST);
		pit->spawn_block(Block::Color::GREEN, RowCol{-2, 3}, Block::State::REST);
		pit->spawn_block(Block::Color::PURPLE, RowCol{-2, 4}, Block::State::REST);
		pit->spawn_block(Block::Color::ORANGE, RowCol{-2, 5}, Block::State::REST);

		pit->spawn_block(Block::Color::RED, RowCol{-3, 2}, Block::State::REST);
		pit->spawn_block(Block::Color::YELLOW, RowCol{-3, 3}, Block::State::REST);
		pit->spawn_block(Block::Color::GREEN, RowCol{-3, 4}, Block::State::REST);

		const int SEED = 0;
		rndgen = std::make_shared<std::mt19937>(SEED);
		director = std::make_unique<BlockDirector>(*pit, rndgen);
	}

	// virtual void TearDown() {}

	void run_game_ticks(int ticks)
	{
		for(int t = 0; t < ticks; t++) {
			pit->update(context);
			director->update();
		}
	}

	MockContext context;
	std::unique_ptr<Pit> pit;
	RndGen rndgen;
	std::unique_ptr<BlockDirector> director;

};

/**
 * Tests whether blocks correctly cause a match when one lands next
 * to others of the same color.
 */
TEST_F(BlockDirectorTest, LandAndMatch)
{
	auto& top_block = pit->spawn_block(Block::Color::RED, RowCol{-7, 2}, Block::State::FALL);
	auto& mid_block = pit->spawn_block(Block::Color::RED, RowCol{-5, 2}, Block::State::FALL);

	// wait until blocks landed and match
	const int FALL_T = std::ceil(static_cast<float>(BLOCK_H)*2/FALL_SPEED);
	run_game_ticks(FALL_T);

	EXPECT_EQ(Block::State::BREAK, top_block.block_state());
	EXPECT_EQ(Block::State::BREAK, mid_block.block_state());

	RowCol top_final_rc = top_block.rc();
	RowCol mid_final_rc = mid_block.rc();

	run_game_ticks(Block::BREAK_TIME);
	EXPECT_FALSE(pit->at(top_final_rc)); // matched blocks are gone
	EXPECT_FALSE(pit->at(mid_final_rc));
}

/**
 * Tests whether blocks correctly cause a match when one lands next
 * to others of the same color. This test is more rigorous than LandAndMatch.
 */
TEST_F(BlockDirectorTest, HorizontalMatch)
{
	pit->spawn_block(Block::Color::RED, RowCol{-3, 0}, Block::State::REST);
	auto& fall_block = pit->spawn_block(Block::Color::RED, RowCol{-4, 2}, Block::State::REST);
	const RowCol swap_target_rc{-4,1};
	bool swapping = director->swap(swap_target_rc);
	ASSERT_TRUE(swapping);
	ASSERT_EQ(swap_target_rc, fall_block.rc());
	ASSERT_EQ(Block::State::SWAP, fall_block.block_state());

	// wait until block has swapped above the gap
	const int SWAP_T = Block::SWAP_TIME;
	ASSERT_EQ(SWAP_T, fall_block.time);
	run_game_ticks(SWAP_T-1);
	EXPECT_EQ(swap_target_rc, fall_block.rc());
	EXPECT_EQ(Block::State::SWAP, fall_block.block_state());
	run_game_ticks(1);
	const RowCol fall_target_rc{-3,1};
	EXPECT_EQ(fall_target_rc, fall_block.rc());
	EXPECT_EQ(Block::State::FALL, fall_block.block_state());

	// wait until block lands and matches
	const int FALL_T = std::ceil(static_cast<float>(BLOCK_H)/FALL_SPEED);
	// EXPECT_EQ(FALL_T, fall_block.time); // NOTE: falling does not run on time (yet)
	run_game_ticks(FALL_T-1);
	EXPECT_EQ(Block::State::FALL, fall_block.block_state());
	run_game_ticks(1);
	EXPECT_EQ(Block::State::BREAK, fall_block.block_state());

	const int BREAK_T = Block::BREAK_TIME;
	EXPECT_EQ(BREAK_T, fall_block.time);
	run_game_ticks(BREAK_T-1);
	EXPECT_EQ(1, fall_block.time);
	run_game_ticks(1);

	// matched blocks are gone
	EXPECT_FALSE(pit->at(RowCol{-3,0}));
	EXPECT_FALSE(pit->at(RowCol{-3,1}));
	EXPECT_FALSE(pit->at(RowCol{-3,2}));
}

/**
 * Tests whether garbage blocks correctly dissolve when
 * hit by a nearby block match.
 */
TEST_F(BlockDirectorTest, DissolveGarbage)
{
	auto& garbage = pit->spawn_garbage(RowCol{-5, 0}, 6, 2); // chain garbage
	garbage.set_state(Physical::State::REST);

	RowCol lrc = RowCol{-2,2};
	RowCol rrc = RowCol{-2,3};
	auto& left_block = *pit->block_at(lrc);
	auto& right_block = *pit->block_at(rrc);

	// 3 in a row
	left_block.swap_toward(rrc);
	right_block.swap_toward(lrc);
	pit->swap(left_block, right_block);

	const int DISSOLVE_T = 52; // ticks until block landed, garbage has shrunk, blocks have fallen down
	run_game_ticks(DISSOLVE_T);

	EXPECT_EQ(1, garbage.rows());
	EXPECT_FALSE(pit->garbage_at(RowCol{-4, 3})); // garbage shrunk
	EXPECT_TRUE(pit->block_at(RowCol{-4, 2})); // block remains
	EXPECT_FALSE(pit->block_at(RowCol{-4, 0})); // this block should be falling
	EXPECT_TRUE(pit->block_at(RowCol{-3, 0})); // down to here
}

/**
 * Tests whether blocks spawned from a dissolving garbage correctly fall down.
 * In particular, there is an issue when blocks are supposed to fall where the
 * garbage-touching match blocks are being removed.
 */
TEST_F(BlockDirectorTest, DissolveAndFall)
{
	auto& garbage = pit->spawn_garbage(RowCol{-5, 0}, 6, 2); // chain garbage
	garbage.set_state(Physical::State::REST);

	RowCol lrc = RowCol{-2,2};
	RowCol rrc = RowCol{-2,3};
	auto& left_block = *pit->block_at(lrc);
	auto& right_block = *pit->block_at(rrc);

	// 3 in a row
	left_block.swap_toward(rrc);
	right_block.swap_toward(lrc);
	pit->swap(left_block, right_block);

	// ticks until block landed, garbage has shrunk, blocks have fallen down
	const int DISSOLVE_T = Block::SWAP_TIME + Garbage::DISSOLVE_TIME + 2;
	run_game_ticks(DISSOLVE_T);

	EXPECT_FALSE(pit->at(rrc)); // blocks have matched away
	EXPECT_FALSE(pit->block_at(RowCol{-4, 3})); // this block has fallen
	EXPECT_TRUE(pit->block_at(RowCol{-3, 3})); // down to here
}

/**
 * Tests whether a partially dissolved garbage block itself correctly falls
 * down when there is immediately no support to hold it up after dissolving it.
 */
TEST_F(BlockDirectorTest, FallAfterShrink)
{
	RowCol garbage_rc{-6,0};
	auto& garbage = pit->spawn_garbage(garbage_rc, 6, 2); // chain garbage
	garbage.set_state(Physical::State::REST);

	// vertical match just under the garbage
	pit->spawn_block(Block::Color::YELLOW, RowCol{-4, 2}, Block::State::REST);

	RowCol lrc = RowCol{-3,2};
	RowCol rrc = RowCol{-3,3};
	auto& left_block = *pit->block_at(lrc);
	auto& right_block = *pit->block_at(rrc);

	// 3 in a row
	left_block.swap_toward(rrc);
	right_block.swap_toward(lrc);
	pit->swap(left_block, right_block);

	// ticks until blocks swapped, garbage shrunk, blocks have started to fall down
	const int DISSOLVE_T = Block::SWAP_TIME + Garbage::DISSOLVE_TIME + 2;
	run_game_ticks(DISSOLVE_T);

	EXPECT_EQ(Physical::State::FALL, garbage.physical_state()); // garbage has moved
	EXPECT_FALSE(pit->garbage_at(garbage_rc)); // garbage has fallen
	EXPECT_EQ(&garbage, pit->garbage_at(RowCol{-5, 3})); // down to here
}

/**
 * Tests whether a swapping block correctly falls down after it arrives in a
 * space with nothing below. All blocks above must fall with it.
 */
TEST_F(BlockDirectorTest, FallAfterSwap)
{
	// This is the block that is going to do the swapping (to the right).
	// At the last moment before it completes the swap, a green block lands
	// on the red block. The red block notices it doesn’t have ground and falls.
	// The green block immediately falls with it.
	Block& red_block = pit->spawn_block(Block::Color::RED, RowCol{-4, 4}, Block::State::REST);
	Block* green_block = nullptr;
	bool swapping = false;

	const int SWAP_T = Block::SWAP_TIME;
	const int FALL_T = std::ceil(static_cast<float>(BLOCK_H)/FALL_SPEED);
	const int LAND_MOMENT = std::max(SWAP_T, FALL_T) + 1;
	const int SWAP_START = LAND_MOMENT - SWAP_T;
	const int SPAWN_MOMENT = LAND_MOMENT - FALL_T - 1;

	for(int t = 0; t < LAND_MOMENT; t++) {
		if(SWAP_START == t) {
			swapping = director->swap(RowCol{-4,4});
		}
		if(SPAWN_MOMENT == t) {
			green_block = &pit->spawn_block(Block::Color::GREEN, RowCol{-6, 5}, Block::State::FALL);
		}

		if(LAND_MOMENT-1 == t) {
			EXPECT_EQ(1, red_block.time);
			EXPECT_EQ(Block::State::SWAP, red_block.block_state());
			EXPECT_EQ(Block::State::LAND, green_block->block_state());
		}

		run_game_ticks(1);
	}

	// both events must have occurred
	ASSERT_TRUE(swapping);
	ASSERT_TRUE(green_block);

	// both blocks are now falling
	RowCol expected_red_rc{-3,5};
	RowCol expected_green_rc{-4,5};
	EXPECT_EQ(expected_red_rc, red_block.rc());
	EXPECT_EQ(Block::State::FALL, red_block.block_state());
	EXPECT_EQ(expected_green_rc, green_block->rc());
	EXPECT_EQ(Block::State::FALL, green_block->block_state());
}

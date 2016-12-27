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
		pit->spawn_block(Block::Color::BLUE, RowCol{0, 0}, Block::State::PREVIEW);
		pit->spawn_block(Block::Color::RED, RowCol{0, 1}, Block::State::PREVIEW);
		pit->spawn_block(Block::Color::YELLOW, RowCol{0, 2}, Block::State::PREVIEW);
		pit->spawn_block(Block::Color::GREEN, RowCol{0, 3}, Block::State::PREVIEW);
		pit->spawn_block(Block::Color::PURPLE, RowCol{0, 4}, Block::State::PREVIEW);
		pit->spawn_block(Block::Color::ORANGE, RowCol{0, 5}, Block::State::PREVIEW);

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
			director->update(context);
			pit->update(context);
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

	const int FALL_T = ((BLOCK_H/FALL_SPEED)+2)*2; // ticks until blocks landed and match
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
 * Tests whether garbage blocks correctly dissolve when
 * hit by a nearby block match.
 */
TEST_F(BlockDirectorTest, DissolveGarbage)
{
	auto& garbage = pit->spawn_garbage(RowCol{-5, 0}, 6, 2); // chain garbage
	garbage.set_state(Physical::State::REST);

	auto& left_block = *pit->block_at(RowCol{-2,2});
	auto& right_block = *pit->block_at(RowCol{-2,3});
	pit->swap(left_block, right_block); // 3 in a row
	left_block.set_state(Physical::State::FALL); // make block hot for match	

	const int DISSOLVE_T = 52; // ticks until block landed, garbage has shrunk, blocks have fallen down
	run_game_ticks(DISSOLVE_T);

	EXPECT_EQ(1, garbage.rows());
	EXPECT_FALSE(pit->garbage_at(RowCol{-4, 3})); // garbage shrunk
	EXPECT_TRUE(pit->block_at(RowCol{-4, 3})); // block remains
	EXPECT_FALSE(pit->block_at(RowCol{-4, 0})); // this block should be falling
	EXPECT_TRUE(pit->block_at(RowCol{-3, 0})); // down to here
}

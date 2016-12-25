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
		pit->spawn_block(Block::Color::BLUE, RowCol{6, 0}, Block::State::PREVIEW);
		pit->spawn_block(Block::Color::RED, RowCol{6, 1}, Block::State::PREVIEW);
		pit->spawn_block(Block::Color::YELLOW, RowCol{6, 2}, Block::State::PREVIEW);
		pit->spawn_block(Block::Color::GREEN, RowCol{6, 3}, Block::State::PREVIEW);
		pit->spawn_block(Block::Color::PURPLE, RowCol{6, 4}, Block::State::PREVIEW);
		pit->spawn_block(Block::Color::ORANGE, RowCol{6, 5}, Block::State::PREVIEW);

		pit->spawn_block(Block::Color::ORANGE, RowCol{5, 0}, Block::State::REST);
		pit->spawn_block(Block::Color::BLUE, RowCol{5, 1}, Block::State::REST);
		pit->spawn_block(Block::Color::RED, RowCol{5, 2}, Block::State::REST);
		pit->spawn_block(Block::Color::YELLOW, RowCol{5, 3}, Block::State::REST);
		pit->spawn_block(Block::Color::GREEN, RowCol{5, 4}, Block::State::REST);
		pit->spawn_block(Block::Color::PURPLE, RowCol{5, 5}, Block::State::REST);

		pit->spawn_block(Block::Color::BLUE, RowCol{4, 0}, Block::State::REST);
		pit->spawn_block(Block::Color::RED, RowCol{4, 1}, Block::State::REST);
		pit->spawn_block(Block::Color::YELLOW, RowCol{4, 2}, Block::State::REST);
		pit->spawn_block(Block::Color::GREEN, RowCol{4, 3}, Block::State::REST);
		pit->spawn_block(Block::Color::PURPLE, RowCol{4, 4}, Block::State::REST);
		pit->spawn_block(Block::Color::ORANGE, RowCol{4, 5}, Block::State::REST);

		pit->spawn_block(Block::Color::RED, RowCol{3, 2}, Block::State::REST);
		pit->spawn_block(Block::Color::YELLOW, RowCol{3, 3}, Block::State::REST);
		pit->spawn_block(Block::Color::GREEN, RowCol{3, 4}, Block::State::REST);

		const int SEED = 0;
		rndgen = std::make_shared<std::mt19937>(SEED);
		director = std::make_unique<BlockDirector>(*pit, rndgen);
	}

	// virtual void TearDown() {}

	MockContext context;
	std::unique_ptr<Pit> pit;
	RndGen rndgen;
	std::unique_ptr<BlockDirector> director;

};

/**
 * Tests whether garbage blocks correctly dissolve when
 * hit by a nearby block match.
 */
TEST_F(BlockDirectorTest, DissolveGarbage)
{
	auto& garbage = pit->spawn_garbage(RowCol{1, 0}, 6, 2); // chain garbage
	garbage.set_state(Physical::State::REST);

	auto& left_block = *pit->block_at(RowCol{4,2});
	auto& right_block = *pit->block_at(RowCol{4,3});
	pit->swap(left_block, right_block); // 3 in a row
	left_block.set_state(Physical::State::FALL); // make block hot for match	

	const int DISSOLVE_T = 32; // ticks until garbage has shrunk, blocks have fallen down
	for(int t = 0; t < DISSOLVE_T; t++)
		director->update(context);

	EXPECT_EQ(1, garbage.rows());
	EXPECT_FALSE(pit->garbage_at(RowCol{2, 3})); // garbage shrunk
	EXPECT_TRUE(pit->block_at(RowCol{2, 3})); // block remains
	EXPECT_FALSE(pit->block_at(RowCol{2, 0})); // this block should be falling
	EXPECT_TRUE(pit->block_at(RowCol{3, 0})); // down to here
}

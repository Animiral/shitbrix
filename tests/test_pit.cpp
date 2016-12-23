/**
 * Tests for the Pit interface and how it handles game objects.
 *
 * The Pit prefers to deal with Physicals instead of concrete
 * game objects where possible, and so do these tests.
 * The tests only explicitly cover the mutating Pit interface,
 * while the const interface is only implicitly tested by using
 * it to observe the actual state of the Pit.
 */

#include "stage.hpp"
#include "mock.hpp"
#include "gtest/gtest.h"

class PitTest : public ::testing::Test
{

protected:

	virtual void SetUp()
	{
		pit = std::make_unique<Pit>(Point{0,0});
	}

	// virtual void TearDown() {}

	std::unique_ptr<Pit> pit;

};

/**
 * Returns true if the pit contains exactly the given number of objects.
 */
bool contains_n(const Pit& pit, size_t blocks, size_t garbages)
{
	size_t actual_blocks = 0;
	size_t actual_garbages = 0;

	for(auto& physical : pit.contents()) {
		if(dynamic_cast<Block*>(&*physical))
			actual_blocks++;
		else if(dynamic_cast<Block*>(&*physical))
			actual_garbages++;
	}

	return actual_blocks == blocks && actual_garbages == garbages;
}

/**
 * Returns the number of objects in the pit whose presence or absence
 * differs from the given content_str.
 * content_str must contain five pit rows represented by characters.
 * ' ' is an empty coordinate.
 * 'B' is a block.
 * 'G' is garbage.
 */
int contents_mismatch(const Pit& pit, const char* content_str)
{
	int difference = 0;

	for(int row = 0; row < 5; row++)
	for(int col = 0; col < PIT_COLS; col++) {
		RowCol rc{row, col};

		switch(content_str[row*PIT_COLS+col]) {
			case ' ': difference += pit.at(rc) ? 1 : 0; break;
			case 'B': difference += pit.block_at(rc) ? 0 : 1; break;
			case 'G': difference += pit.garbage_at(rc) ? 0 : 1; break;
			default: return -1; // error code
		}
	}

	return difference;
}

/**
 * Tests whether a Block correctly appears in the Pit on spawn.
 */
TEST_F(PitTest, SpawnBlock)
{
	RowCol red_rc{1, 2};
	RowCol green_rc{3, 2};
	auto& red_block = pit->spawn_block(Block::Color::RED, red_rc, Block::State::REST);
	auto& green_block = pit->spawn_block(Block::Color::GREEN, green_rc, Block::State::REST);

	EXPECT_TRUE(pit->at(red_rc));
	EXPECT_TRUE(pit->at(green_rc));
	EXPECT_EQ(&red_block, pit->block_at(red_rc));
	EXPECT_EQ(&green_block, pit->block_at(green_rc));
	EXPECT_TRUE(contains_n(*pit, 2, 0));

	const char* content_str =
		"      "
		"  B   "
		"      "
		"  B   "
		"      ";
	EXPECT_EQ(0, contents_mismatch(*pit, content_str));
}

/**
 * Tests whether an illegal Block gets rejected in spawning.
 */
TEST_F(PitTest, SpawnBlockOutOfBounds)
{
	FAIL();
}

/**
 * Tests whether a Garbage correctly appears in the Pit on spawn.
 */
TEST_F(PitTest, SpawnGarbage)
{
	FAIL();
}

/**
 * Tests whether an illegal Garbage gets rejected in spawning.
 */
TEST_F(PitTest, SpawnGarbageOutOfBounds)
{
	FAIL();
}

/**
 * Tests whether can_fall() correctly indicates true when space is free.
 */
TEST_F(PitTest, CanFallBlockYes)
{
	FAIL();
}

/**
 * Tests whether can_fall() correctly indicates false when space is blocked.
 */
TEST_F(PitTest, CanFallBlockNo)
{
	FAIL();
}

/**
 * Tests whether can_fall() correctly indicates true when space is free.
 */
TEST_F(PitTest, CanFallGarbageYes)
{
	FAIL();
}

/**
 * Tests whether can_fall() correctly indicates false when space is blocked.
 */
TEST_F(PitTest, CanFallGarbageNo)
{
	FAIL();
}

/**
 * Tests whether a Block correctly falls.
 */
TEST_F(PitTest, FallBlock)
{
	FAIL();
}

/**
 * Tests error when a Block cannot fall because the space below is blocked.
 */
TEST_F(PitTest, FallBlockFail)
{
	FAIL();
}

/**
 * Tests whether a Garbage correctly falls.
 */
TEST_F(PitTest, FallGarbage)
{
	FAIL();
}

/**
 * Tests error when a Garbage cannot fall because one space below is blocked.
 */
TEST_F(PitTest, FallGarbageFail)
{
	FAIL();
}

/**
 * Tests whether a Block can be removed.
 */
TEST_F(PitTest, KillBlock)
{
	FAIL();
}

/**
 * Tests whether a Garbage can be shrunk and still exist.
 */
TEST_F(PitTest, ShrinkGarbage)
{
	FAIL();
}

/**
 * Tests whether a Garbage disappears when it shrinks to 0 rows.
 */
TEST_F(PitTest, KillGarbage)
{
	FAIL();
}

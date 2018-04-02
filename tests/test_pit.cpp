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
#include "error.hpp"
#include "tests_common.hpp"
#include "gtest/gtest.h"

class PitTest : public ::testing::Test
{

protected:

	virtual void SetUp()
	{
		pit = std::make_unique<Pit>(Point{0,0}, std::make_unique<RainbowBlocksQueue>(), std::make_unique<RainbowBlocksQueue>());
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
		else if(dynamic_cast<Garbage*>(&*physical))
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
	RowCol red_rc{1, -1};
	RowCol green_rc{3, 6};
	EXPECT_THROW(pit->spawn_block(Block::Color::RED, red_rc, Block::State::REST), EnforceException);
	EXPECT_THROW(pit->spawn_block(Block::Color::GREEN, green_rc, Block::State::REST), EnforceException);
}

/**
 * Tests whether a Garbage correctly appears in the Pit on spawn.
 */
TEST_F(PitTest, SpawnGarbage)
{
	RowCol combo_rc{1, 2};
	RowCol chain_rc{3, 0};
	auto& combo_gb = pit->spawn_garbage(combo_rc, 3, 1);
	auto& chain_gb = pit->spawn_garbage(chain_rc, 6, 2);

	EXPECT_TRUE(pit->at(combo_rc));
	EXPECT_TRUE(pit->at(chain_rc));
	EXPECT_EQ(&combo_gb, pit->garbage_at(RowCol{1, 4}));
	EXPECT_EQ(&chain_gb, pit->garbage_at(RowCol{4, 5}));
	EXPECT_TRUE(contains_n(*pit, 0, 2));

	const char* content_str =
		"      "
		"  GGG "
		"      "
		"GGGGGG"
		"GGGGGG";
	EXPECT_EQ(0, contents_mismatch(*pit, content_str));
}

/**
 * Tests whether an illegal Garbage gets rejected in spawning.
 */
TEST_F(PitTest, SpawnGarbageOutOfBounds)
{
	RowCol combo_rc{1, -1};
	RowCol chain_rc{3, 1};
	EXPECT_THROW(pit->spawn_garbage(combo_rc, 3, 1), EnforceException);
	EXPECT_THROW(pit->spawn_garbage(chain_rc, 6, 2), EnforceException);
}

/**
 * Tests whether can_fall() correctly indicates true when space is free.
 */
TEST_F(PitTest, CanFallBlockYes)
{
	RowCol red_rc{1, 2};
	RowCol green_rc{2, 2};
	pit->spawn_block(Block::Color::RED, red_rc, Block::State::REST);
	auto& green_block = pit->spawn_block(Block::Color::GREEN, green_rc, Block::State::REST);

	EXPECT_TRUE(pit->can_fall(green_block));
}

/**
 * Tests whether can_fall() correctly indicates false when space is blocked.
 */
TEST_F(PitTest, CanFallBlockNo)
{
	RowCol red_rc{1, 2};
	RowCol green_rc{2, 2};
	auto& red_block = pit->spawn_block(Block::Color::RED, red_rc, Block::State::REST);
	pit->spawn_block(Block::Color::GREEN, green_rc, Block::State::REST);

	EXPECT_FALSE(pit->can_fall(red_block));
}

/**
 * Tests whether can_fall() correctly indicates true when space is free.
 */
TEST_F(PitTest, CanFallGarbageYes)
{
	RowCol combo_rc{3, 2};
	RowCol chain_rc{1, 0};
	auto& combo_gb = pit->spawn_garbage(combo_rc, 3, 1);
	pit->spawn_garbage(chain_rc, 6, 2);

	EXPECT_TRUE(pit->can_fall(combo_gb));
}

/**
 * Tests whether can_fall() correctly indicates false when space is blocked.
 */
TEST_F(PitTest, CanFallGarbageNo)
{
	RowCol combo_rc{3, 2};
	RowCol chain_rc{1, 0};
	pit->spawn_garbage(combo_rc, 3, 1);
	auto& chain_gb = pit->spawn_garbage(chain_rc, 6, 2);

	EXPECT_FALSE(pit->can_fall(chain_gb));
}

/**
 * Tests whether a Block correctly falls.
 */
TEST_F(PitTest, FallBlock)
{
	RowCol red_rc{1, 2};
	RowCol green_rc{3, 2};
	auto& red_block = pit->spawn_block(Block::Color::RED, red_rc, Block::State::REST);
	pit->spawn_block(Block::Color::GREEN, green_rc, Block::State::REST);

	pit->fall(red_block);

	EXPECT_FALSE(pit->at(red_rc));
	red_rc.r++;
	EXPECT_TRUE(pit->at(red_rc));
	EXPECT_EQ(red_rc, red_block.rc());
	EXPECT_TRUE(contains_n(*pit, 2, 0));

	const char* content_str =
		"      "
		"      "
		"  B   "
		"  B   "
		"      ";
	EXPECT_EQ(0, contents_mismatch(*pit, content_str));
}

/**
 * Tests error when a Block cannot fall because the space below is blocked.
 */
TEST_F(PitTest, FallBlockFail)
{
	RowCol red_rc{2, 2};
	RowCol green_rc{3, 2};
	auto& red_block = pit->spawn_block(Block::Color::RED, red_rc, Block::State::REST);
	pit->spawn_block(Block::Color::GREEN, green_rc, Block::State::REST);

	EXPECT_THROW(pit->fall(red_block), LogicException);
}

/**
 * Tests whether a Garbage correctly falls.
 */
TEST_F(PitTest, FallGarbage)
{
	RowCol combo_rc{4, 2};
	RowCol chain_rc{1, 0};
	pit->spawn_garbage(combo_rc, 3, 1);
	auto& chain_gb = pit->spawn_garbage(chain_rc, 6, 2);

	pit->fall(chain_gb);

	EXPECT_FALSE(pit->at(chain_rc));
	EXPECT_TRUE(pit->at(RowCol{3,3}));
	chain_rc.r++;
	EXPECT_EQ(chain_rc, chain_gb.rc());
	EXPECT_TRUE(contains_n(*pit, 0, 2));

	const char* content_str =
		"      "
		"      "
		"GGGGGG"
		"GGGGGG"
		"  GGG ";
	EXPECT_EQ(0, contents_mismatch(*pit, content_str));
}

/**
 * Tests error when a Garbage cannot fall because one space below is blocked.
 */
TEST_F(PitTest, FallGarbageFail)
{
	RowCol combo_rc{3, 2};
	RowCol chain_rc{1, 0};
	pit->spawn_garbage(combo_rc, 3, 1);
	auto& chain_gb = pit->spawn_garbage(chain_rc, 6, 2);

	EXPECT_THROW(pit->fall(chain_gb), LogicException);
}

/**
 * Tests whether a Block can be removed.
 */
TEST_F(PitTest, KillBlock)
{
	RowCol red_rc{1, 2};
	RowCol green_rc{3, 2};
	RowCol yellow_rc{2, 4};
	auto& red_block = pit->spawn_block(Block::Color::RED, red_rc, Block::State::REST);
	auto& green_block = pit->spawn_block(Block::Color::GREEN, green_rc, Block::State::REST);
	pit->spawn_block(Block::Color::YELLOW, yellow_rc, Block::State::REST);

	red_block.set_state(Physical::State::DEAD);
	green_block.set_state(Physical::State::DEAD);
	pit->remove_dead();
	EXPECT_FALSE(pit->at(red_rc));
	EXPECT_FALSE(pit->at(green_rc));
	EXPECT_TRUE(pit->at(yellow_rc));
	EXPECT_TRUE(contains_n(*pit, 1, 0));

	const char* content_str =
		"      "
		"      "
		"    B "
		"      "
		"      ";
	EXPECT_EQ(0, contents_mismatch(*pit, content_str));
}

/**
 * Tests whether a Garbage can be shrunk and still exist.
 */
TEST_F(PitTest, ShrinkGarbage)
{
	RowCol combo_rc{1, 2};
	RowCol chain_rc{3, 0};
	pit->spawn_garbage(combo_rc, 3, 1);
	auto& chain_gb = pit->spawn_garbage(chain_rc, 6, 2);

	pit->shrink(chain_gb);

	EXPECT_TRUE(pit->at(chain_rc));
	EXPECT_FALSE(pit->at(RowCol{4,1}));
	EXPECT_TRUE(contains_n(*pit, 0, 2));

	const char* content_str =
		"      "
		"  GGG "
		"      "
		"GGGGGG"
		"      ";
	EXPECT_EQ(0, contents_mismatch(*pit, content_str));
}

/**
 * Tests whether a Garbage disappears when it shrinks to 0 rows.
 */
TEST_F(PitTest, KillGarbage)
{
	RowCol combo_rc{1, 2};
	RowCol chain_rc{3, 0};
	auto& combo_gb = pit->spawn_garbage(combo_rc, 3, 1);
	pit->spawn_garbage(chain_rc, 6, 2);

	pit->shrink(combo_gb);

	EXPECT_FALSE(pit->at(combo_rc));
	EXPECT_TRUE(contains_n(*pit, 0, 1));

	const char* content_str =
		"      "
		"      "
		"      "
		"GGGGGG"
		"GGGGGG";
	EXPECT_EQ(0, contents_mismatch(*pit, content_str));
}

/**
 * Tests whether a Garbage block is really gone when shrunk.
 */
TEST_F(PitTest, KillAndErase)
{
	RowCol combo_rc{1, 2};
	RowCol chain_rc{3, 0};
	auto& combo_gb = pit->spawn_garbage(combo_rc, 3, 1);
	pit->spawn_garbage(chain_rc, 6, 2);

	pit->shrink(combo_gb);

	for(auto it = pit->contents().begin(), end = pit->contents().end(); it != end; ++it)
		EXPECT_TRUE(*it); // no nullptr objects left over

	EXPECT_EQ(1, pit->contents().size());
}

/**
 * Tests for behavior of game objects.
 */

#include "stage.hpp"
#include "error.hpp"
#include "tests_common.hpp"
#include "gtest/gtest.h"

class StageTest : public ::testing::Test
{

protected:

	virtual void SetUp()
	{
		configure_context_for_testing();
		gamedata.reset(new GameData{make_gamedata_for_testing()});

		state = &gamedata->state;
		pit = state->pit().at(0).get();
		stage.reset(new Stage(*state));
		indicator = &stage->sobs().at(0).bonus;
	}

	// virtual void TearDown() {}

	std::unique_ptr<GameData> gamedata;
	GameState* state;
	Pit* pit;
	std::unique_ptr<Stage> stage;
	BonusIndicator* indicator;

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
 * Tests whether a falling block correctly updates.
 */
TEST(BlockTest, Fall)
{
	// setup
	Block block(Color::BLUE, RowCol{3,3}, Block::State::REST);
	block.set_state(Block::State::FALL, ROW_HEIGHT, FALL_SPEED);

	const int TICKS = 3; // block updates in this test

	for(int i = 0; i < TICKS; i++)
	{
		block.update();
	}

	EXPECT_FLOAT_EQ(float(ROW_HEIGHT) / FALL_SPEED - TICKS, block.eta());
}

/**
 * Tests whether a Block correctly appears in the Pit on spawn.
 */
TEST_F(StageTest, SpawnBlock)
{
	RowCol red_rc{1, 2};
	RowCol green_rc{3, 2};
	auto& red_block = pit->spawn_block(Color::RED, red_rc, Block::State::REST);
	auto& green_block = pit->spawn_block(Color::GREEN, green_rc, Block::State::REST);

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
TEST_F(StageTest, SpawnBlockOutOfBounds)
{
	RowCol red_rc{1, -1};
	RowCol green_rc{3, 6};
	EXPECT_THROW(pit->spawn_block(Color::RED, red_rc, Block::State::REST), EnforceException);
	EXPECT_THROW(pit->spawn_block(Color::GREEN, green_rc, Block::State::REST), EnforceException);
}

/**
 * Tests whether a Garbage correctly appears in the Pit on spawn.
 */
TEST_F(StageTest, SpawnGarbage)
{
	RowCol combo_rc{1, 2};
	RowCol chain_rc{3, 0};
	auto& combo_gb = spawn_garbage(*pit, combo_rc, 3, 1);
	auto& chain_gb = spawn_garbage(*pit, chain_rc, 6, 2);

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
TEST_F(StageTest, SpawnGarbageOutOfBounds)
{
	RowCol combo_rc{1, -1};
	RowCol chain_rc{3, 1};
	EXPECT_THROW(spawn_garbage(*pit, combo_rc, 3, 1), EnforceException);
	EXPECT_THROW(spawn_garbage(*pit, chain_rc, 6, 2), EnforceException);
}

/**
 * Tests whether can_fall() correctly indicates true when space is free.
 */
TEST_F(StageTest, CanFallBlockYes)
{
	RowCol red_rc{1, 2};
	RowCol green_rc{2, 2};
	pit->spawn_block(Color::RED, red_rc, Block::State::REST);
	auto& green_block = pit->spawn_block(Color::GREEN, green_rc, Block::State::REST);

	EXPECT_TRUE(pit->can_fall(green_block));
}

/**
 * Tests whether can_fall() correctly indicates false when space is blocked.
 */
TEST_F(StageTest, CanFallBlockNo)
{
	RowCol red_rc{1, 2};
	RowCol green_rc{2, 2};
	auto& red_block = pit->spawn_block(Color::RED, red_rc, Block::State::REST);
	pit->spawn_block(Color::GREEN, green_rc, Block::State::REST);

	EXPECT_FALSE(pit->can_fall(red_block));
}

/**
 * Tests whether can_fall() correctly indicates true when space is free.
 */
TEST_F(StageTest, CanFallGarbageYes)
{
	RowCol combo_rc{3, 2};
	RowCol chain_rc{1, 0};
	auto& combo_gb = spawn_garbage(*pit, combo_rc, 3, 1);
	spawn_garbage(*pit, chain_rc, 6, 2);

	EXPECT_TRUE(pit->can_fall(combo_gb));
}

/**
 * Tests whether can_fall() correctly indicates false when space is blocked.
 */
TEST_F(StageTest, CanFallGarbageNo)
{
	RowCol combo_rc{3, 2};
	RowCol chain_rc{1, 0};
	spawn_garbage(*pit, combo_rc, 3, 1);
	auto& chain_gb = spawn_garbage(*pit, chain_rc, 6, 2);

	EXPECT_FALSE(pit->can_fall(chain_gb));
}

/**
 * Tests whether a Block correctly falls.
 */
TEST_F(StageTest, FallBlock)
{
	RowCol red_rc{1, 2};
	RowCol green_rc{3, 2};
	auto& red_block = pit->spawn_block(Color::RED, red_rc, Block::State::REST);
	pit->spawn_block(Color::GREEN, green_rc, Block::State::REST);

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
TEST_F(StageTest, FallBlockFail)
{
	RowCol red_rc{2, 2};
	RowCol green_rc{3, 2};
	auto& red_block = pit->spawn_block(Color::RED, red_rc, Block::State::REST);
	pit->spawn_block(Color::GREEN, green_rc, Block::State::REST);

	EXPECT_THROW(pit->fall(red_block), LogicException);
}

/**
 * Tests whether a Garbage correctly falls.
 */
TEST_F(StageTest, FallGarbage)
{
	RowCol combo_rc{4, 2};
	RowCol chain_rc{1, 0};
	spawn_garbage(*pit, combo_rc, 3, 1);
	auto& chain_gb = spawn_garbage(*pit, chain_rc, 6, 2);

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
TEST_F(StageTest, FallGarbageFail)
{
	RowCol combo_rc{3, 2};
	RowCol chain_rc{1, 0};
	spawn_garbage(*pit, combo_rc, 3, 1);
	auto& chain_gb = spawn_garbage(*pit, chain_rc, 6, 2);

	EXPECT_THROW(pit->fall(chain_gb), LogicException);
}

/**
 * Tests whether a Block can be removed.
 */
TEST_F(StageTest, KillBlock)
{
	RowCol red_rc{1, 2};
	RowCol green_rc{3, 2};
	RowCol yellow_rc{2, 4};
	auto& red_block = pit->spawn_block(Color::RED, red_rc, Block::State::REST);
	auto& green_block = pit->spawn_block(Color::GREEN, green_rc, Block::State::REST);
	pit->spawn_block(Color::YELLOW, yellow_rc, Block::State::REST);

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
TEST_F(StageTest, ShrinkGarbage)
{
	RowCol combo_rc{1, 2};
	RowCol chain_rc{3, 0};
	spawn_garbage(*pit, combo_rc, 3, 1);
	auto& chain_gb = spawn_garbage(*pit, chain_rc, 6, 2);

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
TEST_F(StageTest, KillGarbage)
{
	RowCol combo_rc{1, 2};
	RowCol chain_rc{3, 0};
	auto& combo_gb = spawn_garbage(*pit, combo_rc, 3, 1);
	spawn_garbage(*pit, chain_rc, 6, 2);

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
TEST_F(StageTest, KillAndErase)
{
	RowCol combo_rc{1, 2};
	RowCol chain_rc{3, 0};
	auto& combo_gb = spawn_garbage(*pit, combo_rc, 3, 1);
	spawn_garbage(*pit, chain_rc, 6, 2);

	pit->shrink(combo_gb);

	for(auto it = pit->contents().begin(), end = pit->contents().end(); it != end; ++it)
		EXPECT_TRUE(*it); // no nullptr objects left over

	EXPECT_EQ(1, pit->contents().size());
}

/**
 * Tests that starting raise interrupts recovery.
 */
TEST_F(StageTest, RaiseInterruptsRecovery)
{
	pit->replenish_recovery();
	pit->set_raise(true);
	EXPECT_EQ(0., pit->recovery());
}

/**
 * Tests that active raising suppresses recovery.
 */
TEST_F(StageTest, RaiseSuppressesRecovery)
{
	pit->set_raise(true);
	pit->replenish_recovery();
	EXPECT_EQ(0., pit->recovery());
}

/**
 * Tests that the bonus indicator displays the values set.
 */
TEST_F(StageTest, IndicatorValues)
{
	indicator->display_combo(5);
	indicator->display_chain(3);

	int combo;
	uint8_t combo_fade;
	int chain;
	uint8_t chain_fade;
	indicator->get_indication(combo, combo_fade, chain, chain_fade);

	EXPECT_EQ(5, combo);
	EXPECT_EQ(255, combo_fade);
	EXPECT_EQ(3, chain);
	EXPECT_EQ(255, chain_fade);
}

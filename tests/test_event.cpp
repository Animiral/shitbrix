/**
 * Tests for the director’s usage of the IGameEvent interface.
 */

#include "stage.hpp"
#include "director.hpp"
#include "event.hpp"
#include "replay.hpp"
#include "tests_common.hpp"
#include "gtest/gtest.h"

class GameEventCounter : public evt::IEventObserver
{

public:

	virtual void fire(evt::CursorMoves moved) override { countCursorMoves++; }
	virtual void fire(evt::Swap swapped) override { countSwap++; }
	virtual void fire(evt::Match matched) override { last_match = matched; }
	virtual void fire(evt::Chain chained) override { last_chain = chained; }
	virtual void fire(evt::BlockDies died) override { countBlockDies++; }
	virtual void fire(evt::GarbageDissolves dissolved) override { countGarbageDissolves++; }

	int countCursorMoves = 0;
	int countSwap = 0;
	evt::Match last_match{0, false};
	evt::Chain last_chain{0};
	int countBlockDies = 0;
	int countGarbageDissolves = 0;

};

class GameEventTest : public ::testing::Test
{

protected:

	virtual void SetUp()
	{
		configure_context_for_testing();
		gamedata.reset(new GameData{make_gamedata_for_testing()});

		pit = gamedata->state.pit().at(0).get();
		block_director = &gamedata->rules.block_director;
		gamedata->rules.event_hub.subscribe(counter);
	}

	// virtual void TearDown() {}

	void run_game_ticks(int ticks)
	{
		assert(0 < ticks);
		ticks += gamedata->state.game_time();
		synchronurse(gamedata->state, ticks, gamedata->journal, gamedata->rules);
	}

	std::unique_ptr<GameData> gamedata;
	Pit* pit = nullptr; // special Pit with non-random spawn queue
	BlockDirector* block_director = nullptr; // shortcut to the environment's director
	GameEventCounter counter;

};

/**
 * Tests whether a cursor move event is correctly generated by the CursorDirector.
 */
TEST_F(GameEventTest, CursorMoves)
{
	gamedata->journal.add_input(Input{PlayerInput{1, 0, GameButton::RIGHT, ButtonAction::DOWN}});
	gamedata->journal.add_input(Input{PlayerInput{2, 0, GameButton::LEFT, ButtonAction::DOWN}});

	run_game_ticks(1);
	EXPECT_EQ(1, counter.countCursorMoves);

	run_game_ticks(1);
	EXPECT_EQ(2, counter.countCursorMoves);
}

/**
 * Tests whether a blocks swap event is correctly generated by the BlockDirector.
 */
TEST_F(GameEventTest, Swap)
{
	pit->spawn_block(Color::BLUE, RowCol{0, 0}, Block::State::REST);
	pit->spawn_block(Color::RED, RowCol{0, 1}, Block::State::REST);

	swap_at(*pit, *block_director, RowCol{0, 0});
	EXPECT_EQ(1, counter.countSwap);

	swap_at(*pit, *block_director, RowCol{0, 1});
	EXPECT_EQ(2, counter.countSwap);

	swap_at(*pit, *block_director, RowCol{-1, 1});
	EXPECT_EQ(2, counter.countSwap);
}

/**
 * Tests whether a match event is correctly generated by the BlockDirector.
 */
TEST_F(GameEventTest, Match)
{
	pit->spawn_block(Color::BLUE, RowCol{0, 0}, Block::State::REST);
	pit->spawn_block(Color::BLUE, RowCol{0, 1}, Block::State::REST);
	pit->spawn_block(Color::RED, RowCol{0, 2}, Block::State::REST);
	pit->spawn_block(Color::BLUE, RowCol{0, 3}, Block::State::REST);
	pit->spawn_block(Color::RED, RowCol{0, 4}, Block::State::REST);
	pit->spawn_block(Color::RED, RowCol{-1, 2}, Block::State::REST);

	swap_at(*pit, *block_director, RowCol{0, 2});

	run_game_ticks(SWAP_TIME);
	EXPECT_EQ(3, counter.last_match.combo);
	EXPECT_FALSE(counter.last_match.chaining);

	const int FALL1_TIME = static_cast<int>(std::ceil(static_cast<float>(ROW_HEIGHT)/FALL_SPEED));
	run_game_ticks(BREAK_TIME + FALL1_TIME);
	EXPECT_EQ(3, counter.last_match.combo);
	EXPECT_TRUE(counter.last_match.chaining);
}

/**
 * Tests whether a chain event is correctly generated by the BlockDirector.
 */
TEST_F(GameEventTest, Chain)
{
	pit->spawn_block(Color::BLUE, RowCol{0, 0}, Block::State::REST);
	pit->spawn_block(Color::BLUE, RowCol{0, 1}, Block::State::REST);
	pit->spawn_block(Color::RED, RowCol{0, 2}, Block::State::REST);
	pit->spawn_block(Color::BLUE, RowCol{0, 3}, Block::State::REST);
	pit->spawn_block(Color::RED, RowCol{0, 4}, Block::State::REST);
	pit->spawn_block(Color::RED, RowCol{-1, 2}, Block::State::REST);

	swap_at(*pit, *block_director, RowCol{0, 2});

	const int FALL1_TIME = static_cast<int>(std::ceil(static_cast<float>(ROW_HEIGHT)/FALL_SPEED));
	run_game_ticks(SWAP_TIME + BREAK_TIME + FALL1_TIME + BREAK_TIME);
	EXPECT_EQ(1, counter.last_chain.counter);
}

/**
 * Tests whether a block dies event is correctly generated by the BlockDirector.
 */
TEST_F(GameEventTest, BlockDies)
{
	Block& blue_block = pit->spawn_block(Color::BLUE, RowCol{0, 0}, Block::State::REST);
	blue_block.set_state(Physical::State::BREAK, BREAK_TIME);
	run_game_ticks(BREAK_TIME);
	EXPECT_EQ(1, counter.countBlockDies);

	Block& fake_block = pit->spawn_block(Color::FAKE, RowCol{0, 0}, Block::State::REST);
	fake_block.set_state(Physical::State::BREAK, BREAK_TIME);
	run_game_ticks(BREAK_TIME);
	EXPECT_EQ(1, counter.countBlockDies);
}

/**
 * Tests whether a garbage dissolve event is correctly generated by the BlockDirector.
 */
TEST_F(GameEventTest, GarbageDissolves)
{
	pit->spawn_block(Color::BLUE, RowCol{0, 0}, Block::State::REST);
	pit->spawn_block(Color::BLUE, RowCol{0, 1}, Block::State::REST);
	pit->spawn_block(Color::BLUE, RowCol{0, 3}, Block::State::REST);
	pit->spawn_garbage(RowCol{-1, 2}, 3, 1, rainbow_loot(3));

	swap_at(*pit, *block_director, RowCol{0, 2});
	run_game_ticks(SWAP_TIME + DISSOLVE_TIME);
	EXPECT_EQ(1, counter.countGarbageDissolves);
}

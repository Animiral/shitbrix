/**
 * Tests for screens.
 */

#include "screen.hpp"
#include "game.hpp"
#include "tests_common.hpp"
#include "gtest/gtest.h"

class ScreenTest : public ::testing::Test
{

protected:

	virtual void SetUp() override
	{
		configure_context_for_testing();

		game = std::make_unique<LocalGame>();
		game->game_reset(2);
		game->game_start();
		const GameState& state = game->state();
		draw = std::make_unique<NoDraw>();
		auto stage = std::make_unique<Stage>(state, *draw);
		game_screen = std::make_unique<GameScreen>(std::move(stage), *game);
	}

	// virtual void TearDown() {}

	std::unique_ptr<IGame> game;
	std::unique_ptr<IDraw> draw;
	std::unique_ptr<GameScreen> game_screen;

};

/**
 * When the before_reset event comes from the game, the game screen must exit.
 */
TEST_F(ScreenTest, GameScreenDoneOnReset)
{
	game->game_reset(2);
	game->poll(); // technically correct, but not required for local game
	EXPECT_TRUE(game_screen->done());
	EXPECT_NO_THROW(game_screen->update()); // we can still update the screen without the game
	EXPECT_NO_THROW(game_screen->draw(0.f)); // we can still draw the screen without the game
}

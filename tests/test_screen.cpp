/**
 * Tests for screens.
 */

#include "screen.hpp"
#include "draw.hpp"
#include "game.hpp"
#include "agent.hpp"
#include "tests_common.hpp"

class ScreenTest : public ::testing::Test
{

public:

	explicit ScreenTest()
	{
		game = std::make_shared<LocalGame>(std::make_unique<LocalGameFactory>());
		const Rules rules;
		game->game_reset(2, rules, false);
		game->game_start();
		draw = std::make_unique<NoDraw>();
		game_screen = std::make_unique<GameScreen>(*draw, game, rules, nullptr, nullptr);
	}

protected:

	std::shared_ptr<IGame> game;
	std::unique_ptr<IDraw> draw;
	std::unique_ptr<GameScreen> game_screen;

};

/**
 * When the before_reset event comes from the game, the game screen must exit.
 * It must also not access the game again.
 */
TEST_F(ScreenTest, GameScreenDoneOnReset)
{
	const Rules rules;
	game->game_reset(2, rules, false);
	game->poll(); // technically correct, but not required for local game
	EXPECT_TRUE(game_screen->done());
	EXPECT_NO_THROW(game_screen->update()); // we can still update the screen without the game
	EXPECT_NO_THROW(game_screen->draw(0.f)); // we can still draw the screen without the game

	// the same holds after INTRO_TIME ticks
	for(int i = 0; i < INTRO_TIME; i++) {
		EXPECT_NO_THROW(game_screen->update());
		EXPECT_NO_THROW(game_screen->draw(0.f));
	}

	EXPECT_NO_THROW(game_screen->stop());
}

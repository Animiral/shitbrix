/**
 * Tests for the game logic implementation in BlockDirector.
 */

#include "game.hpp"
#include "tests_common.hpp"
#include "gtest/gtest.h"

class GameTest : public ::testing::Test
{

protected:

	virtual void SetUp() override
	{
		configure_context_for_testing();

		local_game.reset(new LocalGame{});
		auto channels = make_test_channels(1);
		ClientProtocol client_protocol{move(channels.second[0])};
		ServerProtocol server_protocol{move(channels.first)};
		client_game.reset(new ClientGame{std::move(client_protocol)});
		server_game.reset(new ServerGame{std::move(server_protocol)});
	}

	// virtual void TearDown() {}


	std::unique_ptr<LocalGame> local_game;
	std::unique_ptr<ClientGame> client_game;
	std::unique_ptr<ServerGame> server_game;

};

/**
 * When we tell the LocalGame to @c game_reset(), it must become ready.
 */
TEST_F(GameTest, LocalGameReady)
{
	EXPECT_FALSE(local_game->switches().ready);
	local_game->game_reset(2);
	EXPECT_TRUE(local_game->switches().ready);
}

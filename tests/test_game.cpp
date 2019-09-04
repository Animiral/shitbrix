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

		local_game.reset(new LocalGame{std::make_unique<LocalGameFactory>()});
		auto channels = make_test_channels(1);
		auto client_protocol = std::make_unique<ClientProtocol>(move(channels.second[0]));
		auto server_protocol = std::make_unique<ServerProtocol>(move(channels.first));
		this->client_protocol = client_protocol.get(); // preserve access for later
		this->server_protocol = server_protocol.get(); // preserve access for later
		auto client_factory = std::make_unique<ClientGameFactory>();
		client_game.reset(new ClientGame{move(client_factory), std::move(client_protocol)});
		auto server_factory = std::make_unique<ServerGameFactory>(*server_protocol);
		server_game.reset(new ServerGame{move(server_factory), std::move(server_protocol)});
	}

	// virtual void TearDown() {}

	std::unique_ptr<LocalGame> local_game;
	ClientProtocol* client_protocol;
	ServerProtocol* server_protocol;
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

/**
 * When we tell the LocalGame to @c game_reset(), it must call the registered handler.
 */
TEST_F(GameTest, LocalGameBeforeReset)
{
	bool ready = true;
	local_game->before_reset([&ready, this]() { ready = local_game->switches().ready; });
	local_game->game_reset(2);
	EXPECT_FALSE(ready);
}

/**
 * When we tell the LocalGame to @c game_start(), it must call the registered handler.
 */
TEST_F(GameTest, LocalGameAfterStart)
{
	bool ingame = false;
	local_game->after_start([&ingame, this]() { ingame = local_game->switches().ingame; });
	local_game->game_reset(2);
	local_game->game_start();
	EXPECT_TRUE(ingame);
}

/**
 * When the ClientGame receives a meta() message, it must call the registered handler.
 */
TEST_F(GameTest, ClientGameBeforeReset)
{
	bool ready = true;
	client_game->before_reset([&ready, this]() { ready = client_game->switches().ready; });
	server_protocol->meta(GameMeta{2,0});
	client_game->poll();
	EXPECT_FALSE(ready);
}

/**
 * When the ClientGame receives a start() message, it must call the registered handler.
 */
TEST_F(GameTest, ClientGameAfterStart)
{
	bool ingame = false;
	client_game->after_start([&ingame, this]() { ingame = client_game->switches().ingame; });
	server_protocol->meta(GameMeta{2,0});
	server_protocol->start();
	client_game->poll();
	EXPECT_TRUE(ingame);
}

/**
 * When we tell the ServerGame to @c game_reset(), it must call the registered handler.
 */
TEST_F(GameTest, ServerGameBeforeReset)
{
	bool ready = true;
	server_game->before_reset([&ready, this]() { ready = server_game->switches().ready; });
	server_game->game_reset(2);
	EXPECT_FALSE(ready);
}

/**
 * When we tell the ServerGame to @c game_start(), it must call the registered handler.
 */
TEST_F(GameTest, ServerGameAfterStart)
{
	bool ingame = false;
	server_game->after_start([&ingame, this]() { ingame = server_game->switches().ingame; });
	server_game->game_reset(2);
	server_game->game_start();
	EXPECT_TRUE(ingame);
}

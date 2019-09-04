/**
 * Tests for the game logic implementation in BlockDirector.
 */

#include "game.hpp"
#include "state.hpp"
#include "arbiter.hpp"
#include "replay.hpp"
#include "tests_common.hpp"
#include "gtest/gtest.h"

using testing::_;
using testing::An;
using testing::Return;

/**
 * This factory produces objects for use in the game and exposes them to the
 * running test for inspection.
 */
class TestingGameFactory : public IGameFactory
{

public:

	virtual void create(GameMeta meta) override
	{
		base_create(meta);

		auto arbiter = std::make_unique<MockArbiter>();

		m_state_ptr = m_state.get();
		m_journal_ptr = m_journal.get();
		m_director_ptr = m_director.get();
		m_hub_ptr = m_hub.get();
		m_arbiter_ptr = arbiter.get();

		m_arbiter = move(arbiter);
		m_hub->subscribe(*m_arbiter);
	}

	// free access to the created objects
	GameState* m_state_ptr;
	Journal* m_journal_ptr;
	BlockDirector* m_director_ptr;
	evt::GameEventHub* m_hub_ptr;
	MockArbiter* m_arbiter_ptr;

};

class GameTest : public ::testing::Test
{

protected:

	virtual void SetUp() override
	{
		configure_context_for_testing();

		auto factory = std::make_unique<TestingGameFactory>();
		m_local_factory = factory.get();
		local_game.reset(new LocalGame{move(factory)});

		auto channels = make_test_channels(1);
		auto client_protocol = std::make_unique<ClientProtocol>(move(channels.second[0]));
		auto server_protocol = std::make_unique<ServerProtocol>(move(channels.first));
		this->client_protocol = client_protocol.get(); // preserve access for later
		this->server_protocol = server_protocol.get(); // preserve access for later

		factory.reset(new TestingGameFactory());
		m_client_factory = factory.get();
		client_game.reset(new ClientGame{move(factory), std::move(client_protocol)});
		factory.reset(new TestingGameFactory());
		m_server_factory = factory.get();
		server_game.reset(new ServerGame{move(factory), std::move(server_protocol)});
	}

	TestingGameFactory* m_local_factory; //!< access to internals of local_game
	std::unique_ptr<LocalGame> local_game;

	ClientProtocol* client_protocol;
	TestingGameFactory* m_client_factory; //!< access to internals of client_game
	std::unique_ptr<ClientGame> client_game;

	ServerProtocol* server_protocol;
	TestingGameFactory* m_server_factory; //!< access to internals of server_game
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

/**
 * The @c synchronurse function changes the state to the target time, even if
 * the target is in the past.
 */
TEST_F(GameTest, SynchronurseBackwards)
{
	local_game->game_reset(2);
	local_game->game_start();

	local_game->synchronurse(2); // forward
	EXPECT_EQ(m_local_factory->m_state_ptr->game_time(), 2);
	local_game->synchronurse(1); // backward
	EXPECT_EQ(m_local_factory->m_state_ptr->game_time(), 1);
}

/**
 * When we use the @c synchronurse function to advance the game state, it must
 * be able to pick up additional inputs generated during execution of game
 * logic by the arbiter.
 */
TEST_F(GameTest, SynchronurseHandlesArbiterInputs)
{
	local_game->game_reset(2);
	local_game->game_start();

	const RowCol start_rc = m_local_factory->m_state_ptr->pit().at(0)->cursor().rc;

	// between t=0 and t=2, move the cursor one right
	local_game->game_input(Input{PlayerInput{1, 0, GameButton::RIGHT, ButtonAction::DOWN}});
	local_game->game_input(Input{PlayerInput{2, 0, GameButton::RIGHT, ButtonAction::UP}});

	// Since the pits are empty, they will immediately send starve events.
	// One of them will be dealt with by our mock, the other triggers in both updates. -> 3 calls
	auto spawn_input = [this](evt::Starve e) {
		std::array<Color, PIT_COLS> colors;
		colors.fill(Color::BLUE);
		m_local_factory->m_journal_ptr->add_input(Input{SpawnBlockInput{2, 0, 1, colors}});
	};
	EXPECT_CALL(*m_local_factory->m_arbiter_ptr,
		fire(An<evt::Starve>()))
		.Times(3)
		.WillOnce(spawn_input)
		.WillRepeatedly(Return());

	local_game->synchronurse(2); // must process 2 given inputs and the third generated one

	// re-request pit because synchronurse may have re-allocated it
	const Pit& pit = *m_local_factory->m_state_ptr->pit().at(0);

	EXPECT_EQ(start_rc.c + 1, pit.cursor().rc.c); // verify application of cursor move
	for(int i = 0; i < PIT_COLS; i++) {
		EXPECT_TRUE(pit.block_at({1, i})); // verify application of spawn
	}
}

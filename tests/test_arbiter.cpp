/**
 * Tests for the different Arbiter implementations, which give additional inputs
 * to the game depending on nondeterministic factors.
 */

#include "arbiter.hpp"
#include "replay.hpp"
#include "input.hpp"
#include "tests_common.hpp"
#include "gtest/gtest.h"

using testing::Truly;

class ArbiterTest : public ::testing::Test
{

protected:

	virtual void SetUp()
	{
		configure_context_for_testing();
	}

	// virtual void TearDown() {}

	GameMeta meta{2, 0};
	GameState state{meta};
	Journal journal{meta, state};
};


/**
 * When a Starve event reaches the LocalArbiter, it must generate a
 * @c SpawnBlockInput to fill the Pit with new blocks according to its
 * random generator.
 */
TEST_F(ArbiterTest, LocalArbiterSpawnBlocksOnStarve)
{
	LocalArbiter arbiter{state, journal, std::make_unique<RandomColorSupplier>(0, 0)};

	arbiter.fire(evt::Starve{{1, 0}});

	const Inputs& inputs = journal.inputs();
	EXPECT_EQ(1, inputs.size());
}

/**
 * When a Chain event reaches the LocalArbiter, it must generate a
 * @c SpawnGarbageInput to throw a new Garbage block containing loot according
 * to its random generator.
 */
TEST_F(ArbiterTest, LocalArbiterSpawnGarbageOnChain)
{
	LocalArbiter arbiter{state, journal, std::make_unique<RandomColorSupplier>(0, 0)};

	int chain_counter = 3;
	arbiter.fire(evt::Chain{{1, 0}, chain_counter});

	const Inputs& inputs = journal.inputs();
	ASSERT_EQ(1, inputs.size());
	const SpawnGarbageInput& sgi = inputs[0].input.get<SpawnGarbageInput>();
	EXPECT_EQ(sgi.loot.size(), PIT_COLS * chain_counter);
}

/**
 * When a Chain event reaches the LocalArbiter, the counter must be greater
 * than 0 to warrant a gameplay reward.
 */
TEST_F(ArbiterTest, LocalArbiterIgnore0Chain)
{
	LocalArbiter arbiter{state, journal, std::make_unique<RandomColorSupplier>(0, 0)};

	int chain_counter = 0;
	arbiter.fire(evt::Chain{{1, 0}, chain_counter});

	const Inputs& inputs = journal.inputs();
	ASSERT_EQ(0, inputs.size());
}

/**
 * When a Starve event reaches the ServerArbiter, it must send INPUT messages
 * with a @c SpawnBlockInput to all connected clients to fill the Pit with new
 * blocks according to its random generator.
 */
TEST_F(ArbiterTest, ServerArbiterSendSpawnBlocksOnStarve)
{
	auto channels = make_test_channels(1);
	ServerProtocol server_protocol{move(channels.first)};
	ServerArbiter arbiter{server_protocol, state, journal, std::make_unique<RandomColorSupplier>(0, 0)};

	arbiter.fire(evt::Starve{{1, 0}});

	// The appropriate inputs must be in the local journal
	const Inputs& inputs = journal.inputs();
	EXPECT_EQ(1, inputs.size());

	// The appropriate messages must have been sent to the clients
	MockServerMessages recipient;
	ClientProtocol client_protocol{move(channels.second[0])};
	auto matches_input = [](Input i) { auto pi = i.get<SpawnBlockInput>(); return 1 == pi.game_time && 0 == pi.player && 1 == pi.row; };
	EXPECT_CALL(recipient, input(Truly(matches_input))).Times(1);

	client_protocol.poll(recipient);
}

/**
 * When a Chain event reaches the ServerArbiter, it must send INPUT messages
 * with @c SpawnGarbageInput messages to all connected clients to place a new
 * Garbage block containing loot according to its random generator.
 */
TEST_F(ArbiterTest, ServerArbiterSendSpawnGarbageOnChain)
{
	auto channels = make_test_channels(1);
	ServerProtocol server_protocol{move(channels.first)};
	ServerArbiter arbiter{server_protocol, state, journal, std::make_unique<RandomColorSupplier>(0, 0)};

	int chain_counter = 3;
	arbiter.fire(evt::Chain{{1, 0}, chain_counter});
	
	// The appropriate input must be in the local journal
	const Inputs& inputs = journal.inputs();
	ASSERT_EQ(1, inputs.size());
	const SpawnGarbageInput& sgi = inputs[0].input.get<SpawnGarbageInput>();
	EXPECT_EQ(sgi.loot.size(), PIT_COLS * chain_counter);

	// The appropriate message must have been sent to the clients
	MockServerMessages recipient;
	ClientProtocol client_protocol{move(channels.second[0])};
	auto matches_input = [chain_counter](Input i) { auto sgi = i.get<SpawnGarbageInput>(); return 2 == sgi.game_time && 1 == sgi.player && chain_counter == sgi.rows && PIT_COLS == sgi.columns; };
	EXPECT_CALL(recipient, input(Truly(matches_input))).Times(1);

	client_protocol.poll(recipient);
}
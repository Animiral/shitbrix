/**
 * Tests for the different Arbiter implementations, which give additional inputs
 * to the game depending on nondeterministic factors.
 */

#include "arbiter.hpp"
#include "tests_common.hpp"
#include "gtest/gtest.h"

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
 * When a Starve event reaches the ServerArbiter, it must send INPUT messages
 * with a @c SpawnBlockInput to all connected clients to fill the Pit with new
 * blocks according to its random generator.
 */
TEST_F(ArbiterTest, ServerArbiterSendSpawnBlocksOnStarve)
{
	ENetServer server{DEFAULT_PORT};
	ENetClient client{"localhost", DEFAULT_PORT};
	ServerArbiter arbiter{server, state, journal, std::make_unique<RandomColorSupplier>(0, 0)};

	arbiter.fire(evt::Starve{{1, 0}});

	// The appropriate inputs must be in the local journal
	const Inputs& inputs = journal.inputs();
	EXPECT_EQ(1, inputs.size());

	// The appropriate messages must have been sent to the clients
	const std::vector<Message> messages = client.poll();
	ASSERT_EQ(1, messages.size());
	const Message message = messages[0];
	ASSERT_EQ(MsgType::INPUT, message.type);
	const Input input{message.data};
	EXPECT_NO_THROW(input.get<SpawnBlockInput>());
}

/**
 * When a Chain event reaches the ServerArbiter, it must send INPUT messages
 * with @c SpawnGarbageInput messages to all connected clients to place a new
 * Garbage block containing loot according to its random generator.
 */
TEST_F(ArbiterTest, ServerArbiterSendSpawnGarbageOnChain)
{
	ENetServer server{DEFAULT_PORT};
	ENetClient client{"localhost", DEFAULT_PORT};
	ServerArbiter arbiter{server, state, journal, std::make_unique<RandomColorSupplier>(0, 0)};

	int chain_counter = 3;
	arbiter.fire(evt::Chain{{1, 0}, chain_counter});
	
	// The appropriate input must be in the local journal
	const Inputs& inputs = journal.inputs();
	ASSERT_EQ(1, inputs.size());
	const SpawnGarbageInput& sgi = inputs[0].input.get<SpawnGarbageInput>();
	EXPECT_EQ(sgi.loot.size(), PIT_COLS * chain_counter);

	// The appropriate message must have been sent to the clients
	const std::vector<Message> messages = client.poll();
	ASSERT_EQ(1, messages.size());
	const Message message = messages[0];
	ASSERT_EQ(MsgType::INPUT, message.type);
	const Input input{message.data};
	EXPECT_NO_THROW(input.get<SpawnGarbageInput>());
}

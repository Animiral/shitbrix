/**
 * tests_common.hpp
 * Definitions for shared helpers for unit tests.
 */
#pragma once

#include "stage.hpp"
#include "director.hpp"
#include "network.hpp"
#include "gmock/gmock.h"

/**
 * Set the global context to use stub implementations for our test environment.
 */
void configure_context_for_testing();

/**
 * Create a game context for testing game scenarios.
 */
GameData make_gamedata_for_testing();


// helper function for generating non-random loot for garbage bricks
std::vector<Color> rainbow_loot(size_t count);


// helper function for spawning garbage with generic rainbow loot
Garbage& spawn_garbage(Pit& pit, RowCol rc, int columns, int rows);

/**
 * Swap the blocks at the specified location, regardless of the current cursor position.
 * This is not normally allowed in the game (the cursor does not give random access).
 * @return success of the swapping, just like @c BlockDirector::swap.
 */
bool swap_at(Pit& pit, BlockDirector& director, RowCol rc);

/**
 * Place @c PURPLE and @c ORANGE blocks into rows 1-3 in the pit in a
 * checkerboard pattern. This provides a default floor for tests.
 */
void prefill_pit(Pit& pit);

/**
 * A shortcut implementation of a network channel for testing purposes.
 * It simply forwards all messages to one or more other @c TestChannels,
 * where they can be picked up immediately.
 */
class TestChannel : public IChannel
{

public:

	void add_recipient(TestChannel& channel);

	virtual void send(Message message) override;
	virtual std::vector<Message> poll() override;

private:

	std::vector<Message> m_buffer; //!< list of my pending messages
	std::vector<TestChannel*> m_recipients; //!< targets for my sent messages

};

/**
 * Creates one server and one or more client channels for testing purposes.
 * The server and client channels are connected as one would expect.
 */
std::pair<std::unique_ptr<IChannel>, std::vector<std::unique_ptr<IChannel>>> make_test_channels(int clients);

/**
 * Mock for examining interaction with messages from the server.
 */
class MockServerMessages : public IServerMessages
{

public:

	MOCK_METHOD(void, meta, (GameMeta meta), (override));
	MOCK_METHOD(void, input, (Input input), (override));
	MOCK_METHOD(void, speed, (int speed), (override));
	MOCK_METHOD(void, start, (), (override));
	MOCK_METHOD(void, gameend, (int winner), (override));

};

/**
 * Mock for examining interaction with messages from the client.
 */
class MockClientMessages : public IClientMessages
{

public:

	MOCK_METHOD(void, meta, (GameMeta meta), (override));
	MOCK_METHOD(void, input, (Input input), (override));
	MOCK_METHOD(void, speed, (int speed), (override));
	MOCK_METHOD(void, start, (), (override));

};

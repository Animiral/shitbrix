/**
 * Shared helper functions for unit tests.
 */

#include "tests_common.hpp"
#include "arbiter.hpp"
#include "game.hpp"
#include "replay.hpp"
#include "context.hpp"
#include "configuration.hpp"
#include "asset.hpp"
#include "audio.hpp"
#include "error.hpp"

void configure_context_for_testing()
{
	// Destroy any leftover context from previous test runs.
	// This is especially important for objects that own an only-once resource (e.g. SDL)
	the_context.sdl.reset();
	the_context.log.reset();
	the_context.assets.reset();
	the_context.audio.reset();

	Configuration configuration;
	configuration.network_mode = /* NetworkMode::LOCAL */ NetworkMode::CLIENT; // TODO: use least-harm setting
	configuration.player_number = {};
	configuration.joystick_number = {};
	configuration.autorecord = false;
	configuration.replay_path = {};
	configuration.log_path = std::filesystem::path{};
	configuration.server_url = {};

	the_context.configuration.reset(new Configuration(std::move(configuration)));
	the_context.sdl.reset(new Sdl(0));
	the_context.log = create_no_log();
	the_context.assets.reset(new NoAssets);
	the_context.audio.reset(new NoAudio);
}

std::unique_ptr<IGame> make_game_for_testing()
{
	return std::make_unique<LocalGame>(std::make_unique<LocalGameFactory>());
}

std::vector<Color> rainbow_loot(size_t count)
{
	std::vector<Color> result;

	for(int i = 0; i < count; i++)
		result.push_back(static_cast<Color>((i % 6) + 1));

	return result;
}


Garbage& spawn_garbage(Pit& pit, RowCol rc, int columns, int rows)
{
	return pit.spawn_garbage(rc, columns, rows, rainbow_loot((size_t)columns * (size_t)rows));
}

bool swap_at(Pit& pit, BlockDirector& director, RowCol rc)
{
	const_cast<Cursor&>(pit.cursor()).rc = rc;
	director.apply_input(Input(PlayerInput{0, 0, GameButton::SWAP, ButtonAction::DOWN}));

	if(Block* block = pit.block_at(rc))
		return Block::State::SWAP_LEFT == block->block_state();
	else
		return false;
}

void prefill_pit(Pit& pit)
{
	for(int c = 0; c < PIT_COLS; c++)
		for(int r = 1; r <= 3; r++) {
			Color color = (c + r) % 2 ? Color::PURPLE : Color::ORANGE;
			pit.spawn_block(color, {r, c}, Block::State::PREVIEW);
		}
}


void TestChannel::add_recipient(TestChannel& channel)
{
	m_recipients.push_back(&channel);
}

void TestChannel::send(Message message)
{
	for(TestChannel* recipient : m_recipients)
		recipient->m_buffer.push_back(message);
}

std::vector<Message> TestChannel::poll()
{
	std::vector<Message> messages;
	swap(m_buffer, messages);
	return messages;
}


std::pair<std::unique_ptr<IChannel>, std::vector<std::unique_ptr<IChannel>>> make_test_channels(int clients)
{
	enforce(clients >= 0);

	auto server_channel = std::make_unique<TestChannel>();
	std::vector<std::unique_ptr<IChannel>> client_channels;

	for(int i = 0; i < clients; i++) {
		auto client_channel = std::make_unique<TestChannel>();
		server_channel->add_recipient(*client_channel);
		client_channel->add_recipient(*server_channel);
		client_channels.push_back(move(client_channel));
	}

	return {move(server_channel), move(client_channels)};
}

// get rid of interfering SDL macro
#ifdef main
#undef main
#endif

/**
 * Unit tests entry point.
 */
int main(int argc, char** argv)
{
	::testing::InitGoogleTest(&argc, argv);
	configure_context_for_testing();
	return RUN_ALL_TESTS();
}

/**
 * Shared helper functions for unit tests.
 */

#include "tests_common.hpp"
#include "context.hpp"
#include "configuration.hpp"
#include "sdl_helper.hpp"
#include "asset.hpp"
#include "audio.hpp"
#include "error.hpp"

void configure_context_for_testing()
{
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

Block::Color RainbowBlocksQueue::next() noexcept
{
	Block::Color previous = m_color;
	m_color = static_cast<Block::Color>((static_cast<int>(m_color) + 1 - 1) % 6 + 1);
	return previous;
}

void RainbowBlocksQueue::backtrack(size_t index) noexcept
{
	m_color = static_cast<Block::Color>(index % 6 + 1);
}

bool swap_at(Pit& pit, BlockDirector& director, RowCol rc)
{
	const_cast<Cursor&>(pit.cursor()).rc = rc;
	return director.swap(0);
}

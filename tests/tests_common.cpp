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

GameData make_gamedata_for_testing()
{
	// we uniformly use a 2-player deterministic play field
	GameMeta meta{2, 0};
	ColorSupplierFactory color_factory = [](int) { return std::make_unique<RainbowColorSupplier>(); };
	GameState state{meta, color_factory};
	Journal journal{meta, state};

	return GameData{std::move(state), std::move(journal)};
}

Color RainbowColorSupplier::next_spawn() noexcept
{
	Color previous = m_color;
	m_color = static_cast<Color>((static_cast<int>(m_color) + 1 - 1) % 6 + 1);
	return previous;
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
	return pit.spawn_garbage(rc, columns, rows, rainbow_loot(columns * rows));
}

bool swap_at(Pit& pit, BlockDirector& director, RowCol rc)
{
	const_cast<Cursor&>(pit.cursor()).rc = rc;
	director.apply_input(Input(PlayerInput{0, 0, GameButton::SWAP, ButtonAction::DOWN}));

	if(Block* block = pit.block_at(rc))
		return Block::State::SWAP_RIGHT == block->block_state();
	else
		return false;
}

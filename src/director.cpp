/**
 * Implementation of director logic.
 */

#include "director.hpp"
#include "replay.hpp"
#include "error.hpp"
#include <cassert>

// for debug functions
#include <chrono>
#include <ctime>
#include <fstream>

// elemental game logic functions and helpers
namespace
{

/**
 * New blocks in *preview* state appear at the bottom of the pit as it scrolls.
 * At the same time, the previous previews become normal blocks at rest.
 * In this instant, they are tagged as *hot*.
 */
bool spawn_previews(Pit& pit);

/**
 * Place a new garbage block, ready to fall, into the given pit.
 */
void spawn_garbage(Pit& pit, int columns, int rows, bool right_side);

/**
 * Place new garbage blocks, ready to fall, based on a given combo achievement.
 */
void spawn_garbage_from_combo(Pit& pit, int combo);

/**
 * Write a dump file about the given game state.
 * The file path is built out of the optional dump/ directory, the time at which
 * the dump was written and the game_time of the state.
 * If any step does not work, do nothing.
 */
[[ maybe_unused ]]
void debug_dump_state(const GameState& state);

}

BlockDirector::BlockDirector()
: m_state(nullptr),
  m_handler(nullptr)
{}

void BlockDirector::update()
{
	for(int player = 0; player < m_state->pit().size(); player++)
		update_single(player);
}

bool BlockDirector::swap(int player)
{
	Pit& pit = *m_state->pit().at(player);
	const RowCol lrc = pit.cursor().rc; // left row/column
	const RowCol rrc {lrc.r, lrc.c+1}; // right row/column

	// bounds check
	assert(lrc.r >= pit.top());
	assert(lrc.r <= pit.bottom());
	assert(lrc.c >= 0);
	assert(lrc.c <= PIT_COLS - 2);

	Physical* left_phys = pit.at(lrc);
	Physical* right_phys = pit.at(rrc);
	Block* left = dynamic_cast<Block*>(left_phys);
	Block* right = dynamic_cast<Block*>(right_phys);

	if(!left && !right) return false; // 2 non-blocks

	// Both locations must be swappable. For a location to be swappable,
	// it must either contain a swappable block or nothing at all
	// (so that we can substitute a fake block).
	bool left_swappable = left ? left->is_swappable() : !left_phys;
	bool right_swappable = right ? right->is_swappable() : !right_phys;

	if(!left_swappable || !right_swappable) return false;

	// fake blocks - they last only for the duration of the swap, blocking other
	// falling blocks from going through the space.
	if(!left) left = &pit.spawn_block(Block::Color::FAKE, lrc, Block::State::REST);
	if(!right) right = &pit.spawn_block(Block::Color::FAKE, rrc, Block::State::REST);

	// do swap
	left->set_state(Block::State::SWAP_RIGHT, SWAP_TIME);
	right->set_state(Block::State::SWAP_LEFT, SWAP_TIME);
	pit.swap(*left, *right);

	if(m_handler)
		m_handler->fire(evt::Swap{{m_state->game_time(), player}});

	return true;
}

void BlockDirector::debug_spawn_garbage(int columns, int rows)
{
	Pit& pit = *m_state->pit().at(0); // first pit
	int spawn_row = std::min(pit.peak(), pit.top()) - rows - 2;
	pit.spawn_garbage(RowCol{spawn_row, 0}, columns, rows);
}

void BlockDirector::update_single(int player)
{
	// TODO: Is this function still ripe for refactoring?
	//
	// Previous problems:
	// It allocates and deallocates memory every frame, which could be eliminated.
	// The names of the helper functions are nondescriptive and inconsistent (handle_*)
	// Helper return values use out parameters.
	//
	// Measure to check:
	// 1. Rename handle_* -> mark_* if blocks are to be marked, or examine_* if state is to be determined.

	assert(m_state);

	// Target player's pit object
	Pit& pit = *m_state->pit().at(player);

	// Implementation object for low-level pit examination
	Logic logic{pit};

	const long game_time = m_state->game_time();

	bool dead_physical = false; // true if some physical has entered terminal state
	bool dead_block = false;    // true if pit needs to clean up
	bool dead_sound = false;    // true if there was at least one non-fake dead
	bool chainstop = false;     // true if the pit should be examined for chain finish

	pit.untag_all();

	bool new_row = spawn_previews(pit);

	// raise until new row, except if player is holding down the button
	if(new_row)
		pit.stop_raise();

	logic.examine_finish(dead_physical, dead_block, dead_sound, chainstop);

	auto& pit_contents = pit.contents();
	const bool have_dissolvers = std::any_of(begin(pit_contents), end(pit_contents), [](const auto& p) { return p->has_tag(Physical::TAG_DISSOLVE); });
	dead_physical |= have_dissolvers;

	logic.convert_garbage();

	if(have_dissolvers && m_handler)
		m_handler->fire(evt::GarbageDissolves{{game_time, player}});

	if(dead_block)
		pit.remove_dead();

	if(dead_sound && m_handler)
		m_handler->fire(evt::BlockDies{{game_time, player}});

	logic.handle_fallers();

	if(m_handler) {
		pit.for_all(Physical::TAG_LAND, [this, game_time, player](const Physical& p) {
			m_handler->fire(evt::PhysicalLands{{game_time, player}, p}); });
	}

	bool have_match = false;
	bool chaining = false;
	int combo = 0;
	logic.handle_hots(have_match, combo, chaining, chainstop);

	if(have_match) {
		Pit& opponent_pit = *m_state->pit().at(opponent(player));
		spawn_garbage_from_combo(opponent_pit, combo);

		// trigger effects outside game state (these will not roll back)
		if(m_handler)
			m_handler->fire(evt::Match{{game_time, player}, combo, chaining});
	}

	if(chaining)
		pit.do_chain();

	bool recovering = false;
	if(chaining || combo > 3)
		pit.replenish_recovery();
	else
		recovering = pit.do_recovery() > 0;

	bool pit_chaining = false;  // true if there is any block in the pit currently chaining
	bool breaking = false;      // true if there is any physical in the pit currently breaking
	bool pit_full = false;      // true if some resting object overflows the pit

	logic.examine_pit(pit_chaining, breaking, pit_full);

	if(debug_no_gameover)
		pit_full = false; // debug function: in no-gameover mode, pit is never full

	// close current chain
	if(chainstop && !pit_chaining) {
		const int chain = pit.finish_chain();

		if(chain > 0) {
			Pit& opponent_pit = *m_state->pit().at(opponent(player));
			spawn_garbage(opponent_pit, PIT_COLS, chain, false);
		}

		// trigger effects outside game state (these will not roll back)
		if(m_handler)
			m_handler->fire(evt::Chain{{game_time, player}, chain});
	}

	// panic time and game over check
	if(pit_full) {
		if(!pit_chaining && !breaking && !recovering) {
			const bool panic = pit.do_panic() <= 0;
			if(panic && !debug_no_gameover)
				m_winner = opponent(player);
		}
	}
	else {
		pit.replenish_panic();
	}

	if(pit_full || pit_chaining || breaking || recovering)
		pit.stop();
	else
		pit.start();

	// debug: show what the pit considers to be its peak row
	pit.highlight(pit.peak());
}

int BlockDirector::opponent(int player) const noexcept
{
	assert(m_state);
	assert(0 == player || 1 == player || "more than two players not implemented yet");

	// In some test scenarios, we are playing with just one pit.
	// In those cases, we are our own opponent.
	if(1 == m_state->pit().size())
		return 0;
	else
		return 0 == player ? 1 : 0;
}


void apply_input(GameState& state, Rules& rules, GameInput ginput)
{
	assert(GameButton::NONE != ginput.button);
	Log::trace("%s %s", __FUNCTION__, ginput.to_string().c_str());

	switch(ginput.button) {
		case GameButton::LEFT:
		case GameButton::RIGHT:
		case GameButton::UP:
		case GameButton::DOWN:
			if(ButtonAction::DOWN == ginput.action)
			{
				Dir dir = static_cast<Dir>(ginput.button);
				state.pit().at(ginput.player)->cursor_move(dir);
				rules.event_hub.fire(evt::CursorMoves{{ginput.game_time, ginput.player}});
			}

			break;

		case GameButton::SWAP:
			if(ButtonAction::DOWN == ginput.action)
			{
				rules.block_director.swap(ginput.player);
			}

			break;

		case GameButton::RAISE:
			state.pit().at(ginput.player)->set_raise(ButtonAction::DOWN == ginput.action);
			break;

		case GameButton::NONE:
		default:
			assert(false);

	}
}

void synchronurse(GameState& state, long target_time, Journal& journal, Rules& rules)
{
	// get events from journal, from which inputs will be applied to the state
	const long time0 = journal.earliest_undiscovered();

	if(time0 < target_time) {
		state = journal.checkpoint_before(time0);
		Log::trace("%s(%d): revert to checkpoint before time=%d -> at time=%d.", __FUNCTION__, target_time, time0, state.game_time());
		debug_dump_state(state);
	}

	GameInputSpan inputs = journal.discover_inputs(state.game_time() + 1, target_time);
	auto input_it = inputs.first;

	while(state.game_time() < target_time && !rules.block_director.over()) {
		for(; input_it != inputs.second && input_it->input.game_time == state.game_time() + 1; ++input_it) {
			Log::trace("%s(%d): apply input %s.", __FUNCTION__, target_time, input_it->input.to_string().c_str());
			apply_input(state, rules, input_it->input);
		}

		// state.game_time() is incremented here.
		state.update();
		rules.block_director.update();
	}

	if(rules.block_director.over())
		return; // stop feeding the journal now

	// save new checkpoint?
	if(target_time >= journal.checkpoint_before(target_time).game_time() + CHECKPOINT_INTERVAL) {
		Log::trace("%s(%d): save checkpoint at time=%d.", __FUNCTION__, target_time, state.game_time());
		journal.add_checkpoint(GameState(state));
		debug_dump_state(state);
	}
}


// --- implementation of module-specific functions ---

namespace
{

bool spawn_previews(Pit& pit)
{
	// If there are no blocks in the pit’s bottom row, refill previews.
	// For gameplay to work properly, the pit scroll speed must be less
	// than one full row per tick.
	int bottom_row = pit.bottom() + 1;
	RowCol bottom_rc{bottom_row, 0};

	// If the bottom of the pit contains blocks, do not add a new row.
	if(pit.at(bottom_rc))
		return false;

	for(int c = 0; c < PIT_COLS; c++) {
		RowCol spawn_rc{bottom_row, c};
		pit.spawn_random_block(spawn_rc, Block::State::PREVIEW);

		RowCol above_rc{bottom_row-1, c};
		Block* above = pit.block_at(above_rc);

		// in the first spawn_previews, there might not be blocks above.
		if(above && Block::State::PREVIEW == above->block_state()) {
			above->set_state(Physical::State::REST);
			above->set_tag(Physical::TAG_HOT);
		}
	}

	return true;
}

void spawn_garbage(Pit& pit, int columns, int rows, bool right_side)
{
	assert(columns > 0);
	assert(columns <= PIT_COLS);

	int spawn_row = std::min(pit.peak(), pit.top()) - rows - 1;
	RowCol rc{spawn_row, right_side ? PIT_COLS-columns : 0};
	Garbage& garbage = pit.spawn_garbage(rc, columns, rows);
	garbage.set_state(Physical::State::FALL, ROW_HEIGHT, FALL_SPEED);
}

void spawn_garbage_from_combo(Pit& pit, int combo)
{
	int counter = combo - 3; // number of small blocks to drop
	bool right_side = false;

	while(counter > 0) {
		if(1 == counter) spawn_garbage(pit, 3, 1, right_side);
		else if(2 == counter) spawn_garbage(pit, 4, 1, right_side);
		else spawn_garbage(pit, 5, 1, right_side);

		counter -= 3;
		right_side = !right_side;
	}
}

[[ maybe_unused ]]
void debug_dump_state(const GameState& state)
{
	if(!std::filesystem::is_directory("dump"))
		return; // creating the replay directory is the user's opt-in

	// get dump file name with hundreths precision
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	std::time_t now_sec = std::chrono::system_clock::to_time_t(now);
	std::chrono::milliseconds now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
	const struct tm* local_now = std::localtime(&now_sec);

	if(nullptr == local_now)
		return;

	const size_t NOW_BUFSIZE = 20;
	std::string now_buffer(NOW_BUFSIZE, '\0');
	const size_t strftime_size = strftime(&now_buffer[0], NOW_BUFSIZE, "%H-%M-%S", local_now);

	if(0 >= strftime_size)
		return;

	const size_t PATH_BUFSIZE = 40;
	std::string path(PATH_BUFSIZE, '\0');
	const int sprintf_result = sprintf(&path[0], "dump/state_%s.%03d_%d.txt", now_buffer.c_str(), (int)(now_ms.count() % 1000), state.game_time());

	if(sprintf_result < 0)
		return;

	path.resize(sprintf_result); // drop trailing '\0's from sprintf

	// We never overwrite dumps.
	if(std::filesystem::exists(path))
		return;

	std::ofstream stream(path);
	debug_asciiart_state(stream, state);
}

}

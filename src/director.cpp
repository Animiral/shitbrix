/**
 * Implementation of director logic.
 */

#include "director.hpp"
#include "replay.hpp"

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
 * Given the number of one player in the game, returns the target opponent.
 * If the given player loses, the opponent wins.
 * If the given player produces a combo or chain, the opponent receives garbage.
 */
int opponent(int player);

}

BlockDirector::BlockDirector(GameState& state)
: m_state(&state),
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
		m_handler->fire(evt::Swap());

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

	// Target player's pit object
	Pit& pit = *m_state->pit().at(player);

	// Implementation object for low-level pit examination
	Logic logic{pit};

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
		m_handler->fire(evt::GarbageDissolves());

	if(dead_block)
		pit.remove_dead();

	if(dead_sound && m_handler)
		m_handler->fire(evt::BlockDies());

	logic.handle_fallers();

	if(m_handler) {
		pit.for_all(Physical::TAG_LAND, [this](const Physical& p) {
			m_handler->fire(evt::PhysicalLands{p}); });
	}

	bool have_match = false;
	bool chaining = false;
	int combo = 0;
	logic.handle_hots(have_match, combo, chaining, chainstop);

	if(have_match && m_handler)
		m_handler->fire(evt::Match{combo, chaining});

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

	// close current chain
	if(chainstop && m_handler && !pit_chaining) {
		m_handler->fire(evt::Chain{pit.finish_chain()});
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


void apply_input(GameState& state, Rules& rules, GameInput ginput)
{
	assert(GameButton::NONE != ginput.button);
	Log::trace(__FUNCTION__ " %s", ginput.to_string().c_str());

	switch(ginput.button) {
		case GameButton::LEFT:
		case GameButton::RIGHT:
		case GameButton::UP:
		case GameButton::DOWN:
			if(ButtonAction::DOWN == ginput.action)
			{
				Dir dir = static_cast<Dir>(ginput.button);
				state.pit().at(ginput.player)->cursor_move(dir);
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

/**
 * Bring the game state to the @c target_time by calculation from the game
 * journal and the game rules.
 */
void synchronurse(GameState& state, long target_time, Journal& journal, Rules& rules)
{
	// get events from journal, from which inputs will be relayed to the phase
	// TODO: use checkpoints to re-sync on historic inserts
	const long time0 = journal.earliest_undiscovered();

	if(time0 < target_time) {
		state = journal.checkpoint_before(time0);
		Log::trace(__FUNCTION__ "(%d): revert to checkpoint before time=%d -> at time=%d.", target_time, time0, state.game_time());
	}

	GameInputSpan inputs = journal.discover_inputs(state.game_time() + 1, target_time);
	auto input_it = inputs.first;

	while(state.game_time() < target_time && !rules.block_director.over()) {
		for(; input_it != inputs.second && input_it->input.game_time == state.game_time() + 1; ++input_it) {
			apply_input(state, rules, input_it->input);
		}

		// state.game_time() is incremented here.
		state.update();
		rules.block_director.update();
	}

	// save new checkpoint?
	if(target_time >= journal.checkpoint_before(target_time).game_time() + CHECKPOINT_INTERVAL) {
		Log::trace(__FUNCTION__ "(%d): save checkpoint at time=%d.", target_time, state.game_time());
		journal.add_checkpoint(GameState(state));
	}
}


void CursorDirector::move(Dir dir)
{
	enforce(Dir::NONE != dir);

	m_pit.cursor_move(dir);

	if(m_handler)
		m_handler->fire(evt::CursorMoves());
}


void GarbageThrow::fire(evt::Match event)
{
	int combo = event.combo - 3;
	bool right_side = false;

	while(combo > 0) {
		if(1 == combo) spawn(3, 1, right_side);
		else if(2 == combo) spawn(4, 1, right_side);
		else spawn(5, 1, right_side);

		combo -= 3;
		right_side = !right_side;
	}
}

void GarbageThrow::fire(evt::Chain event)
{
	if(event.counter > 0)
		spawn(PIT_COLS, event.counter, false);
}

void GarbageThrow::spawn(int columns, int rows, bool right_side)
{
	enforce(columns > 0);
	enforce(columns <= PIT_COLS);

	int spawn_row = std::min(m_pit.peak(), m_pit.top()) - rows - 1;
	RowCol rc{spawn_row, right_side ? PIT_COLS-columns : 0};
	Garbage& garbage = m_pit.spawn_garbage(rc, columns, rows);
	garbage.set_state(Physical::State::FALL, ROW_HEIGHT, FALL_SPEED);
}


void BonusThrow::fire(evt::Match event)
{
	if(event.combo > 3)
		m_indicator.display_combo(event.combo);
}

void BonusThrow::fire(evt::Chain event)
{
	if(event.counter > 0)
		m_indicator.display_chain(event.counter + 1);
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

int opponent(int player)
{
	assert(0 == player || 1 == player || "more than two players not implemented yet");
	return 0 == player ? 1 : 0;
}

}

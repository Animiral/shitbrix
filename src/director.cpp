/**
 * Implementation of director logic.
 */

#include "director.hpp"
#include "game.hpp"
#include "logic.hpp"
#include "arbiter.hpp"
#include "replay.hpp"
#include "error.hpp"
#include <cassert>

// for debug functions
#include <chrono>
#include <ctime>
#include <fstream>

BlockDirector::BlockDirector()
: m_state(nullptr),
  m_handler(nullptr)
{}

void BlockDirector::update()
{
	for(int player = 0; player < m_state->pit().size(); player++)
		update_single(player);
}

void BlockDirector::apply_input(const Input& input)
{
	input.visit([this](auto&& i) { apply_input(i); });
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
	bool new_row = false;       // true if a row of blocks has just scrolled to active

	pit.untag_all();

	logic.examine_finish(dead_physical, dead_block, dead_sound, chainstop, new_row);

	// raise until new row, except if player is holding down the button
	if(new_row)
		pit.stop_raise();

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

	if(have_match && m_handler)
		m_handler->fire(evt::Match{{game_time, player}, combo, chaining});

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
	bool starving = false;      // true if the bottom+1 row is empty based on scrolling

	logic.examine_pit(pit_chaining, breaking, pit_full, starving);

	if(debug_no_gameover)
		pit_full = false; // debug function: in no-gameover mode, pit is never full

	// close current chain -> Chain event -> SPAWN_GARBAGE input
	if(chainstop && !pit_chaining && m_handler)
		m_handler->fire(evt::Chain{{game_time, player}, pit.finish_chain()});

	// panic time and game over check
	if(pit_full) {
		if(!pit_chaining && !breaking && !recovering) {
			const bool panic = pit.do_panic() <= 0;
			if(panic && !debug_no_gameover)
				m_winner = m_state->opponent(player);
		}
	}
	else {
		pit.replenish_panic();
	}

	if(pit_full || pit_chaining || breaking || recovering)
		pit.stop();
	else
		pit.start();

	// If there are no blocks below the pit’s bottom row, refill previews.
	// For gameplay to work properly, the pit scroll speed must be less
	// than one full row per tick.
	if(starving && m_handler)
		m_handler->fire(evt::Starve{{game_time, player}});

	// debug: show what the pit considers to be its peak row
	pit.highlight(pit.peak());
}

void BlockDirector::apply_input(const PlayerInput& ginput)
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
				m_state->pit().at(ginput.player)->cursor_move(dir);
				if(m_handler)
					m_handler->fire(evt::CursorMoves{{ginput.game_time, ginput.player}});
			}

			break;

		case GameButton::SWAP:
			if(ButtonAction::DOWN == ginput.action)
			{
				swap(ginput.player);
			}

			break;

		case GameButton::RAISE:
			m_state->pit().at(ginput.player)->set_raise(ButtonAction::DOWN == ginput.action);
			break;

		case GameButton::NONE:
		default:
			assert(false);

	}
}

void BlockDirector::apply_input(const SpawnBlockInput& spinput)
{
	Log::trace("%s %s", __FUNCTION__, spinput.to_string().c_str());

	Pit& pit = *m_state->pit().at(spinput.player);
	pit.set_floor(spinput.row + 1);
	for(int c = 0; c < spinput.colors.size(); c++) {
		RowCol spawn_rc{spinput.row, c};
		pit.spawn_block(spinput.colors[c], spawn_rc, Block::State::PREVIEW);
	}
}

void BlockDirector::apply_input(const SpawnGarbageInput& sginput)
{
	Log::trace("%s %s", __FUNCTION__, sginput.to_string().c_str());

	Pit& pit = *m_state->pit().at(sginput.player);
	Garbage& garbage = pit.spawn_garbage(sginput.rc, sginput.columns, sginput.rows, sginput.loot);
	garbage.set_state(Physical::State::FALL, ROW_HEIGHT, FALL_SPEED);
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
	if(!left) left = &pit.spawn_block(Color::FAKE, lrc, Block::State::REST);
	if(!right) right = &pit.spawn_block(Color::FAKE, rrc, Block::State::REST);

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

	std::vector<Color> rainbow; // non-random loot
	for(int i = 0; i < rows * columns; i++)
		rainbow.push_back(static_cast<Color>((i % 6) + 1));

	pit.spawn_garbage(RowCol{spawn_row, 0}, columns, rows, move(rainbow));
}

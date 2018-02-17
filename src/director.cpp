/**
 * Implementation of director logic.
 */

#include "director.hpp"

BlocksQueue::BlocksQueue(unsigned seed)
	: m_record(), m_generator(seed), m_index(0)
{
}

Block::Color BlocksQueue::next() noexcept
{
	if(m_record.size() <= m_index) {
		std::uniform_int_distribution<int> color_distribution { 1, 6 };
		Block::Color color = static_cast<Block::Color>(color_distribution(m_generator));
		m_record.push_back(color);
		m_index++;
		return color;
	}
	else {
		return m_record[m_index++];
	}
}

void BlocksQueue::backtrack(size_t index) noexcept
{
	m_index = index;
}


// elemental game logic functions and helpers
namespace
{

/**
 * Put a block of random color at the specified location in @c PREVIEW state.
 */
Block& spawn_preview_block(Pit& pit, RowCol rc, BlocksQueue& queue);

/**
 * Use the @c queue to random-generate the specified number
 * of loot blocks for a garbage to hide.
 */
Loot pick_loot(BlocksQueue& queue, size_t amount);

/**
 * New blocks in *preview* state appear at the bottom of the pit as it scrolls.
 * At the same time, the previous previews become normal blocks at rest.
 * In this instant, they are tagged as *hot*.
 *
 * @param pit Pit object
 * @param queue Source for new blocks
 */
bool spawn_previews(Pit& pit, BlocksQueue& queue);

}

BlockDirector::BlockDirector(Pit& pit, Logic& logic, BlocksQueue grow_queue)
: pit(pit),
  m_logic(logic),
  m_handler(nullptr),
  m_chain(0),
  m_recovery(0),
  m_panic(PANIC_TIME),
  m_over(false),
  m_raise(false),
  m_grow_queue(std::move(grow_queue))
{}

void BlockDirector::update()
{
	// TODO: This function is ripe for refactoring.
	//
	// It allocates and deallocates memory every frame, which could be eliminated.
	// The names of the helper functions are nondescriptive and inconsistent (handle_*)
	// Helper return values use out parameters.
	//
	// Measures:
	// 1. Instead of out parameters, bundle helper results in dedicated structs.
	// 2. Amend class Physical, Garbage and Block with “tag” fields to be used as markers by this logic. (done)
	// 3. Define filter iterators to pick out all the marked objects with no memory overhead. (done in for_all)
	// 4. Rename handle_* -> mark_* if blocks are to be marked, or examine_* if state is to be determined.
	bool dead_physical = false; // true if some physical has entered terminal state
	bool dead_block = false;    // true if pit needs to clean up
	bool dead_sound = false;    // true if there was at least one non-fake dead
	bool chainstop = false;     // true if the pit should be examined for chain finish

	pit.untag_all();

	bool new_row = spawn_previews(pit, m_grow_queue);

	// raise until new row, except if player is holding down the button
	if(new_row && !m_raise)
		pit.set_speed(SCROLL_SPEED);

	m_logic.examine_finish(dead_physical, dead_block, dead_sound, chainstop);

	auto& pit_contents = pit.contents();
	const bool have_dissolvers = std::any_of(begin(pit_contents), end(pit_contents), [](const auto& p) { return p->has_tag(Physical::TAG_DISSOLVE); });
	dead_physical |= have_dissolvers;

	m_logic.convert_garbage();

	if(have_dissolvers && m_handler)
		m_handler->fire(evt::GarbageDissolves());

	pit.for_all(Physical::TAG_FALL, [](const Physical& physical) {
		game_assert(Physical::State::DEAD != physical.physical_state(), "dead blocks cannot fall");
	});

	if(dead_block)
		pit.remove_dead();

	if(dead_sound && m_handler)
		m_handler->fire(evt::BlockDies());

	m_logic.handle_fallers();

	if(m_handler) {
		pit.for_all(Physical::TAG_LAND, [this](const Physical& p) {
			m_handler->fire(evt::PhysicalLands{p}); });
	}

	bool have_match = false;
	bool chaining = false;
	int combo = 0;
	m_logic.handle_hots(have_match, combo, chaining, chainstop);

	if(have_match && m_handler)
		m_handler->fire(evt::Match{combo, chaining});

	if(chaining)
		m_chain++;

	if(chaining || combo > 3)
		m_recovery = BREAK_TIME + RECOVERY_TIME;
	else if(m_recovery > 0)
		m_recovery--;

	bool pit_chaining = false;  // true if there is any block in the pit currently chaining
	bool breaking = false;      // true if there is any physical in the pit currently breaking
	bool pit_full = false;      // true if some resting object overflows the pit

	m_logic.examine_pit(pit_chaining, breaking, pit_full);

	// close current chain
	if(chainstop && m_handler && !pit_chaining) {
		m_handler->fire(evt::Chain{m_chain});
		m_chain = 0;
	}

	// panic time and game over check
	if(pit_full) {
		if(!pit_chaining && !breaking && m_recovery <= 0) {
			m_panic--;
			if(m_panic < 0 && !debug_no_gameover)
				m_over = true;
		}
	}
	else {
		m_panic = PANIC_TIME; // un-panic
	}

	if(pit_chaining || breaking || pit_full || m_recovery > 0)
		pit.stop();
	else
		pit.start();

	// debug: show what the pit considers to be its peak row
	pit.highlight(pit.peak());
}

bool BlockDirector::swap(RowCol lrc)
{
	// bounds check
	SDL_assert(lrc.r >= pit.top() && lrc.r <= pit.bottom() && lrc.c >= 0 && lrc.c <= PIT_COLS-2);

	RowCol rrc {lrc.r, lrc.c+1};

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

void BlockDirector::set_raise(bool raise)
{
	m_raise = raise;

	if(raise)
		pit.set_speed(RAISE_SPEED);
}

void BlockDirector::debug_spawn_garbage(int columns, int rows)
{
	int spawn_row = std::min(pit.peak(), pit.top()) - rows - 2;
	Loot loot = pick_loot(m_grow_queue, columns * rows); // for debug purposes, we do not care about replay desync here
	pit.spawn_garbage(RowCol{spawn_row, 0}, columns, rows, move(loot));
}


void CursorDirector::move(Dir dir)
{
	RowCol& rc = m_cursor.rc;

	switch(dir) {
		case Dir::NONE: while(rc.r < pit.top()) m_cursor.rc.r++; break; // prevent cursor from scrolling off the top
		case Dir::LEFT: if(rc.c > 0) m_cursor.rc.c--; break;
		case Dir::RIGHT: if(rc.c < PIT_COLS-2) m_cursor.rc.c++; break;
		case Dir::UP: if(rc.r > pit.top()) m_cursor.rc.r--; break;
		case Dir::DOWN: if(rc.r < pit.bottom()) m_cursor.rc.r++; break;
		default: SDL_assert(false);
	}

	if(m_handler && dir != Dir::NONE)
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
	SDL_assert(columns > 0 && columns <= PIT_COLS);

	Loot loot = pick_loot(m_emerge_queue, columns * rows);
	m_logic.throw_garbage(columns, rows, move(loot), right_side);
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

Block& spawn_preview_block(Pit& pit, RowCol rc, BlocksQueue& queue)
{
	Block::Color block_color = queue.next();
	game_assert(block_color >= Block::Color::BLUE && block_color <= Block::Color::ORANGE, "Invalid block color");
	Block& block = pit.spawn_block(block_color, rc, Block::State::PREVIEW);
	return block;
}

Loot pick_loot(BlocksQueue& queue, size_t amount)
{
	Loot loot(amount);

	for(Block::Color& c : loot) {
		c = queue.next();
	}

	return loot;
}

bool spawn_previews(Pit& pit, BlocksQueue& queue)
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
		spawn_preview_block(pit, spawn_rc, queue);

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

}

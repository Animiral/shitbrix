/**
 * Implementation of director logic.
 */

#include "director.hpp"

void MatchBuilder::ignite(Block& block)
{
	Block::Color color = block.col;
	int row = block.rc().r;
	int col = block.rc().c;

	// extents of match
	int left = col;
	int right = col;
	int top = row;
	int bottom = row;

	for(left = col-1; left >= 0 && match_at({row,left}, color); left--);
	for(right = col+1; left < PIT_COLS && match_at({row,right}, color); right++);
	for(top = row-1; top >= pit.top() && match_at({top,col}, color); top--);
	for(bottom = row+1; top <= pit.bottom() && match_at({bottom,col}, color); bottom++);

	// horizontal match >= 3 blocks
	if(right-left-1 >= 3) {
		for(int c = left+1; c < right; c++)
			insert({row,c});
	}

	// vertical match
	if(bottom-top-1 >= 3) {
		for(int r = top+1; r < bottom; r++)
			insert({r,col});
	}
}

void MatchBuilder::find_touch_garbage()
{
	auto insert_if_garbage_at = [this] (RowCol rc)
	{
		Garbage* garbage = pit.garbage_at(rc);
		if(garbage)
			m_touched_garbage.insert(*garbage);
	};

	for(Block& block : m_result) {
		RowCol rc = block.rc();
		insert_if_garbage_at(RowCol{rc.r-1, rc.c});
		insert_if_garbage_at(RowCol{rc.r+1, rc.c});
		insert_if_garbage_at(RowCol{rc.r, rc.c-1});
		insert_if_garbage_at(RowCol{rc.r, rc.c+1});
	}
}

bool MatchBuilder::match_at(RowCol rc, Block::Color color)
{
	Block* next = pit.block_at(rc);
	return next && next->col == color && next->is_matchable();
}

void MatchBuilder::insert(RowCol rc)
{
	Block* match_block = pit.block_at(rc);
	game_assert(match_block, "MatchBuilder: expected block not present");
	m_result.insert(*match_block);
	m_chaining |= match_block->chaining;
}

// elemental game logic functions
namespace
{

/**
 * Put a block of random color at the specified location with the state.
 */
template<typename RndGen>
Block& spawn_random_block(Pit& pit, RowCol rc, Block::State state, RndGen& rndgen);

/**
 * Fake blocks are used to replace empty spaces for the duration of a swap().
 */
Block& spawn_fake_block(Pit& pit, RowCol rc);

/**
 * Mark all objects at the given location and above as potentially falling.
 */
template<typename OutIt>
void trigger_falls(Pit& pit, RowCol rc, OutIt&& fallers, bool chaining);

/**
 * Examine the pit contents and start the pit scrolling, if desired.
 * We look for stalling elements, such as blocks in the process of breaking up.
 * If no such elements are found, re-enable time-based automatic scrolling of
 * the pit.
 */
void try_resume_pitscroll(Pit& pit);

/**
 * Examine the pit contents for chaining blocks.
 * A chain progresses until the last chaining block in the pit disappears
 * or loses its chaining property.
 * @return true if there is an active chain in progress
 */
bool any_chaining(const Pit& pit);

/**
 * New blocks in *preview* state appear at the bottom of the pit as it scrolls.
 * At the same time, the previous previews become normal blocks at rest.
 * In this instant, they are added to the *hots* set.
 *
 * @param pit Pit object
 * @param hots Output iterator for blocks that have become hot (match-ready)
 * @param rndgen Pseudo-random number source for new blocks
 */
template<typename OutIt, typename RndGen>
bool spawn_previews(Pit& pit, OutIt hots, RndGen& rndgen);

/**
 * Classify Physicals whose states are “running out”.
 * For example, an object’s internal timer can run out while they are falling,
 * indicating that they have reached their target location.
 *
 * @param pit Pit object
 * @param dissolvers Output iterator for Garbage that is breaking down
 * @param fallers Output iterator for objects that may fall down
 * @param hots Output iterator for blocks that have become hot (match-ready)
 * @param[out] panic Flag which indicates true if pit objects have stacked to the top
 * @param[out] dead_physical Flag which indicates true if there are new dead physicals
 * @param[out] dead_block Flag which indicates true if there are new dead blocks
 * @param[out] dead_sound Flag which indicates true if there are non-fake dead blocks
 * @param[out] chainstop Flag which indicates true if a chain might be finished
 */
template<typename GarbOutIt, typename PhysOutIt, typename BlockOutIt>
void examine_finish(Pit& pit, GarbOutIt dissolvers, PhysOutIt fallers,
                    BlockOutIt hots, bool& panic, bool& dead_physical,
                    bool& dead_block, bool& dead_sound, bool& chainstop);

/**
 * Shrink or remove expired garbage blocks from the *dissolvers* set.
 *
 * @param pit Pit object
 * @param dissolvers Set of broken Garbage bricks to dissolve
 * @param fallers Output iterator for objects that may fall down
 * @param hots Output iterator for blocks that have become hot (match-ready)
 * @param rndgen Pseudo-random number source for new blocks
 */
template<typename Dissolvers, typename PhysOutIt, typename BlockOutIt, typename RndGen>
void convert_garbage(Pit& pit, Dissolvers& dissolvers, PhysOutIt fallers,
                     BlockOutIt hots, bool& dead_physical, RndGen& rndgen);

/**
 * All physicals in the *fallers* set now actually enter the *fall*
 * state if possible.
 * Successful fallers can not match and are therefore removed from
 * the *hots* set.
 * Since the actual container is generic, the contents are merely rearranged
 * using std::remove. The new end iterator output must be used by the
 * client to actually erase those items.
 *
 * @param pit Pit object
 * @param fallers Set of Physical objects that might fall
 * @param hots Set of Blocks marked as hot
 * @param[out] hots_end New end iterator for reduced set of hots; see std::remove
 */
template<typename Fallers, typename Hots>
void handle_fallers(Pit& pit, Fallers& fallers, Hots& hots,
                    typename Hots::iterator& hots_end);

/**
 * All matching blocks and all adjacent garbage bricks enter the *break* state.
 *
 * @param pit Pit object
 * @param hots Set of Blocks marked as hot
 * @param[out] have_match Flag which indicates true if there is at least one match
 * @param[out] combo Counter for the number of blocks matched
 * @param[out] chaining Flag which indicates true if there is a match involving chaining blocks
 * @param[out] chainstop Flag which indicates true if chaining blocks have come to rest
 */
template<typename Hots>
void handle_hots(Pit& pit, Hots& hots, bool& have_match, int& combo, bool& chaining, bool& chainstop);

}

void BlockDirector::update()
{
	using PhysicalRefVec = std::vector<std::reference_wrapper<Physical>>;
	using BlockRefVec = std::vector<std::reference_wrapper<Block>>;
	using GarbageRefVec = std::vector<std::reference_wrapper<Garbage>>;

	BlockRefVec previews;     // blocks which are fresh spawns and currently inactive
	GarbageRefVec dissolvers; // blocks which are shrinking or dying
	PhysicalRefVec fallers;   // objects which we want to start falling soon
	BlockRefVec hots;         // recently landed or arrived blocks that can start a match

	bool panic = false;         // true if game over is threatened
	bool dead_physical = false; // true if pit needs to resume scrolling
	bool dead_block = false;    // true if pit needs to clean up
	bool dead_sound = false;    // true if there was at least one non-fake dead
	bool chainstop = false;     // true if the pit should be examined for chain finish

	bool new_row = spawn_previews(pit, std::back_inserter(hots), *rndgen);

	// raise until new row, except if player is holding down the button
	if(new_row && !m_raise)
		pit.set_speed(SCROLL_SPEED);

	examine_finish(pit, std::back_inserter(dissolvers), std::back_inserter(fallers),
	               std::back_inserter(hots), panic, dead_physical, dead_block,
	               dead_sound, chainstop);

	// game over check
	if(panic && !debug_no_gameover)
		m_over = true;

	convert_garbage(pit, dissolvers, std::back_inserter(fallers),
	                std::back_inserter(hots), dead_physical, *rndgen);

	if(!dissolvers.empty() && m_handler)
		m_handler->fire(evt::GarbageDissolves());

	for(Physical& phys : fallers)
		game_assert(Physical::State::DEAD != phys.physical_state(), "dead blocks cannot fall");

	if(dead_block)
		pit.remove_dead();

	if(dead_sound && m_handler)
		m_handler->fire(evt::BlockDies());

	BlockRefVec::iterator hots_end = hots.end();
	handle_fallers(pit, fallers, hots, hots_end);
	hots.erase(hots_end, hots.end());

	bool have_match = false;
	bool chaining = false;
	int combo = 0;
	handle_hots(pit, hots, have_match, combo, chaining, chainstop);

	if(have_match && m_handler)
		m_handler->fire(evt::Match{combo, chaining});

	if(chaining)
		m_chain++;

	if(chainstop && m_handler && !any_chaining(pit)) {
		m_handler->fire(evt::Chain{m_chain});
		m_chain = 0;
	}

	if(dead_physical || chainstop)
		try_resume_pitscroll(pit);

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
	if(!left) left = &spawn_fake_block(pit, lrc);
	if(!right) right = &spawn_fake_block(pit, rrc);

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
	pit.spawn_garbage(RowCol{spawn_row, 0}, columns, rows);
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

	int spawn_row = std::min(m_pit.peak(), m_pit.top()) - rows - 2;
	RowCol rc{spawn_row, right_side ? PIT_COLS-columns : 0};
	m_pit.spawn_garbage(rc, columns, rows);
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

template<typename RndGen>
Block& spawn_random_block(Pit& pit, RowCol rc, Block::State state, RndGen& rndgen)
{
	Block::Color block_color = static_cast<Block::Color>(static_cast<int>(Block::Color::BLUE) + rndgen() % 6);
	Block& block = pit.spawn_block(block_color, rc, state);
	return block;
}

Block& spawn_fake_block(Pit& pit, RowCol rc)
{
	Block& block = pit.spawn_block(Block::Color::FAKE, rc, Block::State::REST);
	return block;
}

template<typename OutIt>
void trigger_falls(Pit& pit, RowCol rc, OutIt&& fallers, bool chaining)
{
	Physical* physical = pit.at(rc);

	if(!physical ||
	   !physical->is_fallible() ||
	   Physical::State::DEAD == physical->physical_state())
		return;

	if(Block* block = dynamic_cast<Block*>(physical))
		block->chaining |= chaining;

	*fallers++ = *physical;

	rc = physical->rc();
	for(int c = rc.c; c < rc.c + physical->columns(); c++) {
		trigger_falls(pit, RowCol{rc.r - 1, c}, fallers, chaining);
	}
}

void try_resume_pitscroll(Pit& pit)
{
	// a chain in progress stops the pit
	if(any_chaining(pit))
		return;

	auto& pit_contents = pit.contents();
	auto is_breaking = [] (std::unique_ptr<Physical>& p)
	{
		return p->physical_state() == Physical::State::BREAK;
	};

	if(std::none_of(pit_contents.begin(), pit_contents.end(), is_breaking)) {
		pit.start();
	}
}

bool any_chaining(const Pit& pit)
{
	auto& pit_contents = pit.contents();
	auto is_chaining = [] (const std::unique_ptr<Physical>& p)
	{
		Block* b = dynamic_cast<Block*>(p.get());
		return b && b->chaining;
	};

	return std::any_of(pit_contents.begin(), pit_contents.end(), is_chaining);
}

template<typename OutIt, typename RndGen>
bool spawn_previews(Pit& pit, OutIt hots, RndGen& rndgen)
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
		spawn_random_block(pit, spawn_rc, Block::State::PREVIEW, rndgen);

		RowCol above_rc{bottom_row-1, c};
		Block* above = pit.block_at(above_rc);

		// in the first spawn_previews, there might not be blocks above.
		if(above && Block::State::PREVIEW == above->block_state()) {
			above->set_state(Physical::State::REST);
			*hots++ = *above;
		}
	}

	return true;
}

template<typename GarbOutIt, typename PhysOutIt, typename BlockOutIt>
void examine_finish(Pit& pit, GarbOutIt dissolvers, PhysOutIt fallers,
                    BlockOutIt hots, bool& panic, bool& dead_physical,
                    bool& dead_block, bool& dead_sound, bool& chainstop)
{
	for(auto& physical : pit.contents())
	{
		Physical::State state = physical->physical_state();
		bool is_arriving = physical->is_arriving();

		if(Physical::State::FALL == state && is_arriving) {
			// can never fall lower than the preview row of blocks
			game_assert(physical->rc().r + physical->rows() - 1 <= pit.bottom(), "Object falls too low");

			// Re-enter the object as a candidate for falling and hots.
			// Since falling blocks are automatically excluded from hots,
			// this only takes effect with blocks that actually land.
			*fallers++ = *physical;
			if(Block* block = dynamic_cast<Block*>(&*physical))
				*hots++ = *block;
		}

		if(Physical::State::REST == state && physical->rc().r < pit.top())
			panic = true;

		// Garbage-specifics
		if(Garbage* garbage = dynamic_cast<Garbage*>(&*physical)) {
			// shrink garbage if necessary
			if(Physical::State::BREAK == garbage->physical_state() && is_arriving) {
				*dissolvers++ = *garbage;
			}
		}

		// Block-specifics
		if(Block* block = dynamic_cast<Block*>(&*physical)) {
			Block::State state = block->block_state();
			bool above_fall = false; // whether objects above this one might fall
			bool chaining = false; // whether objects above chain when they fall

			// blocks finished swapping
			if((Block::State::SWAP_LEFT == state || Block::State::SWAP_RIGHT == state) &&
			   is_arriving) {
				// fake blocks are only for swapping and disappear right afterwards
				if(Block::Color::FAKE == block->col) {
					block->set_state(Physical::State::DEAD);
					state = block->block_state(); // NOTE: remember changed state!
				}
				else {
					*fallers++ = *block;
					*hots++ = *block;

					above_fall = true;
				}
			}

			// cleanup dead blocks, resume scrolling if there are no more BREAK blocks
			if(Block::State::DEAD == state) {
				dead_physical = true;
				dead_block = true;

				if(Block::Color::FAKE != block->col) {
					dead_sound = true;
					chaining = true; // blocks to fall from above should get the chaining flag

					// dead blocks can finish chains by being the last chaining blocks to disappear
					if(block->chaining)
						chainstop = true;
				}

				above_fall = true;
			}

			if(above_fall) {
				RowCol rc = block->rc();
				rc.r--;
				trigger_falls(pit, rc, fallers, chaining);
			}
		}
	}
}

template<typename Dissolvers, typename PhysOutIt, typename BlockOutIt, typename RndGen>
void convert_garbage(Pit& pit, Dissolvers& dissolvers, PhysOutIt fallers,
                     BlockOutIt hots, bool& dead_physical, RndGen& rndgen)
{
	for(Garbage& garbage : dissolvers) {
		RowCol garbage_rc = garbage.rc();
		int garbage_columns = garbage.columns();
		int garbage_rows = garbage.rows();
		bool survived = pit.shrink(garbage);

		for(int c = 0; c < garbage_columns; c++) {
			RowCol block_rc{garbage_rc.r + garbage_rows - 1, garbage_rc.c + c};
			Block& block = spawn_random_block(pit, block_rc, Block::State::FALL, rndgen);
			block.chaining = true;
			*fallers++ = block;
			*hots++ = block;
		}

		if(survived) {
			// get rid of the break state, it stops the pit from scrolling
			garbage.set_state(Physical::State::REST);
			*fallers++ = garbage;
		}
	}

	if(!dissolvers.empty())
		dead_physical = true;
}

template<typename Fallers, typename Hots>
void handle_fallers(Pit& pit, Fallers& fallers, Hots& hots,
                    typename Hots::iterator& hots_end)
{
	bool changed = true;
	auto begin = std::begin(fallers);
	auto end = std::end(fallers);

	while(changed) {
		changed = false;

		for(auto it = begin; it != end; ) {
			Physical& physical = *it;
			if(pit.can_fall(physical)) {
				physical.set_state(Physical::State::FALL, ROW_HEIGHT, FALL_SPEED);
				pit.fall(physical);

				// erase the element from our consideration of fallers
				std::swap(*it, *--end);

				changed = true;
			}
			else {
				++it;
			}
		}
	}

	for(auto it = begin; it != end; ++it) {
		Physical& physical = *it;
		Physical::State state = physical.physical_state();

		if(Physical::State::FALL == state) {
			physical.set_state(Physical::State::LAND, LAND_TIME);
		}
		else {
			physical.set_state(Physical::State::REST);
		}
	}

	// blocks cannot match if they are falling down!
	auto is_falling = [](Physical& p) { return Physical::State::FALL == p.physical_state(); };
	hots_end = std::remove_if(std::begin(hots), std::end(hots), is_falling);
}

template<typename Hots>
void handle_hots(Pit& pit, Hots& hots, bool& have_match, int& combo, bool& chaining, bool& chainstop)
{
	if(hots.empty())
		return;

	auto builder = MatchBuilder(pit);

	for(auto& block : hots)
		builder.ignite(block);

	auto& breaks = builder.result();
	combo = builder.combo();
	chaining = builder.chaining();

	if(!breaks.empty()) {
		have_match = true;
		pit.stop();

		for(Block& breaking : breaks)
			breaking.set_state(Physical::State::BREAK, BREAK_TIME);
	}

	// There is only 1 chance per block to make a chain
	for(Block& block : hots) {
		// Chaining blocks which come to rest can finish a chain.
		// Blocks which have now matched are still carrying the chain.
		if(block.chaining && Block::State::BREAK != block.block_state()) {
			block.chaining = false;
			chainstop = true;
		}
	}

	builder.find_touch_garbage();

	for(auto& garbage : builder.touched_garbage()) {
		garbage.get().set_state(Physical::State::BREAK, DISSOLVE_TIME);
	}
}

}

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
}


/**
 * Spawn blocks at regular intervals, clean up dead blocks
 */
void BlockDirector::update(IContext& context)
{
	// spawn blocks from below
	spawn_previews();

	// Handle individual logic for each block
	bool have_dead = false; // true if pit needs to clean up
	bool dead_sound = false; // true if there was at least one non-fake dead

	auto& pit_contents = pit.contents();

	// Blocks
	for(auto& physical : pit_contents)
	if(Block* block = dynamic_cast<Block*>(&*physical)) {
		Block::State state = block->block_state();

		// block above top => game over
		if(block->rc().r < pit.top()) {
			game_over();
			break; // interrupt blocks logic because game_over might invalidate blocks list
		}

		// falling blocks arrived at next row (center)
		if(state == Block::State::FALL) {
			// land block?
			if(block->is_arriving()) {
				block_arrive_fall(*block);
			}
		}

		// blocks finished swapping
		if(Block::State::SWAP == state && block->time <= 0) {
			block_arrive_swap(*block);
			state = block->block_state(); // NOTE: block_arrive_swap may change the state
		}

		// cleanup dead blocks, resume scrolling if there are no more BREAK blocks
		if(Block::State::DEAD == state) {
			have_dead = true; // don’t remove them just yet as it would mess up the iterators
			if(Block::Color::FAKE != block->col)
				dead_sound = true;

			trigger_falls(block->rc());

			auto is_breaking = [] (std::unique_ptr<Physical>& p) { return p->physical_state() == Physical::State::BREAK; };
			auto breaking = std::find_if(pit_contents.begin(), pit_contents.end(), is_breaking);
			if(pit_contents.end() == breaking) {
				pit.start();
			}
		}
	}

	// Garbage
	for(auto& physical : pit.contents())
	if(Garbage* garbage = dynamic_cast<Garbage*>(&*physical)) {
		Physical::State state = garbage->physical_state();

		// falling garbage arrived at next row (center)
		if(state == Physical::State::FALL) {
			// land garbage?
			if(garbage->is_arriving()) {
				garbage_arrive_fall(*garbage);
			}
		}
	}

	// Examine hots for matches
	if(!hots.empty()) {
		auto builder = MatchBuilder(pit);

		for(auto it = hots.begin(); it != hots.end(); ++it) {
			builder.ignite(*it);	
		}

		auto& breaks = builder.result();

		if(!breaks.empty()) {
			context.play(Snd::MATCH);
			pit.stop();

			for(auto it = breaks.begin(); it != breaks.end(); ++it) {
				Block& block = *it;
				block.set_state(Physical::State::BREAK);
			}
		}

		hots.clear();
	}

	// Handle individual logic for each garbage
	for(auto& physical : pit.contents())
	if(Garbage* garbage = dynamic_cast<Garbage*>(&*physical)) {
		Physical::State state = garbage->physical_state();

		// falling blocks arrived at next row (center)
		if(state == Physical::State::FALL) {
			// land block?
			if(garbage->is_arriving()) {
				garbage_arrive_fall(*garbage);
			}
		}
	}

	if(have_dead)
	{
		pit.remove_dead();

		if(dead_sound)
			context.play(Snd::BREAK);
	}

	// make fallers fall
	while(!fallers.empty() || !garbage_fallers.empty()) {
		bool changed = false;

		for(auto it = fallers.begin(); it != fallers.end(); ) {
			Block& block = *it;
			if(pit.can_fall(block)) {
				block.set_state(Physical::State::FALL);
				pit.fall(block);
				it = fallers.erase(it);
				changed = true;
			}
			else {
				++it;
			}
		}

		for(auto it = garbage_fallers.begin(); it != garbage_fallers.end(); ) {
			Garbage& garbage = *it;
			if(pit.can_fall(garbage)) {
				garbage.set_state(Physical::State::FALL);
				pit.fall(garbage);
				it = garbage_fallers.erase(it);
				changed = true;
			}
			else {
				++it;
			}
		}

		if(!changed) {
			garbage_fallers.clear();
			game_assert(fallers.empty(), "falling blocks are stuck");
		}
	}

	// debug: show what the pit considers to be its peak row
	pit.highlight(pit.peak());
}

/**
 * Attempt to set the block or space at lrc to swap with the one to the right of it.
 *
 * The following conditions must be met for success:
 *  - Both blocks must be in a swappable state. These are REST, SWAP, FALL, LAND.
 *  - A block can swap with a space, but two spaces cannot be swapped.
 *
 * Returns true if the swap was successful, false otherwise.
 */
bool BlockDirector::swap(RowCol lrc)
{
	// bounds check
	SDL_assert(lrc.r >= pit.top() && lrc.r <= pit.bottom() && lrc.c >= 0 && lrc.c <= PIT_COLS-2);

	RowCol rrc {lrc.r, lrc.c+1};

	Block* left = pit.block_at(lrc);
	Block* right = pit.block_at(rrc);

	if(!left && !right) return false; // 2 spaces

	bool left_swappable = !left || left->is_swappable();
	bool right_swappable = !right || right->is_swappable();

	if(!left_swappable || !right_swappable) return false;

	// fake blocks - they last only for the duration of the swap, blocking other
	// falling blocks from going through the space.
	if(!left) left = &spawn_fake(lrc);
	if(!right) right = &spawn_fake(rrc);

	// do swap
	left->swap_toward(rrc);
	right->swap_toward(lrc);
	pit.swap(*left, *right);

	return true;
}

void BlockDirector::debug_spawn_garbage(int columns, int rows)
{
	int spawn_row = std::min(pit.peak(), pit.top()) - rows - 2;
	pit.spawn_garbage(RowCol{spawn_row, 0}, columns, rows);
}

/**
 * Bring up a new row of preview blocks and enable the previous row, if necessary.
 */
void BlockDirector::spawn_previews()
{
	while(bottom <= pit.bottom()) {
		activate_previews();

		bottom++;
		for(int i = 0; i < PIT_COLS; i++) {
			RowCol rc {bottom, i};
			Block& block = spawn_block(rc);
			previews.push_back(block);
		}
	}
}

Block& BlockDirector::spawn_block(RowCol rc)
{
	Block::Color spawn_color = static_cast<Block::Color>(static_cast<int>(Block::Color::BLUE) + (*rndgen)() % 6);
	Block& block = pit.spawn_block(spawn_color, rc, Block::State::PREVIEW);
	return block;
}

/**
 * Fake blocks are used to replace empty spaces for the duration of a swap().
 */
Block& BlockDirector::spawn_fake(RowCol rc)
{
	Block& block = pit.spawn_block(Block::Color::FAKE, rc, Block::State::REST);
	return block;
}

void BlockDirector::block_arrive_fall(Block& block)
{
	RowCol rc = block.rc();
	RowCol next { rc.r + 1, rc.c };

	SDL_assert(next.r <= bottom); // can never fall lower than the preview row of blocks

	// If the next space is free, the block goes on to fall. Otherwise, it lands.
	if(pit.block_at(next)) {
		block.set_state(Physical::State::LAND);
		hots.push_back(block);
	}
	else {
		fallers.push_back(block);
	}
}

void BlockDirector::garbage_arrive_fall(Garbage& garbage)
{
	RowCol rc = garbage.rc();
	int next_row = rc.r + garbage.rows();

	SDL_assert(next_row <= bottom); // can never fall lower than the preview row of blocks

	// If the next space is free, the garbage goes on to fall. Otherwise, it lands.
	bool something_there = false;
	for(int c = rc.c; c < rc.c + garbage.columns(); c++) {
		if(pit.at(RowCol{next_row,c})) {
			something_there = true;
		}
	}

	if(something_there) {
		garbage.set_state(Physical::State::LAND);
	}
	else {
		garbage_fallers.push_back(garbage);
	}
}

void BlockDirector::block_arrive_swap(Block& block)
{
	// fake blocks are only for swapping and disappear right afterwards
	if(Block::Color::FAKE == block.col) {
		block.set_state(Physical::State::DEAD);
		return;
	}

	if(pit.can_fall(block)) {
		fallers.push_back(block);
	}
	else {
		block.set_state(Physical::State::REST);
		hots.push_back(block);
	}
}

void BlockDirector::trigger_falls(RowCol free)
{
	// Release blockage & blocks above the dead block => fall down
	trigger_falls_impl(RowCol{free.r - 1, free.c});
}

// Helper function to trigger falling blocks while we don’t yet have base class Physical
void BlockDirector::trigger_falls_impl(RowCol rc)
{
	Block* block = pit.block_at(rc);

	if(block) {
		if(block->is_fallible()) {
			fallers.push_back(*block);
			trigger_falls_impl(RowCol{rc.r - 1, rc.c});
		}
	}

	Garbage* garbage = pit.garbage_at(rc);

	if(garbage) {
		if(garbage->is_fallible()) {
			garbage_fallers.push_back(*garbage);

			rc = garbage->rc();
			for(int c = rc.c; c < rc.c + garbage->columns(); c++) {
				trigger_falls_impl(RowCol{rc.r - 1, c});
			}
		}
	}
}

/**
 * Make all blocks from the preview row into regular matchable resting blocks.
 * This assumes that they have now fully scrolled into view.
 */
void BlockDirector::activate_previews()
{
	for(Block& block : previews) {
		block.set_state(Physical::State::REST);
		hots.push_back(block);
	}

	previews.clear();
}

/**
 * Preliminary game over implementation: kill all blocks and just continue
 */
void BlockDirector::game_over()
{
	pit.clear();

	m_over = true;
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
}

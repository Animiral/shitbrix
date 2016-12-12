/**
 * Implementation of director logic.
 */

#include "director.hpp"

void MatchBuilder::ignite(Block block)
{
	BlockCol color = block->col;
	int row = block->rc().r;
	int col = block->rc().c;

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
			m_result.insert(pit.block_at({row,c}));
	}

	// vertical match
	if(bottom-top-1 >= 3) {
		for(int r = top+1; r < bottom; r++)
			m_result.insert(pit.block_at({r,col}));
	}
}

bool MatchBuilder::match_at(RowCol rc, BlockCol color)
{
	Block next = pit.block_at(rc);
	return next && next->col == color && matchable(next);
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

	for(auto it = pit.blocks_begin(), end = pit.blocks_end(); it != end; ++it) {
		Block block = *it;
		BlockState state = block->state();

		// block above top => game over
		if(block->rc().r < pit.top()) {
			game_over();
			break; // interrupt blocks logic because game_over might invalidate blocks list
		}

		// falling blocks arrived at next row (center)
		if(state == BlockState::FALL) {
			// land block?
			if(block->is_arriving()) {
				block_arrive_fall(block);
			}
		}

		// blocks finished swapping
		if(BlockState::SWAP == state && block->time <= 0) {
			block_arrive_swap(block);
			state = block->state(); // NOTE: block_arrive_swap may change the state
		}

		// cleanup dead blocks, resume scrolling if there are no more BREAK blocks
		if(BlockState::DEAD == state) {
			have_dead = true; // don’t remove them just yet as it would mess up the iterators
			if(BlockCol::FAKE != block->col)
				dead_sound = true;

			trigger_falls(block->rc());

			auto breaking = std::find_if(pit.blocks_begin(), pit.blocks_end(), [] (Block b) { return b->state() == BlockState::BREAK; });
			if(pit.blocks_end() == breaking) {
				pit.start();
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
				Block block = *it;
				block->set_state(BlockState::BREAK);
			}
		}

		hots.clear();
	}

	// Handle individual logic for each garbage
	for(auto it = pit.garbage().begin(); it != pit.garbage().end(); ) {
		GarbagePtr garbage = *it;
		Garbage::State state = garbage->state();

		// falling blocks arrived at next row (center)
		if(state == Garbage::State::FALL) {
			// land block?
			if(garbage->is_arriving()) {
				garbage_arrive_fall(garbage);
			}
		}

		++it;
	}

	if(have_dead)
	{
		pit.remove_dead();

		if(dead_sound)
			context.play(Snd::BREAK);
	}

	// make fallers fall
	while(!fallers.empty()) {
		bool changed = false;

		for(auto it = fallers.begin(); it != fallers.end(); ) {
			Block block = *it;
			RowCol rc = block->rc();
			if(pit.can_fall(rc)) {
				block->set_state(BlockState::FALL);
				pit.fall(rc);
				it = fallers.erase(it);
				changed = true;
			}
			else {
				++it;
			}
		}

		game_assert(changed, "falling blocks are stuck");
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

	Block left = pit.block_at(lrc);
	Block right = pit.block_at(rrc);

	if(!left && !right) return false; // 2 spaces

	bool left_swappable = !left || swappable(left);
	bool right_swappable = !right || swappable(right);

	if(!left_swappable || !right_swappable) return false;

	// fake blocks - they last only for the duration of the swap, blocking other
	// falling blocks from going through the space.
	if(!left) left = spawn_fake(lrc);
	if(!right) right = spawn_fake(rrc);

	// do swap
	left->swap_toward(rrc);
	right->swap_toward(lrc);
	pit.swap(lrc, rrc);

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
			Block block = spawn_block(rc);
			previews.push_back(block);
		}
	}
}

Block BlockDirector::spawn_block(RowCol rc)
{
	BlockCol spawn_color = static_cast<BlockCol>(static_cast<int>(BlockCol::BLUE) + (*rndgen)() % 6);
	pit.spawn_block(spawn_color, rc, BlockState::PREVIEW);
	return pit.block_at(rc);
}

/**
 * Fake blocks are used to replace empty spaces for the duration of a swap().
 */
Block BlockDirector::spawn_fake(RowCol rc)
{
	pit.spawn_block(BlockCol::FAKE, rc, BlockState::REST);
	return pit.block_at(rc);
}

void BlockDirector::block_arrive_fall(Block block)
{
	RowCol rc = block->rc();
	RowCol next { rc.r + 1, rc.c };

	SDL_assert(next.r <= bottom); // can never fall lower than the preview row of blocks

	// If the next space is free, the block goes on to fall. Otherwise, it lands.
	if(pit.block_at(next)) {
		block->set_state(BlockState::LAND);
		hots.push_back(block);
	}
	else {
		fallers.push_back(block);
	}
}

void BlockDirector::garbage_arrive_fall(GarbagePtr garbage)
{
	RowCol rc = garbage->rc();
	RowCol next { rc.r + 1, rc.c };

	SDL_assert(next.r <= bottom); // can never fall lower than the preview row of blocks

	// If the next space is free, the block goes on to fall. Otherwise, it lands.
	if(pit.anything_at(next)) {
		garbage->set_state(Garbage::State::LAND);
	}
	else {
		pit.fall(rc);
	}
}

void BlockDirector::block_arrive_swap(Block block)
{
	// fake blocks are only for swapping and disappear right afterwards
	if(BlockCol::FAKE == block->col) {
		block->set_state(BlockState::DEAD);
		return;
	}

	// If the next space is free, the block starts falling. Otherwise, it rests.
	if(pit.can_fall(block->rc())) {
		fallers.push_back(block);
	}
	else {
		block->set_state(BlockState::REST);
		hots.push_back(block);
	}
}

void BlockDirector::trigger_falls(RowCol free)
{
	// Release blockage & blocks above the dead block => fall down
	RowCol prev { free.r - 1, free.c };
	Block block = pit.block_at(prev);

	while (block && fallible(block)) {
		fallers.push_back(block);

		// continue looking 1 block above
		free = prev;
		prev = RowCol{ free.r - 1, free.c };
		block = pit.block_at(prev);
	}
}

/**
 * Make all blocks from the preview row into regular matchable resting blocks.
 * This assumes that they have now fully scrolled into view.
 */
void BlockDirector::activate_previews()
{
	for(auto block : previews) {
		block->set_state(BlockState::REST);
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

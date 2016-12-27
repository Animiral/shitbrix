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
				arrive_fall(*block);
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
			try_resume_pitscroll();
		}
	}

	// Garbage
	for(auto& physical : pit_contents)
	if(Garbage* garbage = dynamic_cast<Garbage*>(&*physical)) {
		Physical::State state = garbage->physical_state();

		// falling garbage arrived at next row (center)
		if(state == Physical::State::FALL) {
			// land garbage?
			if(garbage->is_arriving()) {
				arrive_fall(*garbage);
			}
		}

		// shrink garbage if necessary
		if(Physical::State::BREAK == state && garbage->time < 0) {
			dissolvers.push_back(*garbage);
		}
	}

	if(!dissolvers.empty()) {
		for(Garbage& garbage : dissolvers) {
			// BUG! invalidates pit_contents
			RowCol garbage_rc = garbage.rc();
			int garbage_columns = garbage.columns();
			int garbage_rows = garbage.rows();
			bool survived = pit.shrink(garbage);

			for(int c = 0; c < garbage_columns; c++) {
				RowCol block_rc{garbage_rc.r + garbage_rows - 1, garbage_rc.c + c};
				Block& block = spawn_random_block(block_rc, Block::State::FALL);
				if(can_fall(block)) {
					fallers.push_back(block);
				}
				else {
					block.set_state(Physical::State::REST);
				}
			}

			if(survived) {
				garbage.set_state(Physical::State::REST);

				if(can_fall(garbage)) {
					fallers.push_back(garbage);
				}
			}

		}
		try_resume_pitscroll();
		dissolvers.clear();
	}

	if(have_dead)
	{
		pit.remove_dead();

		if(dead_sound)
			context.play(Snd::BREAK);
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

		builder.find_touch_garbage();

		for(auto& garbage : builder.touched_garbage()) {
			garbage.get().set_state(Physical::State::BREAK);
		}
	}

	// make fallers fall
	while(!fallers.empty()) {
		bool changed = false;

		for(auto it = fallers.begin(); it != fallers.end(); ) {
			Physical& physical = *it;
			if(pit.can_fall(physical)) {
				physical.set_state(Physical::State::FALL);
				pit.fall(physical);
				it = fallers.erase(it);
				changed = true;
			}
			else {
				++it;
			}
		}

		if(!changed)
			fallers.clear();
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
	if(!left) left = &spawn_fake_block(lrc);
	if(!right) right = &spawn_fake_block(rrc);

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
			Block& block = spawn_random_block(rc, Block::State::PREVIEW);
			previews.push_back(block);
		}
	}
}

Block& BlockDirector::spawn_random_block(RowCol rc, Block::State state)
{
	Block::Color block_color = static_cast<Block::Color>(static_cast<int>(Block::Color::BLUE) + (*rndgen)() % 6);
	Block& block = pit.spawn_block(block_color, rc, state);
	return block;
}

Block& BlockDirector::spawn_fake_block(RowCol rc)
{
	Block& block = pit.spawn_block(Block::Color::FAKE, rc, Block::State::REST);
	return block;
}

void BlockDirector::try_resume_pitscroll()
{
	auto& pit_contents = pit.contents();
	auto is_breaking = [] (std::unique_ptr<Physical>& p) { return p->physical_state() == Physical::State::BREAK; };

	auto breaking = std::find_if(pit_contents.begin(), pit_contents.end(), is_breaking);
	if(pit_contents.end() == breaking) {
		pit.start();
	}
}

bool BlockDirector::can_fall(Physical& physical)
{
	RowCol rc = physical.rc();
	RowCol to {rc.r + physical.rows(), rc.c};

	for(int c = to.c; c < to.c + physical.columns(); c++) {
		RowCol target{to.r, c};

		// TODO: consider can_fall below (time <= 0).
		if(pit.at(target))
			return false;
	}

	return true;
}

void BlockDirector::arrive_fall(Physical& physical)
{
	// can never fall lower than the preview row of blocks
	SDL_assert(physical.rc().r + physical.rows() <= bottom);

	// If the next space is free, the physical goes on to fall. Otherwise, it lands.
	if(can_fall(physical)) {
		fallers.push_back(physical);
	}
	else {
		if(Block* block = dynamic_cast<Block*>(&physical))
			hots.push_back(*block);

		physical.set_state(Physical::State::LAND);
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
			fallers.push_back(*garbage);

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

/**
 * Implementation of director logic.
 */

#include "director.hpp"

/**
 * Spawn blocks at regular intervals, clean up dead blocks
 */
void BlockDirector::update()
{
	pit->update();

	// spawn blocks from below
	while(bottom <= pit->bottom()) {
		activate_previews();

		bottom++;
		for(int i = 0; i < PIT_COLS; i++) {
			RowCol rc {bottom, i};
			spawn_block(rc);
		}
	}

	// random block breakage (for show and debug)
	if(--m_next_break <= 0) {
		if(!blocks.empty()) {
			size_t break_index = rndgen() % blocks.size();
			Block block = blocks[break_index];
			if(BlockState::REST == block->state()) {
				block->set_state(BlockState::BREAK);
			}
		}

		m_next_break = 10 + rndgen() % 30;
	}

	// Handle individual logic for each block

	// Maintain blocks sorted from bottom to top. This way, lower blocks in pillars of falling
	// blocks will fall out of the way before upper blocks stumble over them.
	// TODO: only do this after a block has moved
	std::sort(blocks.begin(), blocks.end(), y_greater);

	for(auto it = blocks.begin(); it != blocks.end(); ) {
		Block block = *it;
		BlockState state = block->state();

		// block above top => game over
		if(block->loc().y < pit->loc().y) {
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

		// cleanup dead blocks
		if(BlockState::DEAD == state) {
			it = reap_block(it);
		}
		else {
			++it;
		}
	}

	// Examine hots for matches
	hots.clear();
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
	SDL_assert(lrc.r >= pit->top() && lrc.r <= pit->bottom() && lrc.c >= 0 && lrc.c <= PIT_COLS-2);

	RowCol rrc {lrc.r, lrc.c+1};

	Block left = pit->block_at(lrc);
	Block right = pit->block_at(rrc);
	auto swappable = [] (BlockState state)
	{
		return BlockState::REST == state ||
		       BlockState::SWAP == state ||
		       BlockState::FALL == state ||
		       BlockState::LAND == state;
	};

	if(!left && !right) return false; // 2 spaces

	bool left_swappable = !left || swappable(left->state());
	bool right_swappable = !right || swappable(right->state());

	if(!left_swappable || !right_swappable) return false;

	// fake blocks - they last only for the duration of the swap, blocking other
	// falling blocks from going through the space.
	if(!left) left = spawn_fake(lrc);
	if(!right) right = spawn_fake(rrc);

	// do swap
	left->swap_toward(rrc);
	left->set_rc(rrc);
	right->swap_toward(lrc);
	right->set_rc(lrc);
	pit->swap(lrc, rrc);

	return true;
}

void BlockDirector::spawn_block(RowCol rc)
{
	BlockCol spawn_color = static_cast<BlockCol>(static_cast<int>(BlockCol::BLUE) + rndgen() % 6);
	auto block = std::make_shared<BlockImpl> (spawn_color, rc, pit);

	ordered_insert(blocks, block, y_greater);
	previews.push_back(block);
	stage->add(static_cast<Animation>(block));
	stage->add(static_cast<Logic>(block));
	pit->block(rc, block);
}

void BlockDirector::spawn_falling(RowCol rc)
{
	BlockCol spawn_color = static_cast<BlockCol>(static_cast<int>(BlockCol::BLUE) + rndgen() % 6);
	auto block = std::make_shared<BlockImpl> (spawn_color, rc, pit);

	ordered_insert(blocks, block, y_greater);
	stage->add(static_cast<Animation>(block));
	stage->add(static_cast<Logic>(block));

	block->set_state(BlockState::FALL);

	// land immediately?
	block_arrive_fall(block);
}

/**
 * Fake blocks are used to replace empty spaces for the duration of a swap().
 */
Block BlockDirector::spawn_fake(RowCol rc)
{
	auto block = std::make_shared<BlockImpl> (BlockCol::FAKE, rc, pit);

	ordered_insert(blocks, block, y_greater);
	stage->add(static_cast<Logic>(block));
	pit->block(rc, block);
	block->set_state(BlockState::REST);

	return block;
}

void BlockDirector::block_arrive_fall(Block block)
{
	RowCol rc = block->rc();
	RowCol next { rc.r + 1, rc.c };

	SDL_assert(next.r <= bottom); // can never fall lower than the preview row of blocks

	// If the next space is free, the block goes on to fall. Otherwise, it lands.
	if(pit->block_at(next)) {
		block->set_state(BlockState::LAND);
		hots.push_back(block);
	}
	else {
		move_block(block, next);
	}
}

void BlockDirector::block_arrive_swap(Block block)
{
	RowCol rc = block->rc();

	// fake blocks are only for swapping and disappear right afterwards
	if(BlockCol::FAKE == block->col) {
		auto it = std::find(blocks.begin(), blocks.end(), block);
		SDL_assert(it != blocks.end());
		block->set_state(BlockState::DEAD);
		return;
	}

	RowCol next { rc.r + 1, rc.c };

	// If the next space is free, the block starts falling. Otherwise, it rests.
	if(pit->block_at(next)) {
		block->set_state(BlockState::REST);
		hots.push_back(block);
	}
	else {
		block->set_state(BlockState::FALL);
		move_block(block, next);
	}
}

/**
 * Changes a block’s logical location.
 * The block itself will adjust its offset to maintain the same draw-position.
 * An approach of the draw-position towards the actual block position always happens
 * gradually using the block’s state and animation.
 */
void BlockDirector::move_block(Block block, RowCol to)
{
	pit->unblock(block->rc());
	pit->block(to, block);
	block->set_rc(to);
}

BlockDirector::BlockVec::iterator BlockDirector::reap_block(BlockDirector::BlockVec::iterator it)
{
	Block block = *it;
	RowCol rc = block->rc();

	// remove from our own list
	it = blocks.erase(it);

	// remove references from other containers
	pit->unblock(rc);
	if(block->col != BlockCol::FAKE)
		stage->remove(static_cast<Animation>(block));
	stage->remove(static_cast<Logic>(block));

	// Release blockage & blocks above the dead block => fall down
	BlockState state;
	auto fallible = [](BlockState s) { return BlockState::REST == s || BlockState::LAND == s; };

	do {
		RowCol prev { rc.r - 1, rc.c };
		block = pit->block_at(prev);

		if(block) {
			state = block->state();

			if(fallible(state)) {
				block->set_state(BlockState::FALL);
				move_block(block, rc);
				rc = prev; // continue looking 1 block above
			}
		}
		else {
			state = BlockState::INVALID; // no more blocks above
		}
	} while (fallible(state));

	return it;
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
	for(auto it = blocks.begin(); it != blocks.end(); ) {
		it = reap_block(it);
	}
}


void CursorDirector::move(Dir dir)
{
	RowCol& rc = cursor->rc;

	switch(dir) {
		case Dir::NONE: while(rc.r < pit->top()) cursor->rc.r++; break; // prevent cursor from scrolling off the top
		case Dir::LEFT: if(rc.c > 0) cursor->rc.c--; break;
		case Dir::RIGHT: if(rc.c < PIT_COLS-2) cursor->rc.c++; break;
		case Dir::UP: if(rc.r > pit->top()) cursor->rc.r--; break;
		case Dir::DOWN: if(rc.r < pit->bottom()) cursor->rc.r++; break;
		default: SDL_assert(false);
	}
}

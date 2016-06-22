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
			// // go to next row?
			// if(block->is_away()) {
			// 	RowCol rc = block->rc();
			// 	rc.r++;
			// 	block->set_rc(rc);
			// }

			// land block?
			if(block->is_arriving()) {
				block_arrive_row(block);
			}
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

void BlockDirector::spawn_block(RowCol rc)
{
	BlockCol spawn_color = static_cast<BlockCol>(static_cast<int>(BlockCol::BLUE) + rndgen() % 6);
	auto block = std::make_shared<BlockImpl> (spawn_color, rc, pit);

	ordered_insert(blocks, block, z_less);
	previews.push_back(block);
	stage->add(static_cast<Animation>(block));
	stage->add(static_cast<Logic>(block));
	pit->block(rc, block);
}

void BlockDirector::spawn_falling(RowCol rc)
{
	BlockCol spawn_color = static_cast<BlockCol>(static_cast<int>(BlockCol::BLUE) + rndgen() % 6);
	auto block = std::make_shared<BlockImpl> (spawn_color, rc, pit);

	ordered_insert(blocks, block, z_less);
	stage->add(static_cast<Animation>(block));
	stage->add(static_cast<Logic>(block));

	block->set_state(BlockState::FALL);

	// land immediately?
	block_arrive_row(block);
}

void BlockDirector::block_arrive_row(Block block)
{
	RowCol rc = block->rc();
	RowCol next { rc.r + 1, rc.c };

	SDL_assert(next.r <= bottom); // can never fall lower than the preview row of blocks

	// If the next space is free, the block goes on to fall. Otherwise, it lands.
	if(pit->block_at(next)) {
		block->set_state(BlockState::LAND);
	}
	else {
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
 
	// Maintain blocks sorted from bottom to top. This way, lower blocks in pillars of falling
	// blocks will fall out of the way before upper blocks stumble over them.
	std::sort(blocks.begin(), blocks.end(), y_greater);
}

BlockDirector::BlockVec::iterator BlockDirector::reap_block(BlockDirector::BlockVec::iterator it)
{
	Block block = *it;
	RowCol rc = block->rc();

	// remove from our own list
	it = blocks.erase(it);

	// remove references from other containers
	pit->unblock(rc);
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

/**
 * Implementation of director logic.
 */

#include "director.hpp"

/**
 * Spawn blocks at regular intervals, clean up dead blocks
 */
void Director::update()
{
	pit->update();

	// spawn blocks from below
	int pit_bottom = pit->bottom();
	while(bottom < pit_bottom) {
		activate_previews();

		bottom++;
		for(int i = 0; i < PIT_COLS; i++) {
			RowCol rc {bottom, i};
			spawn_block(rc);
		}
	}

	// Handle indivitual logic for each block
	for(auto it = blocks.begin(); it != blocks.end(); ) {
		SharedBlock block = *it;
		Block::State state = block->state();

		// block above top => game over
		if(block->loc().y < pit->loc().y) {
			game_over();
			break; // interrupt blocks logic because game_over might invalidate blocks list
		}

		// falling blocks arrived at next row (center)
		if(state == Block::State::FALL && block->entering_row()) {
			block_arrive_row(block);
		}

		// cleanup dead blocks
		if(Block::State::DEAD == state) {
			it = reap_block(it);
		}
		else {
			++it;
		}
	}

	// Examine hots for matches
	hots.clear();
}

void Director::spawn_block(RowCol rc)
{
	Block::Col spawn_color = static_cast<Block::Col>(static_cast<int>(Block::Col::BLUE) + rndgen() % 6);
	auto block = std::make_shared<Block> (spawn_color, rc, pit);

	blocks.push_back(block);
	previews.push_back(block);
	stage->add(static_cast<SharedAnimation>(block));
	stage->add(static_cast<SharedLogic>(block));
	pit->block(rc, block);
}

void Director::spawn_falling(RowCol rc)
{
	Block::Col spawn_color = static_cast<Block::Col>(static_cast<int>(Block::Col::BLUE) + rndgen() % 6);
	auto block = std::make_shared<Block> (spawn_color, rc, pit);

	blocks.push_back(block);
	stage->add(static_cast<SharedAnimation>(block));
	stage->add(static_cast<SharedLogic>(block));

	block->set_state(Block::State::FALL);

	// land immediately?
	block_arrive_row(block);
}

void Director::block_arrive_row(SharedBlock block)
{
	RowCol rc = block->rc;
	RowCol next_row { rc.r + 1, rc.c };

	// hit bottom? TODO: there isn’t a bottom, (bottom blocks are not matchable), so remove this
	if(next_row.r > bottom) {
		block->set_state(Block::State::LAND);
		pit->block(rc, block);
	}

	// hit another block?
	WeakBlock next_block = pit->block_at(next_row);

	if(next_block.lock()) {
		block->set_state(Block::State::LAND);
		pit->block(rc, block);
	}
}

Director::BlockVec::iterator Director::reap_block(Director::BlockVec::iterator it)
{
	// remove from our own list
	SharedBlock block = *it;
	it = blocks.erase(it);

	// remove references from other containers
	pit->unblock(block->rc);
	stage->remove(static_cast<SharedAnimation>(block));
	stage->remove(static_cast<SharedLogic>(block));

	// Release blockage & blocks above the dead block => fall down
	Block::State state;
	auto fallible = [](Block::State s) { return Block::State::REST == s || Block::State::LAND == s; };

	do {
		RowCol rc = block->rc;
		RowCol prev_row { rc.r - 1, rc.c };

		SharedBlock prev_block = pit->block_at(prev_row);

		if(prev_block) {
			state = prev_block->state();

			if(fallible(state)) {
				prev_block->set_state(Block::State::FALL);
				pit->unblock(prev_row);
				block = prev_block; // continue looking 1 block above
			}
		}
		else {
			state = Block::State::INVALID; // no more blocks above
		}
	} while (fallible(state));

	return it;
}

/**
 * Make all blocks from the preview row into regular matchable resting blocks.
 * This assumes that they have now fully scrolled into view.
 */
void Director::activate_previews()
{
	for(auto block : previews) {
		block->set_state(Block::State::REST);
		hots.push_back(block);
	}

	previews.clear();
}

/**
 * Preliminary game over implementation: kill all blocks and just continue
 */
void Director::game_over()
{
	for(auto it = blocks.begin(); it != blocks.end(); ) {
		SharedBlock block = *it;

		if(!block->is_obstacle()) // all blocks must be marked blocking in pit to be reaped
			pit->block(block->rc, block);

		block->set_state(Block::State::DEAD);
		it = reap_block(it);
	}
}

/**
 * director.hpp
 */

#include "block.hpp"
#include "context.hpp"
#include <SDL2/SDL_assert.h>
#include <algorithm>

/**
 * Spawns and removes stuff to and from the stage.
 * The director implements game-logical interactions between objects which these
 * objects cannot handle on their own.
 * Exmaples are spawning and reaping, block collisions and making blocks fall when they lose support.
 * The director does /not/ concern itself with pixel coordinates - it only thinks in block rows and columns.
 */
class BlockDirector
{

public:

	BlockDirector(SharedStage stage, SharedPit pit) : stage(stage), pit(pit), rdev(), rndgen(rdev()), bottom(0) {}

	/**
	 * Spawn blocks at regular intervals, clean up dead blocks
	 */
	void update()
	{
		pit->update();

		// spawn blocks from below
		int pit_bottom = pit->bottom();
		while(bottom < pit_bottom) {
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
	}

private:

	using BlockVec = std::vector<SharedBlock>;

	SharedStage stage;
	SharedPit pit;
	BlockVec blocks;
	std::random_device rdev;
	std::mt19937 rndgen;
	int bottom; // lowest row that we have already spawned blocks for

	void spawn_block(RowCol rc)
	{
		Block::Col spawn_color = static_cast<Block::Col>(static_cast<int>(Block::Col::BLUE) + rndgen() % 6);
		auto block = std::make_shared<Block> (spawn_color, rc, Block::State::REST, pit);

		blocks.push_back(block);
		stage->add(static_cast<SharedAnimation>(block));
		stage->add(static_cast<SharedLogic>(block));
		pit->block(rc, block);
	}

	void spawn_falling(RowCol rc)
	{
		Block::Col spawn_color = static_cast<Block::Col>(static_cast<int>(Block::Col::BLUE) + rndgen() % 6);
		auto block = std::make_shared<Block> (spawn_color, rc, Block::State::FALL, pit);

		blocks.push_back(block);
		stage->add(static_cast<SharedAnimation>(block));
		stage->add(static_cast<SharedLogic>(block));

		// land immediately?
		block_arrive_row(block);
	}

	void block_arrive_row(SharedBlock block)
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

	BlockVec::iterator reap_block(BlockVec::iterator it)
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
	 * Preliminary game over implementation: kill all blocks and just continue
	 */
	void game_over()
	{
		for(auto it = blocks.begin(); it != blocks.end(); ) {
			SharedBlock block = *it;

			if(!block->is_obstacle()) // all blocks must be marked blocking in pit to be reaped
				pit->block(block->rc, block);

			block->set_state(Block::State::DEAD);
			it = reap_block(it);
		}
	}
};

using SharedDirector = std::shared_ptr<BlockDirector>;

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

	BlockDirector(SharedStage stage, SharedPit pit) : stage(stage), pit(pit), rdev(), rndgen(rdev()), next_spawn(rndgen() % 30), bottom(0) {}

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

		// spawn blocks from above
		next_spawn--;

		if(next_spawn <= 0) {
			RowCol rc {-9, static_cast<int>(rndgen() % PIT_COLS)}; // let blocks fall from top row
			spawn_falling(rc);

			next_spawn = 20 + rndgen() % 10;
		}

		// Handle indivitual logic for each block
		for(auto it = blocks.begin(); it != blocks.end(); ) {
			SharedBlock block = *it;
			Block::State state = block->state();

			// falling blocks arrived at next row (center)
			if(state == Block::State::FALL && block->entering_row()) {
				block_arrive_row(block);
			}

			// cleanup dead blocks
			if(Block::State::DEAD == state) {
				// remove references from other containers
				pit->unblock(block->rc);
				stage->remove(static_cast<SharedAnimation>(block));
				stage->remove(static_cast<SharedLogic>(block));

				// remove from our own list
				auto it = std::find(blocks.begin(), blocks.end(), block);
				SDL_assert(it != blocks.end());
				it = blocks.erase(it);

				// unblock blocks above
				block_dead(block);
			}
			else {
				++it;
			}
		}
	}

private:

	SharedStage stage;
	SharedPit pit;
	std::vector<SharedBlock> blocks;
	std::random_device rdev;
	std::mt19937 rndgen;
	int next_spawn;
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

	void block_dead(WeakBlock block)
	{
		// Release blockage & blocks above the dead block => fall down
		SharedBlock locked_block = block.lock();
		SDL_assert(locked_block);
		
		Block::State state;
		auto fallible = [](Block::State s) { return Block::State::REST == s || Block::State::LAND == s; };

		do {
			RowCol rc = locked_block->rc;
			RowCol prev_row { rc.r - 1, rc.c };

			WeakBlock prev_block = pit->block_at(prev_row);
			SharedBlock locked_prev = prev_block.lock();

			if(locked_prev) {
				state = locked_prev->state();

				if(fallible(state)) {
					locked_prev->set_state(Block::State::FALL);
					pit->unblock(prev_row);
					locked_block = locked_prev; // continue looking 1 block above
				}
			}
			else {
				state = Block::State::INVALID; // no more blocks above
			}
		} while (fallible(state));
	}

};

using SharedDirector = std::shared_ptr<BlockDirector>;

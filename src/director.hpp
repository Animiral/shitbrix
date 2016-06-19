/**
 * director.hpp
 * The director implements game-logical interactions between objects which these
 * objects cannot handle on their own.
 * Exmaples are block collisions and making blocks fall when they lose support.
 */

#include "block.hpp"
#include "context.hpp"
#include <SDL2/SDL_assert.h>
#include <algorithm>

/**
 * Spawns and removes stuff to and from the stage.
 */
class BlockDirector
{

public:

	BlockDirector(WeakStage stage, WeakPit pit) : stage(stage), pit(pit), rdev(), rndgen(rdev()), next_spawn(rndgen() % 30) {}

	/**
	 * Spawn blocks at regular intervals, clean up dead blocks
	 */
	void update()
	{
		SharedStage locked_stage = stage.lock();
		SharedPit locked_pit = pit.lock();
		SDL_assert(locked_stage);
		SDL_assert(locked_pit);

		// spawn blocks
		next_spawn--;

		if(next_spawn <= 0) {
			Block::Col spawn_color = static_cast<Block::Col>(static_cast<int>(Block::Col::BLUE) + rndgen() % 6);
			RowCol rc {-9, static_cast<int>(rndgen() % 6)}; // let blocks fall from top row
			Point block_loc = locked_pit->loc();
			block_loc.x += rc.c * BLOCK_W;
			auto block = std::make_shared<Block> (spawn_color, block_loc, rc, Block::State::FALL);

			blocks.push_back(block);
			locked_stage->add(static_cast<SharedAnimation>(block));
			locked_stage->add(static_cast<SharedLogic>(block));

			next_spawn = 20 + rndgen() % 10;
		}

		// Handle indivitual logic for each block
		for(auto it = blocks.begin(); it != blocks.end(); ) {
			SharedBlock block = *it;
			Block::State state = block->state();

			// falling blocks arrived at next row (center)
			if(state == Block::State::FALL && block->offset.y >= 0 && block->offset.y < FALL_SPEED) {
				block_arrive_row(block);
			}

			// cleanup dead blocks
			if(Block::State::DEAD == state) {
				// remove references from other containers
				locked_pit->unblock(block->rc);
				locked_stage->remove(static_cast<SharedAnimation>(block));
				locked_stage->remove(static_cast<SharedLogic>(block));

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

	void block_arrive_row(WeakBlock block)
	{
		SharedBlock locked_block = block.lock();
		SharedPit locked_pit = pit.lock();
		SDL_assert(locked_block);
		SDL_assert(locked_pit);

		RowCol rc = locked_block->rc;
		RowCol next_row { rc.r + 1, rc.c };

		// hit bottom? TODO: there isn’t a bottom, so remove this
		if(next_row.r > 0) {
			locked_block->set_state(Block::State::LAND);
			locked_pit->block(rc, block);
		}

		// hit another block?
		WeakBlock next_block = locked_pit->block_at(next_row);

		if(next_block.lock()) {
			locked_block->set_state(Block::State::LAND);
			locked_pit->block(rc, block);
		}
	}

	void block_dead(WeakBlock block)
	{
		// Release blockage & blocks above the dead block => fall down
		SharedBlock locked_block = block.lock();
		SharedPit locked_pit = pit.lock();
		SDL_assert(locked_block);
		SDL_assert(locked_pit);
		
		Block::State state;
		auto fallible = [](Block::State s) { return Block::State::REST == s || Block::State::LAND == s; };

		do {
			RowCol rc = locked_block->rc;
			RowCol prev_row { rc.r - 1, rc.c };

			WeakBlock prev_block = locked_pit->block_at(prev_row);
			SharedBlock locked_prev = prev_block.lock();

			if(locked_prev) {
				state = locked_prev->state();

				if(fallible(state)) {
					locked_prev->set_state(Block::State::FALL);
					locked_pit->unblock(prev_row);
					locked_block = locked_prev; // continue looking 1 block above
				}
			}
			else {
				state = Block::State::INVALID; // no more blocks above
			}
		} while (fallible(state));
	}

private:

	WeakStage stage;
	WeakPit pit;
	std::vector<SharedBlock> blocks;
	std::random_device rdev;
	std::mt19937 rndgen;
	int next_spawn;

};

using SharedDirector = std::shared_ptr<BlockDirector>;

/**
 * director.hpp
 * The director implements game-logical interactions between objects which these
 * objects cannot handle on their own.
 * Exmaples are block collisions and making blocks fall when they lose support.
 */

#include "block.hpp"

/**
 * Spawns and removes stuff to and from the stage.
 */
class BlockDirector
{

public:

	BlockDirector(std::weak_ptr<Stage> stage, Point loc) : loc(loc), stage(stage), rdev(), rndgen(rdev()), next_spawn(rndgen() % 30) {}

	/**
	 * Spawn blocks at regular intervals, clean up dead blocks
	 */
	void update()
	{
		auto locked_stage = stage.lock(); // get access to shared_ptr from weak_ptr

		// spawn blocks
		next_spawn--;

		if(next_spawn <= 0) {
			Block::Col spawn_color = static_cast<Block::Col>(static_cast<int>(Block::Col::BLUE) + rndgen() % 6);
			Point block_loc = loc;
			int r = 9; // let blocks fall from top row
			int c = rndgen() % 6;
			block_loc.x += c * BLOCK_W;
			auto block = std::make_shared<Block> (spawn_color, block_loc, r, c, Block::State::FALL);

			blocks.push_back(block);
			locked_stage->add(static_cast<SharedAnimation>(block));
			locked_stage->add(static_cast<SharedLogic>(block));

			next_spawn = 10 + rndgen() % 20;
		}

		// cleanup dead blocks
		for(auto it = blocks.begin(); it != blocks.end(); ) {
			auto& block = *it;

			if(Block::State::DEAD == block->state()) {
				locked_stage->remove(static_cast<SharedAnimation>(block));
				locked_stage->remove(static_cast<SharedLogic>(block));
				it = blocks.erase(it);
			}
			else {
				++it;
			}
		}
	}

private:

	Point loc;
	std::weak_ptr<Stage> stage;
	std::vector<SharedBlock> blocks;
	std::random_device rdev;
	std::mt19937 rndgen;
	int next_spawn;

};

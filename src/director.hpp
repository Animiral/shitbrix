/**
 * director.hpp
 * The director implements game-logical interactions between objects which these
 * objects cannot handle on their own.
 * Exmaples are block collisions and making blocks fall when they lose support.
 */

#include "block.hpp"
#include "context.hpp"

/**
 * Spawns and removes stuff to and from the stage.
 */
class BlockDirector : public ILogicSubscriber, public std::enable_shared_from_this<BlockDirector>
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
			auto block = std::make_shared<Block> (spawn_color, block_loc, rc, Block::State::FALL, shared_from_this());

			blocks.push_back(block);
			locked_stage->add(static_cast<SharedAnimation>(block));
			locked_stage->add(static_cast<SharedLogic>(block));

			next_spawn = 20 + rndgen() % 10;
		}

		// cleanup dead blocks
		for(auto it = blocks.begin(); it != blocks.end(); ) {
			auto& block = *it;

			if(Block::State::DEAD == block->state()) {
				locked_pit->unblock(block->rc());
				locked_stage->remove(static_cast<SharedAnimation>(block));
				locked_stage->remove(static_cast<SharedLogic>(block));
				it = blocks.erase(it);
			}
			else {
				++it;
			}
		}
	}

	virtual void notify_block_arrive_row(WeakBlock block) override
	{
		SharedBlock locked_block = block.lock();
		SharedPit locked_pit = pit.lock();
		SDL_assert(locked_block);
		SDL_assert(locked_pit);

		RowCol rc = locked_block->rc();
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

private:

	WeakStage stage;
	WeakPit pit;
	std::vector<SharedBlock> blocks;
	std::random_device rdev;
	std::mt19937 rndgen;
	int next_spawn;

};

using SharedDirector = std::shared_ptr<BlockDirector>;

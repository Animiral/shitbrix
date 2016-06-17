/**
 * block.hpp
 * Block objects
 */
#pragma once

#include "shitbrix.hpp"
#include "context.hpp"
#include "sdl_helper.hpp"
#include <memory>
#include <vector>
#include <algorithm>
#include <random>

/**
 * Basic interface that specifies that an object can be drawn to the screen.
 */
class IScreenObject
{
	public: virtual void draw(const IVideoContext& context, float dt) =0; // dt: fraction of current display frame time elapsed
};

/**
 * Basic interface for animated objects
 */
class IAnimation : public IScreenObject
{
	public: virtual void animate() =0; // Called once per frame to update animation
};

/**
 * Basic interface for objects subject to game logic
 */
class ILogicObject
{
	public: virtual void update() =0; // advance the object by one tick
};

class IHistoryObject {}; // interface go-back etc.

// Single block, comes in 6 colors
// State machine: a block can change state only after its time has run down (1 per tick)
class Block : public IAnimation, public ILogicObject
{
public:
	enum class Col { INVALID, BLUE, RED, YELLOW, GREEN, PURPLE, ORANGE };
	enum class State { INVALID, REST, FALL, LAND, BREAK, DEAD };

	Col col;     // color
	Point loc;   // logical location, upper left corner (not necessarily sprite draw location)
	int c;       // column 0-5 (left to right)
	int r;       // row position relative to the stack origin. + is UP, - is DOWN
	State state; // current block state. On state time out, tell an IStateSubscriber (previously saved via Block::subscribe()) with notify()
	int time;    // number of ticks until we consider a state switch

	Block(Col col, Point loc, int c, int r, State state, int time) : col(col), loc(loc), c(c), r(r), state(state), time(time) {}

	virtual void draw(const IVideoContext& context, float dt) override
	{
		Point draw_loc = loc;

		// bounce when landing
		if(State::LAND == state) {
			draw_loc.y -= (dt < .5f) ? dt * 10.f : (1.f-dt) * 10.f;
		}

		switch(col) {
			case Col::BLUE:   context.drawGfx(Gfx::BLOCK_BLUE, loc);   break;
			case Col::RED:    context.drawGfx(Gfx::BLOCK_RED, loc);    break;
			case Col::YELLOW: context.drawGfx(Gfx::BLOCK_YELLOW, loc); break;
			case Col::GREEN:  context.drawGfx(Gfx::BLOCK_GREEN, loc);  break;
			case Col::PURPLE: context.drawGfx(Gfx::BLOCK_PURPLE, loc); break;
			case Col::ORANGE: context.drawGfx(Gfx::BLOCK_ORANGE, loc); break;
			default: SDL_assert_paranoid(false);
		}
	}

	virtual void animate() override {}

	/**
	 * State machine spaghetti for block behavior
	 */
	virtual void update() override
	{
		time--;

		switch(state) {

		case State::REST:
			// rest lasts forever by default
			break;

		case State::FALL:
			loc.y+=3;
			if(loc.y >= 408) {
				loc.y = 408;
				state = State::LAND;
				time = 30;
			}
			break;

		case State::LAND:
			if(time < 0) {
				state = State::BREAK;
				time = 30;
			}
			break;

		case State::BREAK:
			if(time < 0) {
				state = State::DEAD;
			}
			break;

		case State::DEAD:
			throw GameException("Cannot update() dead block.");

		default:
			SDL_assert_paranoid(false);

		}
	}
};

using SharedAnimation = std::shared_ptr<IAnimation>;
using SharedLogic = std::shared_ptr<ILogicObject>;
using SharedBlock = std::shared_ptr<Block>;

/**
 * Stage is a container for on-screen objects.
 * The Stage owns all its objects as shared_ptrs.
 */
class Stage : public IAnimation, public ILogicObject
{

public:

	Stage() {}

	void add(SharedAnimation animation)
	{
		animations.push_back(animation);
	}

	void add(SharedLogic logic)
	{
		logics.push_back(logic);
	}

	void remove(SharedAnimation animation)
	{
		auto it = std::find(animations.begin(), animations.end(), animation);
		SDL_assert(it != animations.end());
		animations.erase(it);
	}

	void remove(SharedLogic logic)
	{
		auto it = std::find(logics.begin(), logics.end(), logic);
		SDL_assert(it != logics.end());
		logics.erase(it);
	}

	virtual void draw(const IVideoContext& context, float dt) override
	{
		context.drawGfx(Gfx::BACKGROUND, Point{0,0});
		for(auto& animation : animations) animation->draw(context, dt);
	}

	virtual void animate() override
	{
		for(auto& animation : animations) animation->animate();
	}

	virtual void update() override
	{
		for(auto& logic : logics) logic->update();
	}

private:

	std::vector< SharedAnimation > animations;
	std::vector< SharedLogic > logics;

};

/**
 * A pit is the playing area where one player’s blocks fall down.
 * The pit itself does not animate or update the contained blocks and garbage.
 */
class Pit : public IAnimation, public ILogicObject
{

public:

	Pit(Point loc) : loc(loc) {}

	virtual void draw(const IVideoContext& context, float dt) override
	{
		// nothing so far
	}

	virtual void animate() override {}
	virtual void update() override {}

private:

	Point loc;   // location, upper left corner

};

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
			int c = rndgen() % 6;
			block_loc.x += c * BLOCK_W;
			auto block = std::make_shared<Block> (spawn_color, block_loc, 0, 0, Block::State::FALL, 0);

			blocks.push_back(block);
			locked_stage->add(static_cast<SharedAnimation>(block));
			locked_stage->add(static_cast<SharedLogic>(block));

			next_spawn = 10 + rndgen() % 20;
		}

		// cleanup dead blocks
		for(auto it = blocks.begin(); it != blocks.end(); ) {
			auto& block = *it;

			if(Block::State::DEAD == block->state) {
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

class StageBuilder
{
public:
	std::shared_ptr<Stage> construct()
	{
		auto stage = std::make_shared<Stage>();

		// auto block1 = std::make_shared<Block> (Block::Col::BLUE, (Point){30,30}, 0, 0, Block::State::REST, 0);
		// stage->add(static_cast<SharedAnimation>(block1));
		// stage->add(static_cast<SharedLogic>(block1));

		// auto block2 = std::make_shared<Block> (Block::Col::RED, (Point){80,30}, 0, 0, Block::State::FALL, 0);
		// stage->add(static_cast<SharedAnimation>(block2));
		// stage->add(static_cast<SharedLogic>(block2));

		return stage;
	}
};

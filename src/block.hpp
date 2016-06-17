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
class Shaft : public ILogicObject {}; // name?

// Single block, comes in 6 colors
// State machine: a block can change state only after its time has run down (1 per tick)
class Block : public IAnimation, public ILogicObject
{

public:
	enum class Col { INVALID, BLUE, RED, YELLOW, GREEN, PURPLE, ORANGE };
	enum class State { INVALID, REST, FALL, LAND, BREAK };

	Col col;     // color
	Point loc;   // screen draw location
	int c;       // column 0-5 (left to right)
	int r;       // row position relative to the stack origin. + is UP, - is DOWN
	State state; // current block state. On state time out, tell an IStateSubscriber (previously saved via Block::subscribe()) with notify()
	int time;    // number of ticks until we consider a state switch

	Block(Col col, Point loc, int c, int r, State state, int time) : col(col), loc(loc), c(c), r(r), state(state), time(time) {}

	virtual void draw(const IVideoContext& context, float dt) override
	{
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

	virtual void update() override
	{
		time--;

		switch(state) {
			case State::REST: break;
			case State::FALL: loc.y++; break;
			case State::LAND: break;
			case State::BREAK: break;
			default: SDL_assert_paranoid(false);
		}
	}
};

using AnimationPtr = std::shared_ptr<IAnimation>;
using LogicPtr = std::shared_ptr<ILogicObject>;

/**
 * Stage is a container for on-screen objects.
 * The Stage owns all its objects as shared_ptrs.
 */
class Stage : public IAnimation, public ILogicObject
{
public:
	Stage() {}

	void add(AnimationPtr animation)
	{
		animations.push_back(animation);
	}

	void add(LogicPtr logic)
	{
		logics.push_back(logic);
	}

	void remove(AnimationPtr animation)
	{
		auto it = std::find(animations.begin(), animations.end(), animation);
		SDL_assert(it != animations.end());
		animations.erase(it);
	}

	void remove(LogicPtr logic)
	{
		auto it = std::find(logics.begin(), logics.end(), logic);
		SDL_assert(it != logics.end());
		logics.erase(it);
	}

	virtual void draw(const IVideoContext& context, float dt) override
	{
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
	std::vector< AnimationPtr > animations;
	std::vector< LogicPtr > logics;
};

class StageBuilder
{
public:
	Stage construct()
	{
		Stage stage;
		auto block1 = std::make_shared<Block> (Block::Col::BLUE, (Point){30,30}, 0, 0, Block::State::REST, 0);
		auto block2 = std::make_shared<Block> (Block::Col::RED, (Point){80,30}, 0, 0, Block::State::FALL, 0);
		stage.add(static_cast<AnimationPtr>(block1));
		stage.add(static_cast<AnimationPtr>(block2));
		stage.add(static_cast<LogicPtr>(block1));
		stage.add(static_cast<LogicPtr>(block2));

		return stage;
	}
};

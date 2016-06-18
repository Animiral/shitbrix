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

// Single block, comes in 6 colors
// State machine: a block can change state only after its time has run down (1 per tick)
class Block : public IAnimation, public ILogicObject
{

public:

	enum class Col { INVALID, BLUE, RED, YELLOW, GREEN, PURPLE, ORANGE };
	enum class State { INVALID, REST, FALL, LAND, BREAK, DEAD };

	Block(Col col, Point loc, int r, int c, State state) : col(col), loc(loc), r(r), c(c), offset{0,0}, m_state(state), m_time(0) {}

	virtual void draw(const IVideoContext& context, float dt) override
	{
		Point draw_loc = loc;

		// bounce when landing
		if(State::LAND == m_state) {
			draw_loc.y -= BOUNCE_H * ( m_time > LAND_TIME/2 ? (LAND_TIME-m_time+dt) : (m_time-dt) ) / LAND_TIME;
		}

		switch(col) {
			case Col::BLUE:   context.drawGfx(Gfx::BLOCK_BLUE, draw_loc);   break;
			case Col::RED:    context.drawGfx(Gfx::BLOCK_RED, draw_loc);    break;
			case Col::YELLOW: context.drawGfx(Gfx::BLOCK_YELLOW, draw_loc); break;
			case Col::GREEN:  context.drawGfx(Gfx::BLOCK_GREEN, draw_loc);  break;
			case Col::PURPLE: context.drawGfx(Gfx::BLOCK_PURPLE, draw_loc); break;
			case Col::ORANGE: context.drawGfx(Gfx::BLOCK_ORANGE, draw_loc); break;
			default: SDL_assert_paranoid(false);
		}
	}

	virtual void animate() override {}

	/**
	 * State machine spaghetti for block behavior
	 */
	virtual void update() override
	{
		m_time--;

		switch(m_state) {
			case State::REST: break; // rest lasts forever by default 
			case State::FALL: fall(); break;
			case State::LAND: land(); break;
			case State::BREAK: dobreak(); break;
			case State::DEAD: throw GameException("Cannot update() dead block.");
			default: SDL_assert_paranoid(false);
		}
	}

	State state() const
	{
		return m_state;
	}

	void set_state(State state, int time = 0)
	{
		m_state = state;
		m_time = time;
	}

private:

	static constexpr float BOUNCE_H = 10.f;
	static constexpr int LAND_TIME = 20;

	Col col;       // color
	Point loc;     // logical location, upper left corner (not necessarily sprite draw location)
	int r;         // row position relative to the stack origin. + is UP, - is DOWN
	int c;         // column 0-5 (left to right)
	Point offset;  // x/y offset from draw center of r/c location
	State m_state; // current block state. On state time out, tell an IStateSubscriber (previously saved via Block::subscribe()) with notify()
	int m_time;    // number of ticks until we consider a state switch

	/**
	 * Update this falling block
	 */
	void fall()
	{
		loc.y += FALL_SPEED;
		offset.y += FALL_SPEED;

		// go to next row?
		if(offset.y > BLOCK_H/2) {
			r--;
			offset.y -= BLOCK_H;
		}

		// hit bottom?
		if(r <= 0 && offset.y >= 0) {
			offset.y = 0;
			set_state(State::LAND, LAND_TIME);
		}
	}

	/**
	 * Update this landing block
	 */
	void land()
	{
		if(m_time < 0) {
			set_state(State::BREAK, 30);
		}
	}

	/**
	 * Update this breaking block
	 */
	void dobreak()
	{
		if(m_time < 0) {
			set_state(State::DEAD);
		}
	}

};

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

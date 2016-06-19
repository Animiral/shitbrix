/**
 * Definitions for block and stage objects
 */

#include "block.hpp"
#include <SDL2/SDL_assert.h>
#include <algorithm>

void Block::draw(const IVideoContext& context, float dt)
{
	Point draw_loc = m_view->transform(m_loc, dt);

	// bounce when landing
	if(State::LAND == m_state) {
		// TODO: include dt in landing anim, don’t forget FPS-TPS conversion
		draw_loc.y -= BOUNCE_H * ( m_time > LAND_TIME/2 ? (LAND_TIME-m_time) : (m_time) ) / LAND_TIME;
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

/**
 * State machine spaghetti for block behavior
 */
void Block::update()
{
	m_time--;

	switch(m_state) {
		case State::PREVIEW: break;
		case State::REST: break;
		case State::FALL: fall(); break;
		case State::LAND: land(); break;
		case State::BREAK: dobreak(); break;
		case State::DEAD: throw GameException("Cannot update() dead block.");
		default: SDL_assert_paranoid(false);
	}
}

/**
 * Returns true if the block is in a state that prevents other blocks and objects from falling down.
 */
bool Block::is_obstacle() const
{
	return State::PREVIEW == m_state || State::REST == m_state || State::LAND == m_state || State::BREAK == m_state;
}

void Block::set_state(State state)
{
	m_state = state;

	switch(state) {
		case State::LAND:
			// Correct the block by any eventual extra-pixels
			m_loc.x -= offset.x;
			m_loc.y -= offset.y;
			offset = Point{0,0};
			m_time = LAND_TIME;
			break;

		case State::BREAK:
			m_time = BREAK_TIME;
			break;

		default: break;
	}
}

/**
 * Returns true if the block is just now arriving at the center of a new row.
 */
bool Block::entering_row()
{
	return State::FALL == m_state && offset.y >= 0 && offset.y < FALL_SPEED;
}

/**
 * Update this falling block
 */
void Block::fall()
{
	m_loc.y += FALL_SPEED;
	offset.y += FALL_SPEED;

	// go to next row?
	if(offset.y > BLOCK_H/2) {
		rc.r++;
		offset.y -= BLOCK_H;
	}
}

/**
 * Update this landing block
 */
void Block::land()
{
	if(m_time < 0) {
		set_state(State::REST);
		m_time = 10 - 10 * rc.r; // after which auto-breaks
	}
}

/**
 * Update this breaking block
 */
void Block::dobreak()
{
	if(m_time < 0) {
		set_state(State::DEAD);
		// SharedSubscriber locked_subscriber = subscriber.lock();
		// SDL_assert(locked_subscriber);
		// locked_subscriber->notify_block_dead(shared_from_this());
	}
}


/**
 * Returns the number of the bottom visible row in the pit
 */
int Pit::bottom() const
{
	return static_cast<int>((m_scroll + PIT_H - 1) / BLOCK_H);
}

/**
 * Set the given location to blocked.
 */
void Pit::block(RowCol rc, SharedBlock block)
{
	auto result = block_map.emplace(std::make_pair(rc, block));
	game_assert(result.second, "Attempt to block already blocked space in Pit.");
}

/**
 * Set the given location to not blocked.
 */
void Pit::unblock(RowCol rc)
{
	size_t erased = block_map.erase(rc);
	game_assert(1 == erased, "Attempt to unblock empty space in Pit.");
}

/**
 * Return the block at the given location.
 */
SharedBlock Pit::block_at(RowCol rc) const
{
	auto it = block_map.find(rc);
	if(it == block_map.end())
		return SharedBlock(); // default-constructed weak_ptr resembles nullptr
	else
		return it->second;
}

/**
 * The origin {0,0} location of all pit-related objects corresponds with row 0, column 0.
 * We have to transform the object into the pit and from there, apply the pit scrolling.
 */
Point Pit::transform(Point point, float dt) const
{
	point.x += m_loc.x;
	point.y += m_loc.y;
	// TODO: include dt in scroll anim, don’t forget FPS-TPS conversion
	point.y -= m_scroll;
	return point;
}

void Pit::update()
{
	// scroll more
	m_scroll += SCROLL_SPEED;
}


void Stage::add(SharedAnimation animation)
{
	animations.push_back(animation);
}

void Stage::add(SharedLogic logic)
{
	logics.push_back(logic);
}

void Stage::remove(SharedAnimation animation)
{
	auto it = std::find(animations.begin(), animations.end(), animation);
	SDL_assert(it != animations.end());
	animations.erase(it);
}

void Stage::remove(SharedLogic logic)
{
	auto it = std::find(logics.begin(), logics.end(), logic);
	SDL_assert(it != logics.end());
	logics.erase(it);
}

void Stage::draw(const IVideoContext& context, float dt)
{
	context.drawGfx(Gfx::BACKGROUND, Point{0,0});
	for(auto& animation : animations) animation->draw(context, dt);
}

void Stage::animate()
{
	for(auto& animation : animations) animation->animate();
}

void Stage::update()
{
	for(auto& logic : logics) logic->update();
}

std::shared_ptr<Stage> StageBuilder::construct()
{
	SharedStage stage = std::make_shared<Stage>();


	return stage;
}


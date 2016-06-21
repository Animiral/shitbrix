/**
 * Definitions for block and stage objects
 */

#include "block.hpp"
#include <SDL2/SDL_assert.h>
#include <algorithm>
#include <functional>

int operator-(BlockCol lhs, BlockCol rhs)
{
	return static_cast<int>(lhs) - static_cast<int>(rhs);
}

void BlockImpl::draw(const IVideoContext& context, float dt)
{
	Point draw_loc = m_view->transform(m_loc, dt);

	// bounce when landing
	if(BlockState::LAND == m_state) {
		// TODO: include dt in landing anim, don’t forget FPS-TPS conversion
		draw_loc.y -= BOUNCE_H * ( time > LAND_TIME/2 ? LAND_TIME-time : time ) / LAND_TIME;
	}

	Gfx gfx = Gfx::BLOCK_BLUE + (col - BlockCol::BLUE);

	BlockFrame frame = BlockFrame::REST;
	if(BlockState::PREVIEW == m_state) frame = BlockFrame::PREVIEW;
	if(BlockState::BREAK == m_state) frame = m_anim;

	context.drawGfx(draw_loc, gfx, static_cast<size_t>(frame));
}

void BlockImpl::animate()
{
	++m_anim;

	if(BlockState::BREAK == m_state && m_anim >= BlockFrame::BREAK_END)
		m_anim = BlockFrame::BREAK_BEGIN;
}

/**
 * State machine spaghetti for block behavior
 */
void BlockImpl::update()
{
	time--;

	switch(m_state) {
		case BlockState::PREVIEW: break;
		case BlockState::REST: break;
		case BlockState::FALL: fall(); break;
		case BlockState::LAND: land(); break;
		case BlockState::BREAK: dobreak(); break;
		case BlockState::DEAD: throw GameException("Cannot update() dead block.");
		default: SDL_assert_paranoid(false);
	}
}

/**
 * Returns true if the block is in a state that prevents other blocks and objects from falling down.
 */
bool BlockImpl::is_obstacle() const
{
	return BlockState::PREVIEW == m_state || BlockState::REST == m_state || BlockState::LAND == m_state || BlockState::BREAK == m_state;
}

void BlockImpl::set_state(BlockState state)
{
	SDL_assert(state != BlockState::PREVIEW);
	SDL_assert(m_state != BlockState::DEAD); // cannot change out of dead state

	m_state = state;

	switch(state) {
		case BlockState::LAND:
			// Correct the block by any eventual extra-pixels
			m_loc.x -= offset.x;
			m_loc.y -= offset.y;
			offset = Point{0,0};
			time = LAND_TIME;
			break;

		case BlockState::BREAK:
			time = BREAK_TIME;
			m_anim = BlockFrame::BREAK_BEGIN;
			break;

		default: break;
	}
}

/**
 * Returns true if the block is just now arriving at the center of a new row.
 */
bool BlockImpl::entering_row()
{
	return BlockState::FALL == m_state && offset.y >= 0 && offset.y < FALL_SPEED;
}

/**
 * Update this falling block
 */
void BlockImpl::fall()
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
void BlockImpl::land()
{
	if(time < 0) {
		set_state(BlockState::REST);
		time = 10 - 10 * rc.r; // after which auto-breaks
	}
}

/**
 * Update this breaking block
 */
void BlockImpl::dobreak()
{
	if(time < 0) {
		set_state(BlockState::DEAD);
	}
}

/**
 * Returns the number of the top accessible row in the pit
 */
int PitImpl::top() const
{
	return std::ceil(m_scroll / BLOCK_H);
}

/**
 * Returns the number of the bottom accessible row in the pit
 */
int PitImpl::bottom() const
{
	return std::floor((m_scroll + PIT_H) / BLOCK_H) - 1;
}

/**
 * Set the given location to blocked.
 */
void PitImpl::block(RowCol rc, Block block)
{
	auto result = block_map.emplace(std::make_pair(rc, block));
	game_assert(result.second, "Attempt to block already blocked space in Pit.");
}

/**
 * Set the given location to not blocked.
 */
void PitImpl::unblock(RowCol rc)
{
	size_t erased = block_map.erase(rc);
	game_assert(1 == erased, "Attempt to unblock empty space in Pit.");
}

/**
 * Return the block at the given location.
 */
Block PitImpl::block_at(RowCol rc) const
{
	auto it = block_map.find(rc);
	if(it == block_map.end())
		return nullptr;
	else
		return it->second;
}

/**
 * The origin {0,0} location of all pit-related objects corresponds with row 0, column 0.
 * We have to transform the object into the pit and from there, apply the pit scrolling.
 */
Point PitImpl::transform(Point point, float dt) const
{
	point.x += m_loc.x;
	point.y += m_loc.y;
	// TODO: include dt in scroll anim, don’t forget FPS-TPS conversion
	point.y -= m_scroll;
	return point;
}

void PitImpl::update()
{
	// scroll more
	m_scroll += SCROLL_SPEED;
}


void CursorImpl::draw(const IVideoContext& context, float dt)
{
	float x = static_cast<float>(rc.c*BLOCK_W - (CURSOR_W-2*BLOCK_W)/2);
	float y = static_cast<float>(rc.r*BLOCK_H - (CURSOR_H-BLOCK_H)/2);
	Point loc {x, y};
	Point draw_loc = view->transform(loc, dt);

	size_t frame = (anim / FRAME_TIME) % FRAMES;
	context.drawGfx(draw_loc, Gfx::CURSOR, frame);
}

void CursorImpl::animate()
{
	anim++;
}


void StageImpl::add(Animation animation)
{
	// insertion sort - animations in the list are always in ascending z_order
	auto greater = [animation] (Animation a) { return *animation < *a; };
	auto it = std::find_if(animations.begin(), animations.end(), greater);
	animations.insert(it, animation);
}

void StageImpl::add(Logic logic)
{
	logics.push_back(logic);
}

void StageImpl::remove(Animation animation)
{
	auto it = std::find(animations.begin(), animations.end(), animation);
	SDL_assert(it != animations.end());
	animations.erase(it);
}

void StageImpl::remove(Logic logic)
{
	auto it = std::find(logics.begin(), logics.end(), logic);
	SDL_assert(it != logics.end());
	logics.erase(it);
}

void StageImpl::draw(const IVideoContext& context, float dt)
{
	context.drawGfx(Point{0,0}, Gfx::BACKGROUND);
	for(auto& animation : animations) animation->draw(context, dt);
}

void StageImpl::animate()
{
	for(auto& animation : animations) animation->animate();
}

void StageImpl::update()
{
	for(auto& logic : logics) logic->update();
}

Stage StageBuilder::construct()
{
	Stage stage = std::make_shared<StageImpl>();

	left_pit = std::make_shared<PitImpl>(LPIT_LOC);
	right_pit = std::make_shared<PitImpl>(RPIT_LOC);

	RowCol center { (left_pit->top()-left_pit->bottom())/2, PIT_COLS/2-1 };
	// std::cerr << "Place cursor at " << center.r
	left_cursor = std::make_shared<CursorImpl>(center, left_pit);
	// TODO: right cursor

	stage->add(static_cast<Animation>(left_pit));
	stage->add(static_cast<Logic>(left_pit));
	stage->add(static_cast<Animation>(right_pit));
	stage->add(static_cast<Logic>(right_pit));
	stage->add(left_cursor);

	return stage;
}

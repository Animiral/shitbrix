/**
 * Definitions for drawing routines.
 */
#include "draw.hpp"
#include "globals.hpp"
#include <SDL2/SDL.h>

/**
 * Module-internal implementations.
 */
namespace
{

/**
 * Render a block to the screen.
 */
void draw_block(const IContext& context, const BlockImpl& block, float dt, Point origin);
void draw_garbage(const IContext& context, const Garbage& garbage, float dt, Point origin);

}

Draw::Draw(IContext& context)
: m_context(context), m_dt(0.)
{
}

float Draw::dt() const
{
	return m_dt;
}

void Draw::set_dt(float dt)
{
	SDL_assert(dt >= 0.f);
	SDL_assert(dt <= 1.f);

	m_dt = dt;
}

void Draw::draw_background() const
{
	m_context.drawGfx(Point{0,0}, Gfx::BACKGROUND);
}

void Draw::draw_pit(const PitImpl& pit) const
{
	
}

namespace
{

void draw_block(const IContext& context, const BlockImpl& block, float dt)
{

}

void draw_garbage(const IContext& context, const Garbage& garbage, float dt)
{
	
}

}

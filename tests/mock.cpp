/**
 * Definitions for mocks and fakes
 */

#include "mock.hpp"

void MockContext::drawGfx(Point loc, Gfx gfx, size_t frame) const
{
}

void MockContext::clip(Point top_left, int width, int height)
{
}

void MockContext::unclip()
{
}

void MockContext::fade(float fraction)
{
}

void MockContext::play(Snd snd)
{
}

Point MockTransform::transform(Point point, float dt) const
{
	return point;
}

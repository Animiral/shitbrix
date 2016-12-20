/**
 * mock.hpp
 * This helper module defines mock and fake classes for tests.
 */
#pragma once

#include "context.hpp"

class MockContext : public IContext
{
public:
	virtual void drawGfx(Point loc, Gfx gfx, size_t frame = 0) const override {}
	virtual void translate(Point offset) override {}
	virtual void clip(Point top_left, int width, int height) override {}
	virtual void unclip() override {}
	virtual void fade(float fraction) override {}
	virtual void play(Snd snd) override {}
	virtual void highlight(Point top_left, int width, int height) override {}
};

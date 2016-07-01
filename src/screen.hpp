/**
 * screen.hpp
 * Screen class definitions
 * A screen is one (visual) state of the application and presents itself in a distinct design.
 */
#pragma once

#include "globals.hpp"
#include "context.hpp"
#include "block.hpp"
#include "director.hpp"

enum class ScreenPhase { MENU, GAME };

class IScreen : public IAnimation, public ILogicObject
{
public:
	IScreen() : IAnimation(SCREEN_Z) {}

	virtual ScreenPhase phase() const =0; // type enum
	virtual bool done() const =0; // whether the screen has ended

	virtual void input_dir(Dir dir, int player) =0;
	virtual void input_a(int player) =0;
	virtual void input_b(int player) =0;
	virtual void input_debug(int func) =0; // developer help function
};

enum class GamePhase { INTRO, PLAY, RESULT };

class GameScreen : public IScreen
{

public:

	GameScreen();

	virtual void draw(IVideoContext& context, float dt) override;
	virtual void animate() override;
	virtual void update() override;
	virtual ScreenPhase phase() const override { return ScreenPhase::GAME; }
	virtual bool done() const override { return left_blocks->over() || right_blocks->over(); }

	virtual void input_dir(Dir dir, int player);
	virtual void input_a(int player);
	virtual void input_b(int player);
	virtual void input_debug(int func); // developer help function

private:

	GamePhase game_phase;
	Stage stage;
	std::unique_ptr<BlockDirector> left_blocks;
	std::unique_ptr<BlockDirector> right_blocks;
	std::unique_ptr<CursorDirector> left_cursor;
	std::unique_ptr<CursorDirector> right_cursor;
	PitView lpit_view;
	PitView rpit_view;
	Banner banner_left;
	Banner banner_right;

};

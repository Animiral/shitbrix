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

class IScreen : public IAnimation, public ILogic
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

class GameScreen;

/**
 * Determines some of the variable behavior of the GameScreen (strategy pattern).
 * The GameScreen starts with an intro (fade-in or transition), continues with the
 * actual gameplay and ends with a result banner. All of these are implemented as
 * strategies derived from GamePhase.
 * GamePhases control their own life and transition via GameScreen::set_phase(),
 * enabled through the friend relation to GameScreen.
 */
class IGamePhase
{
public:
	IGamePhase(GameScreen* screen) : m_screen(screen) {}
	virtual ~IGamePhase() =0;

	void set_screen(GameScreen* screen) { m_screen = screen; }
	virtual void draw(IContext& context, float dt);
	virtual void update() =0;
	virtual void input_dir(Dir dir, int player) =0;
	virtual void input_a(int player) =0;
	virtual void input_b(int player) =0;
protected:
	GameScreen* m_screen;
};

using GamePhase = std::unique_ptr<IGamePhase>;

class GameIntro : public IGamePhase
{
public:
	GameIntro(GameScreen* screen) : IGamePhase(screen), countdown(INTRO_TIME) {}

	virtual void draw(IContext& context, float dt) override;
	virtual void update() override;
	virtual void input_dir(Dir dir, int player) override {}
	virtual void input_a(int player) override {}
	virtual void input_b(int player) override {}
private:
	static constexpr int INTRO_TIME = 20; // number of animation frames for intro
	int countdown;
};

class GamePlay : public IGamePhase
{
public:
	GamePlay(GameScreen* screen) : IGamePhase(screen) {}

	virtual void update() override;
	virtual void input_dir(Dir dir, int player) override;
	virtual void input_a(int player) override;
	virtual void input_b(int player) override;
};

class GameResult : public IGamePhase
{
public:
	GameResult(GameScreen* screen) : IGamePhase(screen) {}

	virtual void update() override {}
	virtual void input_dir(Dir dir, int player) override {}
	virtual void input_a(int player) override {}
	virtual void input_b(int player) override {}
};

class GameScreen : public IScreen
{

public:

	GameScreen();
	GameScreen& operator=(GameScreen&& rhs);

	virtual void draw(IContext& context, float dt) override { game_phase->draw(context, dt); }
	virtual void animate() override;
	virtual void update() override { game_phase->update(); }
	virtual ScreenPhase phase() const override { return ScreenPhase::GAME; }
	virtual bool done() const override { return left_blocks->over() || right_blocks->over(); }

	virtual void input_dir(Dir dir, int player) { game_phase->input_dir(dir, player); }
	virtual void input_a(int player) { game_phase->input_a(player); }
	virtual void input_b(int player) { game_phase->input_b(player); }
	virtual void input_debug(int func); // developer help function

private:

	std::random_device rdev;
	RndGen rndgen;
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

	void set_phase(GamePhase phase) { game_phase = std::move(phase); }
	void add_banner(Point pit_loc, BannerFrame frame);

	friend class IGamePhase;
	friend class GameIntro;
	friend class GamePlay;
	friend class GameResult;

};

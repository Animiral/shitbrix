/**
 * screen.hpp
 * Screen class definitions
 * A screen is one (visual) state of the application and presents itself in a distinct design.
 */
#pragma once

#include "globals.hpp"
#include "input.hpp"
#include "context.hpp"
#include "block.hpp"
#include "director.hpp"
#include "replay.hpp"
#include <fstream>

enum class ScreenPhase { MENU, GAME };

class IScreen : public IAnimation, public ILogic, public IControllerSink
{
public:
	IScreen() : IAnimation(SCREEN_Z) {}

	virtual ScreenPhase phase() const =0; // type enum
	virtual bool done() const =0; // whether the screen has ended

	virtual void input(ControllerInput cinput) =0;
	virtual void input_debug(int func) {} // developer help function
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
class IGamePhase : public IGameInputSink
{

public:

	IGamePhase(GameScreen* screen) : m_screen(screen) {}
	virtual ~IGamePhase() =0;

	void set_screen(GameScreen* screen) { m_screen = screen; }

	virtual void draw(IContext& context, float dt);
	virtual void update(IContext& context) =0;

protected:

	GameScreen* m_screen;

};

using GamePhase = std::unique_ptr<IGamePhase>;

class GameIntro : public IGamePhase
{
public:
	GameIntro(GameScreen* screen) : IGamePhase(screen), countdown(INTRO_TIME) {}

	virtual void draw(IContext& context, float dt) override;
	virtual void update(IContext& context) override;
	virtual void input(GameInput ginput) override {}
private:
	static constexpr int INTRO_TIME = 20; // number of animation frames for intro
	int countdown;
};

class GamePlay : public IGamePhase
{

public:

	GamePlay(GameScreen* screen);

	virtual void update(IContext& context) override;
	virtual void input(GameInput ginput) override;

};

class GameResult : public IGamePhase
{
public:
	GameResult(GameScreen* screen, int winner);
	~GameResult();

	virtual void update(IContext& context) override;
	virtual void input(GameInput ginput) override {}
};

class GameScreen : public IScreen, public IReplaySink
{

public:

	GameScreen(const char* replay_infile = nullptr, const char* replay_outfile = LAST_REPLAY_FILE);
	GameScreen& operator=(GameScreen&& rhs);

	const long& game_time() const { return m_game_time; }
	void reset();
	virtual void draw(IContext& context, float dt) override { game_phase->draw(context, dt); }
	virtual void animate() override;
	virtual void update(IContext& context) override;
	virtual ScreenPhase phase() const override { return ScreenPhase::GAME; }
	virtual bool done() const override { return m_done; }
	virtual void input(ControllerInput cinput) override;
	virtual void handle(const ReplayEvent& event) override;

private:

	RndGen rndgen;
	GamePhase game_phase;
	long m_game_time; // starts at 0 with each game round
	bool m_done; // true if this screen has reached its end
	GameInputMixer input_mixer;

	Stage stage;
	std::unique_ptr<BlockDirector> left_blocks;
	std::unique_ptr<BlockDirector> right_blocks;
	std::unique_ptr<CursorDirector> left_cursor;
	std::unique_ptr<CursorDirector> right_cursor;
	PitView lpit_view;
	PitView rpit_view;
	Banner banner_left;
	Banner banner_right;

	std::ofstream replay_outstream;
	Journal journal;

	void set_phase(GamePhase phase);
	void add_banner(Point pit_loc, BannerFrame frame);
	void seed(unsigned int rng_seed);

	friend class IGamePhase;
	friend class GameIntro;
	friend class GamePlay;
	friend class GameResult;

};

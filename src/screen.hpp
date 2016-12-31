/**
 * screen.hpp
 * Screen class definitions
 * A screen is one (visual) state of the application and presents itself in a distinct design.
 */
#pragma once

#include "globals.hpp"
#include "input.hpp"
#include "context.hpp"
#include "stage.hpp"
#include "draw.hpp"
#include "director.hpp"
#include "replay.hpp"
#include <fstream>

enum class ScreenPhase { MENU, GAME };

class IScreen : public IControllerSink
{
public:
	IScreen() = default;

	// Screens are complex objects and can not be copied or moved.
	IScreen(const IScreen& ) = delete;
	IScreen(IScreen&& ) = delete;
	IScreen& operator=(const IScreen& ) = delete;
	IScreen& operator=(IScreen&& ) = delete;

	/**
	 * Draw all contents to the screen.
	 * Since this is the “top-level” draw, everything in the game that appears
	 * on the screen at any point must eventually be drawn through this call.
	 */
	virtual void draw(float dt) const =0;
	virtual void update() =0;

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

	virtual void draw() const;
	virtual void update() =0;

protected:

	GameScreen* m_screen;

};

using GamePhase = std::unique_ptr<IGamePhase>;

class GameIntro : public IGamePhase
{
public:
	GameIntro(GameScreen* screen);

	virtual void draw() const override;
	virtual void update() override;
	virtual void input(GameInput ginput) override {}
private:
	static constexpr int INTRO_TIME = 20; // number of animation frames for intro
	int countdown;
};

class GamePlay : public IGamePhase
{

public:

	GamePlay(GameScreen* screen);

	virtual void update() override;
	virtual void input(GameInput ginput) override;

};

class GameResult : public IGamePhase
{
public:
	GameResult(GameScreen* screen, int winner);
	~GameResult();

	virtual void draw() const override;
	virtual void update() override;
	virtual void input(GameInput ginput) override {}

private:

	std::unique_ptr<Banner> banner_left;
	std::unique_ptr<Banner> banner_right;

};

class GameScreen : public IScreen, public IReplaySink
{

public:

	GameScreen(const char* replay_infile, const char* replay_outfile, IContext& context);

	const long& game_time() const { return m_game_time; }
	void reset();

	/**
	 * Draw content to the screen according to the game phase.
	 */
	virtual void draw(float dt) const override;
	virtual void update() override;
	virtual ScreenPhase phase() const override { return ScreenPhase::GAME; }
	virtual bool done() const override { return m_done; }
	virtual void input(ControllerInput cinput) override;
	virtual void handle(const ReplayEvent& event) override;

private:

	RndGen rndgen;
	GamePhase game_phase;
	long m_game_time; // starts at 0 with each game round
	bool m_done; // true if this screen has reached its end
	bool m_pause; // true if tick updates are supressed
	GameInputMixer input_mixer;

	std::ofstream replay_outstream;
	Journal journal;

	IContext& m_context;
	std::unique_ptr<Stage> stage;
	std::unique_ptr<BlockDirector> left_blocks;
	std::unique_ptr<BlockDirector> right_blocks;
	std::unique_ptr<CursorDirector> left_cursor;
	std::unique_ptr<CursorDirector> right_cursor;
	DrawGame m_draw;
	evt::GameEventHub m_event_hub;
	evt::SoundEffects m_sound_effects;

	void set_phase(GamePhase phase);
	void seed(unsigned int rng_seed);

	/**
	 * Pass on the update event to child objects.
	 */
	void update_impl();

	friend class IGamePhase;
	friend class GameIntro;
	friend class GamePlay;
	friend class GameResult;

};

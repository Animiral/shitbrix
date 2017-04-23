/**
 * screen.hpp
 * Screen class definitions
 * A screen is one (visual) state of the application and presents itself in a distinct design.
 */
#pragma once

#include "globals.hpp"
#include "input.hpp"
#include "stage.hpp"
#include "draw.hpp"
#include "audio.hpp"
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

	virtual void update() =0;

protected:

	GameScreen* m_screen;

};

class GameIntro : public IGamePhase
{
public:
	GameIntro(GameScreen* screen);

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

	virtual void update() override;
	virtual void input(GameInput ginput) override {}

};

class GameScreen : public IScreen, public IReplaySink
{

public:

	GameScreen(const char* replay_infile, const char* replay_outfile, DrawGame& draw, const Audio& sound);

	const long& game_time() const { return m_game_time; }
	void reset();

	virtual void update() override;
	virtual ScreenPhase phase() const override { return ScreenPhase::GAME; }
	virtual bool done() const override { return m_done; }
	virtual void input(ControllerInput cinput) override;
	virtual void handle(const ReplayEvent& event) override;

private:

	/**
	 * Assorted objects that are required on this screen once per player.
	 */
	struct PlayerObjects
	{
		PlayerObjects(RndGen& rndgen, Pit& pit, Cursor& cursor, Pit& other_pit, BonusIndicator& bonus)
		: block_director(pit, rndgen), cursor_director(pit, cursor), event_hub(),
		  garbage_throw(other_pit), bonus_throw(bonus)
		{
			block_director.set_handler(event_hub);
			event_hub.append(garbage_throw);
			event_hub.append(bonus_throw);
		}

		// default move would leave dangling references!
		PlayerObjects(const PlayerObjects& ) = delete;
		PlayerObjects(PlayerObjects&& ) = delete;
		PlayerObjects& operator=(const PlayerObjects& ) = delete;
		PlayerObjects& operator=(PlayerObjects&& ) = delete;

		BlockDirector block_director;
		CursorDirector cursor_director;
		evt::GameEventHub event_hub;
		GarbageThrow garbage_throw; // event handler for generating garbage bricks
		BonusThrow bonus_throw; // event handler for displaying stars
	};

	RndGen rndgen;
	long m_game_time; // starts at 0 with each game round
	bool m_done; // true if this screen has reached its end
	bool m_pause; // true if tick updates are supressed
	GameInputMixer input_mixer;

	std::unique_ptr<IGamePhase> m_game_phase;
	std::unique_ptr<IGamePhase> m_next_phase;

	std::ofstream replay_outstream;
	Journal journal;

	std::unique_ptr<Stage> stage;
	DrawGame& m_draw;
	evt::SoundEffects m_sound_effects;
	std::vector<std::unique_ptr<PlayerObjects>> m_pobjects;

	void change_phase(std::unique_ptr<IGamePhase> phase);
	void change_phase_impl();
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

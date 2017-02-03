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

enum class ScreenPhase { SPLASH, MENU, GAME };

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

	/**
	 * Inject the screen’s presentation dependency.
	 */
	void set_context(IContext& context) noexcept;

protected:

	IContext* m_context; //!< presentation output interface

	/**
	 * Template method for subclass presentation injection.
	 */
	virtual void set_context_impl(IContext& context) noexcept {}

};

class SplashScreen;
class GameScreen;
class Transition;

/**
 * Do nothing but display the splash background image.
 */
class SplashScreen : public IScreen
{

public:

	SplashScreen() = default;

	virtual void draw(float dt) const override;
	virtual void update() override {}
	virtual ScreenPhase phase() const override { return ScreenPhase::SPLASH; }
	virtual bool done() const override { return true; }
	virtual void input(ControllerInput cinput) override {}

};

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

	GameScreen();

	/**
	 * Inject the screen’s random generator dependency.
	 */
	void set_rndgen(RndGen& rndgen) noexcept;

	/**
	 * Inject the screen’s input dependency.
	 */
	void set_input(GameInputMixer& input_mixer) noexcept;

	/**
	 * Inject the screen’s journal dependency.
	 */
	void set_journal(Journal& journal) noexcept;

	const long& game_time() const { return m_game_time; }
	bool is_quit() const { return m_quit; }

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

	long m_game_time; //!< starts at 0 with each game round
	bool m_done; //!< true if this screen has reached its end
	bool m_quit; //!< true if the user wants to quit
	bool m_pause; //!< true if tick updates are supressed

	// external component dependencies (plug into this screen):
	RndGen* m_rndgen; //!< random number source interface
	GameInputMixer* m_input_mixer; //!< user input interface
	Journal* m_journal; //!< event recorder interface

	std::unique_ptr<IGamePhase> m_game_phase;
	std::unique_ptr<IGamePhase> m_next_phase;

	std::unique_ptr<Stage> stage;
	DrawGame m_draw;
	evt::SoundEffects m_sound_effects;
	std::vector<std::unique_ptr<PlayerObjects>> m_pobjects;

	void change_phase(std::unique_ptr<IGamePhase> phase);
	void change_phase_impl();

	/**
	 * Pass on the update event to child objects.
	 */
	void update_impl();

	/**
	 * Kludge function to be removed when replays get rid of rng dependency.
	 */
	void seed(unsigned int rng_seed);

	/**
	 * Screen-specefic context injection implementation.
	 */
	virtual void set_context_impl(IContext& context) noexcept override;

	friend class IGamePhase;
	friend class GameIntro;
	friend class GamePlay;
	friend class GameResult;

};

/**
 * A Transition is a pseudo-screen that adds eye candy to the moment when
 * one screen ends and another begins.
 *
 * It keeps hold of two screens, of which it draws a mixture according
 * to its internal rules. Over the lifetime of the Transition, the predecessor
 * screen disappears and the successor screen emerges.
 * The sub-screens do not receive logic updates or user input.
 */
class Transition : public IScreen
{

public:

	Transition(std::unique_ptr<IScreen> predecessor, std::unique_ptr<IScreen> successor);

	std::unique_ptr<IScreen> successor() noexcept { return std::move(m_successor); }

	virtual void update() override;
	virtual ScreenPhase phase() const override { return ScreenPhase::SPLASH; }
	virtual bool done() const override;
	virtual void input(ControllerInput) override {}

protected:

	std::unique_ptr<IScreen> m_predecessor;
	std::unique_ptr<IScreen> m_successor;

	int m_time;

};

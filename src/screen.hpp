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
#include "options.hpp"
#include "sdl_helper.hpp"
#include <fstream>
#include <typeinfo>

class IScreen : public IControllerSink
{
public:
	IScreen() = default;
	virtual ~IScreen() =0;

	// Screens are complex objects and can not be copied or moved.
	IScreen(const IScreen& ) = delete;
	IScreen(IScreen&& ) = delete;
	IScreen& operator=(const IScreen& ) = delete;
	IScreen& operator=(IScreen&& ) = delete;

	virtual void update() =0;
	virtual void draw(float dt) =0;

	virtual bool done() const =0; // whether the screen has ended

	/**
	 * Access the object which can draw this screen.
	 */
	virtual const IDraw& get_draw() const =0;

	virtual void input(ControllerInput cinput) =0;
	virtual void input_debug(int func) {} // developer help function
};

/**
 * Creates Screens.
 */
class ScreenFactory
{

public:

	ScreenFactory(const Options& options, const Assets& assets, const Audio& audio);

	std::unique_ptr<IScreen> create_menu() const;
	std::unique_ptr<IScreen> create_game() const;
	std::unique_ptr<IScreen> create_transition(IScreen& predecessor, IScreen& successor) const;

private:

	// resources to create the Screens
	const Options& m_options;
	const Assets& m_assets;
	const Audio& m_audio;

};

class PinkScreen; //!< debugging screen, never shown
class MenuScreen;
class GameScreen;
class TransitionScreen;

class PinkScreen : public IScreen
{

public:

	PinkScreen(PinkDraw&& draw) : m_draw(draw) {}
	virtual void update() override {}
	virtual void draw(float dt) override { m_draw.draw(dt); }
	virtual bool done() const override { return m_done; }
	virtual const IDraw& get_draw() const override { return m_draw; }
	virtual void input(ControllerInput cinput) override { if(Button::A == cinput.button && ButtonAction::DOWN == cinput.action) m_done = true; }

private:

	PinkDraw m_draw;
	bool m_done = false;

};

class MenuScreen : public IScreen
{

public:

	enum class Result { PLAY, QUIT };

	MenuScreen(DrawMenu&& draw, const Audio& sound);

	virtual void update() override;
	virtual void draw(float dt) override;
	virtual bool done() const override { return m_done; }
	virtual const IDraw& get_draw() const override { return m_draw; }
	virtual void input(ControllerInput cinput) override;

	/**
	 * Return the result of the MenuScreen.
	 * It is an error to ask for this before the screen is done().
	 */
	Result result() const { SDL_assert(m_done); return m_result; }

private:

	long m_game_time; //!< starts at 0 and increases with update()
	bool m_done; //!< true if this screen has reached its end
	Result m_result; //!< valid only when m_done

	DrawMenu m_draw; //!< Interface for drawing the screen

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

	GameScreen(const char* replay_infile, const char* replay_outfile, DrawGame&& draw, const Audio& audio);

	const long& game_time() const { return m_game_time; }
	void reset();

	virtual void update() override;
	virtual void draw(float dt) override;
	virtual bool done() const override { return m_done; }
	virtual const IDraw& get_draw() const override { return m_draw; }
	virtual void input(ControllerInput cinput) override;
	virtual void handle(const ReplayEvent& event) override;

private:

	/**
	 * Assorted objects that are required on this screen once per player.
	 */
	struct PlayerObjects
	{
		PlayerObjects(unsigned seed, Pit& pit, Cursor& cursor, Pit& other_pit, BonusIndicator& bonus)
		: block_director(pit, BlocksQueue(seed)), cursor_director(pit, cursor),
		  event_hub(), garbage_throw(other_pit, BlocksQueue(seed*3)), bonus_throw(bonus)
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

	unsigned m_seed; // game-start PRNG seed which determines block mix
	long m_game_time; // starts at 0 with each game round
	bool m_done; // true if this screen has reached its end
	bool m_pause; // true if tick updates are supressed
	GameInputMixer input_mixer;

	std::unique_ptr<IGamePhase> m_game_phase;
	std::unique_ptr<IGamePhase> m_next_phase;

	std::ofstream replay_outstream;
	Journal journal;

	std::unique_ptr<Stage> stage;
	DrawGame m_draw;
	evt::SoundRelay m_sound_relay;
	ShakeRelay m_shake_relay;
	std::unique_ptr<evt::GameOverRelay> m_gameover_relay;
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

class TransitionScreen : public IScreen
{

public:

	TransitionScreen(IScreen& predecessor, IScreen& successor, DrawTransition&& draw)
	: m_predecessor(predecessor), m_successor(successor), m_time(0), m_draw(std::move(draw))
	{}

	virtual void update() override;
	virtual void draw(float dt) override;
	virtual bool done() const override { return m_time >= TRANSITION_TIME; }
	virtual const IDraw& get_draw() const override { return m_draw; }
	virtual void input(ControllerInput cinput) override { m_successor.input(cinput); }

	IScreen& successor() const { return m_successor; }

private:

	IScreen& m_predecessor;
	IScreen& m_successor;
	int m_time; //!< starts at 0 and increases with update()
	DrawTransition m_draw;

};

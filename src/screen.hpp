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
#include "logic.hpp"
#include "director.hpp"
#include "replay.hpp"
#include "network.hpp"
#include "sdl_helper.hpp"
#include <fstream>
#include <typeinfo>

class IScreen
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
	 * The game loop calls this function when the screen is being exited.
	 */
	virtual void stop() {}

	/**
	 * Access the object which can draw this screen.
	 */
	virtual const IDraw& get_draw() const =0;

	virtual void input(ControllerInput cinput) =0;
};

/**
 * Creates Screens.
 */
class ScreenFactory
{

public:

	/**
	 * Configure the client dependency.
	 * In the future, this will hopefully be taken from a central repository.
	 */
	void set_client(IClient* client) noexcept { m_client = client; }

	/**
	 * Configure the server dependency.
	 * In the future, this will hopefully be taken from a central repository.
	 */
	void set_server(ServerThread* server) noexcept { m_server = server; }

	std::unique_ptr<IScreen> create_menu();
	std::unique_ptr<IScreen> create_game();
	std::unique_ptr<IScreen> create_server();
	std::unique_ptr<IScreen> create_transition(IScreen& predecessor, IScreen& successor);

private:

	// resources to create the Screens
	IClient* m_client;
	ServerThread* m_server;

};

class PinkScreen; //!< debugging screen, never shown
class MenuScreen;
class GameScreen;
class TransitionScreen;

class PinkScreen : public IScreen
{

public:

	PinkScreen(DrawPink&& draw) : m_draw(draw) {}
	virtual void update() override {}
	virtual void draw(float dt) override { m_draw.draw(dt); }
	virtual bool done() const override { return m_done; }
	virtual const IDraw& get_draw() const override { return m_draw; }
	virtual void input(ControllerInput cinput) override { if(Button::A == cinput.button && ButtonAction::DOWN == cinput.action) m_done = true; }

private:

	DrawPink m_draw;
	bool m_done = false;

};

class MenuScreen : public IScreen
{

public:

	enum class Result { PLAY, QUIT };

	MenuScreen(DrawMenu&& draw, IClient& client);

	virtual void update() override;
	virtual void draw(float dt) override;
	virtual bool done() const override { return m_done; }
	virtual const IDraw& get_draw() const override { return m_draw; }
	virtual void input(ControllerInput cinput) override;

	/**
	 * Return the result of the MenuScreen.
	 * It is an error to ask for this before the screen is done().
	 */
	Result result() const { enforce(m_done); return m_result; }

private:

	long m_game_time; //!< starts at 0 and increases with update()
	bool m_done; //!< true if this screen has reached its end
	Result m_result; //!< valid only when m_done

	DrawMenu m_draw; //!< Interface for drawing the screen
	IClient* const m_client; //!< Network communication endpoint

};

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

	virtual void update() =0;

protected:

	GameScreen* m_screen;

};

class GameIntro : public IGamePhase
{

public:

	GameIntro(GameScreen* screen);

	virtual void update() override;

private:

	int countdown;

};

class GamePlay : public IGamePhase
{

public:

	GamePlay(GameScreen* screen) : IGamePhase(screen) {}

	virtual void update() override;

};

class GameResult : public IGamePhase
{

public:

	GameResult(GameScreen* screen, int winner);

	virtual void update() override;

};

class GameScreen : public IScreen
{

public:

	explicit GameScreen(
		std::unique_ptr<Stage> stage,
		std::unique_ptr<DrawGame> draw,
		IClient& client,
		ServerThread* server = nullptr);
	virtual ~GameScreen() noexcept;

	virtual void update() override;
	virtual void draw(float dt) override;
	virtual bool done() const override { return m_done; }
	virtual void stop() override;
	virtual const IDraw& get_draw() const override { assert(m_draw); return *m_draw; }
	virtual void input(ControllerInput cinput) override;

private:

	long m_game_time; // starts at 0 with each game round
	bool m_done; // true if this screen has reached its end

	std::unique_ptr<IGamePhase> m_game_phase;
	std::unique_ptr<IGamePhase> m_next_phase;

	std::unique_ptr<Stage> m_stage;
	std::unique_ptr<DrawGame> m_draw;
	IClient* const m_client;
	ServerThread* const m_server;
	evt::BonusRelay m_bonus_relay;
	evt::DupeFiltered<evt::SoundRelay> m_sound_relay;
	evt::DupeFiltered<ShakeRelay> m_shake_relay;

	void change_phase(std::unique_ptr<IGamePhase> phase);
	void change_phase_impl();

	void start();

	friend class IGamePhase;
	friend class GameIntro;
	friend class GamePlay;
	friend class GameResult;

};

/**
 * A Screen that does nothing but run a ServerThread.
 */
class ServerScreen : public IScreen
{

public:

	explicit ServerScreen(ServerThread& server);
	virtual ~ServerScreen() noexcept;

	virtual void update() override;
	virtual void draw(float dt) override {}
	virtual bool done() const override { return m_done; }
	virtual const IDraw& get_draw() const override { return m_draw; }
	virtual void input(ControllerInput cinput) override;

private:

	ServerThread* const m_server;
	NoDraw m_draw;
	bool m_done;

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

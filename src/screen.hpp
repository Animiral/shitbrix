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
#include "network.hpp"
#include <memory>
#include <cassert>

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
 * The display class for the game board.
 *
 * The GameScreen starts with an intro (ready-prompt), continues with the
 * actual gameplay and ends with a result banner.
 */
class GameScreen : public IScreen
{

public:

	enum class Phase { INTRO, PLAY, RESULT };

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

	Phase m_phase; //!< game round state machine
	long m_time; //!< starts at 0 with the intro and each game round
	bool m_done; //!< true if this screen has reached its end

	std::unique_ptr<DrawGame> m_draw;
	std::unique_ptr<Stage> m_stage;
	IClient* const m_client;
	ServerThread* const m_server;

	/**
	 * Calculate one update tick in the currently active phase.
	 */
	void advance_tick();

	/**
	 * Tick implementation for the intro phase.
	 */
	void update_intro();

	/**
	 * Tick implementation for the intro phase.
	 */
	void update_play();

	void start();

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

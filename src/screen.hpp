/**
 * screen.hpp
 * Screen class definitions
 * A screen is one (visual) state of the application and presents itself in a distinct design.
 */
#pragma once

#include "globals.hpp"
#include "input.hpp"
#include "event.hpp"
#include "stage.hpp"
#include "logic.hpp"
#include "director.hpp"
#include "network.hpp"
#include <memory>
#include <cassert>

class ICanvas;
class IDraw;

class IScreen
{

public:

	explicit IScreen(IDraw& draw);
	virtual ~IScreen() =0;

	// Screens are complex objects and can not be copied or moved.
	IScreen(const IScreen& ) = delete;
	IScreen(IScreen&& ) = delete;
	IScreen& operator=(const IScreen& ) = delete;
	IScreen& operator=(IScreen&& ) = delete;

	virtual void update() =0;

	/**
	 * Draw and render everything visible on the screen.
	 *
	 * This template method leaves the specifics of what to draw to the
	 * derived class' draw implementation.
	 *
	 * @param dt the given fraction elapsed since the last tick
	 */
	void draw(float dt);

	virtual bool done() const =0; // whether the screen has ended

	/**
	 * The game loop calls this function when the screen is being exited.
	 */
	virtual void stop() {}

	virtual void input(ControllerAction cinput) =0;

protected:

	IDraw* m_draw;

	/**
	 * Derived class draw implementation.
	 */
	virtual void draw_impl(float dt) =0;

	friend class TransitionScreen; // may access draw_impl of other screens

};

/**
 * Creates Screens.
 */
class ScreenFactory
{

public:

	/**
	 * Configure the game dependency.
	 * In the future, this will hopefully be taken from a central repository.
	 */
	void set_game(IGame* game) noexcept { m_game = game; }

	/**
	 * Configure the game dependency.
	 * In the future, this will hopefully be taken from a central repository.
	 */
	void set_draw(std::unique_ptr<IDraw> draw) noexcept { m_draw = move(draw); }

	/**
	 * Configure the server dependency.
	 * In the future, this will hopefully be taken from a central repository.
	 */
	void set_server(ServerThread* server) noexcept { m_server = server; }

	std::unique_ptr<IScreen> create_menu();
	std::unique_ptr<IScreen> create_game();
	std::unique_ptr<IScreen> create_server();
	std::unique_ptr<IScreen> create_transition(IScreen& predecessor, IScreen& successor);
	std::unique_ptr<IScreen> create_pink(uint8_t r, uint8_t g, uint8_t b);

private:

	// resources to create the Screens
	std::unique_ptr<IDraw> m_draw;
	IGame* m_game;
	ServerThread* m_server;

};

class PinkScreen; //!< debugging screen, never shown
class MenuScreen;
class GameScreen;
class TransitionScreen;

/**
 * The PinkScreen is a simple screen which displays only one solid color.
 *
 * It serves a temporary role as a screen placeholder or for testing
 * purposes. It can be finished by a press of the A button.
 */
class PinkScreen : public IScreen
{

public:

	explicit PinkScreen(IDraw& draw, uint8_t r, uint8_t g, uint8_t b) noexcept;

	virtual void update() override {}
	virtual bool done() const override { return m_done; }
	virtual void input(ControllerAction cinput) override { if(Button::A == cinput.button && ButtonAction::DOWN == cinput.action) m_done = true; }

protected:

	virtual void draw_impl(float dt) override;

private:

	uint8_t m_r, m_g, m_b;
	bool m_done = false;

};

class MenuScreen : public IScreen
{

public:

	enum class Result { PLAY, QUIT };

	explicit MenuScreen(IDraw& draw, IGame& game);

	virtual void update() override;
	virtual bool done() const override { return m_done; }
	virtual void input(ControllerAction cinput) override;

	/**
	 * Return the result of the MenuScreen.
	 * It is an error to ask for this before the screen is done().
	 */
	Result result() const { enforce(m_done); return m_result; }

protected:

	virtual void draw_impl(float dt) override;

private:

	long m_game_time; //!< starts at 0 and increases with update()
	bool m_done; //!< true if this screen has reached its end
	Result m_result; //!< valid only when m_done

	IDraw* m_draw; //!< Interface for drawing the screen
	IGame* m_game; //!< Game object

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

	explicit GameScreen(IDraw& draw, IGame& game, ServerThread* server = nullptr);
	virtual ~GameScreen() noexcept;

	virtual void update() override;
	virtual bool done() const override { return m_done; }
	virtual void input(ControllerAction cinput) override;

protected:

	virtual void draw_impl(float dt) override;

private:

	Phase m_phase; //!< game round state machine
	long m_time; //!< starts at 0 with the intro and each game round
	bool m_done; //!< true if this screen has reached its end

	std::unique_ptr<Stage> m_stage;
	IGame* m_game;
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

};

/**
 * A Screen that does nothing but run a ServerThread.
 */
class ServerScreen : public IScreen
{

public:

	explicit ServerScreen(IDraw& draw, ServerThread& server) noexcept;
	virtual ~ServerScreen() noexcept;

	virtual void update() override {}
	virtual bool done() const override { return m_done; }
	virtual void input(ControllerAction cinput) override;

protected:

	virtual void draw_impl(float dt) override {}

private:

	ServerThread* const m_server;
	bool m_done;

};

class TransitionScreen : public IScreen
{

public:

	explicit TransitionScreen(IDraw& draw, IScreen& predecessor, IScreen& successor);

	virtual void update() override;
	virtual bool done() const override { return m_time >= TRANSITION_TIME; }
	virtual void input(ControllerAction cinput) override { m_successor.input(cinput); }

	IScreen& successor() const { return m_successor; }

protected:

	virtual void draw_impl(float dt) override;

private:

	IScreen& m_predecessor;
	IScreen& m_successor;
	std::unique_ptr<ICanvas> m_predecessor_canvas;
	std::unique_ptr<ICanvas> m_successor_canvas;
	int m_time; //!< starts at 0 and increases with update()

};

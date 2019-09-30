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

struct GlobalContext;
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

/**
 * The menu screen allows the user to navigate the main menu.
 *
 * The user may change configuration options, start some specific
 * game mode, or exit the application.
 *
 * The GameScreen starts with an intro (ready-prompt), continues with the
 * actual gameplay and ends with a result banner.
 */
class MenuScreen : public IScreen
{

public:

	enum class Result { PLAY_LOCAL, PLAY_HOST, PLAY_CLIENT, QUIT };

	explicit MenuScreen(IDraw& draw, const GlobalContext& context);

	virtual void update() override;
	virtual bool done() const override; // whether the screen has ended
	virtual void input(ControllerAction cinput) override;

	/**
	 * Return the result of the MenuScreen.
	 * It is an error to ask for this before the screen is done().
	 */
	Result result() const { enforce(m_done); return m_result; }

protected:

	virtual void draw_impl(float dt) override;

private:

	static const wrap::Color CHOICE_OUTLINE_COLOR;
	static const wrap::Color CHOICE_FILL_COLOR;
	static const wrap::Color ACTIVE_OUTLINE_COLOR;
	static const wrap::Color ACTIVE_FILL_COLOR;

	enum class MenuAction
	{
		PLAY_LOCAL, //!< start a local game
		PLAY_HOST, //!< start a network game with local server
		PLAY_CLIENT, //!< start a network game as client
		GOMAIN, //!< go to top-level menu
		GOCONFIG, //!< enter the configuration sub-menu
		GOQUIT, //!< exit the application
		TOGGLE_AUTORECORD, //!< change the autorecord flag
	};

	/**
	 * Act out the selected menu option when the user confirms.
	 */
	void dochoice(MenuAction action);

	struct MenuChoice
	{
		const char* label;
		MenuAction action;
	};

	struct SubMenu
	{
		std::vector<MenuChoice> choice;
	};

	static const SubMenu m_menu[];

	const GlobalContext* m_context;
	BitmapFont m_choice_font; //!< font for menu entries
	BitmapFont m_active_font; //!< font for active menu entries
	const SubMenu* m_active_menu; //!< pointer to currently visisble submenu
	int m_active; //!< currently selected entry
	bool m_done; //!< true if this screen has reached its end
	Result m_result; //!< valid only when m_done

};

class PregameScreen : public IScreen
{

public:

	enum class Result { PLAY, QUIT };

	explicit PregameScreen(IDraw& draw, std::shared_ptr<IGame> game);
	virtual ~PregameScreen() noexcept;

	virtual void update() override;
	virtual bool done() const override { return m_done; }
	virtual void input(ControllerAction cinput) override;

	/**
	 * Return the result of the PregameScreen.
	 * It is an error to ask for this before the screen is done().
	 */
	Result result() const { enforce(m_done); return m_result; }

protected:

	virtual void draw_impl(float dt) override;

private:

	long m_time; //!< starts at 0 and increases with update()
	bool m_done; //!< true if this screen has reached its end
	Result m_result; //!< valid only when m_done

	IDraw* m_draw; //!< Interface for drawing the screen
	std::shared_ptr<IGame> m_game; //!< Game object

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

	explicit GameScreen(IDraw& draw, std::shared_ptr<IGame> game, ServerThread* server = nullptr);
	virtual ~GameScreen() noexcept;

	virtual void update() override;
	virtual bool done() const override { return m_done; }
	virtual void input(ControllerAction cinput) override;

	/**
	 * Set whether to automatically save replays.
	 */
	void set_autorecord(bool autorecord) noexcept { m_autorecord = autorecord; }

protected:

	virtual void draw_impl(float dt) override;

private:

	Phase m_phase; //!< game round state machine
	long m_time; //!< starts at 0 with the intro and each game round
	bool m_done; //!< true if this screen has reached its end
	bool m_autorecord = false; //!< true if we want to automatically save replays

	std::unique_ptr<Stage> m_stage;
	std::shared_ptr<IGame> m_game;
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

	/**
	 * If the autorecord configuration is on, write the appropriate file.
	 */
	void autorecord_replay() const;

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

/**
 * Creates Screens from the application's global settings.
 */
class ScreenFactory
{

public:

	explicit ScreenFactory(const GlobalContext& context) noexcept;

	/**
	 * Based on the global settings, create the initial starting screen of the
	 * application. This is most often the PregameScreen.
	 *
	 * @return pointer to the initial screen object or @c nullptr for application exit
	 */
	IScreen* create_default();

	/**
	 * Create the screen that follows after the given screen concludes.
	 *
	 * The predecessor must be @c done().
	 *
	 * @return pointer to the successor screen object or @c nullptr for application exit
	 */
	IScreen* create_next(IScreen& predecessor);

private:

	/**
	 * When transitioning to a game screen, instantiate the appropriate next
	 * screen based on the given replay file configuration.
	 *
	 * If the replay path is configured, automatically load the replay and go
	 * straight ahead to the game screen.
	 * If there is no replay path configured, set up the pregame screen instead.
	 *
	 * @return a pointer to the new screen
	 */
	IScreen* ScreenFactory::create_screen_maybe_replay(std::optional<std::filesystem::path> replay_path);

	// resources to create the Screens
	const GlobalContext* m_context; //!< global settings dependency

	std::unique_ptr<IDraw> m_draw; //!< draw object according to configuration
	std::shared_ptr<IGame> m_game; //!< game object, lives as long as the last dependent screen
	std::unique_ptr<ServerThread> m_server; //!< optional server object

	// all screens are owned and stored by the factory
	std::unique_ptr<MenuScreen> m_menu_screen;
	std::unique_ptr<PregameScreen> m_pregame_screen;
	std::unique_ptr<GameScreen> m_game_screen;
	std::unique_ptr<ServerScreen> m_server_screen;
	std::unique_ptr<TransitionScreen> m_transition_screen;
	std::unique_ptr<PinkScreen> m_pink_screen;
	std::unique_ptr<PinkScreen> m_creme_screen;

};

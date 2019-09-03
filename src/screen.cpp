#include "screen.hpp"
#include "replay.hpp"
#include "configuration.hpp"
#include "game.hpp"
#include "error.hpp"
#include <string>
#include <fstream>
#include <sstream>
#include <random>
#include <cassert>

namespace
{

/**
 * If the autorecord configuration is on, write the appropriate file.
 */
void autorecord_replay(const Journal& journal);

}

IScreen::~IScreen() = default;

std::unique_ptr<IScreen> ScreenFactory::create_menu()
{
	enforce(m_game);

	return std::make_unique<MenuScreen>(*m_draw, *m_game);
}

std::unique_ptr<IScreen> ScreenFactory::create_game()
{
	enforce(m_game);

	const GameState& state = m_game->state();
	auto stage = std::make_unique<Stage>(state, *m_draw);
	auto screen = std::make_unique<GameScreen>(std::move(stage), *m_game);
	return std::move(screen);
}

std::unique_ptr<IScreen> ScreenFactory::create_server()
{
	enforce(m_server);

	return std::make_unique<ServerScreen>(*m_server);
}

std::unique_ptr<IScreen> ScreenFactory::create_transition(IScreen& predecessor, IScreen& successor)
{
	return std::make_unique<TransitionScreen>(predecessor, successor, *m_draw);
}

std::unique_ptr<IScreen> ScreenFactory::create_pink(uint8_t r, uint8_t g, uint8_t b)
{
	return std::make_unique<PinkScreen>(r, g, b, *m_draw);
}


PinkScreen::PinkScreen(uint8_t r, uint8_t g, uint8_t b, IDraw& draw) noexcept
	: m_r(r), m_g(g), m_b(b), m_draw(&draw)
{}

void PinkScreen::draw(float dt)
{
	m_draw->rect(0, 0, CANVAS_W, CANVAS_H, m_r, m_g, m_b, ALPHA_OPAQUE);
}


MenuScreen::MenuScreen(IDraw& draw, IGame& game)
: m_game_time(0),
  m_done(false),
  m_draw(&draw),
  m_game(&game)
{
	Log::info("MenuScreen turn on.");
}

void MenuScreen::update()
{
	m_game_time++;

	// If the game has a fresh new meta/init seed, we know that the game starts.
	if(m_game->switches().ready) {
		m_result = Result::PLAY;
		m_done = true;
	}
}

void MenuScreen::draw(float dt)
{
	m_draw->gfx(0, 0, Gfx::MENUBG);
}

void MenuScreen::input(ControllerAction cinput)
{
	if(ButtonAction::DOWN == cinput.action) {
		if(Button::A == cinput.button) {
			m_game->game_reset(2);
		} else
		if(Button::QUIT == cinput.button) {
			m_result = Result::QUIT;
			m_done = true;
		}
	}
}


GameScreen::GameScreen(std::unique_ptr<Stage> stage, IGame& game, ServerThread* server) :
	m_phase(Phase::INTRO),
	m_time(0),
	m_done(false),
	m_stage(std::move(stage)),
	m_game(&game),
	m_server(server)
{
	assert(m_stage);
	enforce(game.switches().ingame);

	Log::info("GameScreen turn on.");

	// prepare to clear stage's dangling state pointer whenever necessary
	m_game->before_reset([this] { m_stage->set_state(nullptr); m_done = true; });
	m_stage->subscribe_to(m_game->hub());
}

GameScreen::~GameScreen() noexcept
{
	// The network, which can outlive this screen, must not be left with a
	// dangling pointer to our member relay.
	if(m_game->switches().ingame) {
		m_stage->unsubscribe_from(m_game->hub());
	}
}

void GameScreen::update()
{
	// check pause
	if(0 == m_game->switches().speed)
		return;

	advance_tick();
}

void GameScreen::draw(float dt)
{
	m_stage->draw(dt);
}

void GameScreen::stop()
{
	autorecord_replay(m_game->journal());
}

void GameScreen::input(ControllerAction cinput)
{
	// Generally, inputs to the game screen are given to the game object.
	// From there, it might be sent over the network and acknowledged by the
	// server. In any case, the input will finally arrive in the Journal,
	// from which we get them back to display the game on the screen.
	enforce(Button::NONE != cinput.button);

	switch(cinput.button) {
		case Button::LEFT:
		case Button::RIGHT:
		case Button::UP:
		case Button::DOWN:
		case Button::A:
		case Button::B:
		{
			// Forward game input to the network (or other input handler).
			// PlayerInput arrives in the m_phase only after a round trip through
			// the Journal, which consists of server-approved inputs.
			std::optional<PlayerInput> oinput = controller_to_input(cinput);
			if(oinput.has_value()) {
				// TODO: network should assign the actual input time
				oinput->game_time = m_time + 1; // input applies to next frame
				m_game->game_input(Input{oinput.value()});
			}
		}
			break;

		case Button::PAUSE:
			// this is a toggle
			if(ButtonAction::DOWN != cinput.action)
				break;

			if(0 == m_game->switches().speed)
				m_game->set_speed(1);
			else
				m_game->set_speed(0);
			break;

		case Button::RESET:
		{
			// In replay playback mode, there is no reset (only quit).
			if(the_context.configuration->replay_path.has_value())
				break;

			// Only reset once
			if(ButtonAction::DOWN != cinput.action)
				break;

			// preserve the state and replay before it is gone
			autorecord_replay(m_game->journal());

			m_game->game_reset(2);
		}
			break;

		case Button::QUIT:
			m_done = true;
			break;

		case Button::DEBUG1:
			// this is a toggle
			if(ButtonAction::DOWN != cinput.action)
				break;

			m_stage->toggle_pit_debug_overlay();
			m_stage->toggle_pit_debug_highlight();
			break;

		case Button::DEBUG2:
			// this does not work with Network
			if(NetworkMode::LOCAL == the_context.configuration->network_mode)
				advance_tick();
			break;

		case Button::DEBUG3:
			// this does not work with Network
			if(NetworkMode::LOCAL == the_context.configuration->network_mode)
				for(int i = 0; i < 8; i++) advance_tick();
			break;

		case Button::DEBUG4:
			// this does not work with Network
			if(NetworkMode::LOCAL == the_context.configuration->network_mode) {
				m_game->director().debug_no_gameover ^= true;
				// debug_print_pit(stage->pits()[0]->pit);
			}
			break;

		case Button::DEBUG5:
			// this does not work with Network
			if(NetworkMode::LOCAL == the_context.configuration->network_mode) {
				m_game->director().debug_spawn_garbage(6, 2);
				// debug_print_pit(stage->pits()[1]->pit);
			}
			break;

		case Button::NONE:
		default:
			assert(false);

	}
}

void GameScreen::advance_tick()
{
	// logic-independent stage effects
	m_stage->update();

	m_time++;

	switch(m_phase) {

	case Phase::INTRO:
		update_intro();
		break;

	case Phase::PLAY:
		update_play();
		break;

	}
}

void GameScreen::update_intro()
{
	float black_fraction = 1.f - (static_cast<float>(m_time) / INTRO_TIME);
	m_stage->fade(black_fraction);

	if(INTRO_TIME <= m_time) {
		m_phase = Phase::PLAY;
		m_time = m_game->state().game_time();
	}
}

void GameScreen::update_play()
{
	// detect game over
	const Journal& journal = m_game->journal();
	const int winner = journal.meta().winner;
	if(NOONE != winner) {
		m_phase = Phase::RESULT;
		m_stage->show_result(winner);
		autorecord_replay(journal);
		return; // skip the usual; we don't need more game logic
	}

	// run game logic until the target time, considering even retcon inputs
	m_game->synchronurse(m_time);
}

ServerScreen::ServerScreen(ServerThread& server) noexcept
	: m_server(&server), m_done(false)
{
}

ServerScreen::~ServerScreen() noexcept
{
	try {
		m_server->exit();
	}
	catch(const std::exception& ex) {
		show_error(ex);
	}
	catch(...) {
		Log::error("Unknown exception occurred.");
	}
}

void ServerScreen::update()
{
}

void ServerScreen::input(ControllerAction cinput)
{
	if(Button::QUIT == cinput.button)
		m_done = true;
}


TransitionScreen::TransitionScreen(IScreen& predecessor, IScreen& successor, IDraw& draw)
	:
	m_predecessor(predecessor),
	m_successor(successor),
	m_predecessor_canvas(draw.create_canvas()),
	m_successor_canvas(draw.create_canvas()),
	m_time(0),
	m_draw(&draw)
{}

void TransitionScreen::update()
{
	m_predecessor.update();
	m_successor.update();
	m_time++;
}

void TransitionScreen::draw(float dt)
{
	m_predecessor_canvas->use_as_target();
	m_predecessor.draw(dt);

	m_successor_canvas->use_as_target();
	m_successor.draw(dt);

	int progress_px = CANVAS_W * m_time / TRANSITION_TIME;

	// swipe transition: successor screen enters from the left.
	m_draw->reset_target();

	m_draw->clip(0, 0, progress_px, CANVAS_H);
	m_predecessor_canvas->draw();

	m_draw->clip(progress_px, 0, CANVAS_W-progress_px, CANVAS_H);
	m_successor_canvas->draw();
}

namespace
{

void autorecord_replay(const Journal& journal)
{
	if(the_context.configuration->autorecord)
		replay_write(journal);
}

}

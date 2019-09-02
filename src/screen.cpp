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

	return std::make_unique<MenuScreen>(DrawMenu{}, *m_game);
}

std::unique_ptr<IScreen> ScreenFactory::create_game()
{
	enforce(m_game);

	const GameState& state = m_game->state();
	auto stage = std::make_unique<Stage>(state);
	auto draw = std::make_unique<DrawGame>(*stage);
	auto screen = std::make_unique<GameScreen>(std::move(stage), std::move(draw), *m_game);
	return std::move(screen);
}

std::unique_ptr<IScreen> ScreenFactory::create_server()
{
	enforce(m_server);

	return std::make_unique<ServerScreen>(*m_server);
}

std::unique_ptr<IScreen> ScreenFactory::create_transition(IScreen& predecessor, IScreen& successor)
{
	const IDraw2& predecessor_draw = predecessor.get_draw();
	const IDraw2& successor_draw = successor.get_draw();
	DrawTransition draw_transition(predecessor_draw, successor_draw);
	return std::make_unique<TransitionScreen>(predecessor, successor, std::move(draw_transition));
}


MenuScreen::MenuScreen(DrawMenu&& draw, IGame& game)
: m_game_time(0),
  m_done(false),
  m_draw(std::move(draw)),
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
	m_draw.draw(dt);
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


GameScreen::GameScreen(std::unique_ptr<Stage> stage, std::unique_ptr<DrawGame> draw,
                       IGame& game, ServerThread* server) :
	m_phase(Phase::INTRO),
	m_time(0),
	m_done(false),
	m_draw(std::move(draw)),
	m_stage(std::move(stage)),
	m_game(&game),
	m_server(server)
{
	assert(m_stage);
	assert(m_draw);
	enforce(game.switches().ingame);

	Log::info("GameScreen turn on.");

	m_draw->show_cursor(true);
	m_draw->show_banner(false);

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
	// detect game end from server
	// TODO: this is subject to a race condition if the server immediately starts another game.
	//       It will be fixed when the server waits for all clients' ready before starting.
	if(!m_game->switches().ingame) {
		m_done = true;
	}

	// check pause
	if(0 == m_game->switches().speed)
		return;

	advance_tick();
}

void GameScreen::draw(float dt)
{
	m_draw->draw(dt);
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

			m_draw->toggle_pit_debug_overlay();
			m_draw->toggle_pit_debug_highlight();
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
	float fadeness = (static_cast<float>(m_time) / INTRO_TIME);
	m_draw->fade(fadeness);

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
		// display winner
		auto& stage_objects = m_stage->sobs();
		for(size_t i = 0; i < stage_objects.size(); i++) {
			BannerFrame frame = (i == winner) ? BannerFrame::WIN : BannerFrame::LOSE;
			stage_objects[i].banner.frame = frame;
		}

		m_phase = Phase::RESULT;
		m_draw->show_cursor(false);
		m_draw->show_banner(true);
		autorecord_replay(journal);
		return; // skip the usual; we don't need more game logic
	}

	// run game logic until the target time, considering even retcon inputs
	m_game->synchronurse(m_time);
}

ServerScreen::ServerScreen(ServerThread& server)
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


void TransitionScreen::update()
{
	m_predecessor.update();
	m_successor.update();
	m_time++;
}

void TransitionScreen::draw(float dt)
{
	m_draw.set_time(m_time);
	m_draw.draw(dt);
}

namespace
{

void autorecord_replay(const Journal& journal)
{
	if(the_context.configuration->autorecord)
		replay_write(journal);
}

}

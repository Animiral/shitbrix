#include "screen.hpp"
#include "configuration.hpp"
#include "state.hpp"
#include "error.hpp"
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <cassert>

// only for debug functions
#include <iostream>

namespace
{

/**
 * Send a message through the client, requesting to restart the game with
 * the given parameters.
 */
void client_send_reset(IClient& client)
{
	// TODO: this should only work from privileged client.
	// TODO: maybe the randomization should be done on the server.
	static std::random_device rdev;
	const GameMeta meta{2, rdev()};
	client.send_reset(meta);
}

void debug_print_pit(const Pit& pit);

}

IScreen::~IScreen() = default;

std::unique_ptr<IScreen> ScreenFactory::create_menu()
{
	return std::make_unique<MenuScreen>(DrawMenu{}, *m_client);
}

std::unique_ptr<IScreen> ScreenFactory::create_game()
{
	enforce(m_client);
	GameState& state = m_client->gamedata().state;
	auto stage = std::make_unique<Stage>(state);
	auto draw = std::make_unique<DrawGame>(*stage);
	auto screen = std::make_unique<GameScreen>(std::move(stage), std::move(draw), *m_client);
	return std::move(screen);
}

std::unique_ptr<IScreen> ScreenFactory::create_server()
{
	assert(m_server);
	return std::make_unique<ServerScreen>(*m_server);
}

std::unique_ptr<IScreen> ScreenFactory::create_transition(IScreen& predecessor, IScreen& successor)
{
	const IDraw& predecessor_draw = predecessor.get_draw();
	const IDraw& successor_draw = successor.get_draw();
	DrawTransition draw_transition(predecessor_draw, successor_draw);
	return std::make_unique<TransitionScreen>(predecessor, successor, std::move(draw_transition));
}


MenuScreen::MenuScreen(DrawMenu&& draw, IClient& client)
: m_game_time(0),
  m_done(false),
  m_draw(std::move(draw)),
  m_client(&client)
{
	Log::info("MenuScreen turn on.");
}

void MenuScreen::update()
{
	m_game_time++;

	// If the client has a fresh new game state, we know that the game starts.
	if(m_client->is_game_ready()) {
		m_result = Result::PLAY;
		m_done = true;
	}
}

void MenuScreen::draw(float dt)
{
	m_draw.draw(dt);
}

void MenuScreen::input(ControllerInput cinput)
{
	if(ButtonAction::DOWN == cinput.action) {
		if(Button::A == cinput.button) {
			client_send_reset(*m_client);
		} else
		if(Button::QUIT == cinput.button) {
			m_result = Result::QUIT;
			m_done = true;
		}
	}
}


GameScreen::GameScreen(std::unique_ptr<Stage> stage, std::unique_ptr<DrawGame> draw,
                       IClient& client, ServerThread* server) :
	m_phase(Phase::INTRO),
	m_time(0),
	m_done(false),
	m_draw(std::move(draw)),
	m_stage(std::move(stage)),
	m_client(&client),
	m_server(server)
{
	assert(m_stage);
	assert(m_draw);

	Log::info("GameScreen turn on.");

	start();
}

GameScreen::~GameScreen() noexcept
{
	// The network, which can outlive this screen, must not be left with a
	// dangling pointer to our member relay.
	if(m_client->is_ingame()) {
		m_stage->unsubscribe_from(m_client->gamedata().rules.event_hub);
	}
}

void GameScreen::update()
{
	// detect restarts from server
	if(m_client->is_game_ready()) {
		m_client->game_start();
		start();
	}

	// check pause
	if(0 == m_client->gamedata().dials.speed)
		return;

	advance_tick();
}

void GameScreen::draw(float dt)
{
	m_draw->draw(dt);
}

void GameScreen::stop()
{
	autorecord_replay(m_client->gamedata().journal);
}

void GameScreen::input(ControllerInput cinput)
{
	// Generally, inputs to the game screen are sent to the network through
	// the client object.
	// The network sends back all acknowledged inputs and sends them to the Journal,
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
			// GameInput arrives in the m_phase only after a round trip through
			// the Journal, which consists of server-approved inputs.
			std::optional<GameInput> oinput = controller_to_game(cinput);
			if(oinput.has_value()) {
				// TODO: network should assign the actual input time
				oinput->game_time = m_time + 1; // input applies to next frame
				m_client->send_input(oinput.value());
			}
		}
			break;

		case Button::PAUSE:
			// this is a toggle
			if(ButtonAction::DOWN != cinput.action)
				break;

			if(0 == m_client->gamedata().dials.speed)
				m_client->send_speed(1);
			else
				m_client->send_speed(0);
			break;

		case Button::RESET:
		{
			// In replay playback mode, there is no reset (only quit).
			if(the_context.configuration->replay_path.has_value())
				break;

			// Only reset once
			if(ButtonAction::DOWN != cinput.action)
				break;

			client_send_reset(*m_client);

			// preserve the state and replay before it is gone
			autorecord_replay(m_client->gamedata().journal);
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
				m_client->gamedata().rules.block_director.debug_no_gameover ^= true;
				// debug_print_pit(stage->pits()[0]->pit);
			}
			break;

		case Button::DEBUG5:
			// this does not work with Network
			if(NetworkMode::LOCAL == the_context.configuration->network_mode) {
				m_client->gamedata().rules.block_director.debug_spawn_garbage(6, 2);
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
		m_time = m_client->gamedata().state.game_time();
	}
}

void GameScreen::update_play()
{
	// detect game over
	Journal& journal = m_client->gamedata().journal;
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
	GameData& gamedata = m_client->gamedata();
	synchronurse(gamedata.state, m_time, gamedata.journal, gamedata.rules);
}

void GameScreen::start()
{
	GameData& gamedata = m_client->gamedata();
	const GameMeta meta = gamedata.journal.meta();
	Log::info("Game reset: players=%d, seed=%d.", meta.players, meta.seed);

	m_phase = Phase::INTRO;
	m_time = 0;
	m_done = false;
	m_draw->show_cursor(true);
	m_draw->show_banner(false);

	m_stage->subscribe_to(m_client->gamedata().rules.event_hub);
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

void ServerScreen::input(ControllerInput cinput)
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

[[ maybe_unused ]]
void debug_print_pit(const Pit& pit)
{
	std::cerr << "--- Pit blocks:\n\n";

	for(int r = pit.top(); r <= pit.bottom()+1; r++)
	for(int c = 0; c <= PIT_COLS; c++) {
		Block* block = pit.block_at(RowCol{r,c});
		if(!block) continue;

		Block::State state = block->block_state();
		Block::Color color = block->col;
		std::string state_str;
		std::string color_str;

		switch(state) {
			case Block::State::DEAD: state_str = "DEAD"; break;
			case Block::State::PREVIEW: state_str = "PREVIEW"; break;
			case Block::State::REST: state_str = "REST"; break;
			case Block::State::SWAP_LEFT: state_str = "SWAP_LEFT"; break;
			case Block::State::SWAP_RIGHT: state_str = "SWAP_RIGHT"; break;
			case Block::State::FALL: state_str = "FALL"; break;
			case Block::State::LAND: state_str = "LAND"; break;
			case Block::State::BREAK: state_str = "BREAK"; break;
			default: ;
		}

		switch(color) {
			case Block::Color::FAKE: color_str = "fake"; break;
			case Block::Color::BLUE: color_str = "blue"; break;
			case Block::Color::RED: color_str = "red"; break;
			case Block::Color::YELLOW: color_str = "yellow"; break;
			case Block::Color::GREEN: color_str = "green"; break;
			case Block::Color::PURPLE: color_str = "purple"; break;
			case Block::Color::ORANGE: color_str = "orange"; break;
			default: ;
		}

		std::cerr << "r" << r << "c" << c << " " << state_str << " " << color_str << " block\n";
	}

	std::cerr << "\n";
}

}

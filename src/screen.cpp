#include "screen.hpp"
#include "options.hpp"
#include "error.hpp"
#include <sstream>
#include <chrono>
#include <ctime>
#include <iostream>
#include <iomanip>

namespace
{

/**
 * Returns the default name of the file where the replay of this session will
 * be stored. This default name is built from the current date and time.
 */
std::string make_journal_file();
int opponent(int player);
void debug_print_pit(const Pit& pit);

}

IScreen::~IScreen() = default;

ScreenFactory::ScreenFactory(const Options& options,const Assets& assets, const Audio& audio)
: m_options(options), m_assets(assets), m_audio(audio), m_client(nullptr)
{
}

std::unique_ptr<IScreen> ScreenFactory::create_menu()
{
	DrawMenu draw_menu(m_assets);
	return std::make_unique<MenuScreen>(std::move(draw_menu), m_audio, *m_client);
}

std::unique_ptr<IScreen> ScreenFactory::create_game()
{
	enforce(m_client);
	GameState& state = m_client->gamedata().state;
	auto stage = std::make_unique<Stage>(state);
	auto draw = std::make_unique<DrawGame>(*stage, m_assets);
	auto screen = std::make_unique<GameScreen>(std::move(stage), std::move(draw), m_audio, *m_client);
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


MenuScreen::MenuScreen(DrawMenu&& draw, const Audio& audio, BasicClient& client)
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
			m_client->send_reset(); // TODO: this should only work from privileged client
		} else
		if(Button::QUIT == cinput.button) {
			m_result = Result::QUIT;
			m_done = true;
		}
	}
}

IGamePhase::~IGamePhase() =default;

GameIntro::GameIntro(GameScreen* screen)
: IGamePhase(screen), countdown(INTRO_TIME)
{
	m_screen->m_draw->show_cursor(true);
	m_screen->m_draw->show_banner(false);
}

void GameIntro::update()
{
	float fadeness = ((INTRO_TIME - countdown + 1.f) / INTRO_TIME);
	m_screen->m_draw->fade(fadeness);

	if(0 == --countdown) {
		auto phase = std::make_unique<GamePlay>(m_screen);
		m_screen->change_phase(std::move(phase));
	}
}


void GamePlay::update()
{
	// detect game over
	const int winner = m_screen->m_client->gamedata().journal.meta().winner;
	if(NOONE != winner) {
		// display winner
		auto& stage_objects = m_screen->m_stage->sobs();
		for(size_t i = 0; i < stage_objects.size(); i++) {
			BannerFrame frame = (i == winner) ? BannerFrame::WIN : BannerFrame::LOSE;
			stage_objects[i].banner.frame = frame;
		}

		auto phase = std::make_unique<GameResult>(m_screen, winner);
		m_screen->change_phase(std::move(phase));

		return; // skip the usual; we don't need more game logic
	}

	// reach the target time, considering even retcon inputs
	long& game_time = m_screen->m_game_time;
	game_time++;

	GameData& gamedata = m_screen->m_client->gamedata();
	synchronurse(gamedata.state, game_time, gamedata.journal, gamedata.rules);
}

GameResult::GameResult(GameScreen* screen, int winner) : IGamePhase(screen)
{
	m_screen->m_draw->show_cursor(false);
	m_screen->m_draw->show_banner(true);
}

void GameResult::update()
{
	// this is only needed to display the replay correctly
	m_screen->m_game_time++;
}


GameScreen::GameScreen(
	std::unique_ptr<Stage> stage,
	std::unique_ptr<DrawGame> draw,
	const Audio& audio,
	BasicClient& client,
	ServerThread* server)
: m_game_time(0),
  m_done(false),
  m_stage(std::move(stage)),
  m_draw(std::move(draw)),
  m_client(&client),
  m_server(server),
  m_bonus_relay(*m_stage),
  m_sound_relay(audio),
  m_shake_relay(*m_draw)
{
	assert(m_stage);
	assert(m_draw);

	Log::info("GameScreen turn on.");

	start();
}

GameScreen::~GameScreen() noexcept
{
	// The phase object holds a reference to this. To guarantee safe access
	// from the destructor of the phase, we must destroy it before this
	// GameScreen becomes invalid.
	m_next_phase.reset();
	m_game_phase.reset();

	// The client, which can outlive this screen, must not be left with a
	// dangling pointer to our member relay.
	if(m_client->is_ingame()) {
		evt::GameEventHub& hub = m_client->gamedata().rules.event_hub;
		hub.unsubscribe(m_bonus_relay);
		hub.unsubscribe(m_sound_relay);
		hub.unsubscribe(m_shake_relay);
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

	// logic-independent stage effects
	m_stage->update();

	// run game logic
	assert(m_game_phase);
	m_game_phase->update();

	if(m_next_phase)
		change_phase_impl();
}

void GameScreen::draw(float dt)
{
	m_draw->draw(dt);
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
				oinput->game_time = m_game_time + 1; // input applies to next frame
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
			// Only reset once
			if(ButtonAction::DOWN != cinput.action)
				break;

			// TODO: this is supposed to work only in client-with-server mode
			m_client->send_reset();
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
			// TODO: this does not work with Network
			//update_impl();
			break;

		case Button::DEBUG3:
			// TODO: this does not work with Network
			//for(int i = 0; i < 8; i++) update_impl();
			break;

		case Button::DEBUG4:
			// TODO: this does not work with Network
			//m_rules.block_director.debug_no_gameover ^= true;
			// debug_print_pit(stage->pits()[0]->pit);
			break;

		case Button::DEBUG5:
			// TODO: this does not work with Network
			//m_rules.block_director.debug_spawn_garbage(6, 2);
			// debug_print_pit(stage->pits()[1]->pit);
			break;

		case Button::NONE:
		default:
			assert(false);

	}
}

void GameScreen::start()
{
	GameData& gamedata = m_client->gamedata();
	const GameMeta meta = gamedata.journal.meta();
	Log::info("Game reset: players=%d, seed=%d.", meta.players, meta.seed);

	change_phase(std::make_unique<GameIntro>(this));
	change_phase_impl();
	m_game_time = gamedata.state.game_time();
	m_done = false;

	// BUG! Due to lack of RAII wrapping, these relays will not be properly
	// unsubscribed from the hub if this is being called from the GameScreen
	// constructor and one of the subscriptions fails.
	evt::GameEventHub& hub = m_client->gamedata().rules.event_hub;
	hub.subscribe(m_bonus_relay);
	hub.subscribe(m_sound_relay);
	hub.subscribe(m_shake_relay);
}

void GameScreen::change_phase(std::unique_ptr<IGamePhase> phase)
{
	m_next_phase = std::move(phase);
}

void GameScreen::change_phase_impl()
{
	m_game_phase = std::move(m_next_phase);
}


ServerScreen::ServerScreen(ServerThread& server)
	: m_server(&server), m_draw(20, 235, 0), m_done(false)
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

void ServerScreen::draw(float dt)
{
	m_draw.draw(dt);
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

std::string make_journal_file()
{
	using clock = std::chrono::system_clock;
	auto now = clock::now();
	std::time_t time_now = clock::to_time_t(now);
	struct tm ltime = *std::localtime(&time_now);

	if(0 != errno)
		throw GameException("Failed to get localtime for journal file name.");

	std::ostringstream stream;
	stream << std::put_time(&ltime, "replay/%Y-%m-%d_%H-%M.txt");
	return stream.str();
}

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

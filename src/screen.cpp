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
	return std::make_unique<MenuScreen>(std::move(draw_menu), m_audio);
}

std::unique_ptr<IScreen> ScreenFactory::create_game()
{
	enforce(m_client);
	Journal& journal = m_client->journal();
	auto stage = std::make_unique<Stage>(GameState(journal.meta()));
	auto draw = std::make_unique<DrawGame>(*stage, m_assets);
	auto screen = std::make_unique<GameScreen>(std::move(stage), std::move(draw), m_audio, journal, *m_client);
	return std::move(screen);
}

std::unique_ptr<IScreen> ScreenFactory::create_server()
{
	return std::make_unique<ServerScreen>();
}

std::unique_ptr<IScreen> ScreenFactory::create_transition(IScreen& predecessor, IScreen& successor)
{
	const IDraw& predecessor_draw = predecessor.get_draw();
	const IDraw& successor_draw = successor.get_draw();
	DrawTransition draw_transition(predecessor_draw, successor_draw);
	return std::make_unique<TransitionScreen>(predecessor, successor, std::move(draw_transition));
}


MenuScreen::MenuScreen(DrawMenu&& draw, const Audio& audio)
: m_game_time(0),
  m_done(false),
  m_draw(std::move(draw))
{
	Log::info("MenuScreen turn on.");
}

void MenuScreen::update()
{
	m_game_time++;
}

void MenuScreen::draw(float dt)
{
	m_draw.draw(dt);
}

void MenuScreen::input(ControllerInput cinput)
{
	if(ButtonAction::DOWN == cinput.action) {
		if(Button::A == cinput.button) {
			m_result = Result::PLAY;
			m_done = true;
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
	// get events from journal, from which inputs will be relayed to the phase
	long& game_time = m_screen->m_game_time;
	game_time++;

	GameState& state = m_screen->m_stage->state();
	Rules& rules = m_screen->m_rules;
	synchronurse(state, game_time, m_screen->m_journal, rules);

	// NOTE: to determine the winner is a server-side job only.
	if(rules.block_director.over()) {
		int winner = 0; // opponent(static_cast<int>(i));
		m_screen->m_journal.set_winner(winner);

		// NOTE: this should only happen when the Journal tells us that the game is over.
		// We should not assume that the winner that we have detected is valid until
		// it is part of the game record.
		m_screen->m_gameover_relay->fire(evt::GameOver{winner});

		auto phase = std::make_unique<GameResult>(m_screen, winner);
		m_screen->change_phase(std::move(phase));

		return;
	}
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
	Journal& journal,
	ENetClient& client)
: m_stage(std::move(stage)),
  m_draw(std::move(draw)),
  m_pause(false),
  m_sound_relay(audio),
  m_journal(journal),
  m_client(client),
  m_rules{BlockDirector(m_stage->state())}
{
	assert(m_stage);
	assert(m_draw);
	Log::info("GameScreen turn on.");
	start();
/*
	if(replay_infile) {
		Log::info("Read replay from file: %s.", replay_infile);
		std::ifstream stream(replay_infile);

		if(!stream)
			throw ReplayException("Could not open replay file.");

		m_journal = std::make_unique<Journal>(replay_read(stream));
		m_journal->set_sink(this);
	}
	else {
		std::random_device rdev;
		seed(rdev());
	}*/

	//reset(m_journal.meta());
}

GameScreen::~GameScreen() noexcept
{
	// The phase object holds a reference to this. To guarantee safe access
	// from the destructor of the phase, we must destroy it before this
	// GameScreen becomes invalid.
	m_next_phase.reset();
	m_game_phase.reset();
}

void GameScreen::update()
{
	if(!m_pause)
		update_impl();
}

void GameScreen::draw(float dt)
{
	m_draw->draw(dt);
}

void GameScreen::input(ControllerInput cinput)
{
	// Generally, inputs to the game screen are sent to the Network object.
	// This may happen in the form of full-formed ReplayRecords or simply
	// GameInputs, depending on whether we are Server or Client.
	// The Network generates ReplayRecords and sends them to the Journal,
	// from which we get our ReplayRecords to display the game on the screen.

	// Because we do not have Network yet, we always send ReplayRecords directly to the Journal.
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
				m_client.send_input(oinput.value());
			}
		}
			break;

		case Button::PAUSE:
			m_pause = !m_pause;
			break;

		case Button::RESET:
			// This button is disabled until further notice.
			//{
			//	std::random_device rdev;
			//	seed(rdev());
			//}
			//reset(m_journal->meta());
			break;

		case Button::QUIT:
			m_done = true;
			break;

		case Button::DEBUG1:
			m_draw->toggle_pit_debug_overlay();
			m_draw->toggle_pit_debug_highlight();
			break;

		case Button::DEBUG2:
			// TODO: this does not work with Network
			update_impl();
			break;

		case Button::DEBUG3:
			// TODO: this does not work with Network
			for(int i = 0; i < 8; i++) update_impl();
			break;

		case Button::DEBUG4:
			// TODO: this does not work with Network
			m_rules.block_director.debug_no_gameover ^= true;
			// debug_print_pit(stage->pits()[0]->pit);
			break;

		case Button::DEBUG5:
			// TODO: this does not work with Network
			m_rules.block_director.debug_spawn_garbage(6, 2);
			// debug_print_pit(stage->pits()[1]->pit);
			break;

		case Button::NONE:
		default:
			assert(false);

	}
}

void GameScreen::start()
{
	const GameMeta meta = m_journal.meta();
	Log::info("Game reset: players=%d, seed=%d.", meta.players, meta.seed);

	change_phase(std::make_unique<GameIntro>(this));
	change_phase_impl();
	m_game_time = 1L;
	m_done = false;

	m_rules = {BlockDirector(m_stage->state())};
	m_gameover_relay.reset(new evt::GameOverRelay(m_stage->sobs()));
	m_shake_relay.reset(new ShakeRelay(*m_draw));

	//for(auto& pobjs : m_pobjects) {
	//	pobjs->event_hub.subscribe(m_sound_relay);
	//	pobjs->event_hub.subscribe(m_shake_relay);
	//	pobjs->event_hub.subscribe(*m_gameover_relay);
	//}
}

void GameScreen::change_phase(std::unique_ptr<IGamePhase> phase)
{
	m_next_phase = std::move(phase);
}

void GameScreen::change_phase_impl()
{
	m_game_phase = std::move(m_next_phase);
}

void GameScreen::update_impl()
{
	// run game logic
	m_game_phase->update();

	if(m_next_phase)
		change_phase_impl();
}


ServerScreen::ServerScreen()
	: m_server(), m_draw(20, 235, 0), m_done(false)
{
}

ServerScreen::~ServerScreen() noexcept
{
	try {
		m_server.exit();
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

int opponent(int player)
{
	enforce(0 == player || 1 == player);
	return 0 == player ? 1 : 0;
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

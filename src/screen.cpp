#include "screen.hpp"
#include "options.hpp"
#include <sstream>
#include <chrono>
#include <ctime>
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
: m_options(options), m_assets(assets), m_audio(audio)
{
}

std::unique_ptr<IScreen> ScreenFactory::create_menu() const
{
	DrawMenu draw_menu(m_assets);
	return std::make_unique<MenuScreen>(std::move(draw_menu), m_audio);
}

std::unique_ptr<IScreen> ScreenFactory::create_game() const
{
	DrawGame draw_game(m_assets);
	return std::make_unique<GameScreen>(m_options.replay_file(), make_journal_file().c_str(), std::move(draw_game), m_audio);
}

std::unique_ptr<IScreen> ScreenFactory::create_transition(IScreen& predecessor, IScreen& successor) const
{
	DrawTransition draw_transition(predecessor.get_draw(), successor.get_draw());
	return std::make_unique<TransitionScreen>(predecessor, successor, std::move(draw_transition));
}


MenuScreen::MenuScreen(DrawMenu&& draw, const Audio& audio)
: m_game_time(0),
  m_done(false),
  m_draw(std::move(draw))
{
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
	m_screen->m_draw.show_cursor(true);
}

void GameIntro::update()
{
	float fadeness = ((INTRO_TIME - countdown + 1.f) / INTRO_TIME);
	m_screen->m_draw.fade(fadeness);

	if(0 == --countdown) {
		auto phase = std::make_unique<GamePlay>(m_screen);
		m_screen->change_phase(std::move(phase));
	}
}


GamePlay::GamePlay(GameScreen* screen) : IGamePhase(screen)
{
	m_screen->journal << ReplayEvent::make_start();
}

void GamePlay::update()
{
	m_screen->stage->update();

	for(size_t i = 0; i < m_screen->m_pobjects.size(); i++) {
		auto& pobjs = m_screen->m_pobjects[i];
		pobjs->block_director.update();

		if(pobjs->block_director.over()) {
			int winner = opponent(static_cast<int>(i));
			m_screen->m_gameover_relay->fire(evt::GameOver{winner});
			auto phase = std::make_unique<GameResult>(m_screen, winner);
			m_screen->change_phase(std::move(phase));
			break;
		}
	}

	m_screen->m_game_time++;
}

void GamePlay::input(GameInput ginput)
{
	m_screen->journal << ReplayEvent::make_input(m_screen->m_game_time, ginput);

	GameScreen::PlayerObjects& pobjs = *m_screen->m_pobjects[ginput.player];

	switch(ginput.button) {
		case GameButton::LEFT:
		case GameButton::RIGHT:
		case GameButton::UP:
		case GameButton::DOWN:
			if(ButtonAction::DOWN == ginput.action)
			{
				Dir dir = static_cast<Dir>(ginput.button);
				pobjs.cursor_director.move(dir);
			}

			break;

		case GameButton::SWAP:
			if(ButtonAction::DOWN == ginput.action)
			{
				RowCol swap_rc = pobjs.cursor_director.rc();
				pobjs.block_director.swap(swap_rc);
			}

			break;

		case GameButton::RAISE:
			pobjs.block_director.set_raise(ButtonAction::DOWN == ginput.action);
			break;

		case GameButton::NONE:
		default:
			SDL_assert_paranoid(false);

	}
}


GameResult::GameResult(GameScreen* screen, int winner) : IGamePhase(screen)
{
	std::ostringstream stream;
	stream << winner;
	m_screen->journal << ReplayEvent::make_set("winner", stream.str());
	m_screen->m_draw.show_cursor(false);
	m_screen->m_draw.show_banner(true);
}

GameResult::~GameResult()
{
	m_screen->journal << ReplayEvent::make_end(m_screen->m_game_time);
}

void GameResult::update()
{
	// this is only needed to display the replay correctly
	m_screen->m_game_time++;
}


GameScreen::GameScreen(const char* replay_infile, const char* replay_outfile, DrawGame&& draw, const Audio& audio)
: m_pause(false),
  input_mixer(*this, replay_infile),
  replay_outstream(replay_outfile),
  journal(replay_outstream),
  m_draw(std::move(draw)),
  m_sound_relay(audio)
{
	if(!replay_infile) {
		std::random_device rdev;
		seed(rdev());
	}

	reset();
}

void GameScreen::reset()
{
	m_draw.clear();
	m_pobjects.clear();

	change_phase(std::make_unique<GameIntro>(this));
	change_phase_impl();
	m_game_time = 0L;
	m_done = false;

	auto builder = StageBuilder();
	stage = builder.construct();

	Pit& left_pit = *builder.left_pit;
	Cursor& left_cursor = *builder.left_cursor;
	Banner& left_banner = *builder.left_banner;
	BonusIndicator& left_bonus = *builder.left_bonus;

	Pit& right_pit = *builder.right_pit;
	Cursor& right_cursor = *builder.right_cursor;
	Banner& right_banner = *builder.right_banner;
	BonusIndicator& right_bonus = *builder.right_bonus;

	auto left_pobjs = std::make_unique<PlayerObjects>(rndgen, left_pit, left_cursor, right_pit, left_bonus);
	auto right_pobjs = std::make_unique<PlayerObjects>(rndgen, right_pit, right_cursor, left_pit, right_bonus);
	m_pobjects.push_back(std::move(left_pobjs));
	m_pobjects.push_back(std::move(right_pobjs));
	m_draw.add_pit(left_pit, left_cursor, left_banner, left_bonus);
	m_draw.add_pit(right_pit, right_cursor, right_banner, right_bonus);

	m_gameover_relay.reset(new evt::GameOverRelay(stage->sobs()));

	for(auto& pobjs : m_pobjects) {
		pobjs->event_hub.append(m_sound_relay);
		pobjs->event_hub.append(*m_gameover_relay);
	}
}

void GameScreen::update()
{
	if(!m_pause)
		update_impl();

	// auto-move cursor when scrolling out of bounds
	for(auto& pobjs : m_pobjects)
		pobjs->cursor_director.move(Dir::NONE);
}

void GameScreen::draw(float dt)
{
	m_draw.draw(dt);
}

void GameScreen::input(ControllerInput cinput)
{
	switch(cinput.button) {
		case Button::LEFT:
		case Button::RIGHT:
		case Button::UP:
		case Button::DOWN:
		case Button::A:
		case Button::B:
			input_mixer.input(cinput);
			break;

		case Button::PAUSE:
			m_pause = !m_pause;
			break;

		case Button::RESET:
			reset();
			break;

		case Button::QUIT:
			m_done = true;
			break;

		case Button::DEBUG1:
			m_draw.toggle_pit_debug_overlay();
			m_draw.toggle_pit_debug_highlight();
			break;

		case Button::DEBUG2:
			update_impl();
			break;

		case Button::DEBUG3:
			for(int i = 0; i < 8; i++) update_impl();
			break;

		case Button::DEBUG4:
			m_pobjects[0]->block_director.debug_no_gameover ^= true;
			m_pobjects[1]->block_director.debug_no_gameover ^= true;
			// debug_print_pit(stage->pits()[0]->pit);
			break;

		case Button::DEBUG5:
			m_pobjects[1]->block_director.debug_spawn_garbage(6, 2);
			// debug_print_pit(stage->pits()[1]->pit);
			break;

		case Button::NONE:
		default:
			SDL_assert_paranoid(false);

	}
}

void GameScreen::handle(const ReplayEvent& event)
{
	switch(event.type()) {

	case ReplayEvent::Type::SET:
		if("rng_seed" == event.set_name()) {
			std::istringstream stream(event.set_value());
			unsigned int rng_seed;
			stream >> rng_seed;
			seed(rng_seed);
		}
		else {
			// TODO: handle other “set” names
		}
		break;

	case ReplayEvent::Type::START:
		reset();
		break;

	case ReplayEvent::Type::INPUT:
		// ignore (mixer passes this to game phase)
		break;

	case ReplayEvent::Type::END:
		m_done = true;
		break;

	}
}

void GameScreen::change_phase(std::unique_ptr<IGamePhase> phase)
{
	m_next_phase = std::move(phase);
}

void GameScreen::change_phase_impl()
{
	input_mixer.set_game_sink(m_next_phase.get());
	m_game_phase = std::move(m_next_phase);
}

void GameScreen::update_impl()
{
	input_mixer.update(m_game_time);
	m_game_phase->update();

	if(m_next_phase)
		change_phase_impl();
}

void GameScreen::seed(unsigned int rng_seed)
{
	rndgen = std::make_shared<std::mt19937>(rng_seed);
	std::ostringstream stream;
	stream << rng_seed;
	journal << ReplayEvent::make_set("rng_seed", stream.str());
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
	struct tm ltime;

	if (localtime_s(&ltime, &time_now)) {
		std::ostringstream stream;
		stream << std::put_time(&ltime, "replay/%Y-%m-%d_%H-%M.txt");
		return stream.str();
	}
	else {
		return "replay/default.txt";
	}
}

int opponent(int player)
{
	SDL_assert(0 == player || 1 == player);
	return 0 == player ? 1 : 0;
}

[[ maybe_unused, gnu::unused ]]
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

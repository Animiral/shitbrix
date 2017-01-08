#include "screen.hpp"
#include <sstream>

namespace
{

int opponent(int player);
void debug_print_pit(const Pit& pit);

}

IGamePhase::~IGamePhase() =default;

void IGamePhase::draw() const
{
	m_screen->m_draw.draw_all();
}


GameIntro::GameIntro(GameScreen* screen)
: IGamePhase(screen), countdown(INTRO_TIME)
{
	m_screen->m_draw.show_cursors(true);
}

void GameIntro::draw() const
{
	float fadeness = ((INTRO_TIME - countdown + 1.f) / INTRO_TIME);
	m_screen->m_context.fade(fadeness);
	IGamePhase::draw();
}

void GameIntro::update()
{
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
	m_screen->stage->update(m_screen->m_context);

	for(size_t i = 0; i < m_screen->m_pobjects.size(); i++) {
		auto& pobjs = m_screen->m_pobjects[i];
		pobjs->block_director.update();

		if(pobjs->block_director.over()) {
			int winner = opponent(i);
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
			{
				Dir dir = static_cast<Dir>(ginput.button);
				pobjs.cursor_director.move(dir);
			}

			break;

		case GameButton::SWAP:
		case GameButton::RAISE: // TODO: implement raise
			{
				RowCol swap_rc = pobjs.cursor_director.rc();
				pobjs.block_director.swap(swap_rc);
			}

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

	float dx = (PIT_W-BANNER_W)/2;
	float dy = (PIT_H-BANNER_H)/2;
	Point left_banner_loc = LPIT_LOC.offset(dx, dy);
	Point right_banner_loc = RPIT_LOC.offset(dx, dy);

	if(0 == winner) {
		banner_left.reset(new Banner{left_banner_loc, BannerFrame::WIN});
		banner_right.reset(new Banner{right_banner_loc, BannerFrame::LOSE});
	}
	else {
		banner_left.reset(new Banner{left_banner_loc, BannerFrame::LOSE});
		banner_right.reset(new Banner{right_banner_loc, BannerFrame::WIN});
	}

	m_screen->m_draw.show_cursors(false);
}

GameResult::~GameResult()
{
	m_screen->journal << ReplayEvent::make_end(m_screen->m_game_time);
}

void GameResult::draw() const
{
	IGamePhase::draw();

	size_t left_frame = static_cast<size_t>(banner_left->frame);
	m_screen->m_context.drawGfx(banner_left->loc, Gfx::BANNER, left_frame);

	size_t right_frame = static_cast<size_t>(banner_right->frame);
	m_screen->m_context.drawGfx(banner_right->loc, Gfx::BANNER, right_frame);
}

void GameResult::update()
{
	// this is only needed to display the replay correctly
	m_screen->m_game_time++;
}


GameScreen::GameScreen(const char* replay_infile, const char* replay_outfile, IContext& context)
: m_pause(false),
  input_mixer(*this, replay_infile),
  replay_outstream(replay_outfile),
  journal(replay_outstream),
  m_context(context),
  m_draw(context),
  m_sound_effects(context)
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
	BonusIndicator& left_bonus = *builder.left_bonus;

	Pit& right_pit = *builder.right_pit;
	Cursor& right_cursor = *builder.right_cursor;
	BonusIndicator& right_bonus = *builder.right_bonus;

	auto left_pobjs = std::make_unique<PlayerObjects>(rndgen, left_pit, left_cursor, right_pit, left_bonus);
	auto right_pobjs = std::make_unique<PlayerObjects>(rndgen, right_pit, right_cursor, left_pit, right_bonus);
	m_pobjects.push_back(std::move(left_pobjs));
	m_pobjects.push_back(std::move(right_pobjs));
	m_draw.add_pit(left_pit, left_cursor, left_bonus);
	m_draw.add_pit(right_pit, right_cursor, right_bonus);

	for(auto& pobjs : m_pobjects)
		pobjs->event_hub.append(m_sound_effects);
}

void GameScreen::draw(float dt) const
{
	m_draw.set_dt(dt);
	m_context.drawGfx(Point{0,0}, Gfx::BACKGROUND);
	m_game_phase->draw();
}

void GameScreen::update()
{
	if(!m_pause)
		update_impl();

	// auto-move cursor when scrolling out of bounds
	for(auto& pobjs : m_pobjects)
		pobjs->cursor_director.move(Dir::NONE);
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

namespace
{

int opponent(int player)
{
	SDL_assert(0 == player || 1 == player);
	return 0 == player ? 1 : 0;
}

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
			case Block::State::SWAP: state_str = "SWAP"; break;
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

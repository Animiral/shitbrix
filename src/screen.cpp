#include "screen.hpp"
#include <sstream>

namespace
{

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
		m_screen->set_phase(std::move(phase));
	}
}


GamePlay::GamePlay(GameScreen* screen) : IGamePhase(screen)
{
	m_screen->journal << ReplayEvent::make_start();
}

void GamePlay::update()
{
	m_screen->left_blocks->update(m_screen->m_context);
	m_screen->right_blocks->update(m_screen->m_context);
	m_screen->stage->update(m_screen->m_context);

	// // debug: spawn some garbage
	// if(m_screen->m_game_time % 400 == 0) {
	// 	m_screen->left_blocks->debug_spawn_garbage(3, 1);
	// 	m_screen->right_blocks->debug_spawn_garbage(6, 2);
	// }

	bool left_over = m_screen->left_blocks->over();
	bool right_over = m_screen->right_blocks->over();
	if(left_over || right_over) {
		int winner = left_over ? 1 : 0;
		auto phase = std::make_unique<GameResult>(m_screen, winner);
		m_screen->set_phase(std::move(phase));
	}

	m_screen->m_game_time++;
}

void GamePlay::input(GameInput ginput)
{
	m_screen->journal << ReplayEvent::make_input(m_screen->m_game_time, ginput);

	switch(ginput.button) {
		case GameButton::LEFT:
		case GameButton::RIGHT:
		case GameButton::UP:
		case GameButton::DOWN:
			{
				Dir dir = static_cast<Dir>(ginput.button);

				if(0 == ginput.player)
					m_screen->left_cursor->move(dir);
				else if(1 == ginput.player)
					m_screen->right_cursor->move(dir);
			}

			break;

		case GameButton::SWAP:
		case GameButton::RAISE: // TODO: implement raise
			if(0 == ginput.player)
				m_screen->left_blocks->swap(m_screen->left_cursor->rc());
			else if(1 == ginput.player)
				m_screen->right_blocks->swap(m_screen->right_cursor->rc());

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
  m_draw(context)
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

	set_phase(std::make_unique<GameIntro>(this));
	m_game_time = 0L;
	m_done = false;

	auto builder = StageBuilder();
	stage = builder.construct();

	left_blocks = std::make_unique<BlockDirector>(*builder.left_pit, rndgen);
	right_blocks = std::make_unique<BlockDirector>(*builder.right_pit, rndgen);
	left_cursor = std::make_unique<CursorDirector>(*builder.left_pit, *builder.left_cursor);
	right_cursor = std::make_unique<CursorDirector>(*builder.right_pit, *builder.right_cursor);

	m_draw.add_pit(*builder.left_pit, *builder.left_cursor);
	m_draw.add_pit(*builder.right_pit, *builder.right_cursor);
}

void GameScreen::draw(float dt) const
{
	m_draw.set_dt(dt);
	m_context.drawGfx(Point{0,0}, Gfx::BACKGROUND);
	game_phase->draw();
}

void GameScreen::update()
{
	if(!m_pause)
		update_impl();

	// auto-move cursor when scrolling out of bounds
	left_cursor->move(Dir::NONE);
	right_cursor->move(Dir::NONE);
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
			debug_print_pit(stage->pits()[0]->pit);
			break;

		case Button::DEBUG5:
			debug_print_pit(stage->pits()[1]->pit);
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

void GameScreen::set_phase(GamePhase phase)
{
	game_phase = std::move(phase);
	input_mixer.set_game_sink(game_phase.get());
}

void GameScreen::update_impl()
{
	input_mixer.update(m_game_time);
	game_phase->update();
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

void debug_print_pit(const Pit& pit)
{
	std::cerr << "--- Pit blocks:\n\n";

	for(int r = pit.top(); r <= pit.bottom()+1; r++)
	for(int c = 0; c <= PIT_COLS; c++) {
		Block block = pit.block_at(RowCol{r,c});
		if(!block) continue;

		BlockState state = block->state();
		BlockCol color = block->col;
		std::string state_str;
		std::string color_str;

		switch(state) {
			case BlockState::INVALID: state_str = "INVALID"; break;
			case BlockState::PREVIEW: state_str = "PREVIEW"; break;
			case BlockState::REST: state_str = "REST"; break;
			case BlockState::SWAP: state_str = "SWAP"; break;
			case BlockState::FALL: state_str = "FALL"; break;
			case BlockState::LAND: state_str = "LAND"; break;
			case BlockState::BREAK: state_str = "BREAK"; break;
			case BlockState::DEAD: state_str = "DEAD"; break;
			default: ;
		}

		switch(color) {
			case BlockCol::BLUE: color_str = "blue"; break;
			case BlockCol::RED: color_str = "red"; break;
			case BlockCol::YELLOW: color_str = "yellow"; break;
			case BlockCol::GREEN: color_str = "green"; break;
			case BlockCol::PURPLE: color_str = "purple"; break;
			case BlockCol::ORANGE: color_str = "orange"; break;
			case BlockCol::FAKE: color_str = "fake"; break;
			default: ;
		}

		std::cerr << "r" << r << "c" << c << " " << state_str << " " << color_str << " block\n";
	}

	std::cerr << "\n";
}

}

#include "screen.hpp"
#include <sstream>

IGamePhase::~IGamePhase() =default;

void IGamePhase::draw(IContext& context, float dt)
{
	m_screen->stage->draw(context, dt);
}


void GameIntro::draw(IContext& context, float dt)
{
	float fadeness = ((INTRO_TIME - countdown + 1.f) / INTRO_TIME);
	context.fade(fadeness);
	m_screen->stage->draw(context, dt);
}

void GameIntro::update(IContext& context)
{
	if(0 == --countdown) {
		auto phase = std::make_unique<GamePlay>(m_screen);
		m_screen->set_phase(std::move(phase));
	}
}


GamePlay::GamePlay(GameScreen* screen) : IGamePhase(screen), tick(0)
{
	m_screen->journal << ReplayEvent::make_start();
}

GamePlay::~GamePlay()
{
	m_screen->journal << ReplayEvent::make_end(tick);
}

void GamePlay::update(IContext& context)
{
	m_screen->left_blocks->update(context);
	m_screen->right_blocks->update(context);
	m_screen->stage->update(context);

	if(m_screen->done()) {
		int winner = -1;

		if(m_screen->left_blocks->over())
			winner = 1;
		else
		if(m_screen->right_blocks->over())
			winner = 0;

		auto phase = std::make_unique<GameResult>(m_screen, winner);
		m_screen->set_phase(std::move(phase));
	}

	tick++;
}

void GamePlay::input(GameInput ginput)
{
	m_screen->journal << ReplayEvent::make_input(tick, ginput);

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

	if(0 == winner) {
		m_screen->add_banner(LPIT_LOC, BannerFrame::WIN);
		m_screen->add_banner(RPIT_LOC, BannerFrame::LOSE);
	}
	else {
		m_screen->add_banner(LPIT_LOC, BannerFrame::LOSE);
		m_screen->add_banner(RPIT_LOC, BannerFrame::WIN);
	}

	m_screen->stage->remove(m_screen->left_cursor->cursor());
	m_screen->stage->remove(m_screen->right_cursor->cursor());
}

GameScreen::GameScreen()
: replay_file(LAST_REPLAY_FILE),
  journal(replay_file)
{
	reset();
}

GameScreen& GameScreen::operator=(GameScreen&& rhs)
{
	game_phase = std::move(rhs.game_phase);
	game_phase->set_screen(this);
	stage = std::move(rhs.stage);
	left_blocks = std::move(rhs.left_blocks);
	right_blocks = std::move(rhs.right_blocks);
	left_cursor = std::move(rhs.left_cursor);
	right_cursor = std::move(rhs.right_cursor);
	lpit_view = std::move(rhs.lpit_view);
	rpit_view = std::move(rhs.rpit_view);
	banner_left = std::move(rhs.banner_left);
	banner_right = std::move(rhs.banner_right);
	return *this;
}

void GameScreen::reset()
{
	set_phase(std::make_unique<GameIntro>(this));

	std::random_device rdev;
	seed(rdev());
	m_done = false;

	auto builder = StageBuilder();
	stage = builder.construct();

	left_blocks = std::make_unique<BlockDirector>(stage, builder.left_pit, rndgen);
	right_blocks = std::make_unique<BlockDirector>(stage, builder.right_pit, rndgen);
	left_cursor = std::make_unique<CursorDirector>(builder.left_pit, builder.left_cursor);
	right_cursor = std::make_unique<CursorDirector>(builder.right_pit, builder.right_cursor);

	lpit_view = std::make_shared<PitViewImpl>(builder.left_pit);
	rpit_view = std::make_shared<PitViewImpl>(builder.right_pit);

	stage->add(lpit_view);
	stage->add(rpit_view);
}

void GameScreen::animate()
{
	stage->animate();
}

void GameScreen::update(IContext& context)
{
	game_phase->update(context);

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
			GameInput ginput;
			ginput.player = cinput.device; // TODO: properly map dev to player
			ginput.button = static_cast<GameButton>(cinput.button);
			game_phase->input(ginput);
			break;

		case Button::PAUSE:
			// TODO: pause game
			break;

		case Button::RESET:
			reset();
			break;

		case Button::QUIT:
			m_done = true;
			break;

		case Button::DEBUG1:
			lpit_view->toggle();
			break;

		case Button::DEBUG2:
			rpit_view->toggle();
			break;

		case Button::NONE:
		default:
			SDL_assert_paranoid(false);

	}
}

void GameScreen::set_phase(GamePhase phase)
{
	game_phase = std::move(phase);
}

void GameScreen::add_banner(Point pit_loc, BannerFrame frame)
{
	pit_loc.x += (PIT_W-BANNER_W)/2;
	pit_loc.y += (PIT_H-BANNER_H)/2;

	Banner banner = std::make_shared<BannerImpl>(pit_loc, frame);
	stage->add(banner);
}

void GameScreen::seed(unsigned int rng_seed)
{
	rndgen = std::make_shared<std::mt19937>(rng_seed);
	std::ostringstream stream;
	stream << rng_seed;
	journal << ReplayEvent::make_set("rng_seed", stream.str());
}
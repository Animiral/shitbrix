#include "screen.hpp"

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

void GameIntro::update()
{
	if(0 == --countdown) {
		auto phase = std::make_unique<GamePlay>(m_screen);
		m_screen->set_phase(std::move(phase));
	}
}

void GamePlay::update()
{
	m_screen->left_blocks->update();
	m_screen->right_blocks->update();
	m_screen->stage->update();

	if(m_screen->done()) {
		if(m_screen->left_blocks->over())
			m_screen->add_banner(LPIT_LOC, BannerFrame::LOSE);
		else
			m_screen->add_banner(LPIT_LOC, BannerFrame::WIN);

		if(m_screen->right_blocks->over())
			m_screen->add_banner(RPIT_LOC, BannerFrame::LOSE);
		else
			m_screen->add_banner(RPIT_LOC, BannerFrame::WIN);

		m_screen->stage->remove(m_screen->left_cursor->cursor());
		m_screen->stage->remove(m_screen->right_cursor->cursor());

		auto phase = std::make_unique<GameResult>(m_screen);
		m_screen->set_phase(std::move(phase));
	}
}

void GamePlay::input_dir(Dir dir, int player)
{
	if(0 == player)
		m_screen->left_cursor->move(dir);
	else if(1 == player)
		m_screen->right_cursor->move(dir);
}

void GamePlay::input_a(int player)
{
	if(0 == player)
		m_screen->left_blocks->swap(m_screen->left_cursor->rc());
	else if(1 == player)
		m_screen->right_blocks->swap(m_screen->right_cursor->rc());
}

void GamePlay::input_b(int player)
{
	input_a(player);
}


GameScreen::GameScreen()
: rdev(), game_phase(std::make_unique<GameIntro>(this))
{
	rndgen = std::make_shared<std::mt19937>(rdev());

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

void GameScreen::animate()
{
	stage->animate();
}

void GameScreen::input_debug(int func)
{
	if(0 == func)
		lpit_view->toggle();
	else if(1 == func)
		rpit_view->toggle();
}

void GameScreen::add_banner(Point pit_loc, BannerFrame frame)
{
	pit_loc.x += (PIT_W-BANNER_W)/2;
	pit_loc.y += (PIT_H-BANNER_H)/2;

	Banner banner = std::make_shared<BannerImpl>(pit_loc, frame);
	stage->add(banner);
}

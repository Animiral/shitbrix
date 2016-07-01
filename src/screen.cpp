#include "screen.hpp"

GameScreen::GameScreen()
: game_phase(GamePhase::PLAY)
{
	auto builder = StageBuilder();
	stage = builder.construct();

	left_blocks = std::make_unique<BlockDirector>(stage, builder.left_pit);
	right_blocks = std::make_unique<BlockDirector>(stage, builder.right_pit);
	left_cursor = std::make_unique<CursorDirector>(builder.left_pit, builder.left_cursor);
	right_cursor = std::make_unique<CursorDirector>(builder.right_pit, builder.right_cursor);

	lpit_view = std::make_shared<PitViewImpl>(builder.left_pit);
	rpit_view = std::make_shared<PitViewImpl>(builder.right_pit);

	stage->add(lpit_view);
	stage->add(rpit_view);
}

void GameScreen::draw(IVideoContext& context, float dt)
{
	stage->draw(context, dt);
}

void GameScreen::animate()
{
	stage->animate();
}

void GameScreen::update()
{
	if(GamePhase::PLAY == game_phase && done()) {
		game_phase = GamePhase::RESULT;

		Point left_loc { LPIT_LOC.x + (PIT_W-BANNER_W)/2, LPIT_LOC.y + (PIT_H-BANNER_H)/2 };
		Point right_loc { RPIT_LOC.x + (PIT_W-BANNER_W)/2, RPIT_LOC.y + (PIT_H-BANNER_H)/2 };

		if(left_blocks->over())
			banner_left = std::make_shared<BannerImpl>(left_loc, BannerFrame::LOSE);
		else
			banner_left = std::make_shared<BannerImpl>(left_loc, BannerFrame::WIN);

		if(right_blocks->over())
			banner_right = std::make_shared<BannerImpl>(right_loc, BannerFrame::LOSE);
		else
			banner_right = std::make_shared<BannerImpl>(right_loc, BannerFrame::WIN);

		stage->add(banner_left);
		stage->add(banner_right);
	}

	if(GamePhase::PLAY == game_phase) {
		left_blocks->update();
		right_blocks->update();
		stage->update();
	}
}

void GameScreen::input_dir(Dir dir, int player)
{
	if(0 == player)
		left_cursor->move(dir);
	else if(1 == player)
		right_cursor->move(dir);
}

void GameScreen::input_a(int player)
{
	if(0 == player)
		left_blocks->swap(left_cursor->rc());
	else if(1 == player)
		right_blocks->swap(right_cursor->rc());
}

void GameScreen::input_b(int player)
{
	if(0 == player)
		left_blocks->swap(left_cursor->rc());
	else if(1 == player)
		right_blocks->swap(right_cursor->rc());
}

void GameScreen::input_debug(int func)
{
	if(0 == func)
		lpit_view->toggle();
	else if(1 == func)
		rpit_view->toggle();
}

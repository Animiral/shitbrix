#include "screen.hpp"

GameScreen::GameScreen()
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
	left_blocks->update();
	right_blocks->update();
	stage->update();
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

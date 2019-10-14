/**
 * Tests for game presentation.
 */

#include "stage.hpp"
#include "draw.hpp"
#include "error.hpp"
#include "tests_common.hpp"
#include <numeric>

using testing::_;
using testing::AtLeast;

class StageTest : public ::testing::Test
{

public:

	explicit StageTest()
	{
		GameMeta meta{ 2,0 };
		state = std::make_unique<GameState>(meta);
		assets = std::make_unique<NoAssets>();
		draw = std::make_unique<NoDraw>();
		stage = std::make_unique<Stage>(*state, *draw);
		indicator = &stage->sobs().at(0).bonus;
	}

protected:

	std::unique_ptr<GameState> state;
	std::unique_ptr<Assets> assets;
	std::unique_ptr<IDraw> draw;
	std::unique_ptr<Stage> stage;
	BonusIndicator* indicator;

};

/**
 * Tests that the bonus indicator displays the values set.
 */
TEST_F(StageTest, IndicatorValues)
{
	indicator->display_combo(5);
	indicator->display_chain(3);

	int combo;
	uint8_t combo_fade;
	int chain;
	uint8_t chain_fade;
	indicator->get_indication(combo, combo_fade, chain, chain_fade);

	EXPECT_EQ(5, combo);
	EXPECT_EQ(255, combo_fade);
	EXPECT_EQ(3, chain);
	EXPECT_EQ(255, chain_fade);
}

/**
 * Tests that the sprite particles move correctly when updating.
 * This includes the effect of gravity.
 */
TEST_F(StageTest, SpriteParticleMove)
{
	// x, y, orientation, xspeed, yspeed, turn, gravity, ttl
	SpriteParticle particle{ { 50.f, 60.f }, 1.f, -5.f, -2.f, .1f, .2f, 10, Gfx::PARTICLE };

	particle.update();
	EXPECT_FLOAT_EQ(45.f, particle.p().x);
	EXPECT_FLOAT_EQ(58.f, particle.p().y);
	EXPECT_FLOAT_EQ(1.1f, particle.orientation());
	EXPECT_EQ(9, particle.ttl());

	particle.update();
	EXPECT_FLOAT_EQ(40.f, particle.p().x);
	EXPECT_FLOAT_EQ(56.2f, particle.p().y);
	EXPECT_FLOAT_EQ(1.2f, particle.orientation());
	EXPECT_EQ(8, particle.ttl());
}


/**
 * Tests that the trail particles move correctly when updating.
 * This includes the effect of gravity.
 */
TEST_F(StageTest, TrailParticleMove)
{
	ASSERT_LE(2, TrailParticle::TRAIL_MAXLEN); // this test only works with long trail

	std::array<wrap::Color, TrailParticle::TRAIL_MAXLEN> palette;
	palette.fill(wrap::WHITE);
	// x, y, orientation, xspeed, yspeed, turn, gravity, ttl
	TrailParticle particle{ { 50.f, 60.f }, 1.f, -5.f, -2.f, .1f, .2f, 10, palette };

	EXPECT_EQ(0, particle.length());

	particle.update();
	EXPECT_FLOAT_EQ(45.f, particle.p().x);
	EXPECT_FLOAT_EQ(58.f, particle.p().y);
	EXPECT_FLOAT_EQ(1.1f, particle.orientation());
	EXPECT_EQ(9, particle.ttl());
	EXPECT_EQ(1, particle.length());

	particle.update();
	EXPECT_FLOAT_EQ(40.f, particle.p().x);
	EXPECT_FLOAT_EQ(56.2f, particle.p().y);
	EXPECT_FLOAT_EQ(1.2f, particle.orientation());
	EXPECT_EQ(8, particle.ttl());
	EXPECT_EQ(2, particle.length());
}

/**
 * Tests particle generator.
 * The test passes if the expected amount of draw calls result from the generator.
 */
TEST_F(StageTest, ParticleGenerator)
{
	MockDraw draw;
	Point p{ 50.f, 50.f };
	// float orientation, float xspeed, float yspeed, float turn, // randomized
	// float gravity, int ttl, Gfx gfx // fixed
	float intensity = 1.f; // influences speed, gravity and ttl
	int density = 2; // number of particles spawned per trigger
	ParticleGenerator generator(p, density, intensity, draw);
	generator.trigger(); // spawn more particles

	EXPECT_CALL(draw, gfx_rotate(_, _, _, Gfx::PARTICLE, 0, 255)).Times(density);
	generator.draw(0.f); // get particles on the screen

	generator.trigger(); // spawn more particles
	generator.update(); // run logic for dependent particles

	EXPECT_CALL(draw, gfx_rotate(_, _, _, Gfx::PARTICLE, _, 255)).Times(2 * density);
	generator.draw(0.f); // get particles on the screen
}

/**
 * The test passes if the generated particles in a realistic scenario are on
 * average drawn close to the position that it is supposed to indicate.
 */
TEST_F(StageTest, PanicIndicatorPosition)
{
	MockDraw draw;
	const Point pit_loc = LPIT_LOC;

	std::vector<Point> draw_targets; // points at which we have observed particles being drawn
	const auto record_draw = [&draw_targets](const int x, const int y, double, Gfx, size_t, uint8_t) noexcept
	{
		draw_targets.push_back(Point{ (float)x, (float)y });
	};

	float panic = .9f; // panic between 0.0 and 1.0
	PanicIndicator indicator{pit_loc, draw};
	EXPECT_CALL(draw, gfx_rotate(_, _, _, Gfx::PARTICLE, _, _)).Times(AtLeast(1)).WillRepeatedly(record_draw);

	while(panic > .5f) {
		panic -= .05f;
		indicator.set_panic(panic);
		indicator.update();
		indicator.draw(0.f);
	}

	// find the average coordinate
	const size_t samples = 5; // look at the last N particles
	std::reverse(draw_targets.begin(), draw_targets.end());
	if(samples < draw_targets.size())
		draw_targets.resize(samples); // discard older samples

	const auto add_points = [](const Point a, const Point b) noexcept
	{
		return Point{ a.x + b.x, a.y + b.y };
	};
	const Point sum = std::reduce(draw_targets.begin(), draw_targets.end(), Point{ 0.f, 0.f }, add_points);
	const Point average{ sum.x / draw_targets.size(), sum.y / draw_targets.size() };
	const Point expected{ pit_loc.x + PIT_W / 2, pit_loc.y };
	const float threshold = 80.f; // player will recognize indication if within 80px of avg

	EXPECT_LE(std::abs(average.x - expected.x), threshold);
	EXPECT_LE(std::abs(average.y - expected.y), threshold);
}

/**
 * Tests that the PanicIndicator does not draw anything if there is no panic condition.
 */
TEST_F(StageTest, PanicIndicatorNoPanic)
{
	MockDraw draw;
	const Point pit_loc = LPIT_LOC;

	PanicIndicator indicator{ pit_loc, draw };
	EXPECT_CALL(draw, gfx_rotate(_, _, _, Gfx::PARTICLE, _, _)).Times(0);

	indicator.set_panic(1.f); // no panic, 100% of time left -> no draw
	indicator.update();
	indicator.draw(0.f);
}

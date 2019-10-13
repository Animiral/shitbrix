/**
 * Tests for game presentation.
 */

#include "stage.hpp"
#include "draw.hpp"
#include "error.hpp"
#include "tests_common.hpp"

using testing::_;

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
	ASSERT_LE(2, TRAIL_PARTICLE_MAXLEN); // this test only works with long trail

	std::array<wrap::Color, TRAIL_PARTICLE_MAXLEN> palette;
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

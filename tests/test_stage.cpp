/**
 * Tests for game presentation.
 */

#include "stage.hpp"
#include "error.hpp"
#include "tests_common.hpp"
#include "gtest/gtest.h"

class StageTest : public ::testing::Test
{

protected:

	virtual void SetUp()
	{
		configure_context_for_testing();

		GameMeta meta{2,0};
		state = std::make_unique<GameState>(meta);
		stage.reset(new Stage(*state));
		indicator = &stage->sobs().at(0).bonus;
	}

	// virtual void TearDown() {}

	std::unique_ptr<GameState> state;
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

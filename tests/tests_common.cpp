/**
 * Shared helper functions for unit tests.
 */

#include "tests_common.hpp"

std::vector<Block::Color> make_loot(size_t amount)
{
	std::vector<Block::Color> loot;
	while(loot.size() < amount) {
		loot.insert(loot.end(), {Block::Color::BLUE, Block::Color::RED, Block::Color::YELLOW, Block::Color::GREEN, Block::Color::PURPLE, Block::Color::ORANGE});
	}
	loot.resize(amount);
	return loot;
}

/**
 * Implementation of game logic functions.
 */

#include "logic.hpp"
#include <SDL2/SDL_assert.h>

void MatchBuilder::ignite(Block& block)
{
	Block::Color color = block.col;
	int row = block.rc().r;
	int col = block.rc().c;

	// extents of match
	int left = col;
	int right = col;
	int top = row;
	int bottom = row;

	for(left = col-1; left >= 0 && match_at({row,left}, color); left--);
	for(right = col+1; left < PIT_COLS && match_at({row,right}, color); right++);
	for(top = row-1; top >= pit.top() && match_at({top,col}, color); top--);
	for(bottom = row+1; top <= pit.bottom() && match_at({bottom,col}, color); bottom++);

	// horizontal match >= 3 blocks
	if(right-left-1 >= 3) {
		for(int c = left+1; c < right; c++)
			insert({row,c});
	}

	// vertical match
	if(bottom-top-1 >= 3) {
		for(int r = top+1; r < bottom; r++)
			insert({r,col});
	}
}

void MatchBuilder::find_touch_garbage()
{
	auto insert_if_garbage_at = [this] (RowCol rc)
	{
		Garbage* garbage = pit.garbage_at(rc);
		if(garbage)
			m_touched_garbage.insert(*garbage);
	};

	for(Block& block : m_result) {
		RowCol rc = block.rc();
		insert_if_garbage_at(RowCol{rc.r-1, rc.c});
		insert_if_garbage_at(RowCol{rc.r+1, rc.c});
		insert_if_garbage_at(RowCol{rc.r, rc.c-1});
		insert_if_garbage_at(RowCol{rc.r, rc.c+1});
	}
}

bool MatchBuilder::match_at(RowCol rc, Block::Color color)
{
	Block* next = pit.block_at(rc);
	return next && next->col == color && next->is_matchable();
}

void MatchBuilder::insert(RowCol rc)
{
	Block* match_block = pit.block_at(rc);
	game_assert(match_block, "MatchBuilder: expected block not present");
	m_result.insert(*match_block);
	m_chaining |= match_block->chaining;
}


void Logic::throw_garbage(int columns, int rows, Loot loot, bool right_side) const
{
	SDL_assert(columns > 0 && columns <= PIT_COLS);

	int spawn_row = std::min(m_pit.peak(), m_pit.top()) - rows - 1;
	RowCol rc{spawn_row, right_side ? PIT_COLS-columns : 0};
	Garbage& garbage = m_pit.spawn_garbage(rc, columns, rows, move(loot));
	garbage.set_state(Physical::State::FALL, ROW_HEIGHT, FALL_SPEED);
}

void Logic::trigger_falls(RowCol rc, PhysicalRefVec& fallers, bool chaining) const
{
	Physical* physical = m_pit.at(rc);

	if(!physical ||
	   !physical->is_fallible() ||
	   Physical::State::DEAD == physical->physical_state())
		return;

	if(Block* block = dynamic_cast<Block*>(physical))
		block->chaining |= chaining;

	fallers.emplace_back(*physical);

	rc = physical->rc();
	for(int c = rc.c; c < rc.c + physical->columns(); c++) {
		trigger_falls(RowCol{rc.r - 1, c}, fallers, chaining);
	}
}

void Logic::examine_pit(bool& chaining, bool& breaking, bool& full) const noexcept
{
	for(const auto& ptr : m_pit.contents()) {
		if(Block* b = dynamic_cast<Block*>(ptr.get())) {
			chaining |= b->chaining;
		}

		breaking |= ptr->physical_state() == Physical::State::BREAK;
	}

	full = m_pit.is_full();
}

void Logic::examine_finish(GarbageRefVec& dissolvers, PhysicalRefVec& fallers, bool& dead_physical,
                           bool& dead_block, bool& dead_sound, bool& chainstop) const
{
	for(auto& physical : m_pit.contents())
	{
		Physical::State state = physical->physical_state();
		bool is_arriving = physical->is_arriving();

		if(Physical::State::FALL == state && is_arriving) {
			// can never fall lower than the preview row of blocks
			game_assert(physical->rc().r + physical->rows() - 1 <= m_pit.bottom(), "Object falls too low");

			// Re-enter the object as a candidate for falling and hots.
			// Since falling blocks are automatically excluded from hots,
			// this only takes effect with blocks that actually land.
			fallers.emplace_back(*physical);
			if(Block* block = dynamic_cast<Block*>(&*physical))
				block->set_tag(Physical::TAG_HOT);
		}

		// Garbage-specifics
		if(Garbage* garbage = dynamic_cast<Garbage*>(&*physical)) {
			// shrink garbage if necessary
			if(Physical::State::BREAK == garbage->physical_state() && is_arriving) {
				dissolvers.emplace_back(*garbage);

				if(garbage->rows() <= 1) {
					RowCol rc = garbage->rc();
					rc.r--;
					for(int c = rc.c; c < rc.c + garbage->columns(); c++) {
						trigger_falls({rc.r, c}, fallers, true);
					}
				}
			}
		}

		// Block-specifics
		if(Block* block = dynamic_cast<Block*>(&*physical)) {
			Block::State state = block->block_state();
			bool above_fall = false; // whether objects above this one might fall
			bool chaining = false; // whether objects above chain when they fall

			// blocks finished swapping
			if((Block::State::SWAP_LEFT == state || Block::State::SWAP_RIGHT == state) &&
				is_arriving) {
				// fake blocks are only for swapping and disappear right afterwards
				if(Block::Color::FAKE == block->col) {
					block->set_state(Physical::State::DEAD);
					state = block->block_state(); // NOTE: remember changed state!
				}
				else {
					fallers.emplace_back(*block);
					block->set_tag(Physical::TAG_HOT);

					above_fall = true;
				}
			}

			// cleanup dead blocks, resume scrolling if there are no more BREAK blocks
			if(Block::State::DEAD == state) {
				dead_physical = true;
				dead_block = true;

				if(Block::Color::FAKE != block->col) {
					dead_sound = true;
					chaining = true; // blocks to fall from above should get the chaining flag

					// dead blocks can finish chains by being the last chaining blocks to disappear
					if(block->chaining)
						chainstop = true;
				}

				above_fall = true;
			}

			if(above_fall) {
				RowCol rc = block->rc();
				rc.r--;
				trigger_falls(rc, fallers, chaining);
			}
		}
	}
}

void Logic::convert_garbage(GarbageRefVec& dissolvers, PhysicalRefVec& fallers,
                            bool& dead_physical) const
{
	for(Garbage& garbage : dissolvers) {
		RowCol garbage_rc = garbage.rc();
		int garbage_columns = garbage.columns();
		int garbage_rows = garbage.rows();
		auto loot_it = garbage.loot();
		Loot loot(loot_it, loot_it + garbage_columns);
		bool survived = m_pit.shrink(garbage) > 0;

		for(int c = 0; c < garbage_columns; c++) {
			RowCol block_rc{garbage_rc.r + garbage_rows - 1, garbage_rc.c + c};
			Block& block = m_pit.spawn_block(loot[c], block_rc, Block::State::REST);
			block.chaining = true;
			fallers.emplace_back(block);
			block.set_tag(Physical::TAG_HOT);
		}

		if(survived) {
			// get rid of the break state, it stops the pit from scrolling
			garbage.set_state(Physical::State::REST);
			fallers.emplace_back(garbage);
		}
	}

	if(!dissolvers.empty())
		dead_physical = true;
}

void Logic::handle_fallers(PhysicalRefVec& fallers) const
{
	bool changed = true;
	auto begin = std::begin(fallers);
	auto end = std::end(fallers);

	while(changed) {
		changed = false;

		for(auto it = begin; it != end; ) {
			Physical& physical = *it;
			if(m_pit.can_fall(physical)) {
				// If the object is already falling, we do not wish to throw
				// away the slice of their time in which they already fell
				// into the next row.
				if(Physical::State::FALL == physical.physical_state()) {
					physical.continue_state(ROW_HEIGHT);
				}
				else {
					physical.set_state(Physical::State::FALL, ROW_HEIGHT, FALL_SPEED);
				}
				m_pit.fall(physical);

				// erase the element from our consideration of fallers
				std::swap(*it, *--end);

				changed = true;
			}
			else {
				++it;
			}
		}
	}

	for(auto it = begin; it != end; ++it) {
		Physical& physical = *it;
		Physical::State state = physical.physical_state();

		if(Physical::State::FALL == state) {
			physical.set_state(Physical::State::LAND, LAND_TIME);
			physical.set_tag(Physical::TAG_LAND);
		}
		else {
			physical.set_state(Physical::State::REST);
		}
	}

	// blocks cannot match if they are falling down!
	auto& contents = m_pit.contents();
	for(auto it = std::begin(contents), e = std::end(contents); it != e; ++it) {
		Physical& physical = **it;

		if(Physical::State::FALL == physical.physical_state())
			physical.un_tag(Physical::TAG_HOT);
	}
}

void Logic::handle_hots(bool& have_match, int& combo, bool& chaining, bool& chainstop) const
{
	MatchBuilder builder(m_pit);

	m_pit.for_all<Block>(Physical::TAG_HOT, [&builder](Block& block) {
		builder.ignite(block);
	});

	auto& breaks = builder.result();
	combo = builder.combo();
	chaining = builder.chaining();

	if(!breaks.empty()) {
		have_match = true;
		m_pit.stop();

		for(Block& breaking : breaks)
			breaking.set_state(Physical::State::BREAK, BREAK_TIME);
	}

	// There is only 1 chance per block to make a chain
	m_pit.for_all<Block>(Physical::TAG_HOT, [&chainstop](Block& block) {
		// Chaining blocks which come to rest can finish a chain.
		// Blocks which have now matched are still carrying the chain.
		if(block.chaining && Block::State::BREAK != block.block_state()) {
			block.chaining = false;
			chainstop = true;
		}
	});

	builder.find_touch_garbage();

	for(auto& garbage : builder.touched_garbage()) {
		garbage.get().set_state(Physical::State::BREAK, DISSOLVE_TIME);
	}
}

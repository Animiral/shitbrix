#include "logic.hpp"
#include "error.hpp"
#include <cassert>

namespace
{

/**
 * Run the given function on every neighbor of type P to the Physical in the Pit.
 * The function will be called multiple times per neighbor if there are multiple
 * points of contact of the Physical with the neighbor.
 */
template<typename P = Physical, typename Func>
void for_neighbors(Pit& pit, const Physical& phys, Func func);

}

void MatchBuilder::ignite(Block& block)
{
	Color color = block.col;
	int row = block.rc().r;
	int col = block.rc().c;

	// extents of match
	int left = col;
	int right = col;
	int top = row;
	int bottom = row;

	for(left = col-1; left >= 0 && match_at({row,left}, color); left--);
	for(right = col+1; right < PIT_COLS && match_at({row,right}, color); right++);
	for(top = row-1; top >= pit.top() && match_at({top,col}, color); top--);
	for(bottom = row+1; bottom <= pit.bottom() && match_at({bottom,col}, color); bottom++);

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

bool MatchBuilder::match_at(RowCol rc, Color color)
{
	Block* next = pit.block_at(rc);
	return next && next->col == color && next->is_matchable();
}

void MatchBuilder::insert(RowCol rc)
{
	Block* match_block = pit.block_at(rc);

	if(!match_block)
		throwx<LogicException>("MatchBuilder: expected block not present at %dr %dc.", rc.r, rc.c);

	m_result.insert(*match_block);
	m_chaining |= match_block->chaining;
}


void Logic::trigger_falls(RowCol rc, bool chaining) const
{
	Physical* physical = m_pit.at(rc);

	if(!physical ||
	   !physical->is_fallible() ||
	   Physical::State::DEAD == physical->physical_state())
		return;

	// If this is part of a chaining move, we have to set the chaining flag on
	// the block *now* before we forget what the reason for the falling was.
	// If the block does not end up really falling after all, re-evaluate.
	if(Block* block = dynamic_cast<Block*>(physical))
		block->chaining |= chaining;

	physical->set_tag(Physical::TAG_FALL);

	rc = physical->rc();
	for(int c = rc.c; c < rc.c + physical->columns(); c++) {
		trigger_falls(RowCol{rc.r - 1, c}, chaining);
	}
}

void Logic::examine_pit(bool& chaining, bool& breaking, bool& full, bool& starving) const noexcept
{
	for(const auto& ptr : m_pit.contents()) {
		if(Block* b = dynamic_cast<Block*>(ptr.get())) {
			chaining |= b->chaining;
		}

		breaking |= ptr->physical_state() == Physical::State::BREAK;
	}

	full = m_pit.is_full();
	starving = !m_pit.at({m_pit.bottom() + 1, 0}); // check one slot is enough
}

void Logic::examine_finish(bool& dead_physical, bool& dead_block, bool& dead_sound,
                           bool& chainstop, bool& new_row) const
{
	for(auto& physical : m_pit.contents())
	{
		Physical::State state = physical->physical_state();
		bool is_arriving = physical->is_arriving();

		if(Physical::State::FALL == state && is_arriving) {
			// can never fall lower than the preview row of blocks
			if(physical->rc().r + physical->rows() - 1 > m_pit.bottom())
				throwx<LogicException>("Object falls too low. r=%d, rows=%d, bottom=%d", physical->rc().r, physical->rows(), m_pit.bottom());

			// Re-enter the object as a candidate for falling and hots.
			// Since falling blocks are automatically excluded from hots,
			// this only takes effect with blocks that actually land.
			physical->set_tag(Physical::TAG_FALL);
			if(Block* block = dynamic_cast<Block*>(&*physical))
				block->set_tag(Physical::TAG_HOT);
		}

		// Garbage-specifics
		if(Garbage* garbage = dynamic_cast<Garbage*>(&*physical)) {
			// shrink garbage if necessary
			if(Physical::State::BREAK == garbage->physical_state() && is_arriving) {
				garbage->set_tag(Physical::TAG_DISSOLVE);
			}
		}

		// Block-specifics
		if(Block* block = dynamic_cast<Block*>(&*physical)) {
			Block::State state = block->block_state();
			bool above_fall = false; // whether objects above this one might fall
			bool chaining = false; // whether objects above chain when they fall

			// new blocks become active
			if(Block::State::PREVIEW == state && m_pit.bottom() == block->rc().r) {
				block->set_state(Block::State::REST);
				block->set_tag(Physical::Tag::TAG_HOT);
				new_row = true;
			}

			// blocks finished swapping
			if((Block::State::SWAP_LEFT == state || Block::State::SWAP_RIGHT == state) &&
				is_arriving) {
				// fake blocks are only for swapping and disappear right afterwards
				if(Color::FAKE == block->col) {
					block->set_state(Physical::State::DEAD);
					state = block->block_state(); // NOTE: remember changed state!
				}
				else {
					block->set_tag(Physical::TAG_FALL);
					block->set_tag(Physical::TAG_HOT);

					above_fall = true;
				}
			}

			// cleanup dead blocks, resume scrolling if there are no more BREAK blocks
			if(Block::State::DEAD == state) {
				dead_physical = true;
				dead_block = true;

				if(Color::FAKE != block->col) {
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
				trigger_falls(rc, chaining);
			}
		}

		// logic sanity check: dead blocks must not be falling
		assert(Physical::State::DEAD != physical->physical_state() || !physical->has_tag(Physical::TAG_FALL));
	}
}

void Logic::convert_garbage() const
{
	std::vector<std::reference_wrapper<Garbage>> converts;

	// be careful not to iterate at the same time as we change contents
	m_pit.for_all<Garbage>(Physical::TAG_DISSOLVE, [&converts](Garbage& garbage) {
		converts.emplace_back(garbage);
	});

	for(Garbage& garbage : converts) {
		RowCol garbage_rc = garbage.rc();
		int garbage_columns = garbage.columns();
		int garbage_rows = garbage.rows();
		auto loot_it = garbage.loot();
		Loot loot(loot_it, loot_it + garbage_columns);
		bool survived = m_pit.shrink(garbage) > 0;

		for(int c = 0; c < garbage_columns; c++) {
			// extract loot into bottom row of garbage
			RowCol block_rc{garbage_rc.r + garbage_rows - 1, garbage_rc.c + c};
			Block& block = m_pit.spawn_block(loot[c], block_rc, Block::State::REST);
			block.chaining = true;
			block.set_tag(Physical::TAG_FALL);
			block.set_tag(Physical::TAG_HOT);

			// consider falling for everything above garbage
			trigger_falls({garbage_rc.r - 1, c}, true);
		}

		if(survived) {
			// get rid of the break state, it stops the pit from scrolling
			garbage.set_state(Physical::State::REST);
			garbage.set_tag(Physical::TAG_FALL);
		}
	}
}

void Logic::handle_fallers() const
{
	bool changed = true;

	while(changed) {
		changed = false;

		m_pit.for_all(Physical::TAG_FALL, [this, &changed](Physical& physical) {
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
				physical.un_tag(Physical::TAG_FALL);

				changed = true;
			}
		});
	}

	m_pit.for_all(Physical::TAG_FALL, [](Physical& physical) {
		Physical::State state = physical.physical_state();

		if(Physical::State::FALL == state) {
			physical.set_state(Physical::State::LAND, LAND_TIME);
			physical.set_tag(Physical::TAG_LAND);
		}
		else {
			physical.set_state(Physical::State::REST);

			// If we have a block that was only ever *potentially* falling
			// in the first place, it can not be chaining. (Bug #79)
			if(Block* block = dynamic_cast<Block*>(&physical))
				block->chaining = false;
		}
	});

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
	}

	for(Block& breaking : breaks) {
		breaking.set_state(Physical::State::BREAK, BREAK_TIME);

		// If this block touches on garbage, it will also break.
		// Garbage is first recursively identified and later broken.
		for_neighbors<Garbage>(m_pit, breaking, [this](Garbage& g) { touch_garbage(g); });
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

	// execute on the breaking of touched garbages
	m_pit.for_all<Garbage>(Physical::TAG_TOUCH, [](Garbage& garbage) {
		garbage.set_state(Physical::State::BREAK, DISSOLVE_TIME);
	});
}

void Logic::touch_garbage(Garbage& garbage) const
{
	if(!garbage.has_tag(Physical::TAG_TOUCH)) {
		garbage.set_tag(Physical::TAG_TOUCH);
		for_neighbors<Garbage>(m_pit, garbage, [this](Garbage& g) { touch_garbage(g); });
	}
};


namespace
{

template<typename P, typename Func>
void for_neighbors(Pit& pit, const Physical& phys, Func func)
{
	const auto invoke_at = [&pit, func](RowCol rc) {
		if(0 <= rc.c && PIT_COLS > rc.c) {
			P* p = dynamic_cast<P*>(pit.at(rc));
			if(p)
				func(*p);
		}
	};

	const RowCol phys_rc{phys.rc()};

	for(int r = 0; r < phys.rows(); r++) {
		invoke_at({phys_rc.r + r, phys_rc.c - 1});
		invoke_at({phys_rc.r + r, phys_rc.c + phys.columns()});
	}

	for(int c = 0; c < phys.columns(); c++) {
		invoke_at({phys_rc.r - 1, phys_rc.c + c});
		invoke_at({phys_rc.r + phys.rows(), phys_rc.c + c});
	}
}

}

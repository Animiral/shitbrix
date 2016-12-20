/**
 * Tests for the Pit interface and how it handles game objects.
 *
 * The Pit prefers to deal with Physicals instead of concrete
 * game objects where possible, and so do these tests.
 * The tests only explicitly cover the mutating Pit interface,
 * while the const interface is only implicitly tested by using
 * it to observe the actual state of the Pit.
 */

#include "stage.hpp"
#include "mock.hpp"
#include "gtest/gtest.h"

/**
 * Tests whether a Block correctly appears in the Pit on spawn.
 */
TEST(PitTest, SpawnBlock)
{
	FAIL();
}

/**
 * Tests whether an illegal Block gets rejected in spawning.
 */
TEST(PitTest, SpawnBlockOutOfBounds)
{
	FAIL();
}

/**
 * Tests whether a Garbage correctly appears in the Pit on spawn.
 */
TEST(PitTest, SpawnGarbage)
{
	FAIL();
}

/**
 * Tests whether an illegal Garbage gets rejected in spawning.
 */
TEST(PitTest, SpawnGarbageOutOfBounds)
{
	FAIL();
}

/**
 * Tests whether can_fall() correctly indicates true when space is free.
 */
TEST(PitTest, CanFallBlockYes)
{
	FAIL();
}

/**
 * Tests whether can_fall() correctly indicates false when space is blocked.
 */
TEST(PitTest, CanFallBlockNo)
{
	FAIL();
}

/**
 * Tests whether can_fall() correctly indicates true when space is free.
 */
TEST(PitTest, CanFallGarbageYes)
{
	FAIL();
}

/**
 * Tests whether can_fall() correctly indicates false when space is blocked.
 */
TEST(PitTest, CanFallGarbageNo)
{
	FAIL();
}

/**
 * Tests whether a Block correctly falls.
 */
TEST(PitTest, FallBlock)
{
	FAIL();
}

/**
 * Tests error when a Block cannot fall because the space below is blocked.
 */
TEST(PitTest, FallBlockFail)
{
	FAIL();
}

/**
 * Tests whether a Garbage correctly falls.
 */
TEST(PitTest, FallGarbage)
{
	FAIL();
}

/**
 * Tests error when a Garbage cannot fall because one space below is blocked.
 */
TEST(PitTest, FallGarbageFail)
{
	FAIL();
}

/**
 * Tests whether a Block can be removed.
 */
TEST(PitTest, KillBlock)
{
	FAIL();
}

/**
 * Tests whether a Garbage can be shrunk and still exist.
 */
TEST(PitTest, ShrinkGarbage)
{
	FAIL();
}

/**
 * Tests whether a Garbage disappears when it shrinks to 0 rows.
 */
TEST(PitTest, KillGarbage)
{
	FAIL();
}

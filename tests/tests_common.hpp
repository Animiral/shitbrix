/**
 * tests_common.hpp
 * Definitions for shared helpers for unit tests.
 */
#pragma once

#include "stage.hpp"

/**
 * Create a deterministic collection of block colors for use in garbage construction.
 */
Loot make_loot(size_t amount);

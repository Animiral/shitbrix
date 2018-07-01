#include "context.hpp"
#include "configuration.hpp"
#include "sdl_helper.hpp"
#include "error.hpp"
#include "asset.hpp"
#include "audio.hpp"
#include <cstring>

GlobalContext::~GlobalContext() = default;

GlobalContext the_context;

/**
 * sdl_helper.hpp
 * C++-friendly wrappers for SDL stuff
 */
#pragma once

#include <memory>
#include <SDL2/SDL.h>
#include <SDL2/SDL_assert.h>
#include <SDL2/SDL_image.h>

/**
 * An SDL_Texture* with an additional SDL_Rect* specifying its width and height (x,y == 0,0)
 */
struct TextRect
{
	SDL_Texture* texture;
	const SDL_Rect* rect;
};

/**
 * Custom deleter for SDL objects
 */
class SdlDeleter
{
public:
	void operator()(SDL_Surface* p) const { SDL_FreeSurface(p); }
	void operator()(SDL_Texture* p) const { SDL_DestroyTexture(p); }
	void operator()(SDL_Window* p) const { SDL_DestroyWindow(p); }
	void operator()(SDL_Renderer* p) const { SDL_DestroyRenderer(p); }
};

using Surface = std::unique_ptr<SDL_Surface, SdlDeleter>;
using Texture = std::unique_ptr<SDL_Texture, SdlDeleter>;
using Window = std::unique_ptr<SDL_Window, SdlDeleter>;
using Renderer = std::unique_ptr<SDL_Renderer, SdlDeleter>;

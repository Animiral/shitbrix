/**
 * asset.hpp
 * Functions for loading, destroying and identifying the game assets.
 */
#pragma once

#include "globals.hpp"
#include "sdl_helper.hpp"
#include <memory>
#include <vector>

/**
 * Storage class which owns all the assets.
 */
class Assets
{

public:

	Assets(Renderer& renderer) : bg_rect{0, 0, CANVAS_W, CANVAS_H}, block_rect{0,0,BLOCK_W,BLOCK_H}, cursor_rect{0,0,CURSOR_W,CURSOR_H}
	{
		Surface bg = load_surface("gfx/bg.png");
		Surface blocks = load_surface("gfx/blocks.png");
		Surface cursor = load_surface("gfx/cursor.png");

		// background (1 frame only)
		std::vector<Texture> bg_frames;
		bg_frames.emplace_back(make_texture(renderer, bg));
		textures.emplace_back(std::move(bg_frames));

		// colorful blocks
		for(int color = 0; color < 6; color++) {
			std::vector<Texture> block_frames;

			for(int frame = 0; frame < 6; frame++) {
				block_frames.emplace_back(make_frame_texture(renderer, blocks, BLOCK_W, BLOCK_H, color, frame));
			}

			textures.emplace_back(std::move(block_frames));
		}

		// player cursor
		std::vector<Texture> cursor_frames;
		for(int frame = 0; frame < 4; frame++) {
			cursor_frames.emplace_back(make_frame_texture(renderer, cursor, CURSOR_W, CURSOR_H, 0, frame));
		}
		textures.emplace_back(std::move(cursor_frames));
	}

	/**
	 * Returns a TextRect according to the gfx enum id.
	 * Callers must take care not to use the pointers from the obtained
	 * structure beyond the life time of the Assets object.
	 */
	TextRect texture(Gfx gfx, size_t frame = 0) const
	{
		TextRect tr;
		tr.texture = textures[static_cast<size_t>(gfx)][frame].get();

		switch(gfx) {
			case Gfx::BACKGROUND:
				tr.rect = &bg_rect;
				break;
			case Gfx::BLOCK_BLUE:
			case Gfx::BLOCK_RED:
			case Gfx::BLOCK_YELLOW:
			case Gfx::BLOCK_GREEN:
			case Gfx::BLOCK_PURPLE:
			case Gfx::BLOCK_ORANGE:
				tr.rect = &block_rect;
				break;
			case Gfx::CURSOR:
				tr.rect = &cursor_rect;
				break;
			default:
				SDL_assert(false);
		}

		return tr;
	}

private:

	std::vector< std::vector<Texture> > textures;
	const SDL_Rect bg_rect;
	const SDL_Rect block_rect;
	const SDL_Rect cursor_rect;

	static Surface load_surface(const char* file)
	{
		Surface surface(IMG_Load(file));
		game_assert(static_cast<bool>(surface), file);
		return surface;
	}

	static Texture make_texture(Renderer& renderer, Surface& surface)
	{
		Texture texture(SDL_CreateTextureFromSurface(renderer.get(), surface.get()));
		game_assert(static_cast<bool>(texture), SDL_GetError());
		return texture;
	}

	/**
	 * Extract the frame with the given row/column from the surface, which contains a grid/sheet of graphics.
	 */
	static Texture make_frame_texture(Renderer& renderer, Surface& surface, int width, int height, int row, int column)
	{
		// be careful to preserve alpha channel (SDL defaults to amask=0)
	    Uint32 rmask, gmask, bmask, amask;
		#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		    rmask = 0xff000000;
		    gmask = 0x00ff0000;
		    bmask = 0x0000ff00;
		    amask = 0x000000ff;
		#else
		    rmask = 0x000000ff;
		    gmask = 0x0000ff00;
		    bmask = 0x00ff0000;
		    amask = 0xff000000;
		#endif

		Surface temp_block(SDL_CreateRGBSurface(0, width, height, 32, rmask, gmask, bmask, amask));
		game_assert(static_cast<bool>(temp_block), SDL_GetError());

		SDL_Rect srcrect {column*width, row*height, width, height};
		SDL_Rect dstrect {0, 0, width, height};
		SDL_BlitSurface(surface.get(), &srcrect, temp_block.get(), &dstrect);

		Texture texture(SDL_CreateTextureFromSurface(renderer.get(), temp_block.get()));
		game_assert(static_cast<bool>(texture), SDL_GetError());

		return texture;
	}

};

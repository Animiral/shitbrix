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

	Assets(Renderer& renderer)
	: bg_rect{0, 0, CANVAS_W, CANVAS_H},
	  block_rect{0, 0, BLOCK_W, BLOCK_H},
	  cursor_rect{0, 0, CURSOR_W, CURSOR_H},
	  banner_rect{0, 0, BANNER_W, BANNER_H}
	{
		load_sheet(renderer, "gfx/bg.png", CANVAS_W, CANVAS_H, 1, 1);
		load_sheet(renderer, "gfx/blocks.png", BLOCK_W, BLOCK_H, 7, 6);
		load_sheet(renderer, "gfx/cursor.png", CURSOR_W, CURSOR_H, 1, 4);
		load_sheet(renderer, "gfx/banner.png", BANNER_W, BANNER_H, 1, 2);

		load_wav("snd/swap.wav");
		load_wav("snd/break.wav");
		load_wav("snd/match.wav");
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
			case Gfx::PITVIEW:
				tr.rect = &block_rect;
				break;
			case Gfx::CURSOR:
				tr.rect = &cursor_rect;
				break;
			case Gfx::BANNER:
				tr.rect = &banner_rect;
				break;
			default:
				SDL_assert(false);
		}

		return tr;
	}

	Sound sound(Snd snd) const
	{
		return sounds[static_cast<size_t>(snd)];
	}

private:

	std::vector< std::vector<Texture> > textures;
	std::vector< Sound > sounds;

	const SDL_Rect bg_rect;
	const SDL_Rect block_rect;
	const SDL_Rect cursor_rect;
	const SDL_Rect banner_rect;

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

	/**
	 * Loads all the gfx and frames from the given sheet and places them in the textures list.
	 */
	void load_sheet(Renderer& renderer, const char* file, int width, int height, int rows, int cols)
	{
		Surface sheet = load_surface(file);

		for(int r = 0; r < rows; r++) {
			std::vector<Texture> frames;

			for(int c = 0; c < 6; c++) {
				frames.emplace_back(make_frame_texture(renderer, sheet, width, height, r, c));
			}

			textures.emplace_back(std::move(frames));
		}

	}

	/**
	 * Loads a sound file
	 */
	void load_wav(const char* file)
	{
		sounds.emplace_back(std::make_shared<SoundImpl>(file));
	}
};

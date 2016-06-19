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

	Assets(Renderer& renderer) : bg_rect{0, 0, CANVAS_W, CANVAS_H}, block_rect{0,0,BLOCK_W,BLOCK_H}
	{
		Surface bg = load_surface("gfx/bg.png");
		Surface blocks = load_surface("gfx/blocks.png");

		std::vector<Texture> bg_frames;
		bg_frames.emplace_back(make_texture(renderer, bg));
		textures.emplace_back(std::move(bg_frames));

		for(int color = 0; color < 6; color++) {
			std::vector<Texture> block_frames;

			for(int frame = 0; frame < 6; frame++) {
				block_frames.emplace_back(make_block_texture(renderer, blocks, color, frame));
			}

			textures.emplace_back(std::move(block_frames));
		}
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
			default:
				SDL_assert(false);
		}

		return tr;
	}

private:

	std::vector< std::vector<Texture> > textures;
	const SDL_Rect bg_rect;
	const SDL_Rect block_rect;

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
	 * Extract the block with the given index from the surface, which contains all blocks in a row.
	 */
	static Texture make_block_texture(Renderer& renderer, Surface& surface, int row, int column)
	{
		Surface temp_block(SDL_CreateRGBSurface(0, BLOCK_W, BLOCK_H, 32, 0, 0, 0, 0));
		game_assert(static_cast<bool>(temp_block), SDL_GetError());

		SDL_Rect srcrect {column*BLOCK_W, row*BLOCK_H, BLOCK_W, BLOCK_H};
		SDL_Rect dstrect {0, 0, BLOCK_W, BLOCK_H};
		SDL_BlitSurface(surface.get(), &srcrect, temp_block.get(), &dstrect);

		Texture texture(SDL_CreateTextureFromSurface(renderer.get(), temp_block.get()));
		game_assert(static_cast<bool>(texture), SDL_GetError());

		return texture;
	}

};

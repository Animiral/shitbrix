/**
 * asset.hpp
 * Functions for loading, destroying and identifying the game assets.
 */
#pragma once

#include "globals.hpp"
#include "sdl_helper.hpp"
#include <vector>

/**
 * Interface for stored assets.
 */
class Assets
{

public:

	/**
	 * Return a Texture according to the gfx enum id.
	 */
	virtual SDL_Texture& texture(Gfx gfx, size_t frame = 0) const = 0;

	virtual const Sound& sound(Snd snd) const = 0;

};

/**
 * Provides no assets.
 * Calling any member function is an error.
 */
class NoAssets : public Assets
{

public:

	virtual SDL_Texture& texture(Gfx gfx, size_t frame = 0) const override;
	virtual const Sound& sound(Snd snd) const override;

};

/**
 * Loads assets from installed files and stores them in structures.
 */
class FileAssets : public Assets
{

public:

	FileAssets();

	SDL_Texture& texture(Gfx gfx, size_t frame = 0) const;
	const Sound& sound(Snd snd) const;

private:

	std::vector< std::vector<TexturePtr> > textures;
	std::vector< Sound > sounds;

};

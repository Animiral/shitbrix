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

	/**
	 * Return the default true type font.
	 */
	virtual TTF_Font& ttf_font() const = 0;

	/**
	 * Return the template charset for the default bitmap font.
	 */
	virtual SDL_Surface& charset() const = 0;

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
	virtual TTF_Font& ttf_font() const override;
	virtual SDL_Surface& charset() const override;

};

/**
 * Loads assets from installed files and stores them in structures.
 */
class FileAssets : public Assets
{

public:

	explicit FileAssets(const Sdl& sdl);

	virtual SDL_Texture& texture(Gfx gfx, size_t frame = 0) const override;
	virtual const Sound& sound(Snd snd) const override;
	virtual TTF_Font& ttf_font() const override;
	virtual SDL_Surface& charset() const override;

private:

	std::vector< std::vector<TexturePtr> > m_textures;
	std::vector< Sound > m_sounds;
	FontPtr m_ttf_font;
	SurfacePtr m_charset;

};

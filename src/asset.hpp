/**
 * asset.hpp
 * Functions for loading, destroying and identifying the game assets.
 */
#pragma once

#include "globals.hpp"
#include "sdl_helper.hpp"
#include <vector>

/**
 * Implementation for a font based on a single source bitmap, divided into characters.
 *
 * This font implementation is useful for displaying static strings that occur
 * in the game without going beyond the upper-case ASCII character set, such as
 * effect texts and score.
 */
class BitmapFont
{

public:

	/**
	 * Construct the font from the specified input file.
	 *
	 * The arrangement of letters in the file must match the expected layout:
	 * a single-color one-pixel solid grid filled with 4 rows / 16 columns of
	 * character graphics ranging from 0x20 (space) to 0x5f (underscore).
	 *
	 * The placeholder color for the background in the source bitmap must be #909090.
	 * The placeholder color for the outline in the source bitmap must be white.
	 * The placeholder fill color in the source bitmap must be black.
	 *
	 * To allow for the NoAssets class, if a nullptr is passed as file, all textures
	 * will be null pointers. The available characters will be the same.
	 */
	explicit BitmapFont(const Sdl& sdl, const char* file, wrap::Color outline_color, wrap::Color fill_color);
	BitmapFont(const BitmapFont&) = delete; // no copy
	BitmapFont& operator=(const BitmapFont&) = delete; // no assign

	/**
	 * Return true if the given character is available from the source bitmap.
	 *
	 * The characters are currently restricted to upper case ASCII and most punctuation.
	 */
	bool can_print(char c) const noexcept;

	/**
	 * Return the texture for the given character.
	 *
	 * The texture is transparent, with the outline and fill colors of the character
	 * as specified in the constructor.
	 *
	 * @throw GameException if the character is not available
	 */
	SDL_Texture& char_texture(char c) const;

private:

	std::vector<TexturePtr> m_textures;

};

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
	 * Return the default bitmap font.
	 */
	virtual const BitmapFont& bmp_font() const = 0;

};

/**
 * Provides no assets.
 * Calling any member function is an error.
 */
class NoAssets : public Assets
{

public:

	explicit NoAssets() noexcept;

	virtual SDL_Texture& texture(Gfx gfx, size_t frame = 0) const override;
	virtual const Sound& sound(Snd snd) const override;
	virtual TTF_Font& ttf_font() const override;
	virtual const BitmapFont& bmp_font() const override;

private:

	BitmapFont m_bitmap_font;

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
	virtual const BitmapFont& bmp_font() const override;

private:

	std::vector< std::vector<TexturePtr> > m_textures;
	std::vector< Sound > m_sounds;
	FontPtr m_ttf_font;
	std::unique_ptr<BitmapFont> m_bitmap_font;

};

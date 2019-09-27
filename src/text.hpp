/**
 * Definitions for text that can be drawn to the screen.
 *
 * The game supports two kinds of text rendering:
 *
 * - TTF fonts: these are useful for printing unicode glyphs and nice typesetting.
 * - bitmap fonts: these are useful for stylish presentation and different colors.
 */
#pragma once

#include "sdl_helper.hpp"
#include <vector>

/**
 * This is a prepared (rendered and ready) texture containing text from a TTF font.
 *
 * The text may span any number of characters and lines. The texture size will
 * automatically adapt to hold the contents (subject to hardware limitations,
 * managed by the SDL library).
 *
 * Because rendering text can be slow, we render it once and subsequently render
 * the resulting texture as often as necessary.
 */
class TtfText
{

public:

	explicit TtfText(const Sdl& sdl, TTF_Font& font, const char* text, wrap::Color color = wrap::BLACK);
	SDL_Texture& texture() const noexcept { return *m_texture; }

private:

	TexturePtr m_texture;

};

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
	 * Construct the font from the specified charset template.
	 *
	 * The arrangement of letters in the charset must match the expected layout:
	 * a single-color one-pixel solid grid filled with 4 rows / 16 columns of
	 * character graphics ranging from 0x20 (space) to 0x5f (underscore).
	 *
	 * The placeholder color for the background in the source bitmap must be #909090.
	 * The placeholder color for the outline in the source bitmap must be white.
	 * The placeholder fill color in the source bitmap must be black.
	 */
	explicit BitmapFont(const Sdl& sdl, SDL_Surface& charset, wrap::Color outline_color, wrap::Color fill_color);
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

	std::vector<TexturePtr> m_textures; //!< store of one texture for each supported character

};

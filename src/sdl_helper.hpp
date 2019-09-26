/**
 * sdl_helper.hpp
 * C++-friendly wrappers for SDL stuff
 */
#pragma once

#include "globals.hpp"
#include <memory>
#include <cstdint>
#include <vector>

// SDL forward declarations
struct SDL_Surface;
struct SDL_Texture;
struct SDL_Window;
struct SDL_Renderer;
struct _SDL_Joystick;
typedef struct _SDL_Joystick SDL_Joystick;
struct _TTF_Font;
typedef struct _TTF_Font TTF_Font;
struct SDL_AudioSpec;
typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;

// SDL equivalents
namespace wrap
{

struct Rect { int x, y, w, h; };
struct Color { uint8_t r, g, b, a; };

}

/**
 * Custom deleter for SDL objects, to be used with unique_ptrs
 * when manual RAII wrappers are not needed.
 */
class SdlDeleter
{

public:

	void operator()(SDL_Surface* p) const noexcept;
	void operator()(SDL_Texture* p) const noexcept;
	void operator()(SDL_Window* p) const noexcept;
	void operator()(SDL_Renderer* p) const noexcept;
	void operator()(SDL_Joystick* p) const noexcept;
	void operator()(TTF_Font* p) const noexcept;

};

using SurfacePtr = std::unique_ptr<SDL_Surface, SdlDeleter>;
using TexturePtr = std::unique_ptr<SDL_Texture, SdlDeleter>;
using JoystickPtr = std::unique_ptr<SDL_Joystick, SdlDeleter>;
using FontPtr = std::unique_ptr<TTF_Font, SdlDeleter>;

class SoundImpl; // SDL-specifics hidden

/**
 * RAII for an SDL sound asset.
 * Even though this wrapper can load any sound supported by SDL_LoadWAV(),
 * the current basic (non-mixing) implementation of Audio expects:
 *  - frequency: 48000Hz
 *  - format: signed 16-bit little-endian
 *  - channels: 1
 */
class Sound
{

public:

	explicit Sound(const char* file);
	~Sound() noexcept;

	Sound(const Sound& ) = delete;
	Sound(Sound&& rhs) noexcept;
	Sound& operator=(const Sound& ) = delete;
	Sound& operator=(Sound&& rhs) noexcept;

	uint32_t length() const noexcept;
	const uint8_t* buffer() const noexcept;
	const SDL_AudioSpec& spec() const noexcept;

private:

	std::unique_ptr<SoundImpl> m_impl; // pImpl

};

class SdlSoundPlayerImpl; // SDL-specifics hidden

/**
 * Basic SDL sound player.
 * Every instance opens an audio device upon instantiation.
 * The SDL audio subsystem must be initialized to successfully instantiate Audio.
 */
class SdlSoundPlayer
{

public:

	explicit SdlSoundPlayer();
	~SdlSoundPlayer() noexcept;

	SdlSoundPlayer(const SdlSoundPlayer& ) =delete;
	SdlSoundPlayer(SdlSoundPlayer&& ) noexcept;
	SdlSoundPlayer& operator=(const SdlSoundPlayer& ) =delete;
	SdlSoundPlayer& operator=(SdlSoundPlayer&& ) noexcept;

	void play(const Sound& sound);

private:

	std::unique_ptr<SdlSoundPlayerImpl> m_impl; // pImpl

};

/**
 * Wrapper class and factory for SDL asset objects and wrappers.
 * Handles safe initialization and shutdown of the SDL library.
 * Methods get_* return a lazy-initialized object of which there is only one.
 * Methods create_* return a new object, often wrapped in a container.
 */
class Sdl
{

public:

	explicit Sdl(uint32_t flags);
	~Sdl();

	// accessors
	SDL_Window& window() const;
	SDL_Renderer& renderer() const;
	SdlSoundPlayer& audio() const;


	/**
	 * Create an image surface from an image file.
	 *
	 * If the format is specifed, the returned surface will conform to the desired format.
	 *
	 * @param file path to the source file
	 * @param format desired format value from enum SDL_PixelFormatEnum.
	 */
	SurfacePtr load_surface(const char* file, int format = 0) const;

	/**
	 * Create a new texture from the specified region of the source surface.
	 */
	TexturePtr cutout_texture(SDL_Surface& source, wrap::Rect rect) const;

	/**
	 * Create an image texture from an image file.
	 */
	TexturePtr create_texture(const char* file) const;

	/**
	 * Create a screen-sized texture for offscreen drawing.
	 */
	TexturePtr create_target_texture() const;

	std::vector<TexturePtr> create_texture_row(const char* file, int width) const;
	std::vector< std::vector<TexturePtr> > create_texture_sheet(const char* file, int height, int width) const;

	/**
	 * Replace all pixels of the specified before color in
	 * the surface with the given after color.
	 */
	void recolor(SDL_Surface& surface, wrap::Color before, wrap::Color after) const;

	/**
	 * Read the font with SDL_ttf.
	 */
	FontPtr open_font(const char* file, int ptsize) const;

private:

	std::unique_ptr<SDL_Window, SdlDeleter> m_window;
	std::unique_ptr<SDL_Renderer, SdlDeleter> m_renderer;
	std::unique_ptr<SdlSoundPlayer> m_audio;

};

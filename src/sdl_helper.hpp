/**
 * sdl_helper.hpp
 * C++-friendly wrappers for SDL stuff
 */
#pragma once

#include "globals.hpp"
#include <memory>
#include <vector>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_audio.h>
#include <SDL_ttf.h>

/**
 * Custom deleter for SDL objects, to be used with unique_ptrs
 * when manual RAII wrappers are not needed.
 */
class SdlDeleter
{
public:
	void operator()(SDL_Surface* p) const { SDL_FreeSurface(p); }
	void operator()(SDL_Texture* p) const { SDL_DestroyTexture(p); }
	void operator()(SDL_Window* p) const { SDL_DestroyWindow(p); }
	void operator()(SDL_Renderer* p) const { SDL_DestroyRenderer(p); }
	void operator()(SDL_Joystick* p) const { SDL_JoystickClose(p); }
	void operator()(TTF_Font* p) const { TTF_CloseFont(p); }
};

using SurfacePtr = std::unique_ptr<SDL_Surface, SdlDeleter>;
using TexturePtr = std::unique_ptr<SDL_Texture, SdlDeleter>;
using JoystickPtr = std::unique_ptr<SDL_Joystick, SdlDeleter>;
using FontPtr = std::unique_ptr<TTF_Font, SdlDeleter>;

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

	Sound(const char* file);
	~Sound() noexcept;

	Sound(const Sound& ) = delete;
	Sound(Sound&& rhs) noexcept;
	Sound& operator=(const Sound& ) = delete;
	Sound& operator=(Sound&& rhs) noexcept;

	Uint32 length() const noexcept { return m_length; }
	const Uint8* buffer() const noexcept { return m_buffer; }
	const SDL_AudioSpec& spec() const noexcept { return m_spec; }

private:

	Uint32 m_length; // bytes in wav
	Uint8* m_buffer; // wav data
	SDL_AudioSpec m_spec; // metadata

};

/**
 * Basic SDL sound player.
 * Every instance opens an audio device upon instantiation.
 * The SDL audio subsystem must be initialized to successfully instantiate Audio.
 */
class SdlSoundPlayer
{

public:

	SdlSoundPlayer();
	~SdlSoundPlayer();

	SdlSoundPlayer(const SdlSoundPlayer& ) =delete;
	SdlSoundPlayer(SdlSoundPlayer&& ) =default;
	SdlSoundPlayer& operator=(const SdlSoundPlayer& ) =delete;
	SdlSoundPlayer& operator=(SdlSoundPlayer&& ) =default;

	void play(const Sound& sound);

private:

	static void callback(void* userdata, Uint8* stream, int length);

	SDL_AudioDeviceID devid;
	SDL_AudioSpec spec; // metadata of audio device
	const Uint8* pos = nullptr;
	int remaining = 0;

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

	explicit Sdl(Uint32 flags);
	~Sdl();

	// accessors
	SDL_Window& window() const;
	SDL_Renderer& renderer() const;
	SdlSoundPlayer& audio() const;


	/**
	 * Create an image surface from an image file.
	 *
	 * If the format is specifed, the returned surface will conform to the desired format.
	 */
	SurfacePtr load_surface(const char* file, SDL_PixelFormatEnum format = SDL_PIXELFORMAT_UNKNOWN) const;

	/**
	 * Create a new texture from the specified region of the source surface.
	 */
	TexturePtr cutout_texture(SDL_Surface& source, SDL_Rect rect) const;

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
	void recolor(SDL_Surface& surface, SDL_Color before, SDL_Color after) const;

	/**
	 * Read the font with SDL_ttf.
	 */
	FontPtr open_font(const char* file, int ptsize) const;

private:

	std::unique_ptr<SDL_Window, SdlDeleter> m_window;
	std::unique_ptr<SDL_Renderer, SdlDeleter> m_renderer;
	std::unique_ptr<SdlSoundPlayer> m_audio;

};

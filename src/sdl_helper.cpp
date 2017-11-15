#include "sdl_helper.hpp"
#include <cstring>
#include <SDL2/SDL.h>
#include <SDL2/SDL_assert.h>

namespace
{

std::unique_ptr<SDL_Surface, SdlDeleter> create_surface(int width, int height)
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

	auto surface = std::unique_ptr<SDL_Surface, SdlDeleter>(SDL_CreateRGBSurface(0, width, height, 32, rmask, gmask, bmask, amask));
	game_assert(bool(surface), SDL_GetError());
	return surface;
}

}


SdlRaiiImpl::SdlRaiiImpl()
{
	int sdl_result = SDL_Init(SDL_INIT_EVERYTHING);
	game_assert(0 == sdl_result, SDL_GetError());

	int flags = IMG_INIT_PNG;
	int img_result = IMG_Init(flags);
	if((img_result & flags) != flags) {
		SDL_Quit();
		throw GameException(IMG_GetError());
	}
}

SdlRaiiImpl::~SdlRaiiImpl()
{
	IMG_Quit();
	SDL_Quit();
}


TextureImpl::TextureImpl(SdlRaii sdl, SDL_Renderer* renderer, SDL_Surface* sheet, int width, int height, int row, int column)
: sdl(sdl), width(width), height(height)
{
	SDL_assert(bool(sdl));

	auto surface = create_surface(width, height);
	SDL_Rect srcrect{column*width, row*height, width, height};
	SDL_Rect dstrect {0, 0, width, height};
	SDL_BlitSurface(sheet, &srcrect, surface.get(), &dstrect);

	tex.reset(SDL_CreateTextureFromSurface(renderer, surface.get()));
	game_assert(bool(tex), SDL_GetError());
}


SoundImpl::SoundImpl(SdlRaii sdl, const char* file) : sdl(sdl)
{
	SDL_assert(bool(sdl));

	auto load_result = SDL_LoadWAV(file, &spec, &buffer, &length);
	game_assert(load_result, SDL_GetError());
}

SoundImpl::~SoundImpl()
{
	SDL_FreeWAV(buffer);
}


SdlAudio::SdlAudio(SdlRaii sdl) : sdl(sdl)
{
	SDL_assert(bool(sdl));

	SDL_AudioSpec want;
	want.freq = 48000;
	want.format = AUDIO_S16;
	want.channels = 1;
	want.samples = AUDIO_SAMPLES;
	want.callback = &callback;
	want.userdata = this;

	devid = SDL_OpenAudioDevice(nullptr, 0, &want, &spec, 0);
	game_assert(devid > 0, SDL_GetError());

	SDL_PauseAudioDevice(devid, 0);
}

SdlAudio::~SdlAudio()
{
	SDL_CloseAudioDevice(devid);
}

void SdlAudio::play(Sound sound)
{
	pos = sound->buffer;
	remaining = sound->length;
}

void SdlAudio::callback(void* userdata, Uint8* stream, int length)
{
	SdlAudio* audio = reinterpret_cast<SdlAudio*>(userdata);
	int fill = (length > audio->remaining) ? audio->remaining : length;
	std::memcpy(stream, audio->pos, fill);
	// SDL_MixAudioFormat(stream, pos, AUDIO_S16, length, SDL_MIX_MAXVOLUME);

	std::memset(stream+fill, 0, length-fill); // pad with silence

	audio->pos += fill;
	audio->remaining -= fill;
}

SdlFactory::SdlFactory(SdlRaii sdl) : sdl(sdl)
{
	SDL_assert(bool(sdl));
}

SdlRaii SdlFactory::get_sdl() const
{
	if(!bool(sdl))
		sdl = std::make_shared<SdlRaiiImpl>();

	return sdl;
}

std::shared_ptr<SDL_Window> SdlFactory::get_window() const
{
	(void) get_sdl();

	if(!bool(window))
	{
		window = std::shared_ptr<SDL_Window>(SDL_CreateWindow(APP_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, CANVAS_W, CANVAS_H, 0), SdlDeleter());
		game_assert(bool(window), SDL_GetError());
	}

	return window;
}

SDL_Renderer& SdlFactory::get_renderer() const
{
	if(!m_renderer)
	{
		SDL_Window* window = get_window().get();
		m_renderer = std::unique_ptr<SDL_Renderer, SdlDeleter>(SDL_CreateRenderer(window, -1, SDL_RENDERER_TARGETTEXTURE));
		game_assert(bool(m_renderer), SDL_GetError());

		// The renderer must declare the capabilities that we need in this application.
		SDL_RendererInfo info;
		int info_result = SDL_GetRendererInfo(m_renderer.get(), &info);
		game_assert(0 == info_result, SDL_GetError());

		// We need to render stuff offscreen onto target textures.
		game_assert(info.flags & SDL_RENDERER_TARGETTEXTURE, "Render driver does not support target textures.");

		// We need the renderer to support an acceptable pixel format (which we remember).
		game_assert(bool(info.num_texture_formats > 0), "Render driver does not support an acceptable pixel format.");
		m_pxformat = info.texture_formats[0];
	}

	return *m_renderer;
}

std::shared_ptr<SdlAudio> SdlFactory::get_audio() const
{
	if(!bool(audio))
		audio = std::make_shared<SdlAudio>(get_sdl());

	return audio;
}

Texture SdlFactory::create_texture(const char* file) const
{
	auto sheet = std::unique_ptr<SDL_Surface, SdlDeleter>(IMG_Load(file));
	game_assert(bool(sheet), SDL_GetError());

	return std::make_shared<TextureImpl>(get_sdl(), &get_renderer(),
	                                     sheet.get(), sheet->w, sheet->h, 0, 0);
}

std::unique_ptr<SDL_Texture, SdlDeleter> SdlFactory::create_target_texture() const
{
	SDL_Texture* tex = SDL_CreateTexture(&get_renderer(), m_pxformat, SDL_TEXTUREACCESS_TARGET,
	                                     CANVAS_W, CANVAS_H);
	game_assert(bool(tex), SDL_GetError());

	return std::unique_ptr<SDL_Texture, SdlDeleter>(tex);
}

std::vector<Texture> SdlFactory::create_texture_row(const char* file, int width) const
{
	auto sheet = std::unique_ptr<SDL_Surface, SdlDeleter>(IMG_Load(file));
	game_assert(bool(sheet), SDL_GetError());

	int columns = sheet->w/width;
	std::vector<Texture> frames;

	auto& sdl = get_sdl();
	SDL_Renderer* renderer = &get_renderer();

	for(int c = 0; c < columns; c++) {
		frames.emplace_back(std::make_shared<TextureImpl>(sdl, renderer, sheet.get(), width, sheet->h, 0, c));
	}

	return frames;
}

std::vector< std::vector<Texture> > SdlFactory::create_texture_sheet(const char* file, int height, int width) const
{
	auto sheet = std::unique_ptr<SDL_Surface, SdlDeleter>(IMG_Load(file));
	game_assert(bool(sheet), SDL_GetError());

	int rows = sheet->h/height;
	int columns = sheet->w/width;
	std::vector< std::vector<Texture> > textures;

	auto& sdl = get_sdl();
	SDL_Renderer* renderer = &get_renderer();

	for(int r = 0; r < rows; r++) {
		std::vector<Texture> frames;

		for(int c = 0; c < columns; c++) {
			frames.emplace_back(std::make_shared<TextureImpl>(sdl, renderer, sheet.get(), width, height, r, c));
		}

		textures.emplace_back(std::move(frames));
	}

	return textures;
}

Sound SdlFactory::create_sound(const char* file) const
{
	return std::make_shared<SoundImpl>(get_sdl(), file);
}

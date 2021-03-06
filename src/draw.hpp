/**
 * draw.hpp
 * Routines for drawing objects on the screen.
 */
#pragma once

// TODO: use pImpl to remove SDL dependencies from draw interface.
#include "stage.hpp"
#include "event.hpp"
#include "asset.hpp"
#include "text.hpp"
#include "context.hpp"

extern const uint8_t ALPHA_OPAQUE; //!< highest alpha value

/**
 * Represents a screen-sized drawing surface.
 *
 * Its dimensions are fixed by the global @c CANVAS_W and @c CANVAS_H constants.
 *
 * Canvases may be created via the draw implementation.
 */
class ICanvas
{

public:

	virtual ~ICanvas() = 0;

	/**
	 * Establish the canvas as a rendering target for future drawing operations.
	 */
	virtual void use_as_target() = 0;

	/**
	 * Draw the contents of this canvas to the active rendering target.
	 */
	virtual void draw() = 0;

};

/**
 * Facade for drawing operations used by the game.
 */
class IDraw
{

public:

	virtual ~IDraw() = 0;

	/**
	 * Draw one of the graphics from the well-known assets library.
	 */
	virtual void gfx(int x, int y, Gfx gfx, size_t frame = 0, uint8_t a = 255) = 0;

	/**
	 * Draw one of the graphics from the well-known assets library in arbitrary orientation.
	 */
	virtual void gfx_rotate(int x, int y, double angle, Gfx gfx, size_t frame = 0, uint8_t a = 255) = 0;

	/**
	 * Convenince function: x/y are given by a Point.
	 *
	 * The x and y floating-point coordinates are simply cast to int.
	 */
	void gfx(Point loc, Gfx gfx, size_t frame = 0, uint8_t a = 255);

	/**
	 * Draw a primitive rectangle with alpha blending.
	 */
	virtual void rect(wrap::Rect rect, wrap::Color color) = 0;

	/**
	 * Draw a primitive line with alpha blending.
	 */
	virtual void line(int x1, int y1, int x2, int y2, wrap::Color color) = 0;

	/**
	 * Draw a primitive rectangle with additive blending.
	 */
	virtual void highlight(wrap::Rect rect, wrap::Color color) = 0;

	/**
	 * Draw a text string using the default true type font.
	 */
	virtual void text(int x, int y, const TtfText& text) = 0;

	/**
	 * Draw a text string using the custom bitmap font.
	 */
	virtual void text_fixed(int x, int y, const BitmapFont& font, const char* text) = 0;

	/**
	 * Restrict the drawing area to the specified rectangle.
	 */
	virtual void clip(wrap::Rect rect) = 0;

	/**
	 * Remove restrictions on the drawing area.
	 */
	virtual void unclip() = 0;

	/**
	 * Create a new canvas for drawing onto.
	 */
	virtual std::unique_ptr<ICanvas> create_canvas() = 0;

	/**
	 * Draw onto the default rendering target from now on, which is the real screen.
	 */
	virtual void reset_target() = 0;

	/**
	 * Flush all previous drawing operations to the rendering target.
	 */
	virtual void render() = 0;

};

/**
 * Not-drawing canvas implementation.
 *
 * This implementation does nothing when asked to draw.
 * It can be used when SDL's video subsystem was not initialized,
 * i.e. on the server.
 */
class NoDrawCanvas : public ICanvas
{

public:

	virtual void use_as_target() override {}
	virtual void draw() override {}

};

/**
 * Not-drawing implementation.
 *
 * This implementation does nothing when asked to draw.
 * It can be used when SDL's video subsystem was not initialized,
 * i.e. on the server.
 */
class NoDraw : public IDraw
{

public:

	virtual void gfx(int x, int y, Gfx gfx, size_t frame = 0, uint8_t a = 255) override {}
	virtual void gfx_rotate(int x, int y, double angle, Gfx gfx, size_t frame = 0, uint8_t a = 255) override {}
	virtual void rect(wrap::Rect rect, wrap::Color color) override {}
	virtual void line(int x1, int y1, int x2, int y2, wrap::Color color) override {}
	virtual void highlight(wrap::Rect rect, wrap::Color color) override {}
	virtual void text(int x, int y, const TtfText& text) override {}
	virtual void text_fixed(int x, int y, const BitmapFont& font, const char* text) override {}
	virtual void clip(wrap::Rect rect) override {}
	virtual void unclip() override {}
	virtual std::unique_ptr<ICanvas> create_canvas() override { return std::make_unique<NoDrawCanvas>(); }
	virtual void reset_target() override {}
	virtual void render() override {}

};

/**
 * SDL specific canvas implementation.
 */
class SdlCanvas : public ICanvas
{

public:

	explicit SdlCanvas(TexturePtr texture, SDL_Renderer& renderer);

	virtual void use_as_target() override;
	virtual void draw() override;

private:

	TexturePtr m_texture;
	SDL_Renderer* m_renderer;

};

/**
 * SDL specific draw implementation.
 */
class SdlDraw : public IDraw
{

public:

	explicit SdlDraw(SDL_Renderer& renderer, const Assets& assets);

	virtual void gfx(int x, int y, Gfx gfx, size_t frame = 0, uint8_t a = 255) override;
	virtual void gfx_rotate(int x, int y, double angle, Gfx gfx, size_t frame = 0, uint8_t a = 255) override;
	virtual void rect(wrap::Rect rect, wrap::Color color) override;
	virtual void line(int x1, int y1, int x2, int y2, wrap::Color color) override;
	virtual void highlight(wrap::Rect rect, wrap::Color color) override;
	virtual void text(int x, int y, const TtfText& text) override;
	virtual void text_fixed(int x, int y, const BitmapFont& font, const char* text) override;
	virtual void clip(wrap::Rect rect) override;
	virtual void unclip() override;
	virtual std::unique_ptr<ICanvas> create_canvas() override;
	virtual void reset_target() override;
	virtual void render() override;

private:

	SDL_Renderer* m_renderer;
	const Assets* m_assets;

};

// $Id$

#ifndef __V9990SDLRASTERIZER_HH__
#define __V9990SDLRASTERIZER_HH__

#include "V9990Rasterizer.hh"
#include "V9990BitmapConverter.hh"
#include "Renderer.hh"


namespace openmsx {

class V9990;
class V9990VRAM;


/** Rasterizer using SDL.
  */
template <class Pixel, Renderer::Zoom zoom>
class V9990SDLRasterizer : public V9990Rasterizer
{
public:
	/** Constructor.
	  */
	V9990SDLRasterizer(V9990* vdp, SDL_Surface* screen);

	/** Destructor.
	  */
	virtual ~V9990SDLRasterizer();

	// Layer interface:
	virtual void paint();
	virtual const string& getName();

	// Rasterizer interface:
	virtual void reset();
	virtual void frameStart();
	virtual void frameEnd();
	virtual void setDisplayMode(V9990DisplayMode displayMode);
	virtual void setColorMode(V9990ColorMode colorMode);
	virtual void setPalette(int index, byte r, byte g, byte b);
	virtual void setBackgroundColor(int index);
	virtual void setImageWidth(int width);
	virtual void drawBorder(int fromX, int fromY, int limitX, int limitY);
	virtual void drawDisplay(int fromX, int fromY,
		int displayX, int displayY, int displayWidth, int displayHeight);

private:
	/** The VDP of which the video output is being rendered.
	  */
	V9990* vdp;

	/** The VRAM whose contents are rendered.
	  */
	V9990VRAM* vram;

	/** The surface which is visible to the user.
	  */
	SDL_Surface* screen;

	/** Work screen
	  */
	SDL_Surface* workScreen;

	/** The current screen mode
	  */ 
	V9990DisplayMode displayMode;
	V9990ColorMode   colorMode;

	/** Current background color
	  */
	Pixel bgColor;

	/** Image width in pixels
	  */
	int imageWidth;

	/** Palette containing the complete V9990 Color space
	  */
	Pixel palette32768[32768];

	/** The 256 color palette. A fixed subset of the palette32768.
	  */
	Pixel palette256[256];

	/** The 64 palette entries of the VDP - a subset of the palette32768.
	  * These are colors influenced by the palette IO ports and registers
	  */
	Pixel palette64[64];

	/** Bitmap converter. Converts VRAM into pixels
	  */
	V9990BitmapConverter<Pixel, zoom> bitmapConverter;

	/** Fill the palettes.
	  */
	void precalcPalettes(void);
};

} // namespace openmsx

#endif //__V9990SDLRASTERIZER_HH__

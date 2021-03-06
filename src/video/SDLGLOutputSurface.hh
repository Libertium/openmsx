#ifndef SDLGLOUTPUTSURFACE_HH
#define SDLGLOUTPUTSURFACE_HH

#include "MemBuffer.hh"
#include "noncopyable.hh"
#include <string>
#include <memory>

namespace openmsx {

class OutputSurface;
class Texture;

/** This is a common base class for SDLGLVisibleSurface and
  * SDLGLOffScreenSurface. It's only purpose is to have a place to put common
  * code.
  */
class SDLGLOutputSurface : private noncopyable
{
public:
	/** These correspond respectively with the renderers:
	  *   SDLGL-PP, SDLGL-FB16, SDLGL-FB32
	  */
	enum FrameBuffer { FB_NONE, FB_16BPP, FB_32BPP };

	FrameBuffer getFrameBufferType() const { return frameBuffer; }

protected:
	explicit SDLGLOutputSurface(FrameBuffer frameBuffer = FB_NONE);
	~SDLGLOutputSurface();

	void init(OutputSurface& output);
	void flushFrameBuffer(unsigned width, unsigned height);
	void clearScreen();
	void saveScreenshot(const std::string& filename,
	                    unsigned width, unsigned height);

private:
	double texCoordX, texCoordY;
	std::unique_ptr<Texture> fbTex;
	MemBuffer<char> fbBuf;
	const FrameBuffer frameBuffer;
};

} // namespace openmsx

#endif

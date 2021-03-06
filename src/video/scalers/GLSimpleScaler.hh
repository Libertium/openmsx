#ifndef GLSIMPLESCALER_HH
#define GLSIMPLESCALER_HH

#include "GLScaler.hh"
#include "GLUtil.hh"
#include "noncopyable.hh"

namespace openmsx {

class RenderSettings;

class GLSimpleScaler: public GLScaler, private noncopyable
{
public:
	explicit GLSimpleScaler(RenderSettings& renderSettings);

	virtual void scaleImage(
		ColorTexture& src, ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
		unsigned logSrcHeight);

private:
	RenderSettings& renderSettings;
	struct Data {
		ShaderProgram scalerProgram;
		int texSizeLoc;
		int texStepXLoc;
		int cnstLoc;
	} data[2];
};

} // namespace openmsx

#endif // GLSIMPLESCALER_HH

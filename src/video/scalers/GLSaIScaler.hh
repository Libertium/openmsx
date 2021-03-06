#ifndef GLSAISCALER_HH
#define GLSAISCALER_HH

#include "GLScaler.hh"
#include "GLUtil.hh"
#include "noncopyable.hh"

namespace openmsx {

class GLSaIScaler : public GLScaler, private noncopyable
{
public:
	GLSaIScaler();

	virtual void scaleImage(
		ColorTexture& src, ColorTexture* superImpose,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
		unsigned logSrcHeight);

private:
	ShaderProgram scalerProgram[2];
	GLint texSizeLoc[2];
};

} // namespace openmsx

#endif

#include "GLHQLiteScaler.hh"
#include "GLUtil.hh"
#include "HQCommon.hh"
#include "FrameSource.hh"
#include "FileContext.hh"
#include "File.hh"
#include "StringOp.hh"
#include "vla.hh"
#include <cstring>
#include <algorithm>

using std::string;

namespace openmsx {

GLHQLiteScaler::GLHQLiteScaler()
{
	for (int i = 0; i < 2; ++i) {
		string header = string("#define SUPERIMPOSE ")
		              + char('0' + i) + '\n';
		VertexShader   vertexShader  (header, "hqlite.vert");
		FragmentShader fragmentShader(header, "hqlite.frag");
		scalerProgram[i].attach(vertexShader);
		scalerProgram[i].attach(fragmentShader);
		scalerProgram[i].link();

		scalerProgram[i].activate();
		glUniform1i(scalerProgram[i].getUniformLocation("colorTex"),  0);
		if (i == 1) {
			glUniform1i(scalerProgram[i].getUniformLocation("videoTex"),  1);
		}
		glUniform1i(scalerProgram[i].getUniformLocation("edgeTex"),   2);
		glUniform1i(scalerProgram[i].getUniformLocation("offsetTex"), 3);
		glUniform2f(scalerProgram[i].getUniformLocation("texSize"),
		            320.0f, 240.0f);
	}

	edgeTexture.bind();
	edgeTexture.setWrapMode(false);
	glTexImage2D(GL_TEXTURE_2D,    // target
	             0,                // level
	             GL_LUMINANCE16,   // internal format
	             320,              // width
	             240,              // height
	             0,                // border
	             GL_LUMINANCE,     // format
	             GL_UNSIGNED_SHORT,// type
	             nullptr);         // data
	edgeBuffer.setImage(320, 240);

	SystemFileContext context;
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	for (int i = 0; i < 3; ++i) {
		int n = i + 2;
		string offsetName = StringOp::Builder() <<
			"shaders/HQ" << n << "xLiteOffsets.dat";
		File offsetFile(context.resolve(offsetName));
		offsetTexture[i].setWrapMode(false);
		offsetTexture[i].bind();
		size_t size; // dummy
		glTexImage2D(GL_TEXTURE_2D,        // target
		             0,                    // level
		             GL_LUMINANCE8_ALPHA8, // internal format
		             n * 64,               // width
		             n * 64,               // height
		             0,                    // border
		             GL_LUMINANCE_ALPHA,   // format
		             GL_UNSIGNED_BYTE,     // type
		             offsetFile.mmap(size));// data
	}
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // restore to default
}

void GLHQLiteScaler::scaleImage(
	ColorTexture& src, ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
	unsigned logSrcHeight)
{
	unsigned factorX = dstWidth / srcWidth; // 1 - 4
	unsigned factorY = (dstEndY - dstStartY) / (srcEndY - srcStartY);

	auto& prog = scalerProgram[superImpose ? 1 : 0];
	if ((srcWidth == 320) && (factorX > 1) && (factorX == factorY)) {
		src.enableInterpolation();
		glActiveTexture(GL_TEXTURE3);
		offsetTexture[factorX - 2].bind();
		glActiveTexture(GL_TEXTURE2);
		edgeTexture.bind();
		if (superImpose) {
			glActiveTexture(GL_TEXTURE1);
			superImpose->bind();
		}
		glActiveTexture(GL_TEXTURE0);
		prog.activate();
		//prog.validate();
		drawMultiTex(src, srcStartY, srcEndY, src.getHeight(), logSrcHeight,
		             dstStartY, dstEndY, dstWidth);
		src.disableInterpolation();
	} else {
		GLScaler::scaleImage(src, superImpose,
		                     srcStartY, srcEndY, srcWidth,
		                     dstStartY, dstEndY, dstWidth,
		                     logSrcHeight);
	}
}

typedef unsigned Pixel;
void GLHQLiteScaler::uploadBlock(
	unsigned srcStartY, unsigned srcEndY, unsigned lineWidth,
	FrameSource& paintFrame)
{
	if (lineWidth != 320) return;

	uint32_t tmpBuf2[320 / 2]; // 2 x uint16_t
	#ifndef NDEBUG
	// Avoid UMR. In optimized mode we don't care.
	memset(tmpBuf2, 0, sizeof(tmpBuf2));
	#endif

	VLA_SSE_ALIGNED(Pixel, buf1_, lineWidth); auto* buf1 = buf1_;
	VLA_SSE_ALIGNED(Pixel, buf2_, lineWidth); auto* buf2 = buf2_;
	auto* curr = paintFrame.getLinePtr(srcStartY - 1, lineWidth, buf1);
	auto* next = paintFrame.getLinePtr(srcStartY + 0, lineWidth, buf2);
	calcEdgesGL(curr, next, tmpBuf2, EdgeHQLite());

	edgeBuffer.bind();
	if (auto* mapped = edgeBuffer.mapWrite()) {
		for (unsigned y = srcStartY; y < srcEndY; ++y) {
			curr = next;
			std::swap(buf1, buf2);
			next = paintFrame.getLinePtr(y + 1, lineWidth, buf2);
			calcEdgesGL(curr, next, tmpBuf2, EdgeHQLite());
			memcpy(mapped + 320 * y, tmpBuf2, 320 * sizeof(uint16_t));
		}
		edgeBuffer.unmap();

		edgeTexture.bind();
		glTexSubImage2D(GL_TEXTURE_2D,       // target
		                0,                   // level
		                0,                   // offset x
		                srcStartY,           // offset y
		                lineWidth,           // width
		                srcEndY - srcStartY, // height
		                GL_LUMINANCE,        // format
		                GL_UNSIGNED_SHORT,   // type
		                edgeBuffer.getOffset(0, srcStartY)); // data
	}
	edgeBuffer.unbind();
}

} // namespace openmsx

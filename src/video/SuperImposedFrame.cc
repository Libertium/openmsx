#include "SuperImposedFrame.hh"
#include "PixelOperations.hh"
#include "LineScalers.hh"
#include "memory.hh"
#include "unreachable.hh"
#include "vla.hh"
#include "build-info.hh"
#include <algorithm>
#include <cstdint>

namespace openmsx {

template <typename Pixel>
class SuperImposedFrameImpl : public SuperImposedFrame
{
public:
	SuperImposedFrameImpl(const SDL_PixelFormat& format);

private:
	virtual unsigned getLineWidth(unsigned line) const;
	virtual const void* getLineInfo(
		unsigned line, unsigned& width,
		void* buf, unsigned bufWidth) const;

	PixelOperations<Pixel> pixelOps;
};


// class SuperImposedFrame

std::unique_ptr<SuperImposedFrame> SuperImposedFrame::create(
	const SDL_PixelFormat& format)
{
#if HAVE_16BPP
	if (format.BitsPerPixel == 15 || format.BitsPerPixel == 16) {
		return make_unique<SuperImposedFrameImpl<uint16_t>>(format);
	}
#endif
#if HAVE_32BPP
	if (format.BitsPerPixel == 32) {
		return make_unique<SuperImposedFrameImpl<uint32_t>>(format);
	}
#endif
	UNREACHABLE; return nullptr; // avoid warning
}

SuperImposedFrame::SuperImposedFrame(const SDL_PixelFormat& format)
	: FrameSource(format)
{
}

void SuperImposedFrame::init(
	const FrameSource* top_, const FrameSource* bottom_)
{
	top = top_;
	bottom = bottom_;
	setHeight(std::max(top->getHeight(), bottom->getHeight()));
}


// class SuperImposedFrameImpl

template <typename Pixel>
SuperImposedFrameImpl<Pixel>::SuperImposedFrameImpl(
		const SDL_PixelFormat& format)
	: SuperImposedFrame(format)
	, pixelOps(format)
{
}

template <typename Pixel>
unsigned SuperImposedFrameImpl<Pixel>::getLineWidth(unsigned line) const
{
	unsigned tNum = (getHeight() == top   ->getHeight()) ? line : line / 2;
	unsigned bNum = (getHeight() == bottom->getHeight()) ? line : line / 2;
	unsigned tWidth = top   ->getLineWidth(tNum);
	unsigned bWidth = bottom->getLineWidth(bNum);
	return std::max(tWidth, bWidth);
}

template <typename Pixel>
const void* SuperImposedFrameImpl<Pixel>::getLineInfo(
	unsigned line, unsigned& width, void* tBuf_, unsigned bufWidth) const
{
	unsigned tNum = (getHeight() == top   ->getHeight()) ? line : line / 2;
	unsigned bNum = (getHeight() == bottom->getHeight()) ? line : line / 2;
	unsigned tWidth = top   ->getLineWidth(tNum);
	unsigned bWidth = bottom->getLineWidth(bNum);
	width = std::max(tWidth, bWidth);  // as wide as the widest source
	width = std::min(width, bufWidth); // but no wider than the output buffer

	auto* tBuf = static_cast<Pixel*>(tBuf_);
	VLA_SSE_ALIGNED(Pixel, bBuf, width);
	auto* tLine = top   ->getLinePtr(tNum, width, tBuf);
	auto* bLine = bottom->getLinePtr(bNum, width, bBuf);

	AlphaBlendLines<Pixel> blend(pixelOps);
	blend(tLine, bLine, tBuf, width); // possibly tLine == tBuf
	return tBuf;
}

} // namespace openmsx

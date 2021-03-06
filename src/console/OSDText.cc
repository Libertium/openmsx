#include "OSDText.hh"
#include "TTFFont.hh"
#include "SDLImage.hh"
#include "OutputRectangle.hh"
#include "CommandException.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "TclObject.hh"
#include "StringOp.hh"
#include "utf8_core.hh"
#include "unreachable.hh"
#include "memory.hh"
#include "components.hh"
#include <cassert>
#if COMPONENT_GL
#include "GLImage.hh"
#endif

using std::string;
using std::vector;

namespace openmsx {

OSDText::OSDText(const OSDGUI& gui, const string& name)
	: OSDImageBasedWidget(gui, name)
	, fontfile("skins/Vera.ttf.gz")
	, size(12)
	, wrapMode(NONE), wrapw(0.0), wraprelw(1.0)
{
}

OSDText::~OSDText()
{
}

vector<string_ref> OSDText::getProperties() const
{
	auto result = OSDImageBasedWidget::getProperties();
	static const char* const vals[] = {
		"-text", "-font", "-size", "-wrap", "-wrapw", "-wraprelw",
		"-query-size",
	};
	result.insert(result.end(), std::begin(vals), std::end(vals));
	return result;
}

void OSDText::setProperty(string_ref name, const TclObject& value)
{
	if (name == "-text") {
		string_ref val = value.getString();
		if (text != val) {
			text = val.str();
			// note: don't invalidate font (don't reopen font file)
			OSDImageBasedWidget::invalidateLocal();
			invalidateChildren();
		}
	} else if (name == "-font") {
		string val = value.getString().str();
		if (fontfile != val) {
			string file = SystemFileContext().resolve(val);
			if (!FileOperations::isRegularFile(file)) {
				throw CommandException("Not a valid font file: " + val);
			}
			fontfile = val;
			invalidateRecursive();
		}
	} else if (name == "-size") {
		int size2 = value.getInt();
		if (size != size2) {
			size = size2;
			invalidateRecursive();
		}
	} else if (name == "-wrap") {
		string_ref val = value.getString();
		WrapMode wrapMode2;
		if (val == "none") {
			wrapMode2 = NONE;
		} else if (val == "word") {
			wrapMode2 = WORD;
		} else if (val == "char") {
			wrapMode2 = CHAR;
		} else {
			throw CommandException("Not a valid value for -wrap, "
				"expected one of 'none word char', but got '" +
				val + "'.");
		}
		if (wrapMode != wrapMode2) {
			wrapMode = wrapMode2;
			invalidateRecursive();
		}
	} else if (name == "-wrapw") {
		double wrapw2 = value.getDouble();
		if (wrapw != wrapw2) {
			wrapw = wrapw2;
			invalidateRecursive();
		}
	} else if (name == "-wraprelw") {
		double wraprelw2 = value.getDouble();
		if (wraprelw != wraprelw2) {
			wraprelw = wraprelw2;
			invalidateRecursive();
		}
	} else if (name == "-query-size") {
		throw CommandException("-query-size property is readonly");
	} else {
		OSDImageBasedWidget::setProperty(name, value);
	}
}

void OSDText::getProperty(string_ref name, TclObject& result) const
{
	if (name == "-text") {
		result.setString(text);
	} else if (name == "-font") {
		result.setString(fontfile);
	} else if (name == "-size") {
		result.setInt(size);
	} else if (name == "-wrap") {
		string wrapString;
		switch (wrapMode) {
			case NONE: wrapString = "none"; break;
			case WORD: wrapString = "word"; break;
			case CHAR: wrapString = "char"; break;
			default: UNREACHABLE;
		}
		result.setString(wrapString);
	} else if (name == "-wrapw") {
		result.setDouble(wrapw);
	} else if (name == "-wraprelw") {
		result.setDouble(wraprelw);
	} else if (name == "-query-size") {
		double outX, outY;
		getRenderedSize(outX, outY);
		result.addListElement(outX);
		result.addListElement(outY);
	} else {
		OSDImageBasedWidget::getProperty(name, result);
	}
}

void OSDText::invalidateLocal()
{
	font = TTFFont(); // clear font
	OSDImageBasedWidget::invalidateLocal();
}


string_ref OSDText::getType() const
{
	return "text";
}

void OSDText::getWidthHeight(const OutputRectangle& /*output*/,
                             double& width, double& height) const
{
	if (image) {
		width  = image->getWidth();
		height = image->getHeight();
	} else {
		// we don't know the dimensions, must be because of an error
		assert(hasError());
		width  = 0;
		height = 0;
	}
}

byte OSDText::getFadedAlpha() const
{
	return byte((getRGBA(0) & 0xff) * getRecursiveFadeValue());
}

template <typename IMAGE> std::unique_ptr<BaseImage> OSDText::create(
	OutputRectangle& output)
{
	if (text.empty()) {
		return make_unique<IMAGE>(0, 0, 0);
	}
	int scale = getScaleFactor(output);
	if (font.empty()) {
		try {
			string file = SystemFileContext().resolve(fontfile);
			int ptSize = size * scale;
			font = TTFFont(file, ptSize);
		} catch (MSXException& e) {
			throw MSXException("Couldn't open font: " + e.getMessage());
		}
	}
	try {
		double pWidth, pHeight;
		getParent()->getWidthHeight(output, pWidth, pHeight);
		int maxWidth = int(wrapw * scale + wraprelw * pWidth + 0.5);
		// Width can't be negative, if it is make it zero instead.
		// This will put each character on a different line.
		maxWidth = std::max(0, maxWidth);

		// TODO gradient???
		unsigned rgba = getRGBA(0);
		string wrappedText;
		if (wrapMode == NONE) {
			wrappedText = text; // don't wrap
		} else if (wrapMode == WORD) {
			wrappedText = getWordWrappedText(text, maxWidth);
		} else if (wrapMode == CHAR) {
			wrappedText = getCharWrappedText(text, maxWidth);
		} else {
			UNREACHABLE;
		}
		// An alternative is to pass vector<string> to TTFFont::render().
		// That way we can avoid StringOp::join() (in the wrap functions)
		// followed by // StringOp::split() (in TTFFont::render()).
		SDLSurfacePtr surface(font.render(wrappedText,
			(rgba >> 24) & 0xff, (rgba >> 16) & 0xff, (rgba >> 8) & 0xff));
		if (surface) {
			return make_unique<IMAGE>(std::move(surface));
		} else {
			return make_unique<IMAGE>(0, 0, 0);
		}
	} catch (MSXException& e) {
		throw MSXException("Couldn't render text: " + e.getMessage());
	}
}


// Search for a position strictly between min and max which also points to the
// start of a (possibly multi-byte) utf8-character. If no such position exits,
// this function returns 'min'.
static size_t findCharSplitPoint(const string& line, size_t min, size_t max)
{
	auto pos = (min + max) / 2;
	auto beginIt = line.data();
	auto posIt = beginIt + pos;

	auto fwdIt = utf8::sync_forward(posIt);
	auto maxIt = beginIt + max;
	assert(fwdIt <= maxIt);
	if (fwdIt != maxIt) {
		return fwdIt - beginIt;
	}

	auto bwdIt = utf8::sync_backward(posIt);
	auto minIt = beginIt + min;
	assert(minIt <= bwdIt); (void)minIt;
	return bwdIt - beginIt;
}

// Search for a position that's strictly between min and max and which points
// to a character directly following a delimiter character. if no such position
// exits, this function returns 'min'.
// This function works correctly with multi-byte utf8-encoding as long as
// all delimiter characters are single byte chars.
static size_t findWordSplitPoint(string_ref line, size_t min, size_t max)
{
	static const char* const delimiters = " -/";

	// initial guess for a good position
	assert(min < max);
	size_t pos = (min + max) / 2;
	if (pos == min) {
		// can't reduce further
		return min;
	}

	// try searching backward (this also checks current position)
	assert(pos > min);
	auto pos2 = line.substr(min, pos - min).find_last_of(delimiters);
	if (pos2 != string_ref::npos) {
		pos2 += min + 1;
		assert(min < pos2);
		assert(pos2 <= pos);
		return pos2;
	}

	// try searching forward
	auto pos3 = line.substr(pos, max - pos).find_first_of(delimiters);
	if (pos3 != string_ref::npos) {
		pos3 += pos;
		assert(pos3 < max);
		pos3 += 1; // char directly after a delimiter;
		if (pos3 < max) {
			return pos3;
		}
	}

	return min;
}

static size_t takeSingleChar(const string& /*line*/, unsigned /*maxWidth*/)
{
	return 1;
}

template<typename FindSplitPointFunc, typename CantSplitFunc>
size_t OSDText::split(const string& line, unsigned maxWidth,
                      FindSplitPointFunc findSplitPoint,
                      CantSplitFunc cantSplit,
                      bool removeTrailingSpaces) const
{
	if (line.empty()) {
		// empty line always fits (explicitly handle this because
		// SDL_TTF can't handle empty strings)
		return 0;
	}

	unsigned width, height;
	font.getSize(line, width, height);
	if (width <= maxWidth) {
		// whole line fits
		return line.size();
	}

	// binary search till we found the largest initial substring that is
	// not wider than maxWidth
	size_t min = 0;
	size_t max = line.size();
	// invariant: line.substr(0, min) DOES     fit
	//            line.substr(0, max) DOES NOT fit
	size_t cur = findSplitPoint(line, min, max);
	if (cur == 0) {
		// Could not find a valid split point, then split on char
		// (this also handles the case of a single too wide char)
		return cantSplit(line, maxWidth);
	}
	while (true) {
		assert(min < cur);
		assert(cur < max);
		string curStr = line.substr(0, cur);
		if (removeTrailingSpaces) {
			StringOp::trimRight(curStr, ' ');
		}
		font.getSize(curStr, width, height);
		if (width <= maxWidth) {
			// still fits, try to enlarge
			size_t next = findSplitPoint(line, cur, max);
			if (next == cur) {
				return cur;
			}
			min = cur;
			cur = next;
		} else {
			// doesn't fit anymore, try to shrink
			size_t next = findSplitPoint(line, min, cur);
			if (next == min) {
				if (min == 0) {
					// even the first word does not fit,
					// split on char (see above)
					return cantSplit(line, maxWidth);
				}
				return min;
			}
			max = cur;
			cur = next;
		}
	}
}

size_t OSDText::splitAtChar(const std::string& line, unsigned maxWidth) const
{
	return split(line, maxWidth, findCharSplitPoint, takeSingleChar, false);
}

struct SplitAtChar {
	SplitAtChar(const OSDText& osdText_) : osdText(osdText_) {}
	size_t operator()(const string& line, unsigned maxWidth) {
		return osdText.splitAtChar(line, maxWidth);
	}
	const OSDText& osdText;
};
size_t OSDText::splitAtWord(const std::string& line, unsigned maxWidth) const
{
	return split(line, maxWidth, findWordSplitPoint, SplitAtChar(*this), true);
}

string OSDText::getCharWrappedText(const string& text, unsigned maxWidth) const
{
	vector<string_ref> wrappedLines;
	for (auto& line : StringOp::split(text, '\n')) {
		do {
			auto pos = splitAtChar(line.str(), maxWidth);
			wrappedLines.push_back(line.substr(0, pos));
			line = line.substr(pos);
		} while (!line.empty());
	}
	return StringOp::join(wrappedLines, '\n');
}

string OSDText::getWordWrappedText(const string& text, unsigned maxWidth) const
{
	vector<string_ref> wrappedLines;
	for (auto& line : StringOp::split(text, '\n')) {
		do {
			auto pos = splitAtWord(line.str(), maxWidth);
			string_ref first = line.substr(0, pos);
			StringOp::trimRight(first, ' '); // remove trailing spaces
			wrappedLines.push_back(first);
			line = line.substr(pos);
			StringOp::trimLeft(line, " "); // remove leading spaces
		} while (!line.empty());
	}
	return StringOp::join(wrappedLines, '\n');
}

void OSDText::getRenderedSize(double& outX, double& outY) const
{
	SDL_Surface* surface = SDL_GetVideoSurface();
	if (!surface) {
		throw CommandException(
			"Can't query size: no window visible");
	}
	DummyOutputRectangle output(surface->w, surface->h);
	// force creating image (does not yet draw it on screen)
	const_cast<OSDText*>(this)->createImage(output);

	unsigned width = 0;
	unsigned height = 0;
	if (image) {
		width  = image->getWidth();
		height = image->getHeight();
	}

	double scale = getScaleFactor(output);
	outX = width  / scale;
	outY = height / scale;
}

std::unique_ptr<BaseImage> OSDText::createSDL(OutputRectangle& output)
{
	return create<SDLImage>(output);
}

std::unique_ptr<BaseImage> OSDText::createGL(OutputRectangle& output)
{
#if COMPONENT_GL
	return create<GLImage>(output);
#else
	(void)&output;
	return nullptr;
#endif
}

} // namespace openmsx

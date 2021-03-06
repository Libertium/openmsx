#ifndef XMLEXCEPTION_HH
#define XMLEXCEPTION_HH

#include "MSXException.hh"

namespace openmsx {

class XMLException : public MSXException
{
public:
	explicit XMLException(string_ref message)
		: MSXException(message) {}
};

} // namespace openmsx

#endif

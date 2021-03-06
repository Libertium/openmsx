#ifndef DUMMYMIDIOUTDEVICE_HH
#define DUMMYMIDIOUTDEVICE_HH

#include "MidiOutDevice.hh"

namespace openmsx {

class DummyMidiOutDevice : public MidiOutDevice
{
public:
	// SerialDataInterface (part)
	virtual void recvByte(byte value, EmuTime::param time);
	virtual string_ref getDescription() const;
	virtual void plugHelper(Connector& connector, EmuTime::param time);
	virtual void unplugHelper(EmuTime::param time);
};

} // namespace openmsx

#endif

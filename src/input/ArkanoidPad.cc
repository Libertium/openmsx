// $Id$

#include "ArkanoidPad.hh"
#include "MSXEventDistributor.hh"
#include "InputEvents.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include <algorithm>

// Implemented mostly according to the info here: http://www.msx.org/forumtopic7661.html
// This is absolutely not accurate, but good enough to make the pad work in the
// Arkanoid games.

using std::string;

namespace openmsx {

static const int POS_MIN = 152; // minimum to be able to use left exit door in Arkanoid 2
static const int POS_MAX = 309; // minimum to be able to use right exit door in Arkanoid 1
static const int POS_CENTER = (POS_MIN + POS_MAX) / 2;
static const int SCALE = 2;

ArkanoidPad::ArkanoidPad(MSXEventDistributor& eventDistributor_)
	: eventDistributor(eventDistributor_)
	, shiftreg(511) // the 9 bit shift register contains 1's if there's no value in
	, dialpos(POS_CENTER)
	, buttonStatus(0x3E)
	, lastValue(0)
{
	eventDistributor.registerEventListener(*this);
}

ArkanoidPad::~ArkanoidPad()
{
	eventDistributor.unregisterEventListener(*this);
}


// Pluggable
const string& ArkanoidPad::getName() const
{
	static const string name("arkanoidpad");
	return name;
}

const string& ArkanoidPad::getDescription() const
{
	static const string desc("Arkanoid pad.");
	return desc;
}

void ArkanoidPad::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
}

void ArkanoidPad::unplugHelper(EmuTime::param /*time*/)
{
}

// JoystickDevice
byte ArkanoidPad::read(EmuTime::param /*time*/)
{
	return buttonStatus | ((shiftreg & 0x100) >> 8);
}

void ArkanoidPad::write(byte value, EmuTime::param /*time*/)
{
	byte diff = lastValue ^ value;
	lastValue = value;

	if (diff & value & 0x4) {
		// pin 8 from low to high: copy dial position into shift reg
		shiftreg = dialpos;
	}
	if (diff & value & 0x1) {
		// pin 6 from low to high: shift the shift reg
		shiftreg = (shiftreg << 1) | 0x1; // restore 1's in shift reg
	}
}

// EventListener
void ArkanoidPad::signalEvent(shared_ptr<const Event> event, EmuTime::param /*time*/)
{
	switch (event->getType()) {
	case OPENMSX_MOUSE_MOTION_EVENT: {
		const MouseMotionEvent& motionEvent =
			checked_cast<const MouseMotionEvent&>(*event);
		dialpos += (motionEvent.getX() / SCALE);
		dialpos = std::min(POS_MAX, std::max(POS_MIN, dialpos));
		break;
	}
	case OPENMSX_MOUSE_BUTTON_DOWN_EVENT:
		// any button will press the Arkanoid Pad button
		buttonStatus &= ~0x02;
		break;
	case OPENMSX_MOUSE_BUTTON_UP_EVENT:
		// any button will unpress the Arkanoid Pad button
		buttonStatus |= 0x02;
		break;
	default:
		// ignore
		break;
	}
}


template<typename Archive>
void ArkanoidPad::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("shiftreg", shiftreg);
	ar.serialize("lastValue", lastValue);

	// Don't serialize buttonStatus, dialpos.
	// These are controlled via (mouse button/motion) events
}
INSTANTIATE_SERIALIZE_METHODS(ArkanoidPad);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, ArkanoidPad, "ArkanoidPad");

} // namespace openmsx
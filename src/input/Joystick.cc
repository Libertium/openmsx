#include "Joystick.hh"
#include "PluggingController.hh"
#include "PlugException.hh"
#include "MSXEventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "InputEvents.hh"
#include "InputEventGenerator.hh"
#include "StateChange.hh"
#include "TclObject.hh"
#include "StringSetting.hh"
#include "CommandException.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include "memory.hh"
#include "xrange.hh"
#include "build-info.hh"

using std::string;
using std::shared_ptr;

namespace openmsx {

#if PLATFORM_ANDROID
static const int THRESHOLD = 32768 / 4;
#else
static const int THRESHOLD = 32768 / 10;
#endif

void Joystick::registerAll(MSXEventDistributor& eventDistributor,
                           StateChangeDistributor& stateChangeDistributor,
                           CommandController& commandController,
                           PluggingController& controller)
{
#ifdef SDL_JOYSTICK_DISABLED
	(void)eventDistributor;
	(void)stateChangeDistributor;
	(void)controller;
#else
	if (!SDL_WasInit(SDL_INIT_JOYSTICK)) {
		SDL_InitSubSystem(SDL_INIT_JOYSTICK);
		SDL_JoystickEventState(SDL_ENABLE); // joysticks generate events
	}

	unsigned numJoysticks = SDL_NumJoysticks();
	ad_printf("#joysticks: %d\n", numJoysticks);
	for (unsigned i = 0; i < numJoysticks; i++) {
		SDL_Joystick* joystick = SDL_JoystickOpen(i);
		if (joystick) {
			// Avoid devices that have axes but no buttons, like accelerometers.
			// SDL 1.2.14 in Linux has an issue where it rejects a device from
			// /dev/input/event* if it has no buttons but does not reject a
			// device from /dev/input/js* if it has no buttons, while
			// accelerometers do end up being symlinked as a joystick in
			// practice.
			if (InputEventGenerator::joystickNumButtons(joystick) != 0) {
				controller.registerPluggable(
					make_unique<Joystick>(
						eventDistributor,
						stateChangeDistributor,
						commandController,
						joystick));
			}
		}
	}
#endif
}


class JoyState : public StateChange
{
public:
	JoyState() {} // for serialize
	JoyState(EmuTime::param time, unsigned joyNum_, byte press_, byte release_)
		: StateChange(time)
		, joyNum(joyNum_), press(press_), release(release_)
	{
		assert((press != 0) || (release != 0));
		assert((press & release) == 0);
	}
	unsigned getJoystick() const { return joyNum; }
	byte     getPress()    const { return press; }
	byte     getRelease()  const { return release; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChange>(*this);
		ar.serialize("joyNum", joyNum);
		ar.serialize("press", press);
		ar.serialize("release", release);
	}
private:
	unsigned joyNum;
	byte press, release;
};
REGISTER_POLYMORPHIC_CLASS(StateChange, JoyState, "JoyState");


void checkJoystickConfig(TclObject& newValue)
{
	unsigned n = newValue.getListLength();
	if (n & 1) {
		throw CommandException("Need an even number of elements");
	}
	for (unsigned i = 0; i < n; i += 2) {
		string_ref key  = newValue.getListIndex(i + 0).getString();
		TclObject value = newValue.getListIndex(i + 1);
		if ((key != "A"   ) && (key != "B"    ) &&
		    (key != "LEFT") && (key != "RIGHT") &&
		    (key != "UP"  ) && (key != "DOWN" )) {
			throw CommandException(
				"Invalid MSX joystick action: must be one of "
				"'A', 'B', 'LEFT', 'RIGHT', 'UP', 'DOWN'.");
		}
		for (auto j : xrange(value.getListLength())) {
			string_ref host = value.getListIndex(j).getString();
			if (!host.starts_with("button") &&
			    !host.starts_with("+axis") &&
			    !host.starts_with("-axis")) {
				throw CommandException(
					"Invalid host joystick action: must be "
					"one of 'button<N>', '+axis<N>', '-axis<N>'");
			}
		}
	}
}


#ifndef SDL_JOYSTICK_DISABLED
// Note: It's OK to open/close the same SDL_Joystick multiple times (we open it
// once per MSX machine). The SDL documentation doesn't state this, but I
// checked the implementation and a SDL_Joystick uses a 'reference count' on
// the open/close calls.
Joystick::Joystick(MSXEventDistributor& eventDistributor_,
                   StateChangeDistributor& stateChangeDistributor_,
                   CommandController& commandController,
                   SDL_Joystick* joystick_)
	: eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, joystick(joystick_)
	, joyNum(SDL_JoystickIndex(joystick_))
	, name("joystickX") // 'X' is filled in below
	, desc(string(SDL_JoystickName(joyNum)))
{
	const_cast<string&>(name)[8] = char('1' + joyNum);

	// create config setting
	TclObject value;
	value.addListElement("LEFT" ); value.addListElement("-axis0");
	value.addListElement("RIGHT"); value.addListElement("+axis0");
	value.addListElement("UP"   ); value.addListElement("-axis1");
	value.addListElement("DOWN" ); value.addListElement("+axis1");
	TclObject listA, listB;
	for (auto i : xrange(InputEventGenerator::joystickNumButtons(joystick))) {
		string button = "button" + StringOp::toString(i);
		if (i & 1) {
			listB.addListElement(button);
		} else {
			listA.addListElement(button);
		}
	}
	value.addListElement("A"); value.addListElement(listA);
	value.addListElement("B"); value.addListElement(listB);
	configSetting = make_unique<StringSetting>(
		commandController, name + "_config", "joystick configuration",
		value.getString());
	configSetting->setChecker(checkJoystickConfig);

	pin8 = false; // avoid UMR
}

Joystick::~Joystick()
{
	if (isPluggedIn()) {
		Joystick::unplugHelper(EmuTime::dummy());
	}
	if (joystick) {
		SDL_JoystickClose(joystick);
	}
}

// Pluggable
const string& Joystick::getName() const
{
	return name;
}

string_ref Joystick::getDescription() const
{
	return desc;
}

void Joystick::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	if (!joystick) {
		throw PlugException("Failed to open joystick device");
	}
	plugHelper2();
	status = calcState();
}

void Joystick::plugHelper2()
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
}

void Joystick::unplugHelper(EmuTime::param /*time*/)
{
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}


// JoystickDevice
byte Joystick::read(EmuTime::param /*time*/)
{
	return pin8 ? 0x3F : status;
}

void Joystick::write(byte value, EmuTime::param /*time*/)
{
	pin8 = (value & 0x04) != 0;
}

byte Joystick::calcState()
{
	byte result = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	              JOY_BUTTONA | JOY_BUTTONB;
	if (joystick) {
		const TclObject& dict = configSetting->getValue();
		if (getState(dict, "A"    )) result &= ~JOY_BUTTONA;
		if (getState(dict, "B"    )) result &= ~JOY_BUTTONB;
		if (getState(dict, "UP"   )) result &= ~JOY_UP;
		if (getState(dict, "DOWN" )) result &= ~JOY_DOWN;
		if (getState(dict, "LEFT" )) result &= ~JOY_LEFT;
		if (getState(dict, "RIGHT")) result &= ~JOY_RIGHT;
	}
	return result;
}

bool Joystick::getState(const TclObject& dict, string_ref key)
{
	try {
		const auto& list = dict.getDictValue(TclObject(key));
		for (auto i : xrange(list.getListLength())) {
			const auto& elem = list.getListIndex(i).getString();
			if (elem.starts_with("button")) {
				int n = stoi(elem.substr(6));
				if (InputEventGenerator::joystickGetButton(joystick, n)) {
					return true;
				}
			} else if (elem.starts_with("+axis")) {
				int n = stoi(elem.substr(5));
				if (SDL_JoystickGetAxis(joystick, n) > THRESHOLD) {
					return true;
				}
			} else if (elem.starts_with("-axis")) {
				int n = stoi(elem.substr(5));
				if (SDL_JoystickGetAxis(joystick, n) < -THRESHOLD) {
					return true;
				}
			}
		}
	} catch (MSXException&) {
		// ignore
	}
	return false;
}

// MSXEventListener
void Joystick::signalEvent(const shared_ptr<const Event>& event,
                           EmuTime::param time)
{
	auto joyEvent = dynamic_cast<const JoystickEvent*>(event.get());
	if (!joyEvent) return;

	// TODO: It would be more efficient to make a dispatcher instead of
	//       sending the event to all joysticks.
	if (joyEvent->getJoystick() != joyNum) return;

	// TODO: Currently this recalculates the whole joystick state. It might
	// be possible to implement this more efficiently by using the specific
	// event information. Though that's not trivial because e.g. multiple
	// host buttons can map to the same MSX button. Also calcState()
	// involves some string processing. It might be possible to only parse
	// the config once (per setting change). Though this solution is likely
	// good enough.
	createEvent(time, calcState());
}

void Joystick::createEvent(EmuTime::param time, byte newStatus)
{
	byte diff = status ^ newStatus;
	if (!diff) {
		// event won't actually change the status, so ignore it
		return;
	}
	// make sure we create an event with minimal changes
	byte press   =    status & diff;
	byte release = newStatus & diff;
	stateChangeDistributor.distributeNew(std::make_shared<JoyState>(
		time, joyNum, press, release));
}

// StateChangeListener
void Joystick::signalStateChange(const shared_ptr<StateChange>& event)
{
	auto js = dynamic_cast<const JoyState*>(event.get());
	if (!js) return;

	// TODO: It would be more efficient to make a dispatcher instead of
	//       sending the event to all joysticks.
	// TODO an alternative is to log events based on the connector instead
	//      of the joystick. That would make it possible to replay on a
	//      different host without an actual SDL joystick connected.
	if (js->getJoystick() != joyNum) return;

	status = (status & ~js->getPress()) | js->getRelease();
}

void Joystick::stopReplay(EmuTime::param time)
{
	createEvent(time, calcState());
}

// version 1: Initial version, the variable status was not serialized.
// version 2: Also serialize the above variable, this is required for
//            record/replay, see comment in Keyboard.cc for more details.
template<typename Archive>
void Joystick::serialize(Archive& ar, unsigned version)
{
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("status", status);
	}
	if (ar.isLoader()) {
		if (joystick && isPluggedIn()) {
			plugHelper2();
		}
	}
	// no need to serialize 'pin8' it's automatically restored via write()
}
INSTANTIATE_SERIALIZE_METHODS(Joystick);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, Joystick, "Joystick");

#endif // SDL_JOYSTICK_DISABLED

} // namespace openmsx

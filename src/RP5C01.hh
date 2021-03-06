// This class implements the RP5C01 chip (RTC)
//
// * For techncal details on RP5C01 see
//     http://w3.qahwah.net/joost/openMSX/RP5C01.pdf

#ifndef RP5C01_HH
#define RP5C01_HH

#include "Clock.hh"
#include "openmsx.hh"
#include "noncopyable.hh"
#include <memory>
#include <string>

namespace openmsx {

class CommandController;
class SRAM;
template <typename T> class EnumSetting;

class RP5C01 : private noncopyable
{
public:
	enum RTCMode { EMUTIME, REALTIME };

	RP5C01(CommandController& commandController, SRAM& regs,
	       EmuTime::param time, const std::string& name);
	~RP5C01();

	void reset(EmuTime::param time);
	nibble readPort(nibble port, EmuTime::param time);
	void writePort(nibble port, nibble value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void initializeTime();
	void updateTimeRegs(EmuTime::param time);
	void regs2Time();
	void time2Regs();
	void resetAlarm();

	static const unsigned FREQ = 16384;

	SRAM& regs;
	const std::unique_ptr<EnumSetting<RTCMode>> modeSetting;

	Clock<FREQ> reference;
	unsigned fraction;
	unsigned seconds, minutes, hours;
	unsigned dayWeek, years, leapYear;
	int days, months; // these two can be -1

	nibble modeReg, testReg, resetReg;
};

} // namespace openmsx

#endif

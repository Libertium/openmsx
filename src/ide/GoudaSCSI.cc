#include "GoudaSCSI.hh"
#include "WD33C93.hh"
#include "Rom.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include "memory.hh"

namespace openmsx {

GoudaSCSI::GoudaSCSI(const DeviceConfig& config)
	: MSXDevice(config)
	, rom(make_unique<Rom>(getName() + " ROM", "rom", config))
	, wd33c93(make_unique<WD33C93>(config))
{
	reset(EmuTime::dummy());
}

GoudaSCSI::~GoudaSCSI()
{
}

void GoudaSCSI::reset(EmuTime::param /*time*/)
{
	wd33c93->reset(true);
}

byte GoudaSCSI::readIO(word port, EmuTime::param /*time*/)
{
	switch (port & 0x03) {
	case 0:
		return wd33c93->readAuxStatus();
	case 1:
		return wd33c93->readCtrl();
	case 2:
		return 0xb0; // bit 4: 1 = Halt on SCSI parity error
	default:
		UNREACHABLE; return 0;
	}
}

byte GoudaSCSI::peekIO(word port, EmuTime::param /*time*/) const
{
	switch (port & 0x03) {
	case 0:
		return wd33c93->peekAuxStatus();
	case 1:
		return wd33c93->peekCtrl();
	case 2:
		return 0xb0; // bit 4: 1 = Halt on SCSI parity error
	default:
		UNREACHABLE; return 0;
	}
}

void GoudaSCSI::writeIO(word port, byte value, EmuTime::param time)
{
	switch (port & 0x03) {
	case 0:
		wd33c93->writeAdr(value);
		break;
	case 1:
		wd33c93->writeCtrl(value);
		break;
	case 2:
		reset(time);
		break;
	default:
		UNREACHABLE;
	}
}

byte GoudaSCSI::readMem(word address, EmuTime::param /*time*/)
{
	return *GoudaSCSI::getReadCacheLine(address);
}

const byte* GoudaSCSI::getReadCacheLine(word start) const
{
	return &(*rom)[start & (rom->getSize() - 1)];
}


template<typename Archive>
void GoudaSCSI::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("WD33C93", *wd33c93);
}
INSTANTIATE_SERIALIZE_METHODS(GoudaSCSI);
REGISTER_MSXDEVICE(GoudaSCSI, "GoudaSCSI");

} // namespace openmsx

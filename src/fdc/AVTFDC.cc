#include "AVTFDC.hh"
#include "DriveMultiplexer.hh"
#include "WD2793.hh"
#include "serialize.hh"

namespace openmsx {

AVTFDC::AVTFDC(const DeviceConfig& config)
	: WD2793BasedFDC(config)
{
}

byte AVTFDC::readIO(word port, EmuTime::param time)
{
	byte value;
	switch (port & 0x07) {
	case 0:
		value = controller->getStatusReg(time);
		break;
	case 1:
		value = controller->getTrackReg(time);
		break;
	case 2:
		value = controller->getSectorReg(time);
		break;
	case 3:
		value = controller->getDataReg(time);
		break;
	case 4:
		value = 0x7F;
		if (controller->getIRQ(time))  value |=  0x80;
		if (controller->getDTRQ(time)) value &= ~0x40;
		break;
	default:
		value = 255;
		break;
	}
	return value;
}

byte AVTFDC::peekIO(word port, EmuTime::param time) const
{
	byte value;
	switch (port & 0x07) {
	case 0:
		value = controller->peekStatusReg(time);
		break;
	case 1:
		value = controller->peekTrackReg(time);
		break;
	case 2:
		value = controller->peekSectorReg(time);
		break;
	case 3:
		value = controller->peekDataReg(time);
		break;
	case 4:
		value = 0x7F;
		if (controller->peekIRQ(time))  value |=  0x80;
		if (controller->peekDTRQ(time)) value &= ~0x40;
		break;
	default:
		value = 255;
		break;
	}
	return value;
}

void AVTFDC::writeIO(word port, byte value, EmuTime::param time)
{
	switch (port & 0x07) {
	case 0:
		controller->setCommandReg(value, time);
		break;
	case 1:
		controller->setTrackReg(value, time);
		break;
	case 2:
		controller->setSectorReg(value, time);
		break;
	case 3:
		controller->setDataReg(value, time);
		break;
	case 4: /* nothing only read... */
		break;
	case 5:
		// From mohai
		// bit 0:  drive select A (and motor on, as this is a WD1770,
		// we use this as workaround)
		// bit 1:  drive select B (and motor on, as this is a WD1770,
		// we use this as workaround)
		// bit 2:  side select
		// bit 3:  density: 1=single 0=double (not supported by openMSX)
		//
		// Set correct drive
		DriveMultiplexer::DriveNum drive;
		switch (value & 0x03) {
		case 1:
			drive = DriveMultiplexer::DRIVE_A;
			break;
		case 2:
			drive = DriveMultiplexer::DRIVE_B;
			break;
		default:
			// No drive selected or two drives at same time
			// The motor is enabled for all drives at the same time, so
			// in a real machine you must take care to do not select more
			// than one drive at the same time (you could get data
			// collision).
			drive = DriveMultiplexer::NO_DRIVE;
		}
		multiplexer->selectDrive(drive, time);
		multiplexer->setSide((value & 0x04) != 0);
		multiplexer->setMotor(drive != DriveMultiplexer::NO_DRIVE, time);
		break;
	}
}


template<typename Archive>
void AVTFDC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<WD2793BasedFDC>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(AVTFDC);
REGISTER_MSXDEVICE(AVTFDC, "AVTFDC");

} // namespace openmsx

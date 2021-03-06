#include "WD2793BasedFDC.hh"
#include "DriveMultiplexer.hh"
#include "WD2793.hh"
#include "XMLElement.hh"
#include "serialize.hh"
#include "memory.hh"

namespace openmsx {

WD2793BasedFDC::WD2793BasedFDC(const DeviceConfig& config)
	: MSXFDC(config)
	, multiplexer(make_unique<DriveMultiplexer>(
		reinterpret_cast<DiskDrive**>(drives)))
	, controller(make_unique<WD2793>(
		getScheduler(), *multiplexer, getCliComm(), getCurrentTime(),
		config.getXML()->getName() == "WD1770"))
{
}

WD2793BasedFDC::~WD2793BasedFDC()
{
}

void WD2793BasedFDC::reset(EmuTime::param time)
{
	controller->reset(time);
}

template<typename Archive>
void WD2793BasedFDC::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXFDC>(*this);
	ar.serialize("multiplexer", *multiplexer);
	ar.serialize("wd2793", *controller);
}
INSTANTIATE_SERIALIZE_METHODS(WD2793BasedFDC);

} // namespace openmsx

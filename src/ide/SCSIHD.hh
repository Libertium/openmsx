/* Ported from:
** Source: /cvsroot/bluemsx/blueMSX/Src/IoDevice/ScsiDevice.h,v
** Revision: 1.6
** Date: 2007-05-22 20:05:38 +0200 (Tue, 22 May 2007)
**
** More info: http://www.bluemsx.com
**
** Copyright (C) 2003-2007 Daniel Vik, white cat
*/
#ifndef SCSIHD_HH
#define SCSIHD_HH

#include "HD.hh"
#include "SCSIDevice.hh"
#include "noncopyable.hh"

namespace openmsx {

class DeviceConfig;

class SCSIHD : public HD, public SCSIDevice, private noncopyable
{
public:
	SCSIHD(const DeviceConfig& targetconfig,
	       AlignedBuffer& buf, unsigned mode);
	virtual ~SCSIHD();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// SCSI Device
	virtual void reset();
	virtual bool isSelected();
	virtual unsigned executeCmd(const byte* cdb, SCSI::Phase& phase,
	                            unsigned& blocks);
	virtual unsigned executingCmd(SCSI::Phase& phase, unsigned& blocks);
	virtual byte getStatusCode();
	virtual int msgOut(byte value);
	virtual byte msgIn();
	virtual void disconnect();
	virtual void busReset();

	virtual unsigned dataIn(unsigned& blocks);
	virtual unsigned dataOut(unsigned& blocks);

	unsigned inquiry();
	unsigned modeSense();
	unsigned requestSense();
	bool checkReadOnly();
	unsigned readCapacity();
	bool checkAddress();
	unsigned readSectors(unsigned& blocks);
	unsigned writeSectors(unsigned& blocks);
	void formatUnit();

	AlignedBuffer& buffer;

	const unsigned mode;

	unsigned keycode;      // Sense key, ASC, ASCQ
	unsigned currentSector;
	unsigned currentLength;

	const byte scsiId;     // SCSI ID 0..7
	bool unitAttention;    // Unit Attention (was: reset)
	byte message;
	byte lun;
	byte cdb[12];          // Command Descriptor Block
};

} // namespace openmsx

#endif

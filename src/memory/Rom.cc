// $Id$

#include <string>
#include <sstream>
#include "Config.hh"
#include "Rom.hh"
#include "RomInfo.hh"
#include "MSXDiskRomPatch.hh"
#include "MSXTapePatch.hh"
#include "MSXCPUInterface.hh"
#include "MSXRomPatchInterface.hh"
#include "File.hh"
#include "FileContext.hh"
#include "PanasonicMemory.hh"
#include "StringOp.hh"
#include "Debugger.hh"
#include "sha1.hh"
#include "CliCommOutput.hh"

namespace openmsx {

Rom::Rom(const string& name_, const string& description_, Config* config)
	: name(name_), description(description_)
{
	// TODO use SHA1 to fetch file from ROM pool
	if (config->hasParameter("filename")) {
		string filename = config->getParameter("filename");
		read(config, filename);
	} else if (config->hasParameter("firstblock")) {
		int first = config->getParameterAsInt("firstblock");
		int last  = config->getParameterAsInt("lastblock");
		size = (last - first + 1) * 0x2000;
		rom = PanasonicMemory::instance().getRomBlock(first);
		file = NULL;
		assert(last >= first);
		assert(rom);
	} else { // Assumption: this only happens for an empty SCC
		size = 0;
		file = NULL;
	}
	init(*config);
}

Rom::Rom(const string& name_, const string& description_, Config* config,
         const string& filename)
	: name(name_), description(description_)
{
	read(config, filename);	// TODO config
	init(*config);
}

void Rom::read(Config* config, const string& filename)
{
	// open file
	try {
		file = new File(config->getContext().resolve(filename));
	} catch (FileException& e) {
		throw FatalError("Error reading ROM: " + filename);
	}
	
	// get filesize
	int fileSize;
	if (config && config->hasParameter("filesize") &&
	    config->getParameter("filesize") != "auto") {
		fileSize = config->getParameterAsInt("filesize");
	} else {
		fileSize = file->getSize();
	}
	
	// get offset
	int offset = 0;
	if (config && config->hasParameter("skip_headerbytes")) {
		offset = config->getParameterAsInt("skip_headerbytes");
	}
	if (fileSize <= offset) {
		throw FatalError("Offset greater than filesize");
	}
	size = fileSize - offset;
	
	// read file
	byte* tmp = 0;	// avoid warning
	try {
		tmp = file->mmap() + offset;
		rom = tmp;
	} catch (FileException &e) {
		throw FatalError("Error reading ROM image: " + filename);
	}

	// verify SHA1
	if (config && !checkSHA1(*config)) {
		CliCommOutput::instance().printWarning(
			"SHA1 sum for '" + config->getId() +
			"' does not match with sum of '" + filename +
			"'.");
	}
	
	if (config) {
		patch(*config);
	}
}

bool Rom::checkSHA1(const Config& config)
{
	const string& sha1sum = getSHA1Sum();
	XMLElement::Children sums;
	config.getChildren("sha1", sums);
	if (sums.empty()) {
		return true;
	}
	for (XMLElement::Children::const_iterator it = sums.begin();
	     it != sums.end(); ++it) {
		if ((*it)->getData() == sha1sum) {
			return true;
		}
	}
	return false;
}

void Rom::patch(const Config& config)
{
	// for each patchcode parameter, construct apropriate patch
	// object and register it at MSXCPUInterface
	Config::Parameters parameters;
	config.getParametersWithClass("patchcode", parameters);
	for (Config::Parameters::const_iterator it = parameters.begin();
	     it != parameters.end(); ++it) {
		MSXRomPatchInterface* patchInterface;
		if (it->second == "MSXDiskRomPatch") {
			patchInterface = new MSXDiskRomPatch();
		} else if (it->second == "MSXTapePatch") {
			patchInterface = new MSXTapePatch();
		} else {
			throw FatalError("Unknown patch interface");
		}
		romPatchInterfaces.push_back(patchInterface);
		MSXCPUInterface::instance().registerInterface(patchInterface);
	}
	
	// also patch the file if needed:
	byte* tmp = const_cast<byte*>(rom);
	Config::Parameters parameters2;
	config.getParametersWithClass("patch", parameters2);
	for (Config::Parameters::const_iterator i = parameters2.begin();
	     i != parameters2.end(); ++i) {
		unsigned int romOffset = strtol(i->first.c_str(), 0, 0);
		int value  = StringOp::stringToInt(i->second);
		if (romOffset >= size) {
			ostringstream out;
			out << "Illegal ROM patch-offset: 0x" << hex << romOffset;
			throw FatalError(out.str());
		} else {
			PRT_DEBUG("Patching ROM[" << i->first << "]=" << i->second);
			tmp[romOffset] = value; // tmp = rom, but rom is read only
		}
	}
}

void Rom::init(const Config& config)
{
	info = RomInfo::fetchRomInfo(*this, config);
	
	if (!info->getTitle().empty()) {
		name += " (" + info->getTitle() + ')';
	}

	if (size) {
		Debugger::instance().registerDebuggable(name, *this);
	}
}

Rom::~Rom()
{
	if (size) {
		Debugger::instance().unregisterDebuggable(name, *this);
	}
	
	for (vector<MSXRomPatchInterface*>::const_iterator it =
	           romPatchInterfaces.begin();
	     it != romPatchInterfaces.end(); ++it) {
		MSXCPUInterface::instance().unregisterInterface(*it);
		delete (*it);
	}
	if (file) {
		file->munmap();
		delete file;
	}
}


unsigned Rom::getSize() const
{
	return size;
}

const string& Rom::getDescription() const
{
	return description;
}

byte Rom::read(unsigned address)
{
	assert(address < size);
	return rom[address];
}

void Rom::write(unsigned address, byte value)
{
	// ignore
}

const string& Rom::getName() const
{
	return name;
}

const string& Rom::getSHA1Sum() const
{
	if (sha1sum.empty()) {
		SHA1 sha1;
		sha1.update(rom, size);
		sha1.finalize();
		sha1sum = sha1.hex_digest();
	}
	return sha1sum;
}

} // namespace openmsx

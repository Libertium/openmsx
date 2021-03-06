#include "HardwareConfig.hh"
#include "XMLLoader.hh"
#include "XMLException.hh"
#include "DeviceConfig.hh"
#include "XMLElement.hh"
#include "LocalFileReference.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "MSXMotherBoard.hh"
#include "CartridgeSlotManager.hh"
#include "MSXCPUInterface.hh"
#include "DeviceFactory.hh"
#include "CliComm.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "StringOp.hh"
#include "memory.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <cassert>
#include <iostream>

using std::string;
using std::vector;
using std::unique_ptr;
using std::move;

namespace openmsx {

unique_ptr<HardwareConfig> HardwareConfig::createMachineConfig(
	MSXMotherBoard& motherBoard, const string& machineName)
{
	auto result = make_unique<HardwareConfig>(motherBoard, machineName);
	result->load("machines");
	return result;
}

unique_ptr<HardwareConfig> HardwareConfig::createExtensionConfig(
	MSXMotherBoard& motherBoard, const string& extensionName, const string& slotname)
{
	auto result = make_unique<HardwareConfig>(motherBoard, extensionName);
	result->load("extensions");
	result->setName(extensionName);
	result->setSlot(slotname);
	return result;
}

unique_ptr<HardwareConfig> HardwareConfig::createRomConfig(
	MSXMotherBoard& motherBoard, const string& romfile,
	const string& slotname, const vector<string>& options)
{
	auto result = make_unique<HardwareConfig>(motherBoard, "rom");
	const auto& sramfile = FileOperations::getFilename(romfile);
	auto context = make_unique<UserFileContext>("roms/" + sramfile);

	vector<string_ref> ipsfiles;
	string mapper;

	bool romTypeOptionFound = false;

	// parse options
	for (auto it = options.begin(); it != options.end(); ++it) {
		const auto& option = *it++;
		if (it == options.end()) {
			throw MSXException("Missing argument for option \"" +
			                   option + '\"');
		}
		if (option == "-ips") {
			if (!FileOperations::isRegularFile(context->resolve(*it))) {
				throw MSXException("Invalid IPS file: " + *it);
			}
			ipsfiles.push_back(*it);
		} else if (option == "-romtype") {
			if (!romTypeOptionFound) {
				mapper = *it;
				romTypeOptionFound = true;
			} else {
				throw MSXException("Only one -romtype option is allowed");
			}
		} else {
			throw MSXException("Invalid option \"" + option + '\"');
		}
	}

	string resolvedFilename = FileOperations::getAbsolutePath(
		context->resolve(romfile));
	if (!FileOperations::isRegularFile(resolvedFilename)) {
		throw MSXException("Invalid ROM file: " + resolvedFilename);
	}

	XMLElement extension("extension");
	auto& devices = extension.addChild("devices");
	auto& primary = devices.addChild("primary");
	primary.addAttribute("slot", slotname);
	auto& secondary = primary.addChild("secondary");
	secondary.addAttribute("slot", slotname);
	auto& device = secondary.addChild("ROM");
	device.addAttribute("id", "MSXRom");
	auto& mem = device.addChild("mem");
	mem.addAttribute("base", "0x0000");
	mem.addAttribute("size", "0x10000");
	auto& rom = device.addChild("rom");
	rom.addChild("resolvedFilename", resolvedFilename);
	rom.addChild("filename", romfile);
	if (!ipsfiles.empty()) {
		auto& patches = rom.addChild("patches");
		for (auto& s : ipsfiles) {
			patches.addChild("ips", s);
		}
	}
	device.addChild("sound").addChild("volume", "9000");
	device.addChild("mappertype", mapper.empty() ? "auto" : mapper);
	device.addChild("sramname", sramfile + ".SRAM");

	result->setConfig(move(extension));
	result->setName(romfile);
	result->setFileContext(move(context));

	return result;
}

HardwareConfig::HardwareConfig(MSXMotherBoard& motherBoard_, const string& hwName_)
	: motherBoard(motherBoard_)
	, hwName(hwName_)
{
	for (auto ps : xrange(4)) {
		for (auto ss : xrange(4)) {
			externalSlots[ps][ss] = false;
		}
		externalPrimSlots[ps] = false;
		expandedSlots[ps] = false;
		allocatedPrimarySlots[ps] = false;
	}
	userName = motherBoard.getUserName(hwName);
}

HardwareConfig::~HardwareConfig()
{
	motherBoard.freeUserName(hwName, userName);
#ifndef NDEBUG
	try {
		testRemove();
	} catch (MSXException& e) {
		std::cerr << e.getMessage() << std::endl;
		UNREACHABLE;
	}
#endif
	while (!devices.empty()) {
		motherBoard.removeDevice(*devices.back());
		devices.pop_back();
	}
	auto& slotManager = motherBoard.getSlotManager();
	for (auto ps : xrange(4)) {
		for (auto ss : xrange(4)) {
			if (externalSlots[ps][ss]) {
				slotManager.removeExternalSlot(ps, ss);
			}
		}
		if (externalPrimSlots[ps]) {
			slotManager.removeExternalSlot(ps);
		}
		if (expandedSlots[ps]) {
			motherBoard.getCPUInterface().unsetExpanded(ps);
		}
		if (allocatedPrimarySlots[ps]) {
			slotManager.freePrimarySlot(ps, *this);
		}
	}
}

void HardwareConfig::testRemove() const
{
	std::vector<MSXDevice*> alreadyRemoved;
	for (auto it = devices.rbegin(); it != devices.rend(); ++it) {
		(*it)->testRemove(alreadyRemoved);
		alreadyRemoved.push_back(it->get());
	}
	auto& slotManager = motherBoard.getSlotManager();
	for (auto ps : xrange(4)) {
		for (auto ss : xrange(4)) {
			if (externalSlots[ps][ss]) {
				slotManager.testRemoveExternalSlot(ps, ss, *this);
			}
		}
		if (externalPrimSlots[ps]) {
			slotManager.testRemoveExternalSlot(ps, *this);
		}
		if (expandedSlots[ps]) {
			motherBoard.getCPUInterface().testUnsetExpanded(
				ps, alreadyRemoved);
		}
	}
}

const FileContext& HardwareConfig::getFileContext() const
{
	return *context;
}
void HardwareConfig::setFileContext(unique_ptr<FileContext> context_)
{
	context = move(context_);
}

const XMLElement& HardwareConfig::getDevices() const
{
	return getConfig().getChild("devices");
}

XMLElement HardwareConfig::loadConfig(string_ref type, string_ref name)
{
	return loadConfig(getFilename(type, name));
}

XMLElement HardwareConfig::loadConfig(const string& filename)
{
	try {
		LocalFileReference fileRef(filename);
		return XMLLoader::load(fileRef.getFilename(), "msxconfig2.dtd");
	} catch (XMLException& e) {
		throw MSXException(
			"Loading of hardware configuration failed: " +
			e.getMessage());
	}
}

string HardwareConfig::getFilename(string_ref type, string_ref name)
{
	SystemFileContext context;
	try {
		// try <name>.xml
		return context.resolve(FileOperations::join(
			type, name + ".xml"));
	} catch (MSXException& e) {
		// backwards-compatibility:
		//  also try <name>/hardwareconfig.xml
		try {
			return context.resolve(FileOperations::join(
				type, name, "hardwareconfig.xml"));
		} catch (MSXException&) {
			throw e; // signal first error
		}
	}
}

void HardwareConfig::load(string_ref type)
{
	string filename = getFilename(type, hwName);
	setConfig(loadConfig(filename));

	assert(!userName.empty());
	const auto& baseName = FileOperations::getBaseName(filename);
	setFileContext(make_unique<ConfigFileContext>(
		baseName, hwName, userName));
}

void HardwareConfig::parseSlots()
{
	// TODO this code does parsing for both 'expanded' and 'external' slots
	//      once machine and extensions are parsed separately move parsing
	//      of 'expanded' to MSXCPUInterface
	//
	for (auto& psElem : getDevices().getChildren("primary")) {
		const auto& primSlot = psElem->getAttribute("slot");
		int ps = CartridgeSlotManager::getSlotNum(primSlot);
		if (psElem->getAttributeAsBool("external", false)) {
			if (ps < 0) {
				throw MSXException(
				    "Cannot mark unspecified primary slot '" +
				    primSlot + "' as external");
			}
			createExternalSlot(ps);
			continue;
		}
		for (auto& ssElem : psElem->getChildren("secondary")) {
			const auto& secSlot = ssElem->getAttribute("slot");
			int ss = CartridgeSlotManager::getSlotNum(secSlot);
			if (ss < 0) {
				if ((ss >= -128) && (0 <= ps) && (ps < 4) &&
				    motherBoard.getCPUInterface().isExpanded(ps)) {
					ss += 128;
				} else {
					continue;
				}
			}
			if (ps < 0) {
				ps = getFreePrimarySlot();
				auto mutableElem = const_cast<XMLElement*>(psElem);
				mutableElem->setAttribute("slot", StringOp::toString(ps));
			}
			createExpandedSlot(ps);
			if (ssElem->getAttributeAsBool("external", false)) {
				createExternalSlot(ps, ss);
			}
		}
	}
}

void HardwareConfig::createDevices()
{
	createDevices(getDevices(), nullptr, nullptr);
}

void HardwareConfig::createDevices(const XMLElement& elem,
	const XMLElement* primary, const XMLElement* secondary)
{
	for (auto& c : elem.getChildren()) {
		const auto& name = c.getName();
		if (name == "primary") {
			createDevices(c, &c, secondary);
		} else if (name == "secondary") {
			createDevices(c, primary, &c);
		} else {
			auto device = DeviceFactory::create(
				DeviceConfig(*this, c, primary, secondary));
			if (device) {
				addDevice(move(device));
			} else {
				motherBoard.getMSXCliComm().printWarning(
					"Deprecated device: \"" +
					name + "\", please upgrade your "
					"hardware descriptions.");
			}
		}
	}
}

void HardwareConfig::createExternalSlot(int ps)
{
	motherBoard.getSlotManager().createExternalSlot(ps);
	assert(!externalPrimSlots[ps]);
	externalPrimSlots[ps] = true;
}

void HardwareConfig::createExternalSlot(int ps, int ss)
{
	motherBoard.getSlotManager().createExternalSlot(ps, ss);
	assert(!externalSlots[ps][ss]);
	externalSlots[ps][ss] = true;
}

void HardwareConfig::createExpandedSlot(int ps)
{
	if (!expandedSlots[ps]) {
		motherBoard.getCPUInterface().setExpanded(ps);
		expandedSlots[ps] = true;
	}
}

int HardwareConfig::getFreePrimarySlot()
{
	int ps;
	motherBoard.getSlotManager().allocatePrimarySlot(ps, *this);
	assert(!allocatedPrimarySlots[ps]);
	allocatedPrimarySlots[ps] = true;
	return ps;
}

void HardwareConfig::addDevice(std::unique_ptr<MSXDevice> device)
{
	motherBoard.addDevice(*device);
	devices.push_back(move(device));
}

const string& HardwareConfig::getName() const
{
	return name;
}

void HardwareConfig::setName(const string& proposedName)
{
	if (!motherBoard.findExtension(proposedName)) {
		name = proposedName;
	} else {
		unsigned n = 0;
		do {
			name = StringOp::Builder() << proposedName << " (" << ++n << ')';
		} while (motherBoard.findExtension(name));
	}
}

void HardwareConfig::setSlot(const string& slotname)
{
	for (auto& psElem : getDevices().getChildren("primary")) {
		const auto& primSlot = psElem->getAttribute("slot");
		if (primSlot == "any") {
			auto& mutableElem = const_cast<XMLElement*&>(psElem);
			mutableElem->setAttribute("slot", slotname);
		}
	}
}

// version 1: initial version
// version 2: moved FileContext here (was part of config)
// version 3: hold 'config' by-value instead of by-pointer
template<typename Archive>
void HardwareConfig::serialize(Archive& ar, unsigned version)
{
	// filled-in by constructor:
	//   motherBoard, hwName, userName
	// filled-in by parseSlots()
	//   externalSlots, externalPrimSlots, expandedSlots, allocatedPrimarySlots

	if (ar.versionBelow(version, 2)) {
		XMLElement::getLastSerializedFileContext(); // clear any previous value
	}
	ar.serialize("config", config); // fills in getLastSerializedFileContext()
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("context", context);
	} else {
		context = XMLElement::getLastSerializedFileContext();
		assert(context);
	}
	if (ar.isLoader()) {
		if (!motherBoard.getMachineConfig()) {
			// must be done before parseSlots()
			motherBoard.setMachineConfig(this);
		} else {
			// already set because this is an extension
		}
		parseSlots();
		createDevices();
	}
	// only (polymorphically) initialize devices, they are already created
	for (auto& d : devices) {
		ar.serializePolymorphic("device", *d);
	}
	ar.serialize("name", name);
}
INSTANTIATE_SERIALIZE_METHODS(HardwareConfig);

} // namespace openmsx

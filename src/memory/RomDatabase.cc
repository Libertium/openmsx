#include "RomDatabase.hh"
#include "InfoTopic.hh"
#include "CommandException.hh"
#include "TclObject.hh"
#include "FileContext.hh"
#include "File.hh"
#include "FileOperations.hh"
#include "GlobalCommandController.hh"
#include "CliComm.hh"
#include "StringOp.hh"
#include "StringMap.hh"
#include "rapidsax.hh"
#include "unreachable.hh"
#include "memory.hh"
#include "stl.hh"

using std::string;
using std::vector;

namespace openmsx {

class SoftwareInfoTopic : public InfoTopic
{
public:
        SoftwareInfoTopic(InfoCommand& openMSXInfoCommand, RomDatabase& romDatabase);

        virtual void execute(const vector<TclObject>& tokens,
                             TclObject& result) const;
        virtual string help(const vector<string>& tokens) const;
        virtual void tabCompletion(vector<string>& tokens) const;

private:
	const RomDatabase& romDatabase;
};


typedef StringMap<unsigned> UnknownTypes;

class DBParser : public rapidsax::NullHandler
{
public:
	DBParser(RomDatabase::RomDB& db_, UnknownTypes& unknownTypes_,
	         CliComm& cliComm_)
		: db(db_)
		, unknownTypes(unknownTypes_)
		, cliComm(cliComm_)
		, state(BEGIN)
		, unknownLevel(0)
		, initialSize(db.size())
	{
	}

	// rapidsax handler interface
	void start(string_ref name);
	void attribute(string_ref name, string_ref value);
	void text(string_ref text);
	void stop();
	void doctype(string_ref text);

	string_ref getSystemID() const { return systemID; }

private:
	void addEntries();
	void addAllEntries();

	enum State {
		BEGIN,
		SOFTWAREDB,
		SOFTWARE,
		SYSTEM,
		TITLE,
		COMPANY,
		YEAR,
		COUNTRY,
		GENMSXID,
		DUMP_REMARK,
		DUMP_TEXT,
		DUMP,
		ORIGINAL,
		ROM,
		TYPE,
		START,
		HASH,
		END
	};

	struct Dump {
		string_ref remark;
		Sha1Sum hash;
		string_ref origData;
		RomType type;
		bool origValue;
	};

	RomDatabase::RomDB& db;
	UnknownTypes& unknownTypes;
	CliComm& cliComm;

	string_ref systemID;
	string_ref type;
	string_ref startVal;

	vector<Dump> dumps;
	string_ref system;
	string_ref title;
	string_ref company;
	string_ref year;
	string_ref country;
	int genMSXid;

	State state;
	unsigned unknownLevel;
	size_t initialSize;
};

void DBParser::start(string_ref tag)
{
	if (unknownLevel) {
		++unknownLevel;
		return;
	}

	switch (state) {
	case BEGIN:
		if (tag == "softwaredb") {
			state = SOFTWAREDB;
			return;
		}
		throw MSXException("Expected <softwaredb> as root tag.");
	case SOFTWAREDB:
		if (tag == "software") {
			system.clear();
			title.clear();
			company.clear();
			year.clear();
			country.clear();
			genMSXid = 0;
			dumps.clear();
			state = SOFTWARE;
			return;
		}
		break;
	case SOFTWARE: {
		char c = tag.front();
		tag.pop_front();
		switch (c) {
		case 's':
			if (tag == "ystem") {
				state = SYSTEM;
				return;
			}
			break;
		case 't':
			if (tag == "itle") {
				state = TITLE;
				return;
			}
			break;
		case 'c':
			if (tag == "ompany") {
				state = COMPANY;
				return;
			} else if (tag == "ountry") {
				state = COUNTRY;
				return;
			}
			break;
		case 'y':
			if (tag == "ear") {
				state = YEAR;
				return;
			}
			break;
		case 'g':
			if (tag == "enmsxid") {
				state = GENMSXID;
				return;
			}
			break;
		case 'd':
			if (tag == "ump") {
				dumps.resize(dumps.size() + 1);
				dumps.back().type = ROM_UNKNOWN;
				dumps.back().origValue = false;
				state = DUMP;
				return;
			}
			break;
		}
		break;
	}
	case DUMP: {
		char c = tag.front();
		tag.pop_front();
		switch (c) {
		case 'o':
			if (tag == "riginal") {
				dumps.back().origValue = false;
				state = ORIGINAL;
				return;
			}
			break;
		case 'm':
			if (tag == "egarom") {
				type.clear();
				startVal.clear();
				state = ROM;
				return;
			}
			break;
		case 'r':
			if (tag == "om") {
				type = "Mirrored";
				startVal.clear();
				state = ROM;
				return;
			}
			break;
		}
		break;
	}
	case ROM: {
		char c = tag.front();
		tag.pop_front();
		switch (c) {
		case 't':
			if (tag == "ype") {
				state = TYPE;
				return;
			}
			break;
		case 's':
			if (tag == "tart") {
				state = START;
				return;
			}
			break;
		case 'r':
			if (tag == "emark") {
				state = DUMP_REMARK;
				return;
			}
			break;
		case 'h':
			if (tag == "ash") {
				state = HASH;
				return;
			}
			break;
		}
		break;
	}
	case DUMP_REMARK:
		if (tag == "text") {
			state = DUMP_TEXT;
			return;
		}
		break;
	case SYSTEM:
	case TITLE:
	case COMPANY:
	case YEAR:
	case COUNTRY:
	case GENMSXID:
	case ORIGINAL:
	case TYPE:
	case START:
	case HASH:
	case DUMP_TEXT:
		break;

	case END:
		throw MSXException("Unexpected opening tag: " + tag);

	default:
		UNREACHABLE;
	}

	++unknownLevel;
}

void DBParser::attribute(string_ref name, string_ref value)
{
	if (unknownLevel) return;

	switch (state) {
	case ORIGINAL:
		if (name == "value") {
			dumps.back().origValue = StringOp::stringToBool(value);
		}
		break;
	case HASH:
	case BEGIN:
	case SOFTWAREDB:
	case SOFTWARE:
	case SYSTEM:
	case TITLE:
	case COMPANY:
	case YEAR:
	case COUNTRY:
	case GENMSXID:
	case DUMP_REMARK:
	case DUMP_TEXT:
	case DUMP:
	case ROM:
	case TYPE:
	case START:
	case END:
		break;
	default:
		UNREACHABLE;
	}
}

void DBParser::text(string_ref text)
{
	if (unknownLevel) return;

	switch (state) {
	case SYSTEM:
		system = text;
		break;
	case TITLE:
		title = text;
		break;
	case COMPANY:
		company = text;
		break;
	case YEAR:
		year = text;
		break;
	case COUNTRY:
		country = text;
		break;
	case GENMSXID:
		genMSXid = stoi(text);
		// TODO error checks?
		//	cliComm.printWarning(StringOp::Builder() <<
		//		"Ignoring bad Generation MSX id (genmsxid) "
		//		"in entry with title '" << title <<
		//		": " << data);
		break;
	case ORIGINAL:
		dumps.back().origData = text;
		break;
	case TYPE:
		type = text;
		break;
	case START:
		startVal = text;
		break;
	case HASH:
		dumps.back().hash = Sha1Sum(text);
		break;
	case DUMP_REMARK:
	case DUMP_TEXT:
		dumps.back().remark = text;
		break;
	case BEGIN:
	case SOFTWAREDB:
	case SOFTWARE:
	case DUMP:
	case ROM:
	case END:
		break;
	default:
		UNREACHABLE;
	}
}

// called on </software>
void DBParser::addEntries()
{
	if (!system.empty() && (system != "MSX")) {
		// skip non-MSX entries
		return;
	}

	for (auto& d : dumps) {
		db.emplace_back(d.hash, RomInfo(
			title, year, company, country,
			d.origValue, d.origData, d.remark, d.type,
			genMSXid));
	}
}

// called on </softwaredb>
void DBParser::addAllEntries()
{
	// Calculate boundary between old and new entries.
	//  old: [first, mid)   already sorted, no duplicates
	//  new: [mid, last)    not yet sorted, may have duplicates
	//    there may also be duplicates between old and new
	const auto first = db.begin();
	const auto last  = db.end();
	const auto mid = first + initialSize;
	if (mid == last) return; // no new entries

	// Sort new entries, old entries are already sorted.
	sort(mid, last, LessTupleElement<0>());

	// Filter duplicates from new entries. This is similar to the
	// unique() algorithm, except that it also warns about duplicates.
	auto it1 = mid;
	auto it2 = mid + 1;
	// skip initial non-duplicates
	while (it2 != last) {
		if (it1->first == it2->first) break;
		++it1; ++it2;
	}
	// move non-duplicates up
	while (it2 != last) {
		if (it1->first == it2->first) {
			cliComm.printWarning(
				"duplicate softwaredb entry SHA1: " +
				it2->first.toString());
		} else {
			++it1;
			*it1 = std::move(*it2);
		}
		++it2;
	}
	// actually erase the duplicates (typically none)
	db.erase(it1 + 1, last);
	// At this point both old and new entries are sorted and unique. But
	// there may still be duplicates between old and new.

	// Merge new and old entries. This is similar to the inplace_merge()
	// algorithm, except that duplicates (between old and new) are removed.
	if (first == mid) return; // no old entries (common case)
	RomDatabase::RomDB result;
	result.reserve(db.size());
	it1 = first;
	it2 = mid;
	// while both new and old still have elements
	while (it1 != mid && it2 != last) {
		if (it1->first < it2->first) {
			result.push_back(std::move(*it1));
			++it1;
		} else {
			if (it1->first != it2->first) { // *it2 < *it1
				result.push_back(std::move(*it2));
				++it2;
			} else {
				// pick old entry, silently ignore new
				result.push_back(std::move(*it1));
				++it1; ++it2;
			}
		}
	}
	// move remaining old or new entries (one of these is empty)
	move(it1, mid,  back_inserter(result));
	move(it2, last, back_inserter(result));

	// make result the new current database
	swap(result, db);
}

static const char* parseStart(string_ref s)
{
	// we expect "0x0000", "0x4000", "0x8000", "0xc000" or ""
	return ((s.size() == 6) && s.starts_with("0x")) ? (s.data() + 2) : nullptr;
}

void DBParser::stop()
{
	if (unknownLevel) {
		--unknownLevel;
		return;
	}

	switch (state) {
	case SOFTWAREDB:
		addAllEntries();
		state = END;
		break;
	case SOFTWARE:
		addEntries();
		state = SOFTWAREDB;
		break;
	case SYSTEM:
	case TITLE:
	case COMPANY:
	case YEAR:
	case COUNTRY:
	case GENMSXID:
		state = SOFTWARE;
		break;
	case DUMP:
		if (dumps.back().hash.empty()) {
			// no sha1 sum specified, drop this dump
			dumps.pop_back();
		}
		state = SOFTWARE;
		break;
	case ORIGINAL:
		state = DUMP;
		break;
	case ROM: {
		string_ref t = type;
		char buf[12];
		if (t == "Mirrored") {
			if (const char* start = parseStart(startVal)) {
				memcpy(buf, t.data(), 8);
				memcpy(buf + 8, start, 4);
				t = string_ref(buf, 12);
			}
		} else if (t == "Normal") {
			if (const char* start = parseStart(startVal)) {
				memcpy(buf, t.data(), 6);
				memcpy(buf + 6, start, 4);
				t = string_ref(buf, 10);
			}
		}
		RomType romType = RomInfo::nameToRomType(t);
		if (romType == ROM_UNKNOWN) {
			unknownTypes[t]++;
		}
		dumps.back().type = romType;
		state = DUMP;
		break;
	}
	case TYPE:
	case START:
	case HASH:
	case DUMP_REMARK:
		state = ROM;
		break;
	case DUMP_TEXT:
		state = DUMP_REMARK;
		break;
	case BEGIN:
	case END:
		throw MSXException("Unexpected closing tag");

	default:
		UNREACHABLE;
	}
}

void DBParser::doctype(string_ref text)
{
	auto pos1 = text.find(" SYSTEM \"");
	if (pos1 == string_ref::npos) return;
	auto t = text.substr(pos1 + 9);
	auto pos2 = t.find('"');
	if (pos2 == string_ref::npos) return;
	systemID = t.substr(0, pos2);
}

static void parseDB(CliComm& cliComm, const string& filename,
                    MemBuffer<char>& buf, RomDatabase::RomDB& db,
                    UnknownTypes& unknownTypes)
{
	File file(filename);
	auto size = file.getSize();
	buf.resize(size + 1);
	file.read(buf.data(), size);
	buf[size] = 0;

	DBParser handler(db, unknownTypes, cliComm);
	rapidsax::parse<rapidsax::trimWhitespace>(handler, buf.data());

	if (handler.getSystemID() != "softwaredb1.dtd") {
		throw rapidsax::ParseError(
			"Missing or wrong systemID.\n"
			"You're probably using an old incompatible file format.",
			nullptr);
	}
}

RomDatabase::RomDatabase(GlobalCommandController& commandController, CliComm& cliComm)
	: softwareInfoTopic(make_unique<SoftwareInfoTopic>(
		commandController.getOpenMSXInfoCommand(), *this))
{
	db.reserve(3500);
	UnknownTypes unknownTypes;
	// first user- then system-directory
	vector<string> paths = SystemFileContext().getPaths();
	for (auto& p : paths) {
		string filename = FileOperations::join(p, "softwaredb.xml");
		try {
			buffers.emplace_back();
			parseDB(cliComm, filename, buffers.back(), db, unknownTypes);
		} catch (rapidsax::ParseError& e) {
			cliComm.printWarning(StringOp::Builder() <<
				"Rom database parsing failed: " << e.what());
		} catch (MSXException& /*e*/) {
			// Ignore. It's not unusual the DB in the user
			// directory is not found. In case there's an error
			// with both user and system DB, we must give a
			// warning, but that's done below.
		}
	}
	if (db.empty()) {
		cliComm.printWarning(
			"Couldn't load software database.\n"
			"This may cause incorrect ROM mapper types to be used.");
	}
	if (!unknownTypes.empty()) {
		StringOp::Builder output;
		output << "Unknown mapper types in software database: ";
		for (auto& p : unknownTypes) {
			output << p.first() << " (" << p.second << "x); ";
		}
		cliComm.printWarning(output);
	}
}

RomDatabase::~RomDatabase()
{
}

const RomInfo* RomDatabase::fetchRomInfo(const Sha1Sum& sha1sum) const
{
	auto it = lower_bound(db.begin(), db.end(), sha1sum,
	                      LessTupleElement<0>());
	return ((it != db.end()) && (it->first == sha1sum))
		? &it->second : nullptr;
}


// SoftwareInfoTopic

SoftwareInfoTopic::SoftwareInfoTopic(InfoCommand& openMSXInfoCommand,
                                     RomDatabase& romDatabase_)
	: InfoTopic(openMSXInfoCommand, "software")
	, romDatabase(romDatabase_)
{
}

void SoftwareInfoTopic::execute(const vector<TclObject>& tokens,
                                TclObject& result) const
{
	if (tokens.size() != 3) {
		throw CommandException("Wrong number of parameters");
	}

	Sha1Sum sha1sum = Sha1Sum(tokens[2].getString());
	const RomInfo* romInfo = romDatabase.fetchRomInfo(sha1sum);
	if (!romInfo) {
		// no match found
		throw CommandException(
			"Software with sha1sum " + sha1sum.toString() + " not found");
	}

	result.addListElement("title");
	result.addListElement(romInfo->getTitle());
	result.addListElement("year");
	result.addListElement(romInfo->getYear());
	result.addListElement("company");
	result.addListElement(romInfo->getCompany());
	result.addListElement("country");
	result.addListElement(romInfo->getCountry());
	result.addListElement("orig_type");
	result.addListElement(romInfo->getOrigType());
	result.addListElement("remark");
	result.addListElement(romInfo->getRemark());
	result.addListElement("original");
	result.addListElement(romInfo->getOriginal());
	result.addListElement("mapper_type_name");
	result.addListElement(RomInfo::romTypeToName(romInfo->getRomType()));
	result.addListElement("genmsxid");
	result.addListElement(romInfo->getGenMSXid());
}

string SoftwareInfoTopic::help(const vector<string>& /*tokens*/) const
{
	return "Returns information about the software "
	       "given its sha1sum, in a paired list.";
}

void SoftwareInfoTopic::tabCompletion(vector<string>& /*tokens*/) const
{
	// no useful completion possible
}

} // namespace openmsx

#ifndef ROMINFO_HH
#define ROMINFO_HH

#include "RomTypes.hh"
#include "string_ref.hh"
#include <vector>
#include <utility>

namespace openmsx {

class RomInfo
{
public:
	RomInfo(string_ref title_,    string_ref year_,
                string_ref company_,  string_ref country_,
                bool original_,       string_ref origType_,
                string_ref remark_,  RomType romType_,
                int genMSXid_)
		: title   (title_)
		, year    (year_)
		, company (company_)
		, country (country_)
		, origType(origType_)
		, remark  (std::move(remark_))
		, romType(romType_)
		, genMSXid(genMSXid_)
		, original(original_)
	{
	}

	const string_ref getTitle()     const { return title; }
	const string_ref getYear()      const { return year; }
	const string_ref getCompany()   const { return company; }
	const string_ref getCountry()   const { return country; }
	const string_ref getOrigType()  const { return origType; }
	const string_ref getRemark()    const { return remark; }
	RomType          getRomType()   const { return romType; }
	bool             getOriginal()  const { return original; }
	int              getGenMSXid()  const { return genMSXid; }

	static RomType nameToRomType(string_ref name);
	static string_ref romTypeToName(RomType type);
	static std::vector<string_ref> getAllRomTypes();
	static string_ref getDescription(RomType type);
	static unsigned   getBlockSize  (RomType type);

private:
	string_ref title;
	string_ref year;
	string_ref company;
	string_ref country;
	string_ref origType;
	string_ref remark;
	RomType romType;
	int genMSXid;
	bool original;
};

} // namespace openmsx

#endif

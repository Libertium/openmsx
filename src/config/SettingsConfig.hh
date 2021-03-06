#ifndef SETTINGSCONFIG_HH
#define SETTINGSCONFIG_HH

#include "XMLElement.hh"
#include "noncopyable.hh"
#include <string>
#include <memory>

namespace openmsx {

class SettingsManager;
class FileContext;
class HotKey;
class GlobalCommandController;
class CommandController;
class SaveSettingsCommand;
class LoadSettingsCommand;

class SettingsConfig : private noncopyable
{
public:
	SettingsConfig(GlobalCommandController& globalCommandController,
	               HotKey& hotKey);
	~SettingsConfig();

	void loadSetting(const FileContext& context, const std::string& filename);
	void saveSetting(const std::string& filename = "");
	void setSaveSettings(bool save);
	void setSaveFilename(const FileContext& context, const std::string& filename);

	SettingsManager& getSettingsManager();
	XMLElement& getXMLElement() { return xmlElement; }

private:
	CommandController& commandController;

	const std::unique_ptr<SaveSettingsCommand> saveSettingsCommand;
	const std::unique_ptr<LoadSettingsCommand> loadSettingsCommand;

	const std::unique_ptr<SettingsManager> settingsManager;
	XMLElement xmlElement;
	HotKey& hotKey;
	std::string saveName;
	bool mustSaveSettings;
};

} // namespace openmsx

#endif

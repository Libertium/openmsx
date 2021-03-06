#include "GlobalSettings.hh"
#include "IntegerSetting.hh"
#include "BooleanSetting.hh"
#include "StringSetting.hh"
#include "SettingsConfig.hh"
#include "GlobalCommandController.hh"
#include "ThrottleManager.hh"
#include "EnumSetting.hh"
#include "memory.hh"
#include "build-info.hh"

namespace openmsx {

GlobalSettings::GlobalSettings(GlobalCommandController& commandController_)
	: commandController(commandController_)
{
	speedSetting = make_unique<IntegerSetting>(commandController, "speed",
	       "controls the emulation speed: higher is faster, 100 is normal",
	       100, 1, 1000000);
	pauseSetting = make_unique<BooleanSetting>(commandController, "pause",
	       "pauses the emulation", false, Setting::DONT_SAVE);
	powerSetting = make_unique<BooleanSetting>(commandController, "power",
	        "turn power on/off", false, Setting::DONT_SAVE);
	autoSaveSetting = make_unique<BooleanSetting>(commandController,
	        "save_settings_on_exit",
	        "automatically save settings when openMSX exits", true);
	pauseOnLostFocusSetting = make_unique<BooleanSetting>(commandController,
		"pause_on_lost_focus",
	       "pause emulation when the openMSX window loses focus", false);
	umrCallBackSetting = make_unique<StringSetting>(commandController,
	        "umr_callback", "Tcl proc to call when an UMR is detected", "");
	invalidPsgDirectionsSetting = make_unique<StringSetting>(commandController,
		"invalid_psg_directions_callback",
		"Tcl proc called when the MSX program has set invalid PSG port directions",
		"");
	EnumSetting<ResampledSoundDevice::ResampleType>::Map resampleMap = {
		{ "hq",   ResampledSoundDevice::RESAMPLE_HQ },
		{ "fast", ResampledSoundDevice::RESAMPLE_LQ },
		{ "blip", ResampledSoundDevice::RESAMPLE_BLIP } };
	resampleSetting = make_unique<EnumSetting<ResampledSoundDevice::ResampleType>>(
		commandController, "resampler", "Resample algorithm",
#if PLATFORM_DINGUX
		// For Dingux, LQ is good compromise between quality and performance
		ResampledSoundDevice::RESAMPLE_LQ,
#elif PLATFORM_ANDROID
		// For Android, BLIP is good compromise between quality and performance
		ResampledSoundDevice::RESAMPLE_BLIP,
#else
		// For other platforms, default setting may be changed in future
		ResampledSoundDevice::RESAMPLE_BLIP,
#endif
		resampleMap);

	throttleManager = make_unique<ThrottleManager>(commandController);

	getPowerSetting().attach(*this);
}

GlobalSettings::~GlobalSettings()
{
	getPowerSetting().detach(*this);
	commandController.getSettingsConfig().setSaveSettings(
		autoSaveSetting->getBoolean());
}

// Observer<Setting>
void GlobalSettings::update(const Setting& setting)
{
	if (&setting == &getPowerSetting()) { // either on or off
		// automatically unpause after a power off/on cycle
		// this solved a bug, but apart from that this behaviour also
		// makes more sense
		getPauseSetting().setBoolean(false);
	}
}

} // namespace openmsx

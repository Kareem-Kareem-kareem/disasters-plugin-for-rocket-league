#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "version.h"

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

class RLDisasters : public BakkesModPlugin::BakkesModPluginID
{
private:
    bool pluginEnabled = false;
    float currentGravity = -650.0f;
    std::string currentRoundItem = "Spikes";
    bool isMatchRunning = false;

    const std::vector<std::string> rumbleItems = {
        "GrapplingHook",
        "Spikes",
        "Magnet",
        "Freeze",
        "Boot",
        "Haymaker",
        "Tornado",
        "Swapper",
        "Disruptor",
        "PowerHitter",
        "Plunger"
    };

public:
    virtual void onLoad() override;
    virtual void onUnload() override;

    void OnMatchStart(std::string eventName);
    void OnMatchEnd(std::string eventName);
    void OnGoalScored(std::string eventName);
    void OnTick(std::string eventName);

    void ResetMatchState();
    void ApplyGravitySettings();
    void SelectNextRoundItem();
    void EnforcePersistentItem();
};

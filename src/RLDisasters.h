#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include <string>
#include <vector>

class RLDisasters : public BakkesMod::Plugin::BakkesModPlugin
{
private:
    bool pluginEnabled = false;
    float currentGravity = -650.0f;
    std::string currentRoundItem = "Spikes";
    bool isMatchRunning = false;

    const std::vector<std::string> rumbleItems = {
        "GrapplingHook", "Spikes", "Magnet", "Freeze", "Boot", 
        "Haymaker", "Tornado", "Swapper", "Disruptor", "PowerHitter", "Plunger"
    };

public:
    void onLoad() override;
    void onUnload() override;

    void OnMatchStart(std::string eventName);
    void OnMatchEnd(std::string eventName);
    void OnGoalScored(std::string eventName);
    void OnTick(std::string eventName);

    void ResetMatchState();
    void ApplyGravitySettings();
    void SelectNextRoundItem();
    void EnforcePersistentItem();
};

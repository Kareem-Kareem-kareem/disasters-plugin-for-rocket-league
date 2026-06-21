#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include <string>
#include <vector>

class RLDisasters : public BakkesMod::Plugin::BakkesModPlugin
{
public:
    void onLoad() override;
    void onUnload() override;

private:
    // State Flags and Variables
    bool pluginEnabled = false;
    bool isMatchRunning = false;
    float currentGravity = -650.0f;
    std::string currentRoundItem = "GrapplingHook";

    // Item Pool
    std::vector<std::string> rumbleItems = {
        "GrapplingHook", "Boot", "Freeze", "Spikes", 
        "Plunger", "Swapper", "Tornado", "PowerUp", 
        "Disruptor", "Spring", "Magnet"
    };

    // Event Hook Handlers
    void OnMatchStart(std::string eventName);
    void OnMatchEnd(std::string eventName);
    void OnTick(std::string eventName);

    // Helper Functions
    void SelectNextRoundItem();
    void EnforcePersistentItem();
    void ApplyGravitySettings();
    void ResetMatchState();
};

#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include <string>

class RLDisasters : public BakkesMod::Plugin::BakkesModPlugin
{
public:
    void onLoad() override;
    void onUnload() override;

private:
    bool pluginEnabled = false;
    float currentGravity = -650.0f;
    int currentRandomItemIndex = 1;

    void HookEvents();
    void UnhookEvents();

    void OnRoundStart(std::string eventName);
    void OnGoalScored(std::string eventName);

    void ChooseRandomRumbleItem();
    void ApplyGravitySettings();
    void ResetMatchState();
};

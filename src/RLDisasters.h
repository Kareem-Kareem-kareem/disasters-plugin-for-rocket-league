#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "IMGUI/imgui.h"
#include <string>

class RLDisasters : public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginSettingsWindow
{
public:
    void onLoad() override;
    void onUnload() override;

    // Settings Window Interface implementation
    void RenderSettings() override;
    std::string GetPluginName() override;

private:
    // Core Plugin State Flags
    bool pluginEnabled = false;
    float currentGravity = -650.0f;
    int currentRandomItemIndex = 1;

    // Event Registration Hooks
    void HookEvents();
    void UnhookEvents();

    // Event Receivers
    void OnRoundStart(std::string eventName);
    void OnGoalScored(std::string eventName);

    // Modification Utility Methods
    void ChooseRandomRumbleItem();
    void ApplyGravitySettings();
    void ResetMatchState();
};

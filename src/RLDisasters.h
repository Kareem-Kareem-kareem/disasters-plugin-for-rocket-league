#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "imgui.h" 
#include <string>
#include <vector>

class RLDisasters : public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginSettingsWindow
{
public:
    void onLoad() override;
    void onUnload() override;

    void RenderSettings() override;
    std::string GetPluginName() override;

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

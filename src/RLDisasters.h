#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include <string>

struct DisasterState {
    bool closestSpawn      = false;
    bool biggerGoals       = false;
    bool biggerField       = false;
    bool quickRumble       = false;
    bool persistentRumble  = false;
};

class RLDisasters : public BakkesMod::Plugin::BakkesModPlugin,
                    public BakkesMod::Plugin::PluginSettingsWindow
{
public:
    void onLoad() override;
    void onUnload() override;

    void RenderSettings() override;
    std::string GetPluginName() override { return "RL Disasters"; }
    void SetImGuiContext(uintptr_t ctx) override;

private:
    DisasterState disasters;

    int   goalsScored    = 0;
    float goalExtentMult = 1.0f;   // multiplier applied to LocalExtent
    float fieldScaleMult = 1.0f;   // track for display only

    float quickRumbleTimer    = 0.0f;
    float persistRumbleTimer  = 0.0f;

    void HookEvents();
    void UnhookEvents();

    void OnGoalScored(ServerWrapper server, void* params, std::string eventName);
    void OnTick(ServerWrapper server, void* params, std::string eventName);
    void OnSpawn(std::string eventName);

    void DoClosestSpawn();
    void DoGrowGoals();
    void DoGrowField();
    void TickQuickRumble(float delta);
    void TickPersistRumble(float delta);

    void RenderHUD(CanvasWrapper canvas);
    void ResetAll();
};

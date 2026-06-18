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

    // Fixed tracking variables used in the .cpp file
    int blueGoals = 0;
    int orangeGoals = 0;
    float baseGoalScale = 1.0f;
    
    float fieldScaleX = 1.0f;
    float fieldScaleY = 1.0f;

    float rumbleItemTimer = 0.0f;
    bool rumbleActive = false;

    void HookEvents();
    void UnhookEvents();

    void OnMatchStarted(std::string eventName);
    void OnGoalScored(std::string eventName);
    void OnTick(std::string eventName);
    void OnPlayerSpawned(std::string eventName);

    void ApplyClosestSpawn();
    Vector GetClosestSpawnToOwnGoal(CarWrapper car, ServerWrapper server);
    
    void ApplyBiggerGoals(int scoringTeam);
    void SetGoalScale(float scale);
    
    void ApplyBiggerField();
    
    void TickQuickRumble(float delta);
    void GiveRandomRumbleItem(CarWrapper car);
    
    void TickPersistentRumble();

    void RenderHUD(CanvasWrapper canvas);
    void ResetAll();
};

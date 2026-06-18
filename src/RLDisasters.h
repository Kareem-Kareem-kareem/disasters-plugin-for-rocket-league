#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include <string>
#include <vector>

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

    // Settings window
    void RenderSettings() override;
    std::string GetPluginName() override { return "RL Disasters"; }
    void SetImGuiContext(uintptr_t ctx) override;

private:
    // ── Disaster state ──────────────────────────────────────────────
    DisasterState disasters;

    // Goal scale tracking  (1 goal scored = +0.15 scale per team)
    int  blueGoals  = 0;
    int  orangeGoals = 0;
    float baseGoalScale = 1.0f;

    // Field scale tracking
    float fieldScaleX = 1.0f;
    float fieldScaleY = 1.0f;

    // Rumble timers
    float rumbleItemTimer = 0.0f;   // countdown for quick-rumble
    bool  rumbleActive    = false;

    // ── Hooks ────────────────────────────────────────────────────────
    void HookEvents();
    void UnhookEvents();

    // Event callbacks
    void OnMatchStarted(std::string eventName);
    void OnGoalScored(std::string eventName);
    void OnTick(std::string eventName);
    void OnPlayerSpawned(std::string eventName);

    // ── Disaster helpers ─────────────────────────────────────────────
    void ApplyClosestSpawn();
    void ApplyBiggerGoals(int scoringTeam);
    void ApplyBiggerField();
    void TickQuickRumble(float delta);
    void TickPersistentRumble();

    void ResetAll();

    // ── HUD overlay ──────────────────────────────────────────────────
    void RenderHUD(CanvasWrapper canvas);

    // Helpers
    Vector GetClosestSpawnToOwnGoal(CarWrapper car, ServerWrapper server);
    void   SetGoalScale(float scale);
    void   GiveRandomRumbleItem(CarWrapper car);
};

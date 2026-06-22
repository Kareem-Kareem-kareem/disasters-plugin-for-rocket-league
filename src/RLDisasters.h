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
    bool growingBallOn      = false;
    bool persistentRumbleOn = false;
    bool lowGravityGoalsOn  = false;
    bool chaosSpeedOn       = false;

    int   goalsScored        = 0;
    float ballScale          = 1.0f;
    float currentGravity     = -650.0f;
    float gravityStepPerGoal = 100.0f;
    float currentGameSpeed   = 1.0f;
    int   lastPhysicsFrame   = -1;
    bool  lastTickHadPickup  = false;
    std::string currentRumbleName = "none";

    // Rumble cycling list (logging only)
    std::vector<std::string> rumbleCycle = {
        "freeze", "spikes", "boot", "plunger", "grapple", "lasso",
        "spring", "boxingglove", "battarang", "disruptor",
        "magnet", "powerhitter", "haymaker", "ballcarrier"
    };
    int desiredRumbleIndex = 0;

    void HookEvents();
    void UnhookEvents();

    void OnMatchStarted(std::string eventName);
    void OnGoalScored(std::string eventName);
    void OnTick(std::string eventName);

    void GrowBall();
    void TickRumbleTracking();   // now also triggers auto‑use
    void ApplyLowGravityGoals();
    void ApplyChaosSpeed();

    void ResetAll();
};

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

    int   goalsScored      = 0;
    float ballScale        = 1.0f;
    float currentGravity   = -650.0f;
    int   lastPhysicsFrame = -1;

    // the 11 real freeplay rumble item cvars (confirmed via BakkesMod's
    // own "Undocumented console commands" wiki page)
    std::vector<std::string> rumbleAbilities = {
        "boot", "disruptor", "freezer", "grapplinghook", "haymaker",
        "magnetizer", "plunger", "powerhitter", "spike", "swapper", "tornado"
    };

    void HookEvents();
    void UnhookEvents();

    void OnMatchStarted(std::string eventName);
    void OnGoalScored(std::string eventName);
    void OnTick(std::string eventName);

    void GrowBall();
    void DisableAllRumble();
    void PickNewRumble();
    void ApplyLowGravityGoals();

    void ResetAll();
};

#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include <string>

class RLDisasters : public BakkesMod::Plugin::BakkesModPlugin
{
public:
    void onLoad() override;
    void onUnload() override;

private:
    bool persistentRumbleOn = false;
    bool lowGravityGoalsOn  = false;

    int goalsScored = 0;
    float currentGravity = -650.0f;
    float gravityStepPerGoal = 100.0f;
    int currentRandomItem = 1;

    void HookEvents();
    void UnhookEvents();

    void OnMatchStarted(std::string eventName);
    void OnGoalScored(std::string eventName);

    void ChooseRandomRumbleItem();
    void ApplyLowGravityGoals();
    void ResetAll();
};

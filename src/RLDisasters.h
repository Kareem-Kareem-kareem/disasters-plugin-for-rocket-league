#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include <string>

class RLDisasters : public BakkesMod::Plugin::BakkesModPlugin
{
public:
    void onLoad() override;
    void onUnload() override;

private:
    bool growingBallOn      = false;
    bool persistentRumbleOn = false;
    bool lowGravityGoalsOn  = false;

    int   goalsScored        = 0;
    float ballScale          = 1.0f;
    float currentGravity     = -650.0f;
    float gravityStepPerGoal = 100.0f;
    int   lastPhysicsFrame   = -1;
    
    std::string persistentItemCmd = "Spikes_TA"; 

    void HookEvents();
    void UnhookEvents();

    void OnMatchStarted(std::string eventName);
    void OnGoalScored(std::string eventName);
    void OnCarSpawned(std::string eventName);
    void OnKickoffStart(std::string eventName);
    void OnTick(std::string eventName);

    void GrowBall();
    void GivePersistentItem();
    void ApplyLowGravityGoals();

    void ResetAll();
};

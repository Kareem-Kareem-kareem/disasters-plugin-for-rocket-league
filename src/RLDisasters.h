#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include <string>

class RLDisasters : public BakkesMod::Plugin::BakkesModPlugin
{
public:
    void onLoad() override;
    void onUnload() override;

private:
    // running state, mirrors the cvars
    bool closestSpawnOn  = false;
    bool growingBallOn   = false;
    bool flickerBoostOn  = false;
    bool infiniteBoostOn = false;
    bool chaosGravityOn  = false;

    int   goalsScored   = 0;
    float ballScale     = 1.0f;
    float flickerTimer  = 0.0f;
    float gravityTimer  = 0.0f;
    float currentGravity = -650.0f;
    int   lastPhysicsFrame = -1;

    void HookEvents();
    void UnhookEvents();

    void OnMatchStarted(std::string eventName);
    void OnGoalScored(std::string eventName);
    void OnTick(std::string eventName);
    void OnPlayerSpawned(std::string eventName);

    void ApplyClosestSpawn();
    void GrowBall();
    void TickFlickerBoost(float delta);
    void ApplyInfiniteBoost();
    void TickChaosGravity(float delta);

    void ResetAll();
};

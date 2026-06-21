#include "RLDisasters.h"
#include "bakkesmod/wrappers/gamewrapper.h"
#include "bakkesmod/wrappers/gameobject/carwrapper.h"
#include "bakkesmod/wrappers/gameobject/ballwrapper.h"
#include "bakkesmod/wrappers/gameevent/serverwrapper.h"
#include <algorithm>
#include <cstdlib>

// pluginType 0 = no mode restriction. Growing Ball / Low Gravity Goals
// only DO anything when you are the host: Freeplay, Custom Training, or
// a BakkesMod LAN match started with "host". Persistent Rumble uses the
// sv_freeplay_rumble_enable_* cvars, which are freeplay-specific by name,
// so that one is freeplay only.
BAKKESMOD_PLUGIN(RLDisasters, "Rocket League Disasters", "1.0", 0)

// ════════════════════════════════════════════════════════════════════
//  onLoad / onUnload
// ════════════════════════════════════════════════════════════════════
void RLDisasters::onLoad()
{
    cvarManager->log("RLDisasters: loading");

    cvarManager->registerCvar("disasters_growingBall", "0", "Ball grows +8% per goal, caps at 2.5x", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            growingBallOn = cvar.getBoolValue();
            if (!growingBallOn) {
                ballScale = 1.0f;
                if (gameWrapper->IsInGame()) {
                    ServerWrapper server = gameWrapper->GetCurrentGameState();
                    if (!server.IsNull()) {
                        BallWrapper ball = server.GetBall();
                        if (!ball.IsNull()) ball.SetBallScale(1.0f);
                    }
                }
            }
        });

    cvarManager->registerCvar("disasters_persistentRumble", "0", "One random rumble ability is always active, swaps when someone scores", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            persistentRumbleOn = cvar.getBoolValue();
            if (persistentRumbleOn) {
                PickNewRumble();
            } else {
                DisableAllRumble();
            }
        });

    cvarManager->registerCvar("disasters_lowGravityGoals", "0", "Gravity gets weaker every goal scored, caps out floaty", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            lowGravityGoalsOn = cvar.getBoolValue();
            if (!lowGravityGoalsOn) {
                currentGravity = -650.0f;
                cvarManager->executeCommand("sv_soccar_gravity -650", false);
            }
        });

    cvarManager->registerNotifier("disasters_resetall", [this](std::vector<std::string>) {
        cvarManager->getCvar("disasters_growingBall").setValue(0);
        cvarManager->getCvar("disasters_persistentRumble").setValue(0);
        cvarManager->getCvar("disasters_lowGravityGoals").setValue(0);
        ResetAll();
    }, "Turns off every RL Disasters effect and resets state", PERMISSION_ALL);

    HookEvents();
    cvarManager->log("RLDisasters: loaded");
}

void RLDisasters::onUnload()
{
    UnhookEvents();
    ResetAll();
}

// ════════════════════════════════════════════════════════════════════
//  Hooks
// ════════════════════════════════════════════════════════════════════
void RLDisasters::HookEvents()
{
    gameWrapper->HookEvent("Function GameEvent_Soccar_TA.Active.StartRound",
        [this](std::string e) { OnMatchStarted(e); });

    gameWrapper->HookEvent("Function TAGame.Ball_TA.Explode",
        [this](std::string e) { OnGoalScored(e); });

    // Fires every physics tick (per car) while in a match — the
    // documented, game-thread-safe hook real plugins use for continuous
    // logic. (Currently unused by any of the three disasters, since none
    // need per-frame work, but kept hooked for future use / consistent
    // lifecycle with goal/match events.)
    gameWrapper->HookEvent("Function TAGame.Car_TA.SetVehicleInput",
        [this](std::string e) { OnTick(e); });
}

void RLDisasters::UnhookEvents()
{
    gameWrapper->UnhookEvent("Function GameEvent_Soccar_TA.Active.StartRound");
    gameWrapper->UnhookEvent("Function TAGame.Ball_TA.Explode");
    gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
}

// ════════════════════════════════════════════════════════════════════
//  Match lifecycle
// ════════════════════════════════════════════════════════════════════
void RLDisasters::OnMatchStarted(std::string)
{
    ResetAll();
    if (persistentRumbleOn) PickNewRumble();
}

void RLDisasters::OnGoalScored(std::string)
{
    if (!gameWrapper->IsInGame()) return;
    goalsScored++;

    if (growingBallOn)      GrowBall();
    if (persistentRumbleOn) PickNewRumble();
    if (lowGravityGoalsOn)  ApplyLowGravityGoals();
}

void RLDisasters::OnTick(std::string)
{
    if (!gameWrapper->IsInGame()) return;

    // Dedupe multiple cars firing the same physics tick — kept here for
    // consistency even though no per-frame disaster logic currently runs.
    int frame = gameWrapper->GetEngine().GetPhysicsFrame();
    if (frame == lastPhysicsFrame) return;
    lastPhysicsFrame = frame;
}

// ════════════════════════════════════════════════════════════════════
//  Disaster: Growing Ball
// ════════════════════════════════════════════════════════════════════
void RLDisasters::GrowBall()
{
    if (!gameWrapper->IsInGame()) return;
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;
    BallWrapper ball = server.GetBall();
    if (ball.IsNull()) return;

    ballScale = std::min<float>(ballScale + 0.08f, 2.5f);
    ball.SetBallScale(ballScale);
}

// ════════════════════════════════════════════════════════════════════
//  Disaster: Persistent Rumble
//  Turns off all 11 freeplay rumble item types, then turns on exactly
//  one at random. That single type stays active until a goal is scored,
//  at which point a new one is picked (can repeat — confirmed OK).
// ════════════════════════════════════════════════════════════════════
void RLDisasters::DisableAllRumble()
{
    for (const auto& ability : rumbleAbilities) {
        cvarManager->executeCommand("sv_freeplay_rumble_enable_" + ability + " 0", false);
    }
}

void RLDisasters::PickNewRumble()
{
    DisableAllRumble();
    int index = rand() % static_cast<int>(rumbleAbilities.size());
    cvarManager->executeCommand("sv_freeplay_rumble_enable_" + rumbleAbilities[index] + " 1", false);
}

// ════════════════════════════════════════════════════════════════════
//  Disaster: Low Gravity Goals
//  Gravity is negative (default -650). "Weaker" means closer to zero,
//  i.e. less downward pull, i.e. floatier. Each goal moves gravity 60
//  units toward zero, capped at -150 so it never flips sign or goes to
//  true zero-g.
// ════════════════════════════════════════════════════════════════════
void RLDisasters::ApplyLowGravityGoals()
{
    float nextGravity = currentGravity + 60.0f;
    float cap = -150.0f;
    currentGravity = std::min<float>(nextGravity, cap);
    cvarManager->executeCommand("sv_soccar_gravity " + std::to_string(currentGravity), false);
}

// ════════════════════════════════════════════════════════════════════
//  Reset
// ════════════════════════════════════════════════════════════════════
void RLDisasters::ResetAll()
{
    goalsScored    = 0;
    ballScale      = 1.0f;
    currentGravity = -650.0f;

    cvarManager->executeCommand("sv_soccar_gravity -650", false);
    DisableAllRumble();

    if (!gameWrapper->IsInGame()) return;
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;

    BallWrapper ball = server.GetBall();
    if (!ball.IsNull()) ball.SetBallScale(1.0f);
}

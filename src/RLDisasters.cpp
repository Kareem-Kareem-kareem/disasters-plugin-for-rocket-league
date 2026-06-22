#include "RLDisasters.h"
#include "bakkesmod/wrappers/gamewrapper.h"
#include "bakkesmod/wrappers/gameobject/carwrapper.h"
#include "bakkesmod/wrappers/gameobject/ballwrapper.h"
#include "bakkesmod/wrappers/gameevent/serverwrapper.h"
#include "bakkesmod/wrappers/engine/unrealstringwrapper.h"
#include "bakkesmod/wrappers/gameobject/rumblecomponent/RumblePickupComponentWrapper.h"
#include <algorithm>

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

    cvarManager->registerCvar("disasters_persistentRumble", "0", "Auto‑uses any rumble pickup you get, cycles the logged type on goal", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            persistentRumbleOn = cvar.getBoolValue();
            lastTickHadPickup = false;
        });

    cvarManager->registerCvar("disasters_lowGravityGoals", "0", "Gravity gets weaker every goal scored, caps out floaty", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            lowGravityGoalsOn = cvar.getBoolValue();
            if (!lowGravityGoalsOn) {
                currentGravity = -650.0f;
                cvarManager->executeCommand("sv_soccar_gravity -650", false);
            }
        });

    cvarManager->registerCvar("disasters_chaosSpeed", "0", "Game speed randomizes each goal (slow/normal/fast)", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            chaosSpeedOn = cvar.getBoolValue();
            if (!chaosSpeedOn) {
                currentGameSpeed = 1.0f;
                if (gameWrapper->IsInGame()) {
                    ServerWrapper server = gameWrapper->GetCurrentGameState();
                    if (!server.IsNull() && server.HasAuthority())
                        server.SetGameSpeed(1.0f);
                }
            }
        });

    cvarManager->registerCvar("disasters_gravityPerGoal", "100", "How much gravity weakens per goal (units toward zero)", true, true, 10, true, 400)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            gravityStepPerGoal = cvar.getFloatValue();
        });

    cvarManager->registerCvar("disasters_rumbleType", "freeze", "Rumble powerup to cycle to (only for logging)", true)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            std::string val = cvar.getStringValue();
            auto it = std::find(rumbleCycle.begin(), rumbleCycle.end(), val);
            if (it != rumbleCycle.end()) {
                desiredRumbleIndex = (int)std::distance(rumbleCycle.begin(), it);
                cvarManager->log("RLDisasters: desired rumble set to " + val);
            } else {
                cvarManager->log("RLDisasters: unknown rumble type \"" + val + "\"");
            }
        });

    cvarManager->registerNotifier("disasters_resetall", [this](std::vector<std::string>) {
        cvarManager->getCvar("disasters_growingBall").setValue(0);
        cvarManager->getCvar("disasters_persistentRumble").setValue(0);
        cvarManager->getCvar("disasters_lowGravityGoals").setValue(0);
        cvarManager->getCvar("disasters_chaosSpeed").setValue(0);
        ResetAll();
    }, "Turns off every RL Disasters effect and resets state", PERMISSION_ALL);

    cvarManager->registerNotifier("disasters_testballsize", [this](std::vector<std::string>) {
        if (!gameWrapper->IsInGame()) {
            cvarManager->log("RLDisasters TEST: not in game, aborting");
            return;
        }
        ServerWrapper server = gameWrapper->GetCurrentGameState();
        if (server.IsNull()) {
            cvarManager->log("RLDisasters TEST: server is null, aborting");
            return;
        }
        BallWrapper ball = server.GetBall();
        if (ball.IsNull()) {
            cvarManager->log("RLDisasters TEST: ball is null, aborting");
            return;
        }
        ball.SetBallScale(4.0f);
        cvarManager->log("RLDisasters TEST: called SetBallScale(4.0) — look at the ball NOW");
    }, "DIAGNOSTIC: forces ball to 4x scale instantly", PERMISSION_ALL);

    cvarManager->registerNotifier("disasters_checkpickup", [this](std::vector<std::string>) {
        if (!gameWrapper->IsInGame()) {
            cvarManager->log("RLDisasters TEST: not in game, aborting");
            return;
        }
        CarWrapper car = gameWrapper->GetLocalCar();
        if (car.IsNull()) {
            cvarManager->log("RLDisasters TEST: local car is null, aborting");
            return;
        }
        auto pickup = car.GetAttachedPickup();
        if (pickup.IsNull()) {
            cvarManager->log("RLDisasters TEST: you are NOT currently holding a rumble item (requires Rumble mutator to be on to ever hold one)");
        } else {
            cvarManager->log("RLDisasters TEST: you ARE currently holding a rumble item right now");
        }
    }, "DIAGNOSTIC: reports if you're currently holding a rumble item", PERMISSION_ALL);

    HookEvents();
    cvarManager->log("RLDisasters: loaded — all cvars registered, all hooks active");
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
    if (!gameWrapper->IsInGame()) return;

    cvarManager->log("RLDisasters: kickoff started");

    if (growingBallOn) GrowBall();
    if (chaosSpeedOn) ApplyChaosSpeed();

    if (persistentRumbleOn) {
        lastTickHadPickup = false;
        CarWrapper car = gameWrapper->GetLocalCar();
        if (!car.IsNull()) {
            auto pickup = car.GetAttachedPickup();
            std::string name = pickup.IsNull() ? "none" : pickup.GetPickupName().ToString();
            currentRumbleName = name;
            cvarManager->log("RLDisasters: kickoff — current rumble item: " + name);
        }
    }
}

void RLDisasters::OnGoalScored(std::string)
{
    if (!gameWrapper->IsInGame()) return;
    goalsScored++;

    cvarManager->log("RLDisasters: GOAL SCORED total=" + std::to_string(goalsScored)
        + " | growingBall=" + std::to_string(growingBallOn)
        + " lowGravity=" + std::to_string(lowGravityGoalsOn)
        + " chaosSpeed=" + std::to_string(chaosSpeedOn)
        + " rumble=" + std::to_string(persistentRumbleOn));

    if (lowGravityGoalsOn) ApplyLowGravityGoals();

    if (persistentRumbleOn) {
        desiredRumbleIndex = (desiredRumbleIndex + 1) % (int)rumbleCycle.size();
        cvarManager->log("RLDisasters: goal scored — next rumble would be " + rumbleCycle[desiredRumbleIndex]);
    }
}

void RLDisasters::OnTick(std::string)
{
    if (!gameWrapper->IsInGame()) return;

    int frame = gameWrapper->GetEngine().GetPhysicsFrame();
    if (frame == lastPhysicsFrame) return;
    lastPhysicsFrame = frame;

    if (persistentRumbleOn) {
        AutoUsePickup();  // instantly uses any pickup you're holding
        TickRumbleTracking(); // logs changes
    }
}

// ════════════════════════════════════════════════════════════════════
//  Disaster: Growing Ball
// ════════════════════════════════════════════════════════════════════
void RLDisasters::GrowBall()
{
    if (!gameWrapper->IsInGame()) {
        cvarManager->log("RLDisasters: GrowBall — not in game, skipping");
        return;
    }
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) {
        cvarManager->log("RLDisasters: GrowBall — server null, skipping");
        return;
    }
    BallWrapper ball = server.GetBall();
    if (ball.IsNull()) {
        cvarManager->log("RLDisasters: GrowBall — ball null, skipping");
        return;
    }
    ballScale = std::min<float>(ballScale + 0.08f, 2.5f);
    ball.SetBallScale(ballScale);
    cvarManager->log("RLDisasters: GrowBall — set scale to " + std::to_string(ballScale));
}

// ════════════════════════════════════════════════════════════════════
//  Disaster: Persistent Rumble – Auto‑use and logging
// ════════════════════════════════════════════════════════════════════
void RLDisasters::TickRumbleTracking()
{
    CarWrapper car = gameWrapper->GetLocalCar();
    if (car.IsNull()) return;

    auto pickup = car.GetAttachedPickup();
    bool hasPickupNow = !pickup.IsNull();

    if (hasPickupNow != lastTickHadPickup) {
        std::string newName = hasPickupNow ? pickup.GetPickupName().ToString() : "none";
        cvarManager->log("RLDisasters: Rumble change — \""
            + currentRumbleName + "\" -> \"" + newName + "\"");
        currentRumbleName = newName;
        lastTickHadPickup = hasPickupNow;
    }
}

void RLDisasters::AutoUsePickup()
{
    CarWrapper car = gameWrapper->GetLocalCar();
    if (car.IsNull()) return;

    auto pickup = car.GetAttachedPickup();
    if (pickup.IsNull()) return;

    // Immediately activate the powerup (use it)
    pickup.PickupStart();
    cvarManager->log("RLDisasters: auto‑used pickup: " + pickup.GetPickupName().ToString());
}

// ════════════════════════════════════════════════════════════════════
//  Disaster: Low Gravity Goals
// ════════════════════════════════════════════════════════════════════
void RLDisasters::ApplyLowGravityGoals()
{
    float nextGravity = currentGravity + gravityStepPerGoal;
    float cap = -150.0f;
    currentGravity = std::min<float>(nextGravity, cap);
    cvarManager->executeCommand("sv_soccar_gravity " + std::to_string(currentGravity), false);
    cvarManager->log("RLDisasters: ApplyLowGravityGoals — gravity now " + std::to_string(currentGravity));
}

// ════════════════════════════════════════════════════════════════════
//  Disaster: Chaos Speed
// ════════════════════════════════════════════════════════════════════
void RLDisasters::ApplyChaosSpeed()
{
    if (!gameWrapper->IsInGame()) {
        cvarManager->log("RLDisasters: ApplyChaosSpeed — not in game, skipping");
        return;
    }
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) {
        cvarManager->log("RLDisasters: ApplyChaosSpeed — server null, skipping");
        return;
    }
    if (!server.HasAuthority()) {
        cvarManager->log("RLDisasters: ApplyChaosSpeed — no authority (not hosting), skipping");
        return;
    }
    static const float speeds[] = { 0.5f, 0.7f, 1.0f, 1.5f, 2.0f };
    static const int   count    = 5;
    int index = (goalsScored * 3 + goalsScored) % count;
    currentGameSpeed = speeds[index];
    server.SetGameSpeed(currentGameSpeed);
    cvarManager->log("RLDisasters: ApplyChaosSpeed — speed now " + std::to_string(currentGameSpeed));
}

// ════════════════════════════════════════════════════════════════════
void RLDisasters::ResetAll()
{
    goalsScored      = 0;
    ballScale        = 1.0f;
    currentGravity   = -650.0f;
    currentGameSpeed = 1.0f;
    lastTickHadPickup = false;

    cvarManager->executeCommand("sv_soccar_gravity -650", false);

    if (!gameWrapper->IsInGame()) return;
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;

    BallWrapper ball = server.GetBall();
    if (!ball.IsNull()) ball.SetBallScale(1.0f);

    if (server.HasAuthority()) server.SetGameSpeed(1.0f);
}

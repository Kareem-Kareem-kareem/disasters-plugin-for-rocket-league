#include "RLDisasters.h"
#include "bakkesmod/wrappers/gamewrapper.h"
#include "bakkesmod/wrappers/gameobject/carwrapper.h"
#include "bakkesmod/wrappers/gameobject/ballwrapper.h"
#include "bakkesmod/wrappers/gameevent/serverwrapper.h"
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

    cvarManager->registerCvar("disasters_persistentRumble", "0", "Requires Rumble mutator: notifies you on the HUD log whenever you're holding (or not holding) a rumble item", true, true, 0, true, 1)
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

    cvarManager->registerCvar("disasters_gravityPerGoal", "100", "How much gravity weakens per goal (units toward zero)", true, true, 10, true, 400)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            gravityStepPerGoal = cvar.getFloatValue();
        });

    cvarManager->registerNotifier("disasters_resetall", [this](std::vector<std::string>) {
        cvarManager->getCvar("disasters_growingBall").setValue(0);
        cvarManager->getCvar("disasters_persistentRumble").setValue(0);
        cvarManager->getCvar("disasters_lowGravityGoals").setValue(0);
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
    lastTickHadPickup = false;
}

void RLDisasters::OnGoalScored(std::string)
{
    if (!gameWrapper->IsInGame()) return;
    goalsScored++;

    if (growingBallOn) {
        gameWrapper->SetTimeout([this](GameWrapper*) {
            GrowBall();
        }, 0.3f);
    }
    if (lowGravityGoalsOn) ApplyLowGravityGoals();
}

void RLDisasters::OnTick(std::string)
{
    if (!gameWrapper->IsInGame()) return;

    int frame = gameWrapper->GetEngine().GetPhysicsFrame();
    if (frame == lastPhysicsFrame) return;
    lastPhysicsFrame = frame;

    if (persistentRumbleOn) TickRumbleTracking();
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
// ════════════════════════════════════════════════════════════════════
void RLDisasters::TickRumbleTracking()
{
    CarWrapper car = gameWrapper->GetLocalCar();
    if (car.IsNull()) return;

    auto pickup = car.GetAttachedPickup();
    bool hasPickupNow = !pickup.IsNull();

    if (hasPickupNow != lastTickHadPickup) {
        if (hasPickupNow) {
            cvarManager->log("RLDisasters: you picked up a rumble item");
        } else {
            cvarManager->log("RLDisasters: your rumble item is gone (used or expired)");
        }
        lastTickHadPickup = hasPickupNow;
    }
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
}

// ════════════════════════════════════════════════════════════════════
//  Reset
// ════════════════════════════════════════════════════════════════════
void RLDisasters::ResetAll()
{
    goalsScored      = 0;
    ballScale        = 1.0f;
    currentGravity   = -650.0f;
    lastTickHadPickup = false;

    cvarManager->executeCommand("sv_soccar_gravity -650", false);

    if (!gameWrapper->IsInGame()) return;
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;

    BallWrapper ball = server.GetBall();
    if (!ball.IsNull()) ball.SetBallScale(1.0f);
}

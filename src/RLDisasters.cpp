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

    cvarManager->registerCvar("disasters_growingBall", "0", "Ball grows +8% per goal/respawn, caps at 2.5x", true, true, 0, true, 1)
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

    cvarManager->registerCvar("disasters_persistentRumble", "0", "Forces the item pool to always give you the selected rumble item", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            persistentRumbleOn = cvar.getBoolValue();
            UpdateRumbleCVars();
        });

    cvarManager->registerCvar("disasters_persistentRumbleItem", "spikes", "The item to lock (spikes, plunger, grapple, boot, freeze, punch, swapper, tornado, magnet, powerhit, disruptor)", true, false, 0, false, 0)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            persistentItemCmd = cvar.getStringValue();
            UpdateRumbleCVars();
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

    gameWrapper->HookEvent("Function GameEvent_Soccar_TA.Active.OnCarSpawned",
        [this](std::string e) { OnCarSpawned(e); });

    gameWrapper->HookEvent("Function TAGame.Car_TA.SetVehicleInput",
        [this](std::string e) { OnTick(e); });
}

void RLDisasters::UnhookEvents()
{
    gameWrapper->UnhookEvent("Function GameEvent_Soccar_TA.Active.StartRound");
    gameWrapper->UnhookEvent("Function TAGame.Ball_TA.Explode");
    gameWrapper->UnhookEvent("Function GameEvent_Soccar_TA.Active.OnCarSpawned");
    gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
}

// ════════════════════════════════════════════════════════════════════
//  Match lifecycle / Events
// ════════════════════════════════════════════════════════════════════
void RLDisasters::OnMatchStarted(std::string)
{
    UpdateRumbleCVars();
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

void RLDisasters::OnCarSpawned(std::string)
{
    if (!gameWrapper->IsInGame() || !growingBallOn) return;

    // Triggers when a player spawns or respawns after a demolition
    gameWrapper->SetTimeout([this](GameWrapper*) {
        GrowBall();
    }, 0.2f);
}

void RLDisasters::OnTick(std::string)
{
    if (!gameWrapper->IsInGame()) return;

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
// ════════════════════════════════════════════════════════════════════
void RLDisasters::UpdateRumbleCVars()
{
    // Reset all global item force locks back to normal behavior
    cvarManager->executeCommand("sv_rumble_allspikes 0", false);
    cvarManager->executeCommand("sv_rumble_allplungers 0", false);
    cvarManager->executeCommand("sv_rumble_allgrapplinghooks 0", false);
    cvarManager->executeCommand("sv_rumble_allboots 0", false);
    cvarManager->executeCommand("sv_rumble_allfreezers 0", false);
    cvarManager->executeCommand("sv_rumble_allhaymakers 0", false);
    cvarManager->executeCommand("sv_rumble_allswappers 0", false);
    cvarManager->executeCommand("sv_rumble_alltornados 0", false);
    cvarManager->executeCommand("sv_rumble_allmagnets 0", false);
    cvarManager->executeCommand("sv_rumble_allpowerhits 0", false);
    cvarManager->executeCommand("sv_rumble_alldisruptors 0", false);

    // If persistent rumble is enabled, lock the item pool down to your selection
    if (persistentRumbleOn) {
        if (persistentItemCmd == "spikes") cvarManager->executeCommand("sv_rumble_allspikes 1", false);
        else if (persistentItemCmd == "plunger") cvarManager->executeCommand("sv_rumble_allplungers 1", false);
        else if (persistentItemCmd == "grapple" || persistentItemCmd == "grapplinghook") cvarManager->executeCommand("sv_rumble_allgrapplinghooks 1", false);
        else if (persistentItemCmd == "boot" || persistentItemCmd == "kick") cvarManager->executeCommand("sv_rumble_allboots 1", false);
        else if (persistentItemCmd == "freeze" || persistentItemCmd == "freezer") cvarManager->executeCommand("sv_rumble_allfreezers 1", false);
        else if (persistentItemCmd == "punch" || persistentItemCmd == "haymaker") cvarManager->executeCommand("sv_rumble_allhaymakers 1", false);
        else if (persistentItemCmd == "swapper") cvarManager->executeCommand("sv_rumble_allswappers 1", false);
        else if (persistentItemCmd == "tornado") cvarManager->executeCommand("sv_rumble_alltornados 1", false);
        else if (persistentItemCmd == "magnet") cvarManager->executeCommand("sv_rumble_allmagnets 1", false);
        else if (persistentItemCmd == "powerhit") cvarManager->executeCommand("sv_rumble_allpowerhits 1", false);
        else if (persistentItemCmd == "disruptor" || persistentItemCmd == "boostff") cvarManager->executeCommand("sv_rumble_alldisruptors 1", false);
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

    cvarManager->executeCommand("sv_soccar_gravity -650", false);
    UpdateRumbleCVars();

    if (!gameWrapper->IsInGame()) return;
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;

    BallWrapper ball = server.GetBall();
    if (!ball.IsNull()) ball.SetBallScale(1.0f);
}

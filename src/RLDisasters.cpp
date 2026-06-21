#include "RLDisasters.h"
#include "bakkesmod/wrappers/gamewrapper.h"
#include "bakkesmod/wrappers/gameobject/carwrapper.h"
#include "bakkesmod/wrappers/gameobject/ballwrapper.h"
#include "bakkesmod/wrappers/gameevent/serverwrapper.h"
#include <algorithm>

BAKKESMOD_PLUGIN(RLDisasters, "Rocket League Disasters", "1.0", 0)

void RLDisasters::onLoad()
{
    cvarManager->log("RLDisasters: loading");

    cvarManager->registerCvar("disasters_growingBall", "0", "Ball grows +8% per goal/respawn/kickoff, caps at 2.5x", true, true, 0, true, 1)
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

    cvarManager->registerCvar("disasters_persistentRumble", "0", "Forces you to always hold your selected rumble item continuously", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            persistentRumbleOn = cvar.getBoolValue();
        });

    cvarManager->registerCvar("disasters_persistentRumbleItem", "spikes", "The rumble item to lock", true, false, 0, false, 0)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            std::string input = cvar.getStringValue();
            if (input == "spikes") persistentItemCmd = "Spikes_TA";
            else if (input == "plunger") persistentItemCmd = "Plunger_TA";
            else if (input == "grapple" || input == "grapplinghook") persistentItemCmd = "GrapplingHook_TA";
            else if (input == "boot" || input == "kick") persistentItemCmd = "Spring_TA";
            else if (input == "freeze" || input == "freezer") persistentItemCmd = "Freeze_TA";
            else if (input == "punch" || input == "haymaker") persistentItemCmd = "BallBrick_TA";
            else if (input == "swapper") persistentItemCmd = "Swapper_TA";
            else if (input == "tornado") persistentItemCmd = "Tornado_TA";
            else if (input == "magnet") persistentItemCmd = "Magnet_TA";
            else if (input == "powerhit") persistentItemCmd = "SuperBoost_TA";
            else if (input == "disruptor" || input == "boostff") persistentItemCmd = "BoosterPlayer_TA";
            else persistentItemCmd = "Spikes_TA";
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

void RLDisasters::HookEvents()
{
    gameWrapper->HookEvent("Function GameEvent_Soccar_TA.Active.StartRound",
        [this](std::string e) { OnMatchStarted(e); });

    gameWrapper->HookEvent("Function TAGame.Ball_TA.Explode",
        [this](std::string e) { OnGoalScored(e); });

    gameWrapper->HookEvent("Function GameEvent_Soccar_TA.Active.OnCarSpawned",
        [this](std::string e) { OnCarSpawned(e); });

    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.OnKickoffStart",
        [this](std::string e) { OnKickoffStart(e); });

    gameWrapper->HookEvent("Function TAGame.Car_TA.SetVehicleInput",
        [this](std::string e) { OnTick(e); });
}

void RLDisasters::UnhookEvents()
{
    gameWrapper->UnhookEvent("Function GameEvent_Soccar_TA.Active.StartRound");
    gameWrapper->UnhookEvent("Function TAGame.Ball_TA.Explode");
    gameWrapper->UnhookEvent("Function GameEvent_Soccar_TA.Active.OnCarSpawned");
    gameWrapper->UnhookEvent("Function TAGame.GameEvent_Soccar_TA.OnKickoffStart");
    gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
}

void RLDisasters::OnMatchStarted(std::string)
{
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

    gameWrapper->SetTimeout([this](GameWrapper*) {
        GrowBall();
    }, 0.2f);
}

void RLDisasters::OnKickoffStart(std::string)
{
    if (!gameWrapper->IsInGame() || !growingBallOn) return;

    gameWrapper->SetTimeout([this](GameWrapper*) {
        GrowBall();
    }, 0.1f);
}

void RLDisasters::OnTick(std::string)
{
    if (!gameWrapper->IsInGame()) return;

    int frame = gameWrapper->GetEngine().GetPhysicsFrame();
    if (frame == lastPhysicsFrame) return;
    lastPhysicsFrame = frame;

    if (persistentRumbleOn) GivePersistentItem();
}

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

void RLDisasters::GivePersistentItem()
{
    CarWrapper car = gameWrapper->GetLocalCar();
    if (car.IsNull()) return;

    auto pickup = car.GetAttachedPickup();
    if (pickup.IsNull()) {
        gameWrapper->ExecuteUnrealCommand("cheat giveitem " + persistentItemCmd);
    }
}

void RLDisasters::ApplyLowGravityGoals()
{
    float nextGravity = currentGravity + gravityStepPerGoal;
    float cap = -150.0f;
    currentGravity = std::min<float>(nextGravity, cap);
    cvarManager->executeCommand("sv_soccar_gravity " + std::to_string(currentGravity), false);
}

void RLDisasters::ResetAll()
{
    goalsScored      = 0;
    ballScale        = 1.0f;
    currentGravity   = -650.0f;

    cvarManager->executeCommand("sv_soccar_gravity -650", false);

    if (!gameWrapper->IsInGame()) return;
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;

    BallWrapper ball = server.GetBall();
    if (!ball.IsNull()) ball.SetBallScale(1.0f);
}

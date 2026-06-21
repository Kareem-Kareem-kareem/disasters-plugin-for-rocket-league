#include "RLDisasters.h"
#include "bakkesmod/wrappers/gamewrapper.h"
#include "bakkesmod/wrappers/gameobject/carwrapper.h"
#include "bakkesmod/wrappers/gameobject/ballwrapper.h"
#include "bakkesmod/wrappers/gameevent/serverwrapper.h"
#include <algorithm>

// pluginType 0 = no mode restriction. Growing Ball / Low Gravity Goals
// only DO anything when you are the host: Freeplay, Custom Training, or
// a BakkesMod LAN match started with "host".
//
// Persistent Rumble: real rumble items only exist when the match itself
// is hosted with the RUMBLE MUTATOR turned on (selected in the match
// setup screen, same as picking any other mutator). This is confirmed by
// a real published plugin ("Custom Rumble" on bakkesplugins.com, "Only
// works in LAN matches... Must be used with the default rumble mutator").
// The sv_freeplay_rumble_enable_* cvars used in an earlier version of
// this plugin were wrong — those only affect a separate Freeplay-only
// Rumble TRAINING mode, unrelated to LAN matches, which is why they did
// nothing for LAN testing. With Rumble mutator on, the game spawns real
// pickup items on its own; this plugin detects via CarWrapper's attached
// pickup (the same approach used by a real, multiplayer-tested plugin,
// ZpeedTube/rumble-recharge on GitHub) when you have no pickup, and once
// you pick one up it tracks that you're holding something. There is no
// SDK function to spawn a brand-new pickup or force a specific ability
// from nothing — every wrapper class only wraps objects the game itself
// already created.
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

    // DIAGNOSTIC: jumps the ball to an unmistakable 4x size instantly,
    // bypassing goals/timers entirely. Confirmed working — you've already
    // verified the ball visibly grows when this runs.
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

    // DIAGNOSTIC: reports whether your car is currently holding a rumble
    // pickup right now, using the same CarWrapper::GetAttachedPickup()
    // call confirmed working in a real, multiplayer-tested plugin
    // (ZpeedTube/rumble-recharge). REQUIRES the Rumble mutator to be on
    // when you host the match — without it the game never spawns any
    // pickups to detect in the first place.
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

    // Fires every physics tick (per car) while in a match — the
    // documented, game-thread-safe hook real plugins use for continuous
    // logic. Used here to poll rumble-pickup status every tick.
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
    // NOTE: this fires on every round restart, including right after a
    // goal in Freeplay/Soccar — NOT just when a fresh match begins. We
    // deliberately do NOT call ResetAll() here anymore, since that was
    // wiping ball scale / gravity progress immediately after every goal,
    // making Growing Ball and Low Gravity Goals look like they did
    // nothing. Progress now only resets via the explicit Reset All
    // button/notifier, or when the plugin loads/unloads.
    lastTickHadPickup = false;
}

void RLDisasters::OnGoalScored(std::string)
{
    if (!gameWrapper->IsInGame()) return;
    goalsScored++;

    // IMPORTANT: this fires the instant the ball explodes — at that exact
    // moment the old ball object is about to be destroyed and replaced
    // with a freshly spawned one. Calling SetBallScale right here lands
    // on the dying ball, not the new one, so the scale never visibly
    // sticks. We delay slightly so GrowBall runs after the new ball
    // exists. Gravity isn't tied to a specific object instance, so it
    // doesn't have this problem and can run immediately.
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

    // Dedupe multiple cars firing the same physics tick.
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
//  Requires the Rumble mutator to be enabled when hosting the match —
//  that's what makes the game spawn real pickup items at all. This
//  plugin doesn't (and per the SDK, can't) create or force a specific
//  ability; it tracks via CarWrapper::GetAttachedPickup() whether you're
//  currently holding one, and logs when that changes, using the same
//  approach confirmed working in real multiplayer by the published
//  ZpeedTube/rumble-recharge plugin.
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
//  Gravity is negative (default -650). "Weaker" means closer to zero,
//  i.e. less downward pull, i.e. floatier. Each goal moves gravity
//  toward zero by gravityStepPerGoal (adjustable, default 100), capped
//  at -150 so it never flips sign or goes to true zero-g.
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

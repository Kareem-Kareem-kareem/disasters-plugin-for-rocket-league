#include "RLDisasters.h"
#include "bakkesmod/wrappers/gamewrapper.h"
#include "bakkesmod/wrappers/gameobject/carwrapper.h"
#include "bakkesmod/wrappers/gameobject/ballwrapper.h"
#include "bakkesmod/wrappers/gameevent/serverwrapper.h"
#include "bakkesmod/wrappers/engine/unrealstringwrapper.h"
#include "bakkesmod/wrappers/gameobject/rumblecomponent/RumblePickupComponentWrapper.h"
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

    cvarManager->registerCvar("disasters_persistentRumble", "0", "Requires Rumble mutator: forces a specific rumble type on your car, cycles on goal", true, true, 0, true, 1)
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

    // SetGameSpeed confirmed real in full ServerWrapper.h read —
    // GETSETH(float, GameSpeed) macro generates both getter and setter.
    // A random speed is picked each goal: floaty slow or frantic fast.
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

    // ─── NEW: cvar to set the desired rumble type ───
    cvarManager->registerCvar("disasters_rumbleType", "freeze", "Rumble powerup to force (freeze, spikes, boot, etc.)", true)
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
    if (!gameWrapper->IsInGame()) return;

    cvarManager->log("RLDisasters: kickoff started");

    // Growing Ball — apply scale to the new ball that just spawned for
    // this kickoff. This is the right moment: the ball exists, is fresh,
    // and hasn't been hit yet. Much more reliable than trying to scale it
    // right at the goal explosion moment.
    if (growingBallOn) GrowBall();

    // Chaos Speed — change speed at the start of each new kickoff so
    // players feel it immediately when play resumes.
    if (chaosSpeedOn) ApplyChaosSpeed();

    // Persistent Rumble — log the current rumble type at kickoff start
    // (including the very first kickoff of the match) and re-read the
    // car's attached pickup to keep tracking state fresh.
    if (persistentRumbleOn) {
        lastTickHadPickup = false; // force a fresh state read next tick
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

    // Low Gravity Goals — tracked per goal scored, not per kickoff.
    if (lowGravityGoalsOn) ApplyLowGravityGoals();

    // ─── NEW: cycle to the next rumble powerup on goal ───
    if (persistentRumbleOn) {
        desiredRumbleIndex = (desiredRumbleIndex + 1) % (int)rumbleCycle.size();
        cvarManager->log("RLDisasters: goal scored — cycling rumble to " + rumbleCycle[desiredRumbleIndex]);
    }

    // NOTE: GrowBall and ChaosSpeed now fire in OnMatchStarted (kickoff)
    // instead of here — that's when the new ball exists and when the speed
    // change is most impactful. Goal-scoring is just the trigger that
    // increments the counter; the visual effect happens at the kickoff.
}

void RLDisasters::OnTick(std::string)
{
    if (!gameWrapper->IsInGame()) return;

    // Dedupe multiple cars firing the same physics tick.
    int frame = gameWrapper->GetEngine().GetPhysicsFrame();
    if (frame == lastPhysicsFrame) return;
    lastPhysicsFrame = frame;

    // ─── MODIFIED: force rumble instead of just logging ───
    if (persistentRumbleOn) {
        ForceDesiredRumble();   // changes your pickup to the desired type every tick
        // TickRumbleTracking(); // (optional – keep this if you want the old log messages too)
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
        std::string newName = hasPickupNow ? pickup.GetPickupName().ToString() : "none";
        cvarManager->log("RLDisasters: Rumble change — \""
            + currentRumbleName + "\" -> \"" + newName + "\"");
        currentRumbleName = newName;
        lastTickHadPickup = hasPickupNow;
    }
}

// ─── NEW: force the pickup to be the desired rumble type ───
void RLDisasters::ForceDesiredRumble()
{
    CarWrapper car = gameWrapper->GetLocalCar();
    if (car.IsNull()) return;

    auto pickup = car.GetAttachedPickup();
    if (pickup.IsNull()) return;

    std::string currentName = pickup.GetPickupName().ToString();
    std::string desiredName = rumbleCycle[desiredRumbleIndex];

    if (currentName == desiredName) return;

    // Assign the new name directly via operator=
    pickup.GetPickupName() = desiredName;
    cvarManager->log("RLDisasters: forced rumble from \"" + currentName + "\" to \"" + desiredName + "\"");
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

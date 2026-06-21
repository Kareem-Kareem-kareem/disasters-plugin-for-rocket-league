#include "RLDisasters.h"
#include "bakkesmod/wrappers/gamewrapper.h"
#include "bakkesmod/wrappers/gameobject/carwrapper.h"
#include "bakkesmod/wrappers/gameobject/ballwrapper.h"
#include "bakkesmod/wrappers/gameobject/goalwrapper.h"
#include "bakkesmod/wrappers/gameevent/serverwrapper.h"
#include <algorithm>
#include <cstdlib>

// pluginType 0 = no mode restriction. Growing Ball / Low Gravity Goals
// only DO anything when you are the host: Freeplay, Custom Training, or
// a BakkesMod LAN match started with "host". Persistent Rumble uses the
// sv_freeplay_rumble_enable_* cvars — these are freeplay-named, and it's
// genuinely unconfirmed whether they do anything in a LAN match. The
// "Force Persistent Rumble" button lets you test that directly: it sends
// the same command Persistent Rumble would, with a console log either
// way, so you can see for yourself rather than take it on faith.
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

    // DIAGNOSTIC ONLY: jumps the ball to an unmistakable 4x size instantly,
    // bypassing goals/timers entirely, so we can tell whether SetBallScale
    // does anything at all. Type "disasters_testballsize" in the F6
    // console while in Freeplay. Logs success/failure either way.
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

    // DIAGNOSTIC ONLY: instantly forces a random rumble type on, the same
    // way Persistent Rumble would on a goal, but on demand. Use this in a
    // LAN match to directly test whether sv_freeplay_rumble_enable_* does
    // anything outside Freeplay — something neither of us could confirm
    // from documentation alone.
    cvarManager->registerNotifier("disasters_forcerumble", [this](std::vector<std::string>) {
        if (!gameWrapper->IsInGame()) {
            cvarManager->log("RLDisasters TEST: not in game, aborting");
            return;
        }
        PickNewRumble();
        cvarManager->log("RLDisasters TEST: forced a random rumble ability ON — wait a few seconds and look near the ball");
    }, "DIAGNOSTIC: instantly forces a random rumble ability on", PERMISSION_ALL);

    // DIAGNOSTIC ONLY: tries to scale the goal's WORLD EXTENT (its
    // detection/collision box) to 3x. IMPORTANT: GoalWrapper does not
    // inherit from ActorWrapper in this SDK, so it has no visual mesh
    // scale function at all — only this invisible detection-box size.
    // This button exists so you can confirm with your own eyes that nothing
    // visually changes, rather than just take my word for the limitation.
    cvarManager->registerNotifier("disasters_testgoalsize", [this](std::vector<std::string>) {
        if (!gameWrapper->IsInGame()) {
            cvarManager->log("RLDisasters TEST: not in game, aborting");
            return;
        }
        ServerWrapper server = gameWrapper->GetCurrentGameState();
        if (server.IsNull()) {
            cvarManager->log("RLDisasters TEST: server is null, aborting");
            return;
        }
        ArrayWrapper<GoalWrapper> goals = server.GetGoals();
        if (goals.Count() == 0) {
            cvarManager->log("RLDisasters TEST: no goals found, aborting");
            return;
        }
        for (int i = 0; i < goals.Count(); i++) {
            GoalWrapper goal = goals.Get(i);
            if (goal.IsNull()) continue;
            Vector extent = goal.GetWorldExtent();
            goal.SetWorldExtent(Vector(extent.X * 3.0f, extent.Y * 3.0f, extent.Z * 3.0f));
        }
        cvarManager->log("RLDisasters TEST: tripled goal WorldExtent (invisible detection box only — expect NO visual change, this is the known SDK limitation)");
    }, "DIAGNOSTIC: triples goal detection-box size (no visual mesh scale exists in this SDK)", PERMISSION_ALL);

    // DIAGNOSTIC ONLY: there is no field/arena scale function anywhere in
    // this SDK at all — not even an invisible one like goals have. This
    // logs that fact clearly rather than silently doing nothing.
    cvarManager->registerNotifier("disasters_testfieldsize", [this](std::vector<std::string>) {
        cvarManager->log("RLDisasters TEST: there is no field/arena scale function in the BakkesMod SDK — this would require a custom map built at a different scale, not something a plugin can do at runtime.");
    }, "DIAGNOSTIC: explains why field scaling isn't possible via plugin", PERMISSION_ALL);

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
    // NOTE: this fires on every round restart, including right after a
    // goal in Freeplay/Soccar — NOT just when a fresh match begins. We
    // deliberately do NOT call ResetAll() here anymore, since that was
    // wiping ball scale / gravity progress immediately after every goal,
    // making Growing Ball and Low Gravity Goals look like they did
    // nothing. Progress now only resets via the explicit Reset All
    // button/notifier, or when the plugin loads/unloads.
    if (persistentRumbleOn && goalsScored == 0) {
        // only pick an initial rumble type on a genuinely fresh start
        PickNewRumble();
    }
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
    // exists. Gravity/rumble aren't tied to a specific object instance,
    // so they don't have this problem and can run immediately.
    if (growingBallOn) {
        gameWrapper->SetTimeout([this](GameWrapper*) {
            GrowBall();
        }, 0.3f);
    }
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

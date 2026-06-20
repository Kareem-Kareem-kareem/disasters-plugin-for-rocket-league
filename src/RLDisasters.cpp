#include "RLDisasters.h"
#include "bakkesmod/wrappers/gamewrapper.h"
#include "bakkesmod/wrappers/gameobject/carwrapper.h"
#include "bakkesmod/wrappers/gameobject/ballwrapper.h"
#include "bakkesmod/wrappers/gameobject/vehiclewrapper.h"
#include "bakkesmod/wrappers/gameobject/carcomponent/boostwrapper.h"
#include "bakkesmod/wrappers/gameevent/serverwrapper.h"
#include <algorithm>
#include <cmath>
#include <vector>

// pluginType 0 = no mode restriction. This plugin only DOES anything when
// you are the host: Freeplay, Custom Training, or a BakkesMod LAN match
// started with the "host" console command. In a normal online match the
// real Rocket League server owns car/ball/world state, so these calls are
// silently ignored there — that's a BakkesMod limitation, not a bug here.
BAKKESMOD_PLUGIN(RLDisasters, "Rocket League Disasters", "1.0", 0)

// ════════════════════════════════════════════════════════════════════
//  onLoad / onUnload
// ════════════════════════════════════════════════════════════════════
void RLDisasters::onLoad()
{
    cvarManager->log("RLDisasters: loading");

    cvarManager->registerCvar("disasters_closestSpawn", "0", "Spawn in line with your own goal", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            closestSpawnOn = cvar.getBoolValue();
        });

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

    cvarManager->registerCvar("disasters_flickerBoost", "0", "Everyone's boost flips unlimited/normal every second", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            flickerBoostOn = cvar.getBoolValue();
            flickerTimer = 0.0f;
        });

    cvarManager->registerCvar("disasters_infiniteBoost", "0", "Everyone has permanent unlimited boost", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            infiniteBoostOn = cvar.getBoolValue();
            if (!infiniteBoostOn && gameWrapper->IsInGame()) {
                ServerWrapper server = gameWrapper->GetCurrentGameState();
                if (!server.IsNull()) {
                    ArrayWrapper<CarWrapper> cars = server.GetCars();
                    for (int i = 0; i < cars.Count(); i++) {
                        CarWrapper car = cars.Get(i);
                        if (car.IsNull()) continue;
                        BoostWrapper boost = car.GetBoostComponent();
                        if (!boost.IsNull()) boost.SetUnlimitedBoost2(false);
                    }
                }
            }
        });

    cvarManager->registerCvar("disasters_chaosGravity", "0", "Gravity randomly drifts every few seconds", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            chaosGravityOn = cvar.getBoolValue();
            if (!chaosGravityOn) {
                currentGravity = -650.0f;
                cvarManager->executeCommand("sv_soccar_gravity -650", false);
            }
        });

    cvarManager->registerNotifier("disasters_resetall", [this](std::vector<std::string>) {
        cvarManager->getCvar("disasters_closestSpawn").setValue(0);
        cvarManager->getCvar("disasters_growingBall").setValue(0);
        cvarManager->getCvar("disasters_flickerBoost").setValue(0);
        cvarManager->getCvar("disasters_infiniteBoost").setValue(0);
        cvarManager->getCvar("disasters_chaosGravity").setValue(0);
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

    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.InitPlayer",
        [this](std::string e) { OnPlayerSpawned(e); });

    // Fires every physics tick (per car) while in a match — this is the
    // documented, game-thread-safe hook real plugins use for continuous
    // logic. Safe to call SetLocation/SetBallScale/etc. from here.
    gameWrapper->HookEvent("Function TAGame.Car_TA.SetVehicleInput",
        [this](std::string e) { OnTick(e); });
}

void RLDisasters::UnhookEvents()
{
    gameWrapper->UnhookEvent("Function GameEvent_Soccar_TA.Active.StartRound");
    gameWrapper->UnhookEvent("Function TAGame.Ball_TA.Explode");
    gameWrapper->UnhookEvent("Function TAGame.GameEvent_Soccar_TA.InitPlayer");
    gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
}

// ════════════════════════════════════════════════════════════════════
//  Match lifecycle
// ════════════════════════════════════════════════════════════════════
void RLDisasters::OnMatchStarted(std::string)
{
    ResetAll();
}

void RLDisasters::OnGoalScored(std::string)
{
    if (!gameWrapper->IsInGame()) return;
    goalsScored++;
    if (growingBallOn) GrowBall();
}

void RLDisasters::OnTick(std::string)
{
    if (!gameWrapper->IsInGame()) return;

    // SetVehicleInput fires once PER CAR per physics tick, so in a match
    // with multiple cars this callback runs multiple times per frame.
    // Use the engine's physics frame counter to make sure our timers only
    // advance once per actual tick, regardless of car count.
    int frame = gameWrapper->GetEngine().GetPhysicsFrame();
    if (frame == lastPhysicsFrame) return;
    lastPhysicsFrame = frame;

    const float delta = 1.0f / 60.0f; // matches default physics framerate

    if (flickerBoostOn)  TickFlickerBoost(delta);
    if (infiniteBoostOn) ApplyInfiniteBoost();
    if (chaosGravityOn)  TickChaosGravity(delta);
}

void RLDisasters::OnPlayerSpawned(std::string)
{
    if (closestSpawnOn) ApplyClosestSpawn();
}

// ════════════════════════════════════════════════════════════════════
//  Disaster: Closest Spawn
//  Moves the local car to the spot directly in line with its own goal.
// ════════════════════════════════════════════════════════════════════
void RLDisasters::ApplyClosestSpawn()
{
    if (!gameWrapper->IsInGame()) return;
    CarWrapper car = gameWrapper->GetLocalCar();
    if (car.IsNull()) return;

    unsigned char team = car.GetTeamNum2();

    // Field goals sit at Y = ±5120. Park the car ~2300 units out, centred on X.
    float spawnY = (team == 0) ? -2304.0f : 2304.0f;
    Vector spawnPos(0.0f, spawnY, 18.0f);

    car.SetLocation(spawnPos);
    car.SetVelocity(Vector(0, 0, 0));
    car.SetAngularVelocity(Vector(0, 0, 0), false);

    Rotator rot;
    rot.Pitch = 0;
    rot.Roll  = 0;
    rot.Yaw   = (team == 0) ? 16384 : -16384; // face up-field / down-field
    car.SetRotation(rot);
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

    ballScale = std::min(ballScale + 0.08f, 2.5f);
    ball.SetBallScale(ballScale);
}

// ════════════════════════════════════════════════════════════════════
//  Disaster: Flicker Boost — unlimited boost flips on/off every second
// ════════════════════════════════════════════════════════════════════
void RLDisasters::TickFlickerBoost(float delta)
{
    if (!gameWrapper->IsInGame()) return;
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;

    flickerTimer += delta;
    bool boostOnPhase = std::fmod(flickerTimer, 2.0f) < 1.0f;

    ArrayWrapper<CarWrapper> cars = server.GetCars();
    for (int i = 0; i < cars.Count(); i++) {
        CarWrapper car = cars.Get(i);
        if (car.IsNull()) continue;
        BoostWrapper boost = car.GetBoostComponent();
        if (boost.IsNull()) continue;
        boost.SetUnlimitedBoost2(boostOnPhase);
    }
}

// ════════════════════════════════════════════════════════════════════
//  Disaster: Infinite Boost — always on
// ════════════════════════════════════════════════════════════════════
void RLDisasters::ApplyInfiniteBoost()
{
    if (!gameWrapper->IsInGame()) return;
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;

    ArrayWrapper<CarWrapper> cars = server.GetCars();
    for (int i = 0; i < cars.Count(); i++) {
        CarWrapper car = cars.Get(i);
        if (car.IsNull()) continue;
        BoostWrapper boost = car.GetBoostComponent();
        if (boost.IsNull()) continue;
        boost.SetUnlimitedBoost2(true);
    }
}

// ════════════════════════════════════════════════════════════════════
//  Disaster: Chaos Gravity — drifts to a random value every few seconds
// ════════════════════════════════════════════════════════════════════
void RLDisasters::TickChaosGravity(float delta)
{
    gravityTimer += delta;
    if (gravityTimer < 4.0f) return;
    gravityTimer = 0.0f;

    // Random gravity between -1100 (heavy) and -250 (floaty), never near 0
    int roll = rand() % 851; // 0..850
    currentGravity = -1100.0f + static_cast<float>(roll);

    cvarManager->executeCommand("sv_soccar_gravity " + std::to_string(currentGravity), false);
}

// ════════════════════════════════════════════════════════════════════
//  Reset
// ════════════════════════════════════════════════════════════════════
void RLDisasters::ResetAll()
{
    goalsScored   = 0;
    ballScale     = 1.0f;
    flickerTimer  = 0.0f;
    gravityTimer  = 0.0f;
    currentGravity = -650.0f;

    cvarManager->executeCommand("sv_soccar_gravity -650", false);

    if (!gameWrapper->IsInGame()) return;
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;

    BallWrapper ball = server.GetBall();
    if (!ball.IsNull()) ball.SetBallScale(1.0f);

    ArrayWrapper<CarWrapper> cars = server.GetCars();
    for (int i = 0; i < cars.Count(); i++) {
        CarWrapper car = cars.Get(i);
        if (car.IsNull()) continue;
        BoostWrapper boost = car.GetBoostComponent();
        if (!boost.IsNull()) boost.SetUnlimitedBoost2(false);
    }
}

// Settings/toggles are handled entirely by the .set file in
// bakkesmod/plugins/settings/ — see RLDisasters.set. No custom GUI code
// needed; BakkesMod renders checkboxes for each registered cvar directly
// in the F2 > Plugins tab.

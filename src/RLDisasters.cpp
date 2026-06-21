#include "RLDisasters.h"
#include "bakkesmod/wrappers/gamewrapper.h"
#include "bakkesmod/wrappers/gameobject/carwrapper.h"
#include "bakkesmod/wrappers/gameevent/serverwrapper.h"
#include <algorithm>

BAKKESMOD_PLUGIN(RLDisasters, "Rocket League Disasters", "1.0", 0)

void RLDisasters::onLoad()
{
    cvarManager->registerCvar("disasters_persistentRumble", "0", "Enable persistent rumble item generation", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            persistentRumbleOn = cvar.getBoolValue();
        });

    cvarManager->registerCvar("disasters_rumbleItem", "Spikes_TA", "Item class name to force persistently", true, false, 0, false, 0);

    cvarManager->registerCvar("disasters_lowGravityGoals", "0", "Gravity gets weaker every goal scored", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            lowGravityGoalsOn = cvar.getBoolValue();
            if (!lowGravityGoalsOn) {
                currentGravity = -650.0f;
                cvarManager->executeCommand("sv_soccar_gravity -650", false);
            }
        });

    cvarManager->registerCvar("disasters_gravityPerGoal", "100", "Gravity units toward zero per goal", true, true, 10, true, 400)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            gravityStepPerGoal = cvar.getFloatValue();
        });

    cvarManager->registerNotifier("disasters_resetall", [this](std::vector<std::string>) {
        cvarManager->getCvar("disasters_persistentRumble").setValue(0);
        cvarManager->getCvar("disasters_lowGravityGoals").setValue(0);
        ResetAll();
    }, "Resets all disasters", PERMISSION_ALL);

    HookEvents();
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

    gameWrapper->HookEvent("Function TAGame.Car_TA.SetVehicleInput",
        [this](std::string e) { OnTick(e); });
}

void RLDisasters::UnhookEvents()
{
    gameWrapper->UnhookEvent("Function GameEvent_Soccar_TA.Active.StartRound");
    gameWrapper->UnhookEvent("Function TAGame.Ball_TA.Explode");
    gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
}

void RLDisasters::OnMatchStarted(std::string)
{
}

void RLDisasters::OnGoalScored(std::string)
{
    if (!gameWrapper->IsInGame()) return;
    goalsScored++;
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

void RLDisasters::TickRumbleTracking()
{
    CarWrapper car = gameWrapper->GetLocalCar();
    if (car.IsNull()) return;

    auto pickup = car.GetAttachedPickup();
    if (pickup.IsNull()) {
        std::string item = cvarManager->getCvar("disasters_rumbleItem").getStringValue();
        cvarManager->executeCommand("cheat giveitem " + item, false);
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
    goalsScored = 0;
    currentGravity = -650.0f;
    cvarManager->executeCommand("sv_soccar_gravity -650", false);
}

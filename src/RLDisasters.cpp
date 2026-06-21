#include "RLDisasters.h"
#include "bakkesmod/wrappers/cvarwrapper.h"
#include "bakkesmod/wrappers/gamewrapper.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "bakkesmod/wrappers/arraywrapper.h"
#include <cstdlib>
#include <ctime>

BAKKESMOD_PLUGIN(RLDisasters, "Rocket League Disaster Chaos Mod", "1.0", 0)

void RLDisasters::onLoad()
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    cvarManager->registerCvar("rl_disasters_enabled", "0", "Enable the Disaster Chaos Mod plugin", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
            pluginEnabled = cvar.getBoolValue();
            if (!pluginEnabled) {
                ResetMatchState();
            }
        });

    gameWrapper->HookEvent("Function TAGame.GameEvent_TA.OnInit", std::bind(&RLDisasters::OnMatchStart, this, std::placeholders::_1));
    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded", std::bind(&RLDisasters::OnMatchEnd, this, std::placeholders::_1));
    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.EventGoalScored", std::bind(&RLDisasters::OnGoalScored, this, std::placeholders::_1));
    gameWrapper->HookEvent("Function TAGame.Car_TA.SetVehicleInput", std::bind(&RLDisasters::OnTick, this, std::placeholders::_1));
}

void RLDisasters::onUnload()
{
    gameWrapper->UnhookEvent("Function TAGame.GameEvent_TA.OnInit");
    gameWrapper->UnhookEvent("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded");
    gameWrapper->UnhookEvent("Function TAGame.GameEvent_Soccar_TA.EventGoalScored");
    gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
}

void RLDisasters::OnMatchStart(std::string eventName)
{
    if (!pluginEnabled) return;

    isMatchRunning = true;
    currentGravity = -650.0f;
    SelectNextRoundItem();
    ApplyGravitySettings();
}

void RLDisasters::OnMatchEnd(std::string eventName)
{
    ResetMatchState();
}

void RLDisasters::OnGoalScored(std::string eventName)
{
    if (!pluginEnabled || !isMatchRunning) return;

    currentGravity += 100.0f;
    if (currentGravity > -150.0f) {
        currentGravity = -150.0f;
    }
    ApplyGravitySettings();
    SelectNextRoundItem();
}

void RLDisasters::OnTick(std::string eventName)
{
    if (!pluginEnabled || !isMatchRunning) return;

    CVarWrapper ballScaleCvar = cvarManager->getCvar("sv_soccar_ballscale");
    if (ballScaleCvar && ballScaleCvar.getFloatValue() != 1.0f) {
        ballScaleCvar.setValue(1.0f);
    }

    EnforcePersistentItem();
}

void RLDisasters::ResetMatchState()
{
    isMatchRunning = false;
    currentGravity = -650.0f;
    
    CVarWrapper gravityCvar = cvarManager->getCvar("sv_soccar_gravity");
    if (gravityCvar) {
        gravityCvar.setValue(-650.0f);
    }

    CVarWrapper ballScaleCvar = cvarManager->getCvar("sv_soccar_ballscale");
    if (ballScaleCvar) {
        ballScaleCvar.setValue(1.0f);
    }
}

void RLDisasters::ApplyGravitySettings()
{
    CVarWrapper gravityCvar = cvarManager->getCvar("sv_soccar_gravity");
    if (gravityCvar) {
        gravityCvar.setValue(currentGravity);
    }
}

void RLDisasters::SelectNextRoundItem()
{
    if (rumbleItems.empty()) return;

    int randomIndex = std::rand() % rumbleItems.size();
    currentRoundItem = rumbleItems[randomIndex];

    cvarManager->log("Disaster Mod: Next round item rolled successfully -> " + currentRoundItem);
}

void RLDisasters::EnforcePersistentItem()
{
    ServerWrapper server = gameWrapper->GetGameEventAsServer();
    if (!server) return;

    ArrayWrapper<CarWrapper> cars = server.GetCars();
    for (int i = 0; i < cars.Count(); i++) {
        CarWrapper car = cars.Get(i);
        if (!car) continue;

        if (car.GetAttachedPickup().IsNull()) {
            std::string command = "tweak item " + currentRoundItem;
            cvarManager->executeCommand(command);
        }
    }
}

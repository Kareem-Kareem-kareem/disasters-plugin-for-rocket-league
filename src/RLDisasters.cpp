#include "RLDisasters.h"
#include "bakkesmod/wrappers/gamewrapper.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>

BAKKESMOD_PLUGIN(RLDisasters, "Rocket League Disasters", "1.0", 0)

void RLDisasters::onLoad()
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    cvarManager->registerCvar("rl_disasters_enabled", "0", "Enable the Disaster Chaos Mod plugin", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            pluginEnabled = cvar.getBoolValue();
            if (!pluginEnabled) {
                ResetMatchState();
            } else {
                ChooseRandomRumbleItem();
            }
        });

    HookEvents();
}

void RLDisasters::onUnload()
{
    UnhookEvents();
    ResetMatchState();
}

void RLDisasters::HookEvents()
{
    gameWrapper->HookEvent("Function GameEvent_Soccar_TA.Active.StartRound",
        [this](std::string e) { OnRoundStart(e); });

    gameWrapper->HookEvent("Function TAGame.Ball_TA.Explode",
        [this](std::string e) { OnGoalScored(e); });
}

void RLDisasters::UnhookEvents()
{
    gameWrapper->UnhookEvent("Function GameEvent_Soccar_TA.Active.StartRound");
    gameWrapper->UnhookEvent("Function TAGame.Ball_TA.Explode");
}

void RLDisasters::OnRoundStart(std::string eventName)
{
    if (!pluginEnabled) return;

    ApplyGravitySettings();
    cvarManager->log("RLDisasters: Round started. Active rolled item index for this round slot: " + std::to_string(currentRandomItemIndex));
}

void RLDisasters::OnGoalScored(std::string eventName)
{
    if (!gameWrapper->IsInGame() || !pluginEnabled) return;

    // Weakens gravity toward zero by adding 100 units per goal
    currentGravity += 100.0f;
    if (currentGravity > -150.0f) {
        currentGravity = -150.0f; 
    }

    ChooseRandomRumbleItem();
}

void RLDisasters::ChooseRandomRumbleItem()
{
    // Selects a random item identifier index from 1 to 11
    currentRandomItemIndex = 1 + (std::rand() % 11);
    cvarManager->log("RLDisasters: A goal was scored! Next round item slot set to: " + std::to_string(currentRandomItemIndex));
}

void RLDisasters::ApplyGravitySettings()
{
    cvarManager->executeCommand("sv_soccar_gravity " + std::to_string(currentGravity), false);
}

void RLDisasters::ResetMatchState()
{
    currentGravity = -650.0f;
    currentRandomItemIndex = 1;
    cvarManager->executeCommand("sv_soccar_gravity -650", false);
}

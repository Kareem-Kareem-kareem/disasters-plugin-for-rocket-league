#include "RLDisasters.h"
#include "bakkesmod/wrappers/gamewrapper.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>

BAKKESMOD_PLUGIN(RLDisasters, "Rocket League Disasters", "1.0", 0)

void RLDisasters::onLoad()
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    cvarManager->registerCvar("disasters_persistentRumble", "0", "Enable persistent rumble item generation", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            persistentRumbleOn = cvar.getBoolValue();
            if (persistentRumbleOn) {
                ChooseRandomRumbleItem();
            } else {
                cvarManager->executeCommand("sv_freeplay_rumble_enable_item 0", false);
            }
        });

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
}

void RLDisasters::UnhookEvents()
{
    gameWrapper->UnhookEvent("Function GameEvent_Soccar_TA.Active.StartRound");
    gameWrapper->UnhookEvent("Function TAGame.Ball_TA.Explode");
}

void RLDisasters::OnMatchStarted(std::string)
{
    if (persistentRumbleOn) {
        cvarManager->executeCommand("sv_freeplay_rumble_enable_item " + std::to_string(currentRandomItem), false);
    }
}

void RLDisasters::OnGoalScored(std::string)
{
    if (!gameWrapper->IsInGame()) return;
    goalsScored++;
    if (lowGravityGoalsOn) ApplyLowGravityGoals();
    if (persistentRumbleOn) ChooseRandomRumbleItem();
}

void RLDisasters::ChooseRandomRumbleItem()
{
    currentRandomItem = 1 + (std::rand() % 11);
    cvarManager->executeCommand("sv_freeplay_rumble_enable_item " + std::to_string(currentRandomItem), false);
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
    currentRandomItem = 0;
    cvarManager->executeCommand("sv_soccar_gravity -650", false);
    cvarManager->executeCommand("sv_freeplay_rumble_enable_item 0", false);
}

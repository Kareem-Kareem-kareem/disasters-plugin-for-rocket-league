#include "RLDisasters.h"
#include "imgui.h"
#include <cmath>
#include <string>
#include <vector>
#include <memory>
#include <cstdlib>

BAKKESMOD_PLUGIN(RLDisasters, "Rocket League Disasters Plugin", "1.0", 0)

std::shared_ptr<CVarManagerWrapper> _cvarManager;

static int blueGoals = 0;
static int orangeGoals = 0;
static int lastTotalGoals = -1;
static float rumbleItemTimer = 0.0f;

static const std::vector<std::string> UNREAL_RUMBLE_CHEATS = {
    "Cheat_GiveSpikes", 
    "Cheat_GivePlunger", 
    "Cheat_GiveMagnet", 
    "Cheat_GiveBoot", 
    "Cheat_GivePowerHit", 
    "Cheat_GiveIce", 
    "Cheat_GiveSwapper", 
    "Cheat_GiveTornado"
};
static size_t currentRumbleIndex = 0;

void RLDisasters::onLoad()
{
    _cvarManager = cvarManager;

    if (!_cvarManager)
    {
        return;
    }

    _cvarManager->log("RLDisasters: Beginning full system load sequence.");

    disasters.closestSpawn = false;
    disasters.biggerGoals = false;
    disasters.biggerField = false;
    disasters.quickRumble = false;
    disasters.persistentRumble = false;

    _cvarManager->registerCvar("disasters_closestSpawn", "0", "Enable Closest Spawn", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string oldVal, CVarWrapper cvar) {
            if (cvar.IsNull()) return;
            disasters.closestSpawn = cvar.getBoolValue();
        });

    _cvarManager->registerCvar("disasters_biggerGoals", "0", "Enable Bigger Goals", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string oldVal, CVarWrapper cvar) {
            if (cvar.IsNull()) return;
            disasters.biggerGoals = cvar.getBoolValue();
        });

    _cvarManager->registerCvar("disasters_biggerField", "0", "Enable Bigger Field", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string oldVal, CVarWrapper cvar) {
            if (cvar.IsNull()) return;
            disasters.biggerField = cvar.getBoolValue();
        });

    _cvarManager->registerCvar("disasters_quickRumble", "0", "Enable 1-sec Rumble", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string oldVal, CVarWrapper cvar) {
            if (cvar.IsNull()) return;
            disasters.quickRumble = cvar.getBoolValue();
        });

    _cvarManager->registerCvar("disasters_persistentRumble", "0", "Enable Persistent Rumble", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string oldVal, CVarWrapper cvar) {
            if (cvar.IsNull()) return;
            disasters.persistentRumble = cvar.getBoolValue();
        });

    HookEvents();

    _cvarManager->log("RLDisasters: Initialization sequence successfully finalized.");
}

void RLDisasters::onUnload()
{
    UnhookEvents();
    ResetAll();
}

void RLDisasters::HookEvents()
{
    if (!gameWrapper)
    {
        return;
    }

    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.ResetPlayers",
        [this](std::string e) { OnPlayersReset(e); });

    gameWrapper->HookEvent("Function TAGame.Car_TA.SetVehicleInput",
        [this](std::string e) { OnTick(e); });
}

void RLDisasters::UnhookEvents()
{
    if (!gameWrapper)
    {
        return;
    }

    gameWrapper->UnhookEvent("Function TAGame.GameEvent_Soccar_TA.ResetPlayers");
    gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
}

void RLDisasters::OnTick(std::string eventName)
{
    if (!gameWrapper) return;
    if (!gameWrapper->IsInGame()) return;

    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;

    auto teams = server.GetTeams();
    int currentTotalGoals = 0;
    if (!teams.IsNull() && teams.Count() >= 2)
    {
        auto blueTeam = teams.Get(0);
        auto orangeTeam = teams.Get(1);
        if (!blueTeam.IsNull() && !orangeTeam.IsNull())
        {
            blueGoals = blueTeam.GetScore();
            orangeGoals = orangeTeam.GetScore();
            currentTotalGoals = blueGoals + orangeGoals;
        }
    }

    auto ball = server.GetBall();
    if (!ball.IsNull())
    {
        if (disasters.biggerField)
        {
            float targetBallScale = 1.0f + (static_cast<float>(currentTotalGoals) * 0.25f);
            if (targetBallScale > 3.5f)
            {
                targetBallScale = 3.5f;
            }
            ball.SetDrawScale3D(Vector{ targetBallScale, targetBallScale, targetBallScale });
        }
        else
        {
            ball.SetDrawScale3D(Vector{ 1.0f, 1.0f, 1.0f });
        }
    }

    if (currentTotalGoals != lastTotalGoals)
    {
        lastTotalGoals = currentTotalGoals;

        if (disasters.persistentRumble && !UNREAL_RUMBLE_CHEATS.empty())
        {
            currentRumbleIndex = (currentRumbleIndex + 1) % UNREAL_RUMBLE_CHEATS.size();
        }

        if (disasters.biggerGoals)
        {
            auto goals = server.GetGoals();
            if (!goals.IsNull())
            {
                float targetGoalScale = 1.0f + (static_cast<float>(currentTotalGoals) * 0.20f);
                if (targetGoalScale > 2.5f)
                {
                    targetGoalScale = 2.5f;
                }
                for (int i = 0; i < goals.Count(); ++i)
                {
                    auto goal = goals.Get(i);
                    if (goal.IsNull()) continue;

                    Vector standardExtent = Vector{ 400.0f, 900.0f, 300.0f };
                    goal.SetLocalExtent(Vector{ standardExtent.X * targetGoalScale, standardExtent.Y * targetGoalScale, standardExtent.Z * targetGoalScale });
                }
            }
        }
    }

    float frameDelta = 0.016667f;
    rumbleItemTimer += frameDelta;

    if (disasters.persistentRumble)
    {
        if (rumbleItemTimer >= 1.5f)
        {
            rumbleItemTimer = 0.0f;
            if (!UNREAL_RUMBLE_CHEATS.empty())
            {
                gameWrapper->ExecuteUnrealCommand(UNREAL_RUMBLE_CHEATS[currentRumbleIndex]);
            }
        }
    }
    else if (disasters.quickRumble)
    {
        if (rumbleItemTimer >= 1.0f)
        {
            rumbleItemTimer = 0.0f;
            if (!UNREAL_RUMBLE_CHEATS.empty())
            {
                int randIndex = std::rand() % UNREAL_RUMBLE_CHEATS.size();
                gameWrapper->ExecuteUnrealCommand(UNREAL_RUMBLE_CHEATS[randIndex]);
            }
        }
    }

    if (disasters.quickRumble || disasters.persistentRumble)
    {
        auto cars = server.GetCars();
        if (!cars.IsNull())
        {
            for (int i = 0; i < cars.Count(); ++i)
            {
                auto car = cars.Get(i);
                if (car.IsNull()) continue;

                auto boostComp = car.GetBoostComponent();
                if (!boostComp.IsNull())
                {
                    boostComp.SetBoostAmount(1.0f);
                }
            }
        }
    }
}

void RLDisasters::OnPlayersReset(std::string eventName)
{
    if (!disasters.closestSpawn) return;
    if (!gameWrapper || !gameWrapper->IsInGame()) return;

    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;

    auto cars = server.GetCars();
    if (cars.IsNull()) return;

    for (int index = 0; index < cars.Count(); ++index)
    {
        auto currentCar = cars.Get(index);
        if (currentCar.IsNull()) continue;

        unsigned char teamIdentifier = currentCar.GetTeamNum2();
        Vector positionVector = currentCar.GetLocation();

        float structuralTargetY = (teamIdentifier == 0) ? 5120.0f : -5120.0f;

        Vector headingTrajectory = Vector(0.0f, structuralTargetY, 0.0f) - positionVector;
        
        float calculationYawRad = atan2f(headingTrajectory.Y, headingTrajectory.X);
        float alignmentScalingFactor = 65536.0f / (2.0f * 3.14159265f);
        int targetUnrealYaw = static_cast<int>(calculationYawRad * alignmentScalingFactor);

        Rotator transformationRotator;
        transformationRotator.Pitch = 0;
        transformationRotator.Yaw = targetUnrealYaw;
        transformationRotator.Roll = 0;

        currentCar.SetRotation(transformationRotator);
        currentCar.SetVelocity(Vector(0.0f, 0.0f, 0.0f));
        currentCar.SetAngularVelocity(Vector(0.0f, 0.0f, 0.0f), false);
    }
}

void RLDisasters::ResetAll()
{
    blueGoals = 0;
    orangeGoals = 0;
    lastTotalGoals = -1;
    rumbleItemTimer = 0.0f;
    currentRumbleIndex = 0;

    if (gameWrapper && gameWrapper->IsInGame())
    {
        ServerWrapper server = gameWrapper->GetCurrentGameState();
        if (!server.IsNull())
        {
            auto ball = server.GetBall();
            if (!ball.IsNull())
            {
                ball.SetDrawScale3D(Vector{ 1.0f, 1.0f, 1.0f });
            }
        }
    }
}

void RLDisasters::RenderSettings()
{
    ImGui::Text("Rocket League Disasters System Configurations");
    ImGui::Separator();

    if (ImGui::Checkbox("Closest Spawn Realignment", &disasters.closestSpawn))
    {
        if (_cvarManager) _cvarManager->getCvar("disasters_closestSpawn").setValue(disasters.closestSpawn ? 1 : 0);
    }
    if (ImGui::Checkbox("Expanding Goal Hitboxes", &disasters.biggerGoals))
    {
        if (_cvarManager) _cvarManager->getCvar("disasters_biggerGoals").setValue(disasters.biggerGoals ? 1 : 0);
    }
    if (ImGui::Checkbox("Expanding Visual Ball Size", &disasters.biggerField))
    {
        if (_cvarManager) _cvarManager->getCvar("disasters_biggerField").setValue(disasters.biggerField ? 1 : 0);
    }
    if (ImGui::Checkbox("Quick Boost & Item Injection", &disasters.quickRumble))
    {
        if (_cvarManager) _cvarManager->getCvar("disasters_quickRumble").setValue(disasters.quickRumble ? 1 : 0);
    }
    if (ImGui::Checkbox("Persistent Locked Rumble Power-Up", &disasters.persistentRumble))
    {
        if (_cvarManager) _cvarManager->getCvar("disasters_persistentRumble").setValue(disasters.persistentRumble ? 1 : 0);
    }
}

void RLDisasters::SetImGuiContext(uintptr_t ctx)
{
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

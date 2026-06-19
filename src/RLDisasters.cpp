#include "RLDisasters.h"
#include "imgui.h"
#include <cmath>
#include <string>
#include <vector>
#include <memory>

BAKKESMOD_PLUGIN(RLDisasters, "Rocket League Disasters Plugin", "1.0", 0)

std::shared_ptr<CVarManagerWrapper> _cvarManager;

static int blueGoals = 0;
static int orangeGoals = 0;
static float rumbleItemTimer = 0.0f;

static const std::vector<std::string> RUMBLE_ITEMS = {
    "spikes", "plunger", "magnet", "boot", "haymaker", 
    "ballfreeze", "swapper", "tornado", "powershot", "booster"
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

    gameWrapper->HookEvent("Function GameEvent_Soccar_TA.Active.StartRound",
        [this](std::string e) { OnMatchStarted(e); });

    gameWrapper->HookEvent("Function TAGame.Ball_TA.Explode",
        [this](std::string e) { OnGoalScored(e); });

    gameWrapper->HookEvent("Function TAGame.Car_TA.SetVehicleInput",
        [this](std::string e) { OnTick(e); });

    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.InitPlayer",
        [this](std::string e) { OnPlayerSpawned(e); });
}

void RLDisasters::UnhookEvents()
{
    if (!gameWrapper)
    {
        return;
    }

    gameWrapper->UnhookEvent("Function GameEvent_Soccar_TA.Active.StartRound");
    gameWrapper->UnhookEvent("Function TAGame.Ball_TA.Explode");
    gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
    gameWrapper->UnhookEvent("Function TAGame.GameEvent_Soccar_TA.InitPlayer");
}

void RLDisasters::OnTick(std::string eventName)
{
    if (!gameWrapper) return;
    if (!gameWrapper->IsInGame()) return;

    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;

    float frameDelta = 0.016667f;

    if (disasters.persistentRumble)
    {
        rumbleItemTimer += frameDelta;
        if (rumbleItemTimer >= 1.5f)
        {
            rumbleItemTimer = 0.0f;
            if (_cvarManager && currentRumbleIndex < RUMBLE_ITEMS.size())
            {
                _cvarManager->executeCommand("bms_rumble_give " + RUMBLE_ITEMS[currentRumbleIndex]);
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

void RLDisasters::OnMatchStarted(std::string eventName)
{
    ResetAll();
}

void RLDisasters::OnGoalScored(std::string eventName)
{
    if (!gameWrapper) return;
    if (!gameWrapper->IsInGame()) return;

    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;

    auto teams = server.GetTeams();
    if (teams.IsNull() || teams.Count() < 2) return;

    auto blueTeam = teams.Get(0);
    auto orangeTeam = teams.Get(1);

    if (blueTeam.IsNull() || orangeTeam.IsNull()) return;

    blueGoals = blueTeam.GetScore();
    orangeGoals = orangeTeam.GetScore();

    int totalGoalsCalculated = blueGoals + orangeGoals;

    if (RUMBLE_ITEMS.size() > 0)
    {
        currentRumbleIndex = (currentRumbleIndex + 1) % RUMBLE_ITEMS.size();
        if (_cvarManager)
        {
            _cvarManager->log("RLDisasters: Match goal registered. Shifting assigned active powerup asset to: " + RUMBLE_ITEMS[currentRumbleIndex]);
        }
    }

    if (disasters.biggerField)
    {
        auto ball = server.GetBall();
        if (!ball.IsNull())
        {
            float targetBallScale = 1.0f + (static_cast<float>(totalGoalsCalculated) * 0.25f);
            if (targetBallScale > 3.5f)
            {
                targetBallScale = 3.5f;
            }
            ball.SetDrawScale3D(Vector{ targetBallScale, targetBallScale, targetBallScale });
        }
    }

    if (disasters.biggerGoals)
    {
        auto goals = server.GetGoals();
        if (!goals.IsNull())
        {
            float targetGoalScale = 1.0f + (static_cast<float>(totalGoalsCalculated) * 0.20f);
            if (targetGoalScale > 2.5f)
            {
                targetGoalScale = 2.5f;
            }
            for (int i = 0; i < goals.Count(); ++i)
            {
                auto goal = goals.Get(i);
                if (goal.IsNull()) continue;

                Vector baseExtent = goal.GetLocalExtent();
                goal.SetLocalExtent(Vector{ baseExtent.X * targetGoalScale, baseExtent.Y * targetGoalScale, baseExtent.Z * targetGoalScale });
            }
        }
    }
}

void RLDisasters::OnPlayerSpawned(std::string eventName)
{
    if (!disasters.closestSpawn) return;
    if (!gameWrapper) return;
    if (!gameWrapper->IsInGame()) return;

    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;

    auto cars = server.GetCars();
    if (cars.IsNull()) return;

    int totalActiveCars = cars.Count();
    for (int index = 0; index < totalActiveCars; ++index)
    {
        auto currentCar = cars.Get(index);
        if (currentCar.IsNull()) continue;

        unsigned char teamIdentifier = currentCar.GetTeamNum2();
        Vector positionVector = currentCar.GetLocation();

        float structuralTargetY = 0.0f;
        if (teamIdentifier == 0)
        {
            structuralTargetY = -5120.0f;
        }
        else
        {
            structuralTargetY = 5120.0f;
        }

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
    ImGui::TextProtected("Rocket League Disasters System Configurations");
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
    if (ImGui::Checkbox("Quick Boost Injection", &disasters.quickRumble))
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

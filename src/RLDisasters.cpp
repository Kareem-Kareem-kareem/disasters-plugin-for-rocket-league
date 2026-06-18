#include "RLDisasters.h"
#include "imgui.h"
#include <cmath>
#include <string>
#include <sstream>
#include <memory>

BAKKESMOD_PLUGIN(RLDisasters, "Rocket League Disasters Plugin", "1.0", 0)

std::shared_ptr<CVarManagerWrapper> _cvarManager;

static int blueGoals = 0;
static int orangeGoals = 0;
static float currentFieldScale = 1.0f;
static float rumbleTimerToken = 0.0f;
static bool blockActiveState = false;

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
            if (_cvarManager)
            {
                _cvarManager->log("RLDisasters: Closest Spawn property has been altered.");
            }
        });

    _cvarManager->registerCvar("disasters_biggerGoals", "0", "Enable Bigger Goals", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string oldVal, CVarWrapper cvar) {
            if (cvar.IsNull()) return;
            disasters.biggerGoals = cvar.getBoolValue();
            if (!disasters.biggerGoals)
            {
                if (gameWrapper && gameWrapper->IsInGame() && _cvarManager)
                {
                    _cvarManager->executeCommand("sv_soccar_goal_size 1.0");
                }
            }
        });

    _cvarManager->registerCvar("disasters_biggerField", "0", "Enable Bigger Field", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string oldVal, CVarWrapper cvar) {
            if (cvar.IsNull()) return;
            disasters.biggerField = cvar.getBoolValue();
            if (!disasters.biggerField)
            {
                if (gameWrapper && gameWrapper->IsInGame() && _cvarManager)
                {
                    _cvarManager->executeCommand("cl_soccar_set_scale 1.0");
                }
            }
        });

    _cvarManager->registerCvar("disasters_quickRumble", "0", "Enable 1-sec Rumble", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string oldVal, CVarWrapper cvar) {
            if (cvar.IsNull()) return;
            disasters.quickRumble = cvar.getBoolValue();
            if (disasters.quickRumble)
            {
                if (gameWrapper && gameWrapper->IsInGame() && _cvarManager)
                {
                    _cvarManager->executeCommand("sv_soccar_rumbletime 1");
                }
            }
            else
            {
                if (gameWrapper && gameWrapper->IsInGame() && _cvarManager)
                {
                    _cvarManager->executeCommand("sv_soccar_rumbletime 10");
                }
            }
        });

    _cvarManager->registerCvar("disasters_persistentRumble", "0", "Enable Persistent Rumble", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string oldVal, CVarWrapper cvar) {
            if (cvar.IsNull()) return;
            disasters.persistentRumble = cvar.getBoolValue();
            if (_cvarManager)
            {
                _cvarManager->log("RLDisasters: Persistent Rumble structural configuration updated.");
            }
        });

    HookEvents();

    _cvarManager->log("RLDisasters: Initialization sequence successfully finalized.");
}

void RLDisasters::onUnload()
{
    if (_cvarManager)
    {
        _cvarManager->log("RLDisasters: Initiating standard teardown procedures.");
    }

    UnhookEvents();
    ResetAll();

    if (_cvarManager)
    {
        _cvarManager->log("RLDisasters: Module fully detached from host runtime.");
    }
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

    if (_cvarManager)
    {
        _cvarManager->log("RLDisasters: Global event listeners successfully bound.");
    }
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

    if (_cvarManager)
    {
        _cvarManager->log("RLDisasters: Global event listeners successfully severed.");
    }
}

void RLDisasters::OnTick(std::string eventName)
{
    if (!gameWrapper) return;
    if (!gameWrapper->IsInGame()) return;

    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;

    float frameDelta = 0.016667f;

    if (disasters.quickRumble)
    {
        TickQuickRumble(frameDelta);
    }

    if (disasters.persistentRumble)
    {
        TickPersistentRumble();
    }
}

void RLDisasters::OnMatchStarted(std::string eventName)
{
    if (_cvarManager)
    {
        _cvarManager->log("RLDisasters: Match startup event registered.");
    }
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

    if (_cvarManager)
    {
        _cvarManager->log("RLDisasters: Goal event parsed. Cumulative metric: " + std::to_string(totalGoalsCalculated));
    }

    if (disasters.biggerGoals)
    {
        float targetGoalScale = 1.0f + (static_cast<float>(totalGoalsCalculated) * 0.15f);
        if (targetGoalScale > 2.5f)
        {
            targetGoalScale = 2.5f;
        }
        if (_cvarManager)
        {
            _cvarManager->executeCommand("sv_soccar_goal_size " + std::to_string(targetGoalScale));
            _cvarManager->log("RLDisasters: Applying updated goal metrics inline.");
        }
    }

    if (disasters.biggerField)
    {
        currentFieldScale = 1.0f + (static_cast<float>(totalGoalsCalculated) * 0.10f);
        if (currentFieldScale > 1.3f)
        {
            currentFieldScale = 1.3f;
        }
        if (_cvarManager)
        {
            _cvarManager->executeCommand("cl_soccar_set_scale " + std::to_string(currentFieldScale));
            _cvarManager->log("RLDisasters: Applying updated field scaling metrics inline.");
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

    if (_cvarManager)
    {
        _cvarManager->log("RLDisasters: Realignment logic successfully applied to active actors.");
    }
}

void RLDisasters::TickQuickRumble(float delta)
{
    if (!gameWrapper) return;
    if (!gameWrapper->IsInGame()) return;

    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;

    rumbleTimerToken += delta;
    if (rumbleTimerToken >= 1.0f)
    {
        rumbleTimerToken = 0.0f;
        if (_cvarManager)
        {
            _cvarManager->executeCommand("sv_soccar_rumbletime 1");
        }

        auto cars = server.GetCars();
        if (cars.IsNull()) return;

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

void RLDisasters::TickPersistentRumble()
{
    if (!gameWrapper) return;
    if (!gameWrapper->IsInGame()) return;

    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;

    auto cars = server.GetCars();
    if (cars.IsNull()) return;

    int internalCarCount = cars.Count();
    for (int trackingIndex = 0; trackingIndex < internalCarCount; ++trackingIndex)
    {
        auto activeCarActor = cars.Get(trackingIndex);
        if (activeCarActor.IsNull()) continue;

        auto boostComp = activeCarActor.GetBoostComponent();
        if (!boostComp.IsNull())
        {
            boostComp.SetBoostAmount(1.0f);
        }
    }
}

void RLDisasters::ResetAll()
{
    blueGoals = 0;
    orangeGoals = 0;
    currentFieldScale = 1.0f;
    rumbleTimerToken = 0.0f;
    blockActiveState = false;

    if (gameWrapper && gameWrapper->IsInGame() && _cvarManager)
    {
        _cvarManager->executeCommand("sv_soccar_goal_size 1.0");
        _cvarManager->executeCommand("cl_soccar_set_scale 1.0");
        _cvarManager->executeCommand("sv_soccar_rumbletime 10");
        _cvarManager->log("RLDisasters: Environmental systems structural defaults restored.");
    }
    else if (_cvarManager)
    {
        _cvarManager->log("RLDisasters: Out of game context cache cleared successfully.");
    }
}

void RLDisasters::RenderSettings()
{
}

void RLDisasters::SetImGuiContext(uintptr_t ctx)
{
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

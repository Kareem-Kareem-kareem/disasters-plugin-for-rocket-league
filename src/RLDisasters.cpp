#include "pch.h"
#include "RLDisasters.h"
#include <random>

BAKKESMOD_PLUGIN(RLDisasters, "Rocket League Disasters Plugin", "1.0", PLUGINGAME_ROCKETLEAGUE)

std::shared_ptr<CVarManagerWrapper> _cvarManager;

// ════════════════════════════════════════════════════════════════════
//  Plugin Initialization & Destruction
// ════════════════════════════════════════════════════════════════════

void RLDisasters::onLoad()
{
    _cvarManager = cvarManager;
    cvarManager->log("RLDisasters Plugin loaded successfully.");

    // Initialize Disaster States
    disasters.closestSpawn = false;
    disasters.biggerGoals = false;
    disasters.biggerField = false;
    disasters.quickRumble = false;
    disasters.persistentRumble = false;

    // Register Cvars linked directly to the RLDisasters.set UI file
    cvarManager->registerCvar("disasters_closestSpawn", "0", "Enable Closest Spawn", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string oldVal, CVarWrapper cvar) {
            disasters.closestSpawn = cvar.getBoolValue();
            cvarManager->log("Closest Spawn toggled: " + std::to_string(disasters.closestSpawn));
        });

    cvarManager->registerCvar("disasters_biggerGoals", "0", "Enable Bigger Goals", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string oldVal, CVarWrapper cvar) {
            disasters.biggerGoals = cvar.getBoolValue();
            if (!disasters.biggerGoals) ResetGoalScale();
        });

    cvarManager->registerCvar("disasters_biggerField", "0", "Enable Bigger Field", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string oldVal, CVarWrapper cvar) {
            disasters.biggerField = cvar.getBoolValue();
            if (!disasters.biggerField) ResetFieldScale();
        });

    cvarManager->registerCvar("disasters_quickRumble", "0", "Enable 1-sec Rumble", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string oldVal, CVarWrapper cvar) {
            disasters.quickRumble = cvar.getBoolValue();
        });

    cvarManager->registerCvar("disasters_persistentRumble", "0", "Enable Persistent Rumble", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string oldVal, CVarWrapper cvar) {
            disasters.persistentRumble = cvar.getBoolValue();
        });

    HookEvents();
}

void RLDisasters::onUnload()
{
    UnhookEvents();
    ResetAll();
    cvarManager->log("RLDisasters Plugin unloaded.");
}

// ════════════════════════════════════════════════════════════════════
//  Event Hook Management
// ════════════════════════════════════════════════════════════════════

void RLDisasters::HookEvents()
{
    gameWrapper->HookEvent("Function GameEvent_Soccar_TA.Active.StartRound",
        [this](std::string e){ OnMatchStarted(e); });

    gameWrapper->HookEvent("Function TAGame.Ball_TA.Explode",
        [this](std::string e){ OnGoalScored(e); });

    gameWrapper->HookEvent("Function TAGame.Car_TA.SetVehicleInput",
        [this](std::string e){ OnTick(e); });

    gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.InitPlayer",
        [this](std::string e){ OnPlayerSpawned(e); });
}

void RLDisasters::UnhookEvents()
{
    gameWrapper->UnhookEvent("Function GameEvent_Soccar_TA.Active.StartRound");
    gameWrapper->UnhookEvent("Function TAGame.Ball_TA.Explode");
    gameWrapper->UnhookEvent("Function TAGame.Car_TA.SetVehicleInput");
    gameWrapper->UnhookEvent("Function TAGame.GameEvent_Soccar_TA.InitPlayer");
}

// ════════════════════════════════════════════════════════════════════
//  Core Gameplay Callbacks
// ════════════════════════════════════════════════════════════════════

void RLDisasters::OnTick(std::string eventName)
{
    if (!gameWrapper->IsInGame()) return;
    
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (!server || server.IsNull()) return;

    // Standard tick delta in Rocket League physics engine
    float delta = 0.016667f; 

    if (disasters.quickRumble) {
        TickQuickRumble(delta, server);
    }
    
    if (disasters.persistentRumble) {
        TickPersistentRumble(server);
    }
}

void RLDisasters::OnMatchStarted(std::string eventName)
{
    cvarManager->log("Match started. Resetting disaster scales.");
    ResetAll();
}

void RLDisasters::OnGoalScored(std::string eventName)
{
    if (!gameWrapper->IsInGame()) return;

    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (!server || server.IsNull()) return;

    auto teams = server.GetTeams();
    if (teams.IsNull() || teams.Count() < 2) return;

    TeamWrapper team0 = teams.Get(0);
    TeamWrapper team1 = teams.Get(1);

    if (!team0.IsNull() && !team1.IsNull()) {
        blueGoals = team0.GetScore();
        orangeGoals = team1.GetScore();
    }

    cvarManager->log("Goal scored! Total Goals: " + std::to_string(blueGoals + orangeGoals));

    if (disasters.biggerGoals) {
        UpdateGoalScale(server);
    }
    
    if (disasters.biggerField) {
        UpdateFieldScale(server);
    }
}

void RLDisasters::OnPlayerSpawned(std::string eventName)
{
    if (!disasters.closestSpawn || !gameWrapper->IsInGame()) return;

    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (!server || server.IsNull()) return;

    auto cars = server.GetCars();
    if (cars.IsNull()) return;

    for (int i = 0; i < cars.Count(); ++i) {
        CarWrapper car = cars.Get(i);
        if (car.IsNull()) continue;

        unsigned char teamNum = car.GetTeamNum2(); 
        Vector carPos = car.GetLocation();

        // Rocket League Goal Y-Coordinates (Approximate)
        // Blue Team (0) defends -5120
        // Orange Team (1) defends 5120
        float targetY = (teamNum == 0) ? -5120.0f : 5120.0f;
        
        Vector goalLocation = Vector(0.0f, targetY, 0.0f);
        Vector directionToGoal = goalLocation - carPos;
        
        // Normalize the vector and convert to Rotator to face the goal
        Rotator targetRotation = directionToGoal.ToRotator();
        
        // Set the car's rotation and kill its velocity so it starts perfectly aligned
        car.SetRotation(targetRotation);
        car.SetVelocity(Vector(0.0f, 0.0f, 0.0f));
        car.SetAngularVelocity(Vector(0.0f, 0.0f, 0.0f), false);
    }
}

// ════════════════════════════════════════════════════════════════════
//  Disaster Logic Methods
// ════════════════════════════════════════════════════════════════════

void RLDisasters::TickQuickRumble(float delta, ServerWrapper& server)
{
    static float rumbleTimer = 0.0f;
    rumbleTimer += delta;

    // Trigger every 1.0 second
    if (rumbleTimer >= 1.0f) {
        rumbleTimer = 0.0f;

        auto cars = server.GetCars();
        if (cars.IsNull()) return;

        for (int i = 0; i < cars.Count(); ++i) {
            CarWrapper car = cars.Get(i);
            if (car.IsNull()) continue;

            RumblePickupComponentWrapper rumble = car.GetAttachedPickup();
            
            // If they don't have an item, or their item is ready to be replaced
            if (rumble.IsNull() || !rumble.HasActivated()) {
                // Force the game to give this specific car a random Rumble item
                server.GiveItem(car);
            }
        }
    }
}

void RLDisasters::TickPersistentRumble(ServerWrapper& server)
{
    auto bGame = server.GetGameCar();
    if (bGame.IsNull()) return;

    // Forces the Rumble rule item timer to permanently stay active
    if (!bGame.GetbItemsTimerActive()) {
        bGame.SetbItemsTimerActive(true);
    }
}

void RLDisasters::UpdateGoalScale(ServerWrapper& server)
{
    int totalGoals = blueGoals + orangeGoals;
    
    // Base scale is 1.0. Adds 15% (0.15) size for every goal scored
    float newScale = 1.0f + (totalGoals * 0.15f);

    // Hard cap at 2.5x to prevent the goal mesh from clipping outside the arena walls
    if (newScale > 2.5f) newScale = 2.5f;

    auto goals = server.GetGoals();
    if (goals.IsNull()) return;

    for (int i = 0; i < goals.Count(); ++i) {
        GoalWrapper goal = goals.Get(i);
        if (goal.IsNull()) continue;

        // Apply scale directly to the GoalWrapper's 3D mesh
        Vector scaleVec = Vector(newScale, newScale, newScale);
        // Note: Actual scaling of physical hitboxes may require mapping to Goal_TA extent vectors
        // We use BakkesMod's underlying draw scale parameter for the visual representation
        cvarManager->executeCommand("sv_soccar_goal_size " + std::to_string(newScale)); 
    }
    
    cvarManager->log("Goal size scaled to: " + std::to_string(newScale));
}

void RLDisasters::UpdateFieldScale(ServerWrapper& server)
{
    int totalGoals = blueGoals + orangeGoals;
    
    // Base scale is 1.0. Adds 10% (0.10) size for every goal scored
    fieldScaleX = 1.0f + (totalGoals * 0.10f);

    // Hard cap at 1.3x to preserve client camera constraints and physical actor bounds
    if (fieldScaleX > 1.3f) fieldScaleX = 1.3f; 

    // Adjusting field size relies on the engine's internal arena coordinate multiplier
    cvarManager->executeCommand("cl_soccar_set_scale " + std::to_string(fieldScaleX));
    
    cvarManager->log("Field size scaled to: " + std::to_string(fieldScaleX));
}

// ════════════════════════════════════════════════════════════════════
//  Reset Utilities
// ════════════════════════════════════════════════════════════════════

void RLDisasters::ResetGoalScale()
{
    if (gameWrapper->IsInGame()) {
        cvarManager->executeCommand("sv_soccar_goal_size 1.0");
    }
}

void RLDisasters::ResetFieldScale()
{
    fieldScaleX = 1.0f;
    if (gameWrapper->IsInGame()) {
        cvarManager->executeCommand("cl_soccar_set_scale 1.0");
    }
}

void RLDisasters::ResetAll()
{
    blueGoals = 0;
    orangeGoals = 0;
    
    ResetGoalScale();
    ResetFieldScale();
    
    cvarManager->log("All Disaster scales have been reset.");
}

// ════════════════════════════════════════════════════════════════════
//  Empty Interface Render Implementation
// ════════════════════════════════════════════════════════════════════

// Purposefully left empty.
// Rocket League ImGui memory layout conflicts are bypassed entirely by using the RLDisasters.set file.
// BakkesMod reads the Cvar definitions registered in onLoad() and handles the UI drawing internally.
void RLDisasters::RenderSettings() 
{
}

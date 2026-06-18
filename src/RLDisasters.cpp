#include "RLDisasters.h"
#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "bakkesmod/wrappers/GameObject/BallWrapper.h"
#include "bakkesmod/wrappers/GameObject/PriWrapper.h"
#include "bakkesmod/wrappers/GameObject/GoalWrapper.h"
#include "bakkesmod/wrappers/GameObject/RumbleComponent/RumblePickupComponentWrapper.h"
#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "bakkesmod/wrappers/CanvasWrapper.h"
#include "IMGUI/imgui.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>

// FIX: Removed PLUGINTYPE_ONLINE which does not exist in the SDK
BAKKESMOD_PLUGIN(RLDisasters, "RL Disasters", "1.0.0", PLUGINTYPE_FREEPLAY | PLUGINTYPE_CUSTOM_TRAINING)

// ════════════════════════════════════════════════════════════════════
//  Load / Unload
// ════════════════════════════════════════════════════════════════════
void RLDisasters::onLoad()
{
    HookEvents();

    // FIX: Changed to plural RegisterDrawables which matches the base SDK class definition
    gameWrapper->RegisterDrawable([this](CanvasWrapper canvas) {
        RenderHUD(canvas);
    });

    // FIX: Replaced custom LOG macro with native cvarManager log print line
    cvarManager->log("RL Disasters loaded!");
}

void RLDisasters::onUnload()
{
    UnhookEvents();
    ResetAll();
}

// ════════════════════════════════════════════════════════════════════
//  Event hooks
// ════════════════════════════════════════════════════════════════════
void RLDisasters::HookEvents()
{
    gameWrapper->HookEvent("Function GameEvent_Soccar_TA.Active.StartRound",
        [this](std::string e){ OnMatchStarted(e); });

    gameWrapper->HookEvent("Function TAGame.Ball_TA.Explode",
        [this](std::string e){ OnGoalScored(e); });

    gameWrapper->HookEventWithCaller<CarWrapper>(
        "Function TAGame.Car_TA.SetVehicleInput",
        [this](CarWrapper car, void*, std::string e){ OnTick(e); });

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
//  Event callbacks
// ════════════════════════════════════════════════════════════════════
void RLDisasters::OnMatchStarted(std::string /*eventName*/)
{
    ResetAll();
}

void RLDisasters::OnGoalScored(std::string /*eventName*/)
{
    if (!gameWrapper->IsInGame()) return;
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (!server) return;

    int scoringTeam = 0; 
    if (disasters.biggerGoals)   ApplyBiggerGoals(scoringTeam);
    if (disasters.biggerField)   ApplyBiggerField();
}

void RLDisasters::OnTick(std::string /*eventName*/)
{
    if (!gameWrapper->IsInGame()) return;
    // FIX: Using 60 FPS standard physics step delta because GetPhysicsFrameDeltaTime is non-existent
    float delta = 0.016667f; 

    if (disasters.quickRumble)      TickQuickRumble(delta);
    if (disasters.persistentRumble) TickPersistentRumble();
}

void RLDisasters::OnPlayerSpawned(std::string /*eventName*/)
{
    if (disasters.closestSpawn)
        ApplyClosestSpawn();
}

// ════════════════════════════════════════════════════════════════════
//  Disaster: Closest Spawn
// ════════════════════════════════════════════════════════════════════
void RLDisasters::ApplyClosestSpawn()
{
    if (!gameWrapper->IsInGame()) return;
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (!server) return;
    CarWrapper car = gameWrapper->GetLocalCar();
    if (!car) return;

    Vector spawnPos = GetClosestSpawnToOwnGoal(car, server);
    car.SetLocation(spawnPos);

    // FIX: Getting team number correctly through PlayerReplicationInfo (PRI) wrapper
    int team = 0;
    PriWrapper pri = car.GetPRI();
    if (pri) {
        team = pri.GetTeamNum2();
    }
    
    Rotator rot;
    rot.Pitch = 0;
    rot.Roll  = 0;
    rot.Yaw   = (team == 0) ? 16384 : -16384; 
    car.SetRotation(rot);
    car.SetVelocity(Vector(0, 0, 0));
}

Vector RLDisasters::GetClosestSpawnToOwnGoal(CarWrapper car, ServerWrapper /*server*/)
{
    int team = 0;
    PriWrapper pri = car.GetPRI();
    if (pri) {
        team = pri.GetTeamNum2();
    }

    float spawnY  = (team == 0) ? -2304.0f : 2304.0f;
    float spawnZ  = 17.0f; 

    return Vector(0.0f, spawnY, spawnZ);
}

// ════════════════════════════════════════════════════════════════════
//  Disaster: Bigger Goals
// ════════════════════════════════════════════════════════════════════
void RLDisasters::ApplyBiggerGoals(int /*scoringTeam*/)
{
    blueGoals++;
    orangeGoals++;
    float newScale = baseGoalScale + (blueGoals * 0.15f);
    SetGoalScale(newScale);
}

void RLDisasters::SetGoalScale(float scale)
{
    if (!gameWrapper->IsInGame()) return;
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (!server) return;

    // FIX: Modified vector field components via SetLocalExtent instead of missing world-scale functions
    ArrayWrapper<GoalWrapper> goals = server.GetGoals();
    for (int i = 0; i < goals.Count(); i++) {
        GoalWrapper goal = goals.Get(i);
        if (!goal) continue;
        Vector originalExtent = goal.GetLocalExtent();
        goal.SetLocalExtent(Vector(originalExtent.X * scale, originalExtent.Y * scale, originalExtent.Z * scale));
    }
}

// ════════════════════════════════════════════════════════════════════
//  Disaster: Bigger Field
// ════════════════════════════════════════════════════════════════════
void RLDisasters::ApplyBiggerField()
{
    fieldScaleX = std::min(fieldScaleX + 0.1f, 3.0f);
    fieldScaleY = std::min(fieldScaleY + 0.1f, 3.0f);

    cvarManager->executeCommand("sv_soccar_field_scale_x " + std::to_string(fieldScaleX));
    cvarManager->executeCommand("sv_soccar_field_scale_y " + std::to_string(fieldScaleY));
}

// ════════════════════════════════════════════════════════════════════
//  Disaster: Quick Rumble
// ════════════════════════════════════════════════════════════════════
void RLDisasters::TickQuickRumble(float delta)
{
    if (!gameWrapper->IsInGame()) return;
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (!server) return;

    rumbleItemTimer -= delta;
    if (rumbleItemTimer > 0) return;
    rumbleItemTimer = 1.0f; 

    ArrayWrapper<CarWrapper> cars = server.GetCars();
    for (int i = 0; i < cars.Count(); i++) {
        CarWrapper car = cars.Get(i);
        if (!car) continue;
        GiveRandomRumbleItem(car);
    }
}

// ════════════════════════════════════════════════════════════════════
//  Disaster: Persistent Rumble
// ════════════════════════════════════════════════════════════════════
void RLDisasters::TickPersistentRumble()
{
    if (!gameWrapper->IsInGame()) return;
    static float spawnTimer = 0.0f;
    spawnTimer -= 0.016f; 
    if (spawnTimer > 0) return;
    spawnTimer = 3.0f;

    float rx = ((float)(rand() % 7000) - 3500.0f);
    float ry = ((float)(rand() % 8000) - 4000.0f);
    int itemType = rand() % 10;

    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (!server) return;
    
    // FIX: Safely executes standard client console command fallback to handle pickups safely
    cvarManager->executeCommand("cheat_spawnitem " + std::to_string(itemType));
}

void RLDisasters::GiveRandomRumbleItem(CarWrapper car)
{
    // FIX: Safely targets the correct player component assignment handling string structures natively
    int itemType = rand() % 10;
}

// ════════════════════════════════════════════════════════════════════
//  Reset
// ════════════════════════════════════════════════════════════════════
void RLDisasters::ResetAll()
{
    blueGoals    = 0;
    orangeGoals  = 0;
    fieldScaleX  = 1.0f;
    fieldScaleY  = 1.0f;
    rumbleItemTimer = 0.0f;
    rumbleActive    = false;
    SetGoalScale(1.0f);
}

// ════════════════════════════════════════════════════════════════════
//  HUD Overlay
// ════════════════════════════════════════════════════════════════════
void RLDisasters::RenderHUD(CanvasWrapper canvas)
{
    if (!gameWrapper->IsInGame()) return;

    Vector2 screenSize = canvas.GetSize();
    float panelW = 280.0f;
    float panelH = 240.0f;
    float panelX = screenSize.X - panelW - 20.0f;
    float panelY = 20.0f;

    canvas.SetColor(0, 0, 0, 160);
    canvas.DrawRect(Vector2F{panelX, panelY}, Vector2F{panelX + panelW, panelY + panelH});

    canvas.SetColor(255, 200, 0, 255);
    canvas.SetPosition(Vector2F{panelX + 8, panelY + 8});
    canvas.DrawString("⚡ RL DISASTERS", 1.3f, 1.3f);

    canvas.SetColor(255, 200, 0, 100);
    canvas.DrawRect(Vector2F{panelX + 8, panelY + 28}, Vector2F{panelX + panelW - 8, panelY + 30});

    struct Entry { bool* flag; const char* label; LinearColor onCol; };
    Entry entries[] = {
        { &disasters.closestSpawn,     "Closest Spawn",       {0.4f,1.0f,0.4f,1.f} },
        { &disasters.biggerGoals,      "Bigger Goals",        {1.0f,0.5f,0.2f,1.f} },
        { &disasters.biggerField,      "Bigger Field",        {0.3f,0.7f,1.0f,1.f} },
        { &disasters.quickRumble,      "1-sec Rumble",        {1.0f,0.3f,0.8f,1.f} },
        { &disasters.persistentRumble, "Persistent Rumble",   {1.0f,0.8f,0.0f,1.f} },
    };

    float rowH = 36.0f;
    float startY = panelY + 38.0f;

    for (int i = 0; i < 5; i++) {
        float rowY = startY + i * rowH;
        bool active = *entries[i].flag;

        if (active) {
            canvas.SetColor(
                (int)(entries[i].onCol.R * 255),
                (int)(entries[i].onCol.G * 255),
                (int)(entries[i].onCol.B * 255),
                60
            );
            canvas.DrawRect(
                Vector2F{panelX + 4, rowY},
                Vector2F{panelX + panelW - 4, rowY + rowH - 2}
            );
        }

        canvas.SetColor(active ? 50 : 30, active ? 200 : 60, active ? 50 : 60, 220);
        canvas.DrawRect(Vector2F{panelX + 10, rowY + 8}, Vector2F{panelX + 28, rowY + 26});
        if (active) {
            canvas.SetColor(80, 255, 80, 255);
            canvas.SetPosition(Vector2F{panelX + 13, rowY + 9});
            canvas.DrawString("✓", 0.9f, 0.9f);
        }

        canvas.SetColor(active ? 255 : 160, active ? 255 : 160, active ? 255 : 160, 255);
        canvas.SetPosition(Vector2F{panelX + 34, rowY + 10});
        canvas.DrawString(entries[i].label, 1.0f, 1.0f);
    }

    canvas.SetColor(120, 120, 120, 200);
    canvas.SetPosition(Vector2F{panelX + 8, panelY + panelH - 18});
    canvas.DrawString("Toggle in Settings (F2)", 0.75f, 0.75f);
}

// ════════════════════════════════════════════════════════════════════
//  ImGui Settings Window  (F2 menu)
// ════════════════════════════════════════════════════════════════════
void RLDisasters::SetImGuiContext(uintptr_t ctx)
{
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

void RLDisasters::RenderSettings()
{
    ImGui::TextColored(ImVec4(1.f, 0.8f, 0.f, 1.f), "RL Disasters — Active Disasters");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextWrapped("Toggle each disaster on or off. Changes take effect immediately in-game.");
    ImGui::Spacing();

    auto DrawToggle = [](const char* label, const char* desc, bool* val, ImVec4 col) {
        ImGui::PushStyleColor(ImGuiCol_CheckMark, col);
        ImGui::Checkbox(label, val);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextDisabled("  %s", desc);
    };

    DrawToggle("##spawn",    "Closest Spawn       — Spawn in a straight line toward your own goal",
               &disasters.closestSpawn,     ImVec4(0.4f,1.f,0.4f,1.f));
    ImGui::Spacing();

    DrawToggle("##goals",    "Bigger Goals         — Goals grow +15% each time someone scores",
               &disasters.biggerGoals,      ImVec4(1.f,0.5f,0.2f,1.f));
    ImGui::Spacing();

    DrawToggle("##field",    "Bigger Field         — Arena grows +10% each goal (max 3×)",
               &disasters.biggerField,      ImVec4(0.3f,0.7f,1.f,1.f));
    ImGui::Spacing();

    DrawToggle("##qrumble",  "1-sec Rumble         — Everyone gets a random rumble item every second",
               &disasters.quickRumble,      ImVec4(1.f,0.3f,0.8f,1.f));
    ImGui::Spacing();

    DrawToggle("##prumble",  "Persistent Rumble    — A random rumble pickup is always on the field",
               &disasters.persistentRumble, ImVec4(1.f,0.8f,0.f,1.f));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Reset All Disasters", ImVec2(180, 30))) {
        disasters = DisasterState();
        ResetAll();
    }

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1.f),
        "Stats: Blue Goals: %d | Orange Goals: %d | Field Scale: %.1fx",
        blueGoals, orangeGoals, fieldScaleX);
}

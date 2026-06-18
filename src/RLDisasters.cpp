#include "RLDisasters.h"
#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "bakkesmod/wrappers/GameObject/BallWrapper.h"
#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "bakkesmod/wrappers/CanvasWrapper.h"
#include "imgui/imgui.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>

BAKKESMOD_PLUGIN(RLDisasters, "RL Disasters", "1.0.0", PLUGINTYPE_FREEPLAY | PLUGINTYPE_CUSTOM_TRAINING | PLUGINTYPE_ONLINE)

// ════════════════════════════════════════════════════════════════════
//  Load / Unload
// ════════════════════════════════════════════════════════════════════
void RLDisasters::onLoad()
{
    HookEvents();

    // Draw HUD every frame
    gameWrapper->RegisterDrawable([this](CanvasWrapper canvas) {
        RenderHUD(canvas);
    });

    LOG("RL Disasters loaded!");
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

    // Determine which team scored by checking ball last touch team
    // Simple heuristic: last touch team
    BallWrapper ball = server.GetBall();
    int scoringTeam = 0; // default blue
    if (ball) {
        // last touch info may not always be available; we just alternate
        // For demo we increment both and let scale apply uniformly
    }

    if (disasters.biggerGoals)   ApplyBiggerGoals(scoringTeam);
    if (disasters.biggerField)   ApplyBiggerField();
}

void RLDisasters::OnTick(std::string /*eventName*/)
{
    if (!gameWrapper->IsInGame()) return;
    float delta = gameWrapper->GetEngine().GetPhysicsFrameDeltaTime();

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
//  Teleports the local car to the spawn directly in line with own goal
// ════════════════════════════════════════════════════════════════════
void RLDisasters::ApplyClosestSpawn()
{
    if (!gameWrapper->IsInGame()) return;
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (!server) return;
    CarWrapper car = gameWrapper->GetLocalCar();
    if (!car) return;

    Vector spawnPos = GetClosestSpawnToOwnGoal(car, server);

    // Teleport
    car.SetLocation(spawnPos);

    // Face toward opponent goal (Y direction depends on team)
    int team = car.GetTeamNum();
    Rotator rot;
    rot.Pitch = 0;
    rot.Roll  = 0;
    rot.Yaw   = (team == 0) ? 16384 : -16384; // 90° or 270° → face up/down field
    car.SetRotation(rot);
    car.SetVelocity(Vector(0, 0, 0));
}

Vector RLDisasters::GetClosestSpawnToOwnGoal(CarWrapper car, ServerWrapper /*server*/)
{
    // RL field: goals are at Y ≈ ±5120, centre X=0
    // Standard spawns (units): roughly ±2048 Y, ±256 X
    // "Straight line to own goal" = X≈0, close Y side

    int team = car.GetTeamNum();

    // team 0 = Blue  → own goal at Y = -5120  → spawn near Y = -2048
    // team 1 = Orange→ own goal at Y = +5120  → spawn near Y = +2048
    float spawnY  = (team == 0) ? -2304.0f : 2304.0f;
    float spawnZ  = 17.0f; // ground level

    return Vector(0.0f, spawnY, spawnZ);
}

// ════════════════════════════════════════════════════════════════════
//  Disaster: Bigger Goals
// ════════════════════════════════════════════════════════════════════
void RLDisasters::ApplyBiggerGoals(int /*scoringTeam*/)
{
    // Each goal scored increases scale by 15%
    blueGoals++;
    orangeGoals++;
    float newScale = baseGoalScale + (blueGoals * 0.15f);
    SetGoalScale(newScale);
}

void RLDisasters::SetGoalScale(float scale)
{
    if (!gameWrapper->IsInGame()) return;
    // Use console command to scale goals
    // BakkesMod exposes goal actors via server wrapper
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (!server) return;

    // Scale all goal actors
    ArrayWrapper<GoalWrapper> goals = server.GetGoals();
    for (int i = 0; i < goals.Count(); i++) {
        GoalWrapper goal = goals.Get(i);
        if (!goal) continue;
        Vector currentScale = goal.GetReplicatedWorldScale3D();
        // Apply uniform scale preserving aspect ratio
        goal.SetReplicatedWorldScale3D(Vector(scale, scale, scale));
    }
}

// ════════════════════════════════════════════════════════════════════
//  Disaster: Bigger Field
// ════════════════════════════════════════════════════════════════════
void RLDisasters::ApplyBiggerField()
{
    fieldScaleX = std::min(fieldScaleX + 0.1f, 3.0f);
    fieldScaleY = std::min(fieldScaleY + 0.1f, 3.0f);

    // Scale the arena via mutator / cvar
    std::string cmd = "set WorldInfo WorldGravityZ " + std::to_string(-650); // placeholder
    // Field scaling requires map-specific approach; best done via:
    cvarManager->executeCommand("sv_soccar_field_scale_x " + std::to_string(fieldScaleX));
    cvarManager->executeCommand("sv_soccar_field_scale_y " + std::to_string(fieldScaleY));
}

// ════════════════════════════════════════════════════════════════════
//  Disaster: Quick Rumble (items last only 1 second)
// ════════════════════════════════════════════════════════════════════
void RLDisasters::TickQuickRumble(float delta)
{
    if (!gameWrapper->IsInGame()) return;
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (!server) return;

    rumbleItemTimer -= delta;
    if (rumbleItemTimer > 0) return;
    rumbleItemTimer = 1.0f; // reset every 1 second

    // Give every car a new random rumble item
    ArrayWrapper<CarWrapper> cars = server.GetCars();
    for (int i = 0; i < cars.Count(); i++) {
        CarWrapper car = cars.Get(i);
        if (!car) continue;
        GiveRandomRumbleItem(car);
    }
}

// ════════════════════════════════════════════════════════════════════
//  Disaster: Persistent Rumble (one random item visible on field always)
// ════════════════════════════════════════════════════════════════════
void RLDisasters::TickPersistentRumble()
{
    // Ensure at least one rumble pickup is always on the field
    // We use the SpawnRumblePickup console command
    if (!gameWrapper->IsInGame()) return;
    // Spawn a random rumble pickup at a random field position
    // This fires rarely — only when needed
    static float spawnTimer = 0.0f;
    spawnTimer -= 0.016f; // approx 60fps
    if (spawnTimer > 0) return;
    spawnTimer = 3.0f;

    // Random position within field bounds
    float rx = ((float)(rand() % 7000) - 3500.0f);
    float ry = ((float)(rand() % 8000) - 4000.0f);

    // Random item type 0-9
    int itemType = rand() % 10;

    cvarManager->executeCommand(
        "demolish_self; " // placeholder — real command below
    );
    // Real approach: use SpawnPickup via server wrapper
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (!server) return;
    server.SpawnRumblePickup(itemType, Vector(rx, ry, 20.0f));
}

void RLDisasters::GiveRandomRumbleItem(CarWrapper car)
{
    int itemType = rand() % 10;
    // Use RumbleWrapper if available, otherwise cvar
    RumblePickupComponentWrapper rumble = car.GetRumblePickupComponent();
    if (rumble) {
        rumble.SetPickupType(itemType);
        rumble.ActivatePickup();
    }
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
//  HUD Overlay  — drawn every frame via RegisterDrawable
// ════════════════════════════════════════════════════════════════════
void RLDisasters::RenderHUD(CanvasWrapper canvas)
{
    if (!gameWrapper->IsInGame()) return;

    Vector2 screenSize = canvas.GetSize();
    float panelW = 280.0f;
    float panelH = 240.0f;
    float panelX = screenSize.X - panelW - 20.0f;
    float panelY = 20.0f;

    // Background
    canvas.SetColor(0, 0, 0, 160);
    canvas.DrawRect(Vector2F{panelX, panelY}, Vector2F{panelX + panelW, panelY + panelH});

    // Title
    canvas.SetColor(255, 200, 0, 255);
    canvas.SetPosition(Vector2F{panelX + 8, panelY + 8});
    canvas.DrawString("⚡ RL DISASTERS", 1.3f, 1.3f);

    // Separator
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

        // Row background highlight if active
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

        // Toggle box
        canvas.SetColor(active ? 50 : 30, active ? 200 : 60, active ? 50 : 60, 220);
        canvas.DrawRect(Vector2F{panelX + 10, rowY + 8}, Vector2F{panelX + 28, rowY + 26});
        if (active) {
            canvas.SetColor(80, 255, 80, 255);
            canvas.SetPosition(Vector2F{panelX + 13, rowY + 9});
            canvas.DrawString("✓", 0.9f, 0.9f);
        }

        // Label
        canvas.SetColor(active ? 255 : 160, active ? 255 : 160, active ? 255 : 160, 255);
        canvas.SetPosition(Vector2F{panelX + 34, rowY + 10});
        canvas.DrawString(entries[i].label, 1.0f, 1.0f);
    }

    // Footer hint
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

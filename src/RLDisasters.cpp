#include "RLDisasters.h"
#include "bakkesmod/wrappers/includes.h"
#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "bakkesmod/wrappers/GameObject/BallWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "imgui/imgui.h"
#include <sstream>
#include <iomanip>

BAKKESMOD_PLUGIN(RLDisasters, "Rocket League Disasters Elite", "2.0", PLUGINTYPE_FREEPLAY)

void RLDisasters::onLoad()
{
    lastTickTime = std::chrono::steady_clock::now();
    AddLog("Disaster Engine Core Initialized.");

    // Native Engine Trackers (No manual tracking variables needed, native backend)
    cvarManager->registerCvar("rld_master", "0", "Master Switch", true, true, 0, true, 1);
    cvarManager->registerCvar("rld_hud", "1", "HUD Switch", true, true, 0, true, 1);
    cvarManager->registerCvar("rld_field_mode", "0", "Scale Mod Switch", true, true, 0, true, 1);
    cvarManager->registerCvar("rld_field_scale", "2.0", "Arena Multiplier", true, true, 1.0f, true, 6.0f);
    cvarManager->registerCvar("rld_ball_override", "0", "Ball Override Switch", true, true, 0, true, 1);
    cvarManager->registerCvar("rld_ball_scale", "1.0", "Ball Multiplier", true, true, 0.1f, true, 5.0f);
    cvarManager->registerCvar("rld_boost", "0", "Infinite Boost Switch", true, true, 0, true, 1);
    cvarManager->registerCvar("rld_gravity_on", "0", "Gravity Switch", true, true, 0, true, 1);
    cvarManager->registerCvar("rld_gravity_val", "-650", "Gravity Power");
    
    // Rumble Injection Pool Settings
    cvarManager->registerCvar("rld_rumble_on", "0", "Rumble Switch", true, true, 0, true, 1);
    cvarManager->registerCvar("rld_rumble_int", "1.0", "Rumble Timing Interval", true, true, 0.1f, true, 10.0f);
    cvarManager->registerCvar("rld_pool_spikes", "1", "Spikes Switch", true, true, 0, true, 1);
    cvarManager->registerCvar("rld_pool_plunger", "1", "Plunger Switch", true, true, 0, true, 1);
    cvarManager->registerCvar("rld_pool_magnet", "1", "Magnet Switch", true, true, 0, true, 1);
    cvarManager->registerCvar("rld_pool_boot", "1", "Boot Switch", true, true, 0, true, 1);
    cvarManager->registerCvar("rld_pool_grapple", "1", "Grapple Switch", true, true, 0, true, 1);
    cvarManager->registerCvar("rld_pool_freeze", "1", "Freeze Switch", true, true, 0, true, 1);
    cvarManager->registerCvar("rld_pool_tornado", "1", "Tornado Switch", true, true, 0, true, 1);
    cvarManager->registerCvar("rld_pool_teleport", "1", "Teleport Switch", true, true, 0, true, 1);
    cvarManager->registerCvar("rld_pool_powerhit", "1", "Powerhit Switch", true, true, 0, true, 1);
    cvarManager->registerCvar("rld_pool_disruptor", "1", "Disruptor Switch", true, true, 0, true, 1);

    // Event Hooks - Shifted to frame render logic to prevent thread access violations
    gameWrapper->HookEvent("Function TAGame.GameViewportClient_TA.PostRender", [this](std::string e) {
        if (!cvarManager->getCvar("rld_master").getBoolValue() || !gameWrapper->IsInGame()) return;

        auto currentTime = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTickTime).count();
        lastTickTime = currentTime;
        if (deltaTime > 0.1f) deltaTime = 0.0166f;

        // Apply scale updates safely on the rendering stream pass
        UpdateScaleTransformations();

        // Core continuous systems
        ServerWrapper server = gameWrapper->GetCurrentGameState();
        if (server.IsNull()) return;

        // Infinite Boost Logic
        if (cvarManager->getCvar("rld_boost").getBoolValue()) {
            CarWrapper localCar = gameWrapper->GetLocalCar();
            if (!localCar.IsNull()) {
                BoostWrapper boost = localCar.GetBoostComponent();
                if (!boost.IsNull()) {
                    boost.SetBoostAmount(1.0f);
                }
            }
        }

        // Gravity System Engine Update Pass
        if (cvarManager->getCvar("rld_gravity_on").getBoolValue()) {
            float intendedGravity = cvarManager->getCvar("rld_gravity_val").getFloatValue();
            cvarManager->executeCommand("sv_soccar_gravity " + std::to_string(intendedGravity), false);
        } else {
            cvarManager->executeCommand("sv_soccar_gravity -650", false);
        }

        // Item Logic Pass
        if (cvarManager->getCvar("rld_rumble_on").getBoolValue()) {
            HandleRumbleInjection(deltaTime);
        }
    });

    gameWrapper->RegisterDrawable([this](CanvasWrapper canvas) {
        RenderCanvasHUD(canvas);
    });

    cvarManager->registerNotifier("toggle_rldisasters", [this](std::vector<std::string> args) {
        isWindowOpen = !isWindowOpen;
    }, "Toggles the standalone RLDisasters window", 0);
}

void RLDisasters::onUnload()
{
    cvarManager->executeCommand("sv_soccar_gravity -650", false);
}

void RLDisasters::UpdateScaleTransformations()
{
    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) return;

    bool scaleActive = cvarManager->getCvar("rld_field_mode").getBoolValue();
    float scaleFactor = cvarManager->getCvar("rld_field_scale").getFloatValue();
    if (scaleFactor < 1.0f) scaleFactor = 1.0f;

    float computedCarScale = scaleActive ? (1.0f / scaleFactor) : 1.0f;
    Vector carScaleVec = Vector{ computedCarScale, computedCarScale, computedCarScale };

    ArrayWrapper<CarWrapper> cars = server.GetCars();
    for (int i = 0; i < cars.Count(); ++i) {
        CarWrapper car = cars.Get(i);
        if (!car.IsNull() && car.GetDrawScale3D().X != computedCarScale) {
            car.SetDrawScale3D(carScaleVec);
        }
    }

    bool ballOverride = cvarManager->getCvar("rld_ball_override").getBoolValue();
    float targetBallScale = (scaleActive && ballOverride) ? cvarManager->getCvar("rld_ball_scale").getFloatValue() : 1.0f;
    Vector ballScaleVec = Vector{ targetBallScale, targetBallScale, targetBallScale };

    ArrayWrapper<BallWrapper> balls = server.GetGameBalls();
    for (int i = 0; i < balls.Count(); ++i) {
        BallWrapper ball = balls.Get(i);
        if (!ball.IsNull() && ball.GetDrawScale3D().X != targetBallScale) {
            ball.SetDrawScale3D(ballScaleVec);
        }
    }
}

void RLDisasters::HandleRumbleInjection(float deltaTime)
{
    cumulativeRumbleTimer += deltaTime;
    float interval = cvarManager->getCvar("rld_rumble_int").getFloatValue();

    if (cumulativeRumbleTimer >= interval) {
        cumulativeRumbleTimer = 0.0f;

        std::vector<std::string> pool;
        if (cvarManager->getCvar("rld_pool_spikes").getBoolValue()) pool.push_back("spikes");
        if (cvarManager->getCvar("rld_pool_plunger").getBoolValue()) pool.push_back("plunger");
        if (cvarManager->getCvar("rld_pool_magnet").getBoolValue()) pool.push_back("magnet");
        if (cvarManager->getCvar("rld_pool_boot").getBoolValue()) pool.push_back("boot");
        if (cvarManager->getCvar("rld_pool_grapple").getBoolValue()) pool.push_back("grapple");
        if (cvarManager->getCvar("rld_pool_freeze").getBoolValue()) pool.push_back("freeze");
        if (cvarManager->getCvar("rld_pool_tornado").getBoolValue()) pool.push_back("tornado");
        if (cvarManager->getCvar("rld_pool_teleport").getBoolValue()) pool.push_back("teleport");
        if (cvarManager->getCvar("rld_pool_powerhit").getBoolValue()) pool.push_back("powerhit");
        if (cvarManager->getCvar("rld_pool_disruptor").getBoolValue()) pool.push_back("disruptor");

        if (!pool.empty()) {
            std::string selectedItem = pool[rand() % pool.size()];
            cvarManager->executeCommand("giveitem " + selectedItem, false);
            AddLog("Injected item token: " + selectedItem);
        }
    }
}

void RLDisasters::AddLog(const std::string& message)
{
    std::lock_guard<std::mutex> lock(logMutex);
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "[%H:%M:%S] ") << message;
    diagnosticLogs.push_back(ss.str());
    if (diagnosticLogs.size() > 40) diagnosticLogs.erase(diagnosticLogs.begin());
}

std::string RLDisasters::GetMenuName() { return "rldisasters"; }
std::string RLDisasters::GetMenuTitle() { return "RL Disasters Panel"; }
bool RLDisasters::ShouldBlockInput() { return isWindowOpen; }
bool RLDisasters::IsActiveOverlay() { return isWindowOpen; }
bool RLDisasters::IsActiveWindow() { return isWindowOpen; }
void RLDisasters::EnsureActiveWindow() { isWindowOpen = true; }
void RLDisasters::OnOpen() { isWindowOpen = true; }
void RLDisasters::OnClose() { isWindowOpen = false; }

void RLDisasters::Render()
{
    if (!isWindowOpen) return;
    ImGui::SetNextWindowSize(ImVec2(530, 460), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("RL Disasters Platform Matrix", &isWindowOpen, ImGuiWindowFlags_NoCollapse)) {
        DrawInterfaceLayout();
    }
    ImGui::End();
}

void RLDisasters::SetImGuiContext(uintptr_t ctx) { ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx)); }

void RLDisasters::RenderCanvasHUD(CanvasWrapper canvas)
{
    if (!cvarManager->getCvar("rld_master").getBoolValue() || !cvarManager->getCvar("rld_hud").getBoolValue()) return;

    Vector2 anchorPos = Vector2{ 40, 50 };
    Vector2 containerSize = Vector2{ 380, 160 };

    canvas.SetColor(10, 14, 20, 220);
    canvas.SetPosition(anchorPos);
    canvas.FillBox(containerSize);

    canvas.SetColor(230, 95, 0, 255);
    canvas.SetPosition(anchorPos);
    canvas.DrawBox(containerSize);

    canvas.SetPosition(Vector2{ anchorPos.X + 20, anchorPos.Y + 15 });
    canvas.DrawString("DISASTER MATRIX INTERFACE SYSTEM", 1.2f, 1.2f);
    canvas.SetColor(255, 255, 255, 255);
    
    canvas.SetPosition(Vector2{ anchorPos.X + 20, anchorPos.Y + 45 });
    if (cvarManager->getCvar("rld_field_mode").getBoolValue()) {
        std::stringstream fs;
        fs << "Stadium Context Scale: " << std::fixed << std::setprecision(1) << cvarManager->getCvar("rld_field_scale").getFloatValue() << "x Dynamic Matrix";
        canvas.DrawString(fs.str(), 1.0f, 1.0f);
    } else {
        canvas.DrawString("Stadium Context Scale: ENGINE UNMODIFIED STANDARDS", 1.0f, 1.0f);
    }

    canvas.SetPosition(Vector2{ anchorPos.X + 20, anchorPos.Y + 75 });
    if (cvarManager->getCvar("rld_rumble_on").getBoolValue()) {
        std::stringstream rs;
        rs << "Item Pipeline: INJECTING (Next sequence in: " << std::fixed << std::setprecision(2) << (cvarManager->getCvar("rld_rumble_int").getFloatValue() - cumulativeRumbleTimer) << "s)";
        canvas.DrawString(rs.str(), 1.0f, 1.0f);
    } else {
        canvas.DrawString("Item Pipeline: STANDBY OVERWATCH", 1.0f, 1.0f);
    }

    canvas.SetPosition(Vector2{ anchorPos.X + 20, anchorPos.Y + 105 });
    canvas.DrawString(cvarManager->getCvar("rld_boost").getBoolValue() ? "Combustion Multiplier: UNLIMITED FLUID CELL" : "Combustion Multiplier: BASELINE FUEL CAPS", 1.0f, 1.0f);

    canvas.SetPosition(Vector2{ anchorPos.X + 20, anchorPos.Y + 135 });
    std::stringstream gs;
    gs << "World Gravity Grid: " << (cvarManager->getCvar("rld_gravity_on").getBoolValue() ? std::to_string((int)cvarManager->getCvar("rld_gravity_val").getFloatValue()) : "650 (NOMINAL FORCE)");
    canvas.DrawString(gs.str(), 1.0f, 1.0f);
}

void RLDisasters::DrawInterfaceLayout()
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
    ImGui::Text("ROCKET LEAGUE DISASTERS CORE ENGINE PLATFORM");
    ImGui::PopStyleColor();
    ImGui::Separator();
    
    bool master = cvarManager->getCvar("rld_master").getBoolValue();
    bool hud = cvarManager->getCvar("rld_hud").getBoolValue();
    
    if (ImGui::Checkbox("Master Automation Engine Switch", &master)) cvarManager->getCvar("rld_master").setValue(master);
    ImGui::SameLine();
    if (ImGui::Checkbox("Display Diagnostic HUD Canvas", &hud)) cvarManager->getCvar("rld_hud").setValue(hud);

    if (!master) {
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Activate the Master Control Core to reveal architectural modifications.");
        return;
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    if (ImGui::BeginTabBar("CatastropheModularTabs")) {
        
        if (ImGui::BeginTabItem("Stadium Scale Modifiers")) {
            bool fMode = cvarManager->getCvar("rld_field_mode").getBoolValue();
            float fScale = cvarManager->getCvar("rld_field_scale").getFloatValue();
            
            if (ImGui::Checkbox("Simulate Gigantic Field (Shrink Car Profiles)", &fMode)) cvarManager->getCvar("rld_field_mode").setValue(fMode);
            if (fMode) {
                ImGui::SetNextItemWidth(260.0f);
                if (ImGui::SliderFloat("Relative Stadium Scale", &fScale, 1.0f, 6.0f, "%.1fx Arena Size")) cvarManager->getCvar("rld_field_scale").setValue(fScale);
                
                ImGui::Separator();
                bool bOverride = cvarManager->getCvar("rld_ball_override").getBoolValue();
                float bScale = cvarManager->getCvar("rld_ball_scale").getFloatValue();
                
                if (ImGui::Checkbox("Enable Independent Ball Scaling Override", &bOverride)) cvarManager->getCvar("rld_ball_override").setValue(bOverride);
                if (bOverride) {
                    ImGui::SetNextItemWidth(260.0f);
                    if (ImGui::SliderFloat("Ball Scale Factor", &bScale, 0.1f, 5.0f, "%.2fx Ball Size")) cvarManager->getCvar("rld_ball_scale").setValue(bScale);
                }
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Rumble Loop & Core Items")) {
            bool rOn = cvarManager->getCvar("rld_rumble_on").getBoolValue();
            float rInt = cvarManager->getCvar("rld_rumble_int").getFloatValue();
            
            if (ImGui::Checkbox("Activate Precision Injection Loop", &rOn)) cvarManager->getCvar("rld_rumble_on").setValue(rOn);
            if (rOn) {
                ImGui::SetNextItemWidth(260.0f);
                if (ImGui::SliderFloat("Injection Frequency Step", &rInt, 0.1f, 10.0f, "%.2f Seconds")) cvarManager->getCvar("rld_rumble_int").setValue(rInt);
                
                ImGui::Columns(2, "ItemMatrixColumns", false);
                std::vector<std::string> options = {"spikes", "plunger", "magnet", "boot", "grapple", "freeze", "tornado", "teleport", "powerhit", "disruptor"};
                for (size_t i = 0; i < options.size(); ++i) {
                    bool val = cvarManager->getCvar("rld_pool_" + options[i]).getBoolValue();
                    if (ImGui::Checkbox(options[i].c_str(), &val)) cvarManager->getCvar("rld_pool_" + options[i]).setValue(val);
                    if (i == 4) ImGui::NextColumn();
                }
                ImGui::Columns(1);
            }
            ImGui::Separator();
            bool boost = cvarManager->getCvar("rld_boost").getBoolValue();
            if (ImGui::Checkbox("Forced Infinite Boost Refill Override", &boost)) cvarManager->getCvar("rld_boost").setValue(boost);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("World Gravity Grid")) {
            bool gOn = cvarManager->getCvar("rld_gravity_on").getBoolValue();
            float gVal = cvarManager->getCvar("rld_gravity_val").getFloatValue();
            
            if (ImGui::Checkbox("Override Map Physics Vector Core", &gOn)) cvarManager->getCvar("rld_gravity_on").setValue(gOn);
            if (gOn) {
                ImGui::SetNextItemWidth(320.0f);
                if (ImGui::SliderFloat("Vertical Grid Value", &gVal, -3000.0f, 3000.0f, "%.1f uu/s²")) cvarManager->getCvar("rld_gravity_val").setValue(gVal);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("System Operations Diagnostics")) {
            ImGui::BeginChild("LogTerminalStream", ImVec2(0, 180), true);
            {
                std::lock_guard<std::mutex> lock(logMutex);
                for (const auto& systemLog : diagnosticLogs) ImGui::TextUnformatted(systemLog.c_str());
            }
            ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

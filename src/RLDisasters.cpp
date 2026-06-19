#include "RLDisasters.h"
#include "bakkesmod/wrappers/includes.h"
#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "bakkesmod/wrappers/GameObject/BallWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "imgui/imgui.h"
#include <sstream>
#include <iomanip>
#include <cmath>

BAKKESMOD_PLUGIN(RLDisasters, "Rocket League Disasters Elite", "2.0", PLUGINTYPE_FREEPLAY)

void RLDisasters::onLoad()
{
    lastTickTime = std::chrono::steady_clock::now();
    AddLog("RLDisasters Engine Initialized successfully.");

    gameWrapper->HookEvent("Function TAGame.Car_TA.SetVehicleInput", [this](std::string e) { 
        OnTick(e); 
    });

    gameWrapper->RegisterDrawable([this](CanvasWrapper canvas) {
        RenderCanvasHUD(canvas);
    });
}

void RLDisasters::onUnload()
{
    cvarManager->executeCommand("sv_soccar_gravity -650");
}

std::string RLDisasters::GetPluginName()
{
    return "RLDisasters";
}

std::string RLDisasters::GetMenuName()
{
    return "rldisasters";
}

std::string RLDisasters::GetMenuTitle()
{
    return "RL Disasters Control Panel";
}

bool RLDisasters::ShouldBlockInput()
{
    return isWindowOpen;
}

bool RLDisasters::IsActiveOverlay()
{
    return isWindowOpen;
}

void RLDisasters::Render()
{
    if (!isWindowOpen) return;
    ImGui::Begin("RL Disasters Engine", &isWindowOpen, ImGuiWindowFlags_None);
    RenderSettings();
    ImGui::End();
}

void RLDisasters::SetImGuiContext(uintptr_t ctx)
{
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

void RLDisasters::AddLog(const std::string& message)
{
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "[%H:%M:%S] ") << message;
    
    diagnosticLogs.push_back(ss.str());
    if (diagnosticLogs.size() > 40) {
        diagnosticLogs.erase(diagnosticLogs.begin());
    }
}

bool RLDisasters::IsActiveWindow() { return isWindowOpen; }
void RLDisasters::EnsureActiveWindow() { isWindowOpen = true; }
void RLDisasters::OnOpen() { isWindowOpen = true; }
void RLDisasters::OnClose() { isWindowOpen = false; }

std::vector<std::string> RLDisasters::BuildActiveItemPool()
{
    std::vector<std::string> pool;
    if (config.allowSpikes) pool.push_back("spikes");
    if (config.allowPlunger) pool.push_back("plunger");
    if (config.allowMagnet) pool.push_back("magnet");
    if (config.allowBoot) pool.push_back("boot");
    if (config.allowGrapple) pool.push_back("grapple");
    if (config.allowFreeze) pool.push_back("freeze");
    if (config.allowTornado) pool.push_back("tornado");
    if (config.allowTeleport) pool.push_back("teleport");
    if (config.allowPowerhit) pool.push_back("powerhit");
    if (config.allowDisruptor) pool.push_back("disruptor");
    return pool;
}

void RLDisasters::OnTick(std::string eventName)
{
    if (!config.masterEnabled || !gameWrapper->IsInGame()) {
        return;
    }

    ServerWrapper server = gameWrapper->GetCurrentGameState();
    if (server.IsNull()) {
        return;
    }

    auto currentTime = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - lastTickTime).count();
    lastTickTime = currentTime;

    if (deltaTime > 0.1f) deltaTime = 0.0166f;

    if (config.biggerFieldMode) {
        ApplyFieldScale(server);
    } else {
        ResetScales(server);
    }

    if (config.customGravity) {
        cvarManager->executeCommand("sv_soccar_gravity " + std::to_string(config.gravityValue));
    }

    auto localPlayer = gameWrapper->GetLocalPrimaryPlayer();
    if (!localPlayer.IsNull()) {
        auto localCar = localPlayer.GetCar();
        if (!localCar.IsNull()) {
            if (config.infiniteBoost) {
                auto boost = localCar.GetBoostComponent();
                if (!boost.IsNull()) {
                    boost.SetBoostAmount(1.0f);
                }
            }

            if (config.quickRumbleEnabled) {
                cumulativeRumbleTimer += deltaTime;
                if (cumulativeRumbleTimer >= config.rumbleInterval) {
                    cumulativeRumbleTimer = 0.0f;
                    
                    std::vector<std::string> pool = BuildActiveItemPool();
                    if (!pool.empty()) {
                        std::string targetItem = pool[rand() % pool.size()];
                        cvarManager->executeCommand("giveitem " + targetItem);
                        AddLog("Rumble loop triggered item: " + targetItem);
                    }
                }
            }
        }
    }
}

void RLDisasters::ApplyFieldScale(ServerWrapper& server)
{
    if (config.fieldScaleMultiplier < 1.0f) config.fieldScaleMultiplier = 1.0f;
    
    float inversionScale = 1.0f / config.fieldScaleMultiplier;
    config.carScaleModifier = inversionScale;
    Vector carScaleVector = Vector{ config.carScaleModifier, config.carScaleModifier, config.carScaleModifier };

    ArrayWrapper<CarWrapper> cars = server.GetCars();
    for (int i = 0; i < cars.Count(); ++i) {
        CarWrapper car = cars.Get(i);
        if (!car.IsNull()) {
            car.SetDrawScale3D(carScaleVector);
        }
    }

    ArrayWrapper<BallWrapper> balls = server.GetGameBalls();
    for (int i = 0; i < balls.Count(); ++i) {
        BallWrapper ball = balls.Get(i);
        if (!ball.IsNull()) {
            if (config.independentBallScale) {
                Vector ballScaleVector = Vector{ config.ballScaleModifier, config.ballScaleModifier, config.ballScaleModifier };
                ball.SetDrawScale3D(ballScaleVector);
            } else {
                ball.SetDrawScale3D(Vector{ 1.0f, 1.0f, 1.0f });
            }
        }
    }
}

void RLDisasters::ResetScales(ServerWrapper& server)
{
    Vector normalVector = Vector{ 1.0f, 1.0f, 1.0f };

    ArrayWrapper<BallWrapper> balls = server.GetGameBalls();
    for (int i = 0; i < balls.Count(); ++i) {
        BallWrapper ball = balls.Get(i);
        if (!ball.IsNull() && ball.GetDrawScale3D().X != 1.0f) {
            ball.SetDrawScale3D(normalVector);
        }
    }

    ArrayWrapper<CarWrapper> cars = server.GetCars();
    for (int i = 0; i < cars.Count(); ++i) {
        CarWrapper car = cars.Get(i);
        if (!car.IsNull() && car.GetDrawScale3D().X != 1.0f) {
            car.SetDrawScale3D(normalVector);
        }
    }
}

void RLDisasters::RenderCanvasHUD(CanvasWrapper canvas)
{
    if (!config.masterEnabled || !config.showHud) return;

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
    if (config.biggerFieldMode) {
        std::stringstream fs;
        fs << "Stadium Context Scale: " << std::fixed << std::setprecision(1) << config.fieldScaleMultiplier << "x Dynamic Matrix";
        canvas.DrawString(fs.str(), 1.0f, 1.0f);
    } else {
        canvas.DrawString("Stadium Context Scale: ENGINE UNMODIFIED STANDARDS", 1.0f, 1.0f);
    }

    canvas.SetPosition(Vector2{ anchorPos.X + 20, anchorPos.Y + 75 });
    if (config.quickRumbleEnabled) {
        std::stringstream rs;
        rs << "Item Pipeline: INJECTING (Next sequence in: " << std::fixed << std::setprecision(2) << (config.rumbleInterval - cumulativeRumbleTimer) << "s)";
        canvas.DrawString(rs.str(), 1.0f, 1.0f);
    } else {
        canvas.DrawString("Item Pipeline: STANDBY OVERWATCH", 1.0f, 1.0f);
    }

    canvas.SetPosition(Vector2{ anchorPos.X + 20, anchorPos.Y + 105 });
    canvas.DrawString(config.infiniteBoost ? "Combustion Multiplier: UNLIMITED FLUID CELL" : "Combustion Multiplier: BASELINE FUEL CAPS", 1.0f, 1.0f);

    canvas.SetPosition(Vector2{ anchorPos.X + 20, anchorPos.Y + 135 });
    std::stringstream gs;
    gs << "World Gravity Grid: " << (config.customGravity ? std::to_string((int)config.gravityValue) : "650 (NOMINAL FORCE)");
    canvas.DrawString(gs.str(), 1.0f, 1.0f);
}

void RLDisasters::RenderSettings()
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
    ImGui::Text("ROCKET LEAGUE DISASTERS CORE ENGINE PLATFORM");
    ImGui::PopStyleColor();
    ImGui::Separator();
    
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    ImGui::Checkbox("Master Automation Engine Switch", &config.masterEnabled);
    ImGui::SameLine();
    ImGui::Checkbox("Display Diagnostic HUD Canvas", &config.showHud);

    if (!config.masterEnabled) {
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Activate the Master Control Core to reveal architectural modifications.");
        return;
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    if (ImGui::BeginTabBar("CatastropheModularTabs")) {
        
        if (ImGui::BeginTabItem("Stadium Scale Modifiers")) {
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            ImGui::Checkbox("Simulate Gigantic Field (Shrink Car Profiles)", &config.biggerFieldMode);
            
            if (config.biggerFieldMode) {
                ImGui::SetNextItemWidth(260.0f);
                ImGui::SliderFloat("Relative Stadium Scale", &config.fieldScaleMultiplier, 1.0f, 6.0f, "%.1fx Arena Size");
                
                ImGui::Separator();
                ImGui::Checkbox("Enable Independent Ball Scaling Override", &config.independentBallScale);
                if (config.independentBallScale) {
                    ImGui::SetNextItemWidth(260.0f);
                    ImGui::SliderFloat("Ball Scale Factor", &config.ballScaleModifier, 0.1f, 5.0f, "%.2fx Ball Size");
                }
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Rumble Loop & Core Items")) {
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            ImGui::Checkbox("Activate 1-Second Precision Injection Loop", &config.quickRumbleEnabled);
            
            if (config.quickRumbleEnabled) {
                ImGui::SetNextItemWidth(260.0f);
                ImGui::SliderFloat("Injection Frequency Step", &config.rumbleInterval, 0.1f, 10.0f, "%.2f Seconds");
                
                ImGui::Dummy(ImVec2(0.0f, 5.0f));
                ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.1f, 1.0f), "Rumble Target Manifest Filter:");
                ImGui::Separator();
                
                ImGui::Columns(2, "ItemMatrixColumns", false);
                ImGui::Checkbox("Spikes", &config.allowSpikes);
                ImGui::Checkbox("Plunger", &config.allowPlunger);
                ImGui::Checkbox("Magnet", &config.allowMagnet);
                ImGui::Checkbox("Boot", &config.allowBoot);
                ImGui::Checkbox("Grapple", &config.allowGrapple);
                
                ImGui::NextColumn();
                ImGui::Checkbox("Freeze", &config.allowFreeze);
                ImGui::Checkbox("Tornado", &config.allowTornado);
                ImGui::Checkbox("Teleport", &config.allowTeleport);
                ImGui::Checkbox("Power Hit", &config.allowPowerhit);
                ImGui::Checkbox("Disruptor", &config.allowDisruptor);
                ImGui::Columns(1);
            }
            
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            ImGui::Separator();
            ImGui::Checkbox("Forced Infinite Boost Refill Override", &config.infiniteBoost);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("World Gravity Grid")) {
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            ImGui::Checkbox("Override Map Physics Vector Core", &config.customGravity);
            
            if (config.customGravity) {
                ImGui::SetNextItemWidth(320.0f);
                ImGui::SliderFloat("Vertical Grid Value", &config.gravityValue, -3000.0f, 3000.0f, "%.1f uu/s²");
                if (ImGui::Button("Recalibrate Normal Gravity Constant")) {
                    config.gravityValue = -650.0f;
                }
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("System Operations Diagnostics")) {
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            ImGui::Text("Core Engine Log Buffer Output:");
            
            ImGui::BeginChild("LogTerminalStream", ImVec2(0, 200), true, ImGuiWindowFlags_HorizontalScrollbar);
            for (const auto& systemLog : diagnosticLogs) {
                ImGui::TextUnformatted(systemLog.c_str());
            }
            ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
            
            if (ImGui::Button("Flush System Buffer Logs")) {
                diagnosticLogs.clear();
                AddLog("Diagnostic matrix metrics cleaned from cache.");
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

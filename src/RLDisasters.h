#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include <string>
#include <vector>
#include <chrono>

struct PluginConfig {
    bool masterEnabled = false;
    bool biggerFieldMode = false;
    float fieldScaleMultiplier = 2.0f;
    bool quickRumbleEnabled = false;
    float rumbleInterval = 1.0f;
    bool infiniteBoost = false;
    bool customGravity = false;
    float gravityValue = -650.0f;
    bool showHud = true;
    bool allowSpikes = true;
    bool allowPlunger = true;
    bool allowMagnet = true;
    bool allowBoot = true;
    bool allowGrapple = true;
    bool allowFreeze = true;
    bool allowTornado = true;
    bool allowTeleport = true;
    bool allowPowerhit = true;
    bool allowDisruptor = true;
    float carScaleModifier = 0.5f;
    bool independentBallScale = false;
    float ballScaleModifier = 1.0f;
};

class RLDisasters : public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginWindow
{
public:
    void onLoad() override;
    void onUnload() override;
    void OnTick(std::string eventName);
    void SetImGuiContext(uintptr_t ctx) override;
    void RenderCanvasHUD(CanvasWrapper canvas);
    
    void Render() override;
    std::string GetMenuName() override;
    std::string GetMenuTitle() override;
    bool ShouldBlockInput() override;
    bool IsActiveOverlay() override;
    bool IsActiveWindow();
    void EnsureActiveWindow();
    void OnOpen() override;
    void OnClose() override;

private:
    PluginConfig config;
    std::chrono::steady_clock::time_point lastTickTime;
    float cumulativeRumbleTimer = 0.0f;
    bool isWindowOpen = false;
    std::vector<std::string> diagnosticLogs;
    
    void AddLog(const std::string& message);
    void ApplyFieldScale(class ServerWrapper& server);
    void ResetScales(class ServerWrapper& server);
    std::vector<std::string> BuildActiveItemPool();
    void DrawInterfaceLayout();
};

#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include <string>
#include <vector>
#include <chrono>
#include <mutex>

class RLDisasters : public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginWindow
{
public:
    void onLoad() override;
    void onUnload() override;
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
    bool isWindowOpen = false;
    float cumulativeRumbleTimer = 0.0f;
    std::chrono::steady_clock::time_point lastTickTime;
    
    std::vector<std::string> diagnosticLogs;
    std::mutex logMutex;
    
    void AddLog(const std::string& message);
    void UpdateScaleTransformations();
    void HandleRumbleInjection(float deltaTime);
    void DrawInterfaceLayout();
};

#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include <vector>
#include <string>
#include <chrono>

struct DisasterState {
    bool enabled = false;
    bool biggerField = false;
    bool quickRumble = false;
    bool persistentRumble = false;
    bool extremeGravity = false;
    float gravityMultiplier = 1.0f;
    float rumbleTimer = 0.0f;
};

class RLDisasters : public BakkesModPlugin, public PluginWindow
{
private:
    DisasterState disasters;
    std::chrono::steady_clock::time_point lastTime;

public:
    void onLoad() override;
    void onUnload() override;
    void OnTick(std::string eventName);
    void RenderSettings() override;
    std::string GetPluginName() override;
    void SetImGuiContext(uintptr_t ctx) override;
};

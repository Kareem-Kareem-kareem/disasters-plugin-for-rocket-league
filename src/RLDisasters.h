#pragma once
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include <string>

struct DisasterStates {
    bool closestSpawn;
    bool biggerGoals;
    bool biggerField;
    bool quickRumble;
    bool persistentRumble;
};

class RLDisasters : public BakkesModPlugin, public PluginSettingsWindow
{
public:
    virtual void onLoad();
    virtual void onUnload();
    
    void HookEvents();
    void UnhookEvents();
    void OnTick(std::string eventName);
    void OnMatchStarted(std::string eventName);
    void OnGoalScored(std::string eventName);
    void OnPlayersReset(std::string eventName);
    void ResetAll();

    virtual void RenderSettings();
    virtual std::string GetPluginName() { return "RLDisasters"; }
    virtual void SetImGuiContext(uintptr_t ctx);

private:
    DisasterStates disasters;
};

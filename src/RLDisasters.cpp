#include "RLDisasters.h"
#include "bakkesmod/wrappers/cvarwrapper.h"
#include "bakkesmod/wrappers/gamewrapper.h"
#include <algorithm>
#include <cstdlib>
#include <ctime>

BAKKESMOD_PLUGIN(RLDisasters, "Rocket League Disasters", "1.0", 0)

void RLDisasters::onLoad()
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    cvarManager->registerCvar("rl_disasters_enabled", "0", "Enable the Disaster Chaos Mod plugin", true, true, 0, true, 1)
        .addOnValueChanged([this](std::string, CVarWrapper cvar) {
            pluginEnabled = cvar.getBoolValue();
            if (!pluginEnabled) {
                ResetMatchState();
            } else {
                ChooseRandomRumbleItem();
            }
        });

    HookEvents();
}

void RLDisasters::onUnload()
{
    UnhookEvents();
    ResetMatchState();
}

void RLDisasters::HookEvents()
{
    gameWrapper->HookEvent("Function GameEvent_Soccar_TA.Active.StartRound",
        [this](std::string e) { OnRoundStart(e); });

    gameWrapper->HookEvent("Function TAGame.Ball_TA.Explode",
        [this](std::string e) { OnGoalScored(e); });
}

void RLDisasters::UnhookEvents()
{
    gameWrapper->UnhookEvent("Function GameEvent_Soccar_TA.Active.StartRound");
    gameWrapper->UnhookEvent("Function TAGame.Ball_TA.Explode");
}

void RLDisasters::OnRoundStart(std::string eventName)
{
    if (!pluginEnabled) return;

    ApplyGravitySettings();
    cvarManager->log("RLDisasters: Round started. Active rolled item index for this round slot: " + std::to_string(currentRandomItemIndex));
}

void RLDisasters::OnGoalScored(std::string eventName)
{
    if (!gameWrapper->IsInGame() || !pluginEnabled) return;

    currentGravity += 100.0f;
    if (currentGravity > -150.0f) {
        currentGravity = -150.0f; 
    }

    ChooseRandomRumbleItem();
}

void RLDisasters::ChooseRandomRumbleItem()
{
    currentRandomItemIndex = 1 + (std::rand() % 11);
    cvarManager->log("RLDisasters: A goal was scored! Next round item slot set to: " + std::to_string(currentRandomItemIndex));
}

void RLDisasters::ApplyGravitySettings()
{
    cvarManager->executeCommand("sv_soccar_gravity " + std::to_string(currentGravity), false);
}

void RLDisasters::RenderSettings()
{
    ImGui::Text("Rocket League Disasters Plugin");
    ImGui::Separator();

    // The main toggle for the plugin
    bool enabled = cvarManager->getCvar("disasters_enabled").getBoolValue();
    if (ImGui::Checkbox("Enable Disasters Mod", &enabled)) {
        cvarManager->executeCommand("disasters_enabled " + std::to_string(enabled));
    }

    ImGui::Spacing();

    // Check if a local server instance exists (works in LAN, Custom Maps, and Freeplay)
    ServerWrapper server = gameWrapper->GetGameEventAsServer();
    if (!server) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Status: Must be in a Match or Freeplay to synchronize physics.");
        return;
    }

    // You have server authority locally
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Status: Host / Server Authority Active");
    ImGui::Separator();

    // Active Status Display
    ImGui::Text("Current Selected Item ID: %d", currentRandomItemIndex);
    ImGui::Text("Active Gravity: %.1f", currentGravity);
}

std::string RLDisasters::GetPluginName()
{
    return "Rocket League Disasters";
}

void RLDisasters::RenderSettings()
{
    ImGui::Text("Rocket League Disasters Plugin");
    ImGui::Separator();

    // Toggle plugin activation state
    if (ImGui::Checkbox("Enable Disasters Mod", &pluginEnabled)) {
        if (pluginEnabled) {
            HookEvents();
        } else {
            UnhookEvents();
        }
    }

    ImGui::Spacing();

    // Safe Server validation checks
    if (!gameWrapper) return;
    
    ServerWrapper server = gameWrapper->GetGameEventAsServer();
    if (!server.IsNull()) {
        // You are running a local match instance or hosting a LAN game
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Status: Host / Server Authority Active");
        ImGui::Separator();

        // Active Status Display
        ImGui::Text("Current Selected Item ID: %d", currentRandomItemIndex);
        ImGui::Text("Active Gravity: %.1f", currentGravity);
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Status: Must be in a Match or Freeplay to synchronize physics.");
    }
}

void RLDisasters::ResetMatchState()
{
    currentGravity = -650.0f;
    currentRandomItemIndex = 1;
    cvarManager->executeCommand("sv_soccar_gravity -650", false);
}

#pragma once
#include <string>
#include <memory>

// Forward declarations to avoid including heavy headers here
class CVarManagerWrapper;

// Function to register the "give_rumble" command
// Call this from your plugin's onLoad()
void RegisterRumbleCommands(std::shared_ptr<CVarManagerWrapper> cvarManager);

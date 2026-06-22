#include "RumbleCommands.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "bakkesmod/wrappers/GameObject/RumbleComponent/RumblePickup.h"
#include "bakkesmod/wrappers/GameObject/RumbleComponent/AttachmentPickup.h"
#include "bakkesmod/wrappers/GameObject/RumbleComponent/BallFreezePickup.h"
// Include others if you want more powerups (Lasso, Spikes, etc.)

#include "bakkesmod/wrappers/GameObject/RumbleComponent/PickupType.h" // Check if this exists; otherwise use the ones below
#include "bakkesmod/core/CVarManagerWrapper.h"
#include "bakkesmod/core/GameWrapper.h"
#include <unordered_map>
#include <algorithm>

// Helper: map string to PickupType
static std::unordered_map<std::string, PickupType> nameToPickupType = {
    {"boot", PickupType::Boot},
    {"freeze", PickupType::Freeze},
    {"spikes", PickupType::Spikes},
    {"plunger", PickupType::Plunger},
    {"grapple", PickupType::Grapple},
    {"lasso", PickupType::Lasso},
    {"spring", PickupType::Spring},    // maybe "BallCarSpring"
    {"boxingglove", PickupType::BoxingGlove},
    {"battarang", PickupType::Battarang},
    {"disruptor", PickupType::Disruptor},
    {"magnet", PickupType::Magnet},
    {"powerhitter", PickupType::PowerHitter},
    {"haymaker", PickupType::Haymaker},
    {"ballcarrier", PickupType::BallCarrier}, // might be "Basketball"?
    // Add all that exist. Check your SDK's PickupType.h for the exact list.
};

static std::shared_ptr<CVarManagerWrapper> _cvarManager;
static std::shared_ptr<GameWrapper> _gameWrapper; // You'll need to pass this too, or get it from global

// The actual function to give the powerup
static void GiveRumblePowerup(const std::string& powerupName) {
    auto car = _gameWrapper->GetLocalCar();
    if (!car) {
        _cvarManager->log("No local car found.");
        return;
    }

    auto rumble = car.GetRumblePickup();
    if (!rumble) {
        _cvarManager->log("Car does not have a Rumble component. Are you in a Rumble match?");
        return;
    }

    // Convert to lowercase for case-insensitive matching
    std::string lowerName = powerupName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    auto it = nameToPickupType.find(lowerName);
    if (it == nameToPickupType.end()) {
        _cvarManager->log("Unknown rumble powerup: " + powerupName + ". Available: boot, freeze, spikes, etc.");
        return;
    }

    rumble.SetPickupType(it->second);
    rumble.PickupStart();
    _cvarManager->log("Gave " + powerupName + " powerup!");
}

void RegisterRumbleCommands(std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gameWrapper) {
    _cvarManager = cvarManager;
    _gameWrapper = gameWrapper;

    cvarManager->registerCommand(
        "give_rumble",
        "Gives the local player a specific rumble powerup. Usage: give_rumble <freeze|boot|spikes|...>",
        [](const std::vector<std::string>& args) {
            if (args.size() < 2) {
                _cvarManager->log("Usage: give_rumble <powerup>");
                return;
            }
            GiveRumblePowerup(args[1]);
        },
        CVarFlags::None
    );
}

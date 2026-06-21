#include "YourPluginName.h"
#include <random>
#include <vector>

BAKKESMOD_PLUGIN(YourPluginName, "Random Rumble Round Plugin", "1.0", PLUGINTYPE_SERVER)

void YourPluginName::onLoad()
{
    gameWrapper->HookEvent("TAGame.GameEvent_Soccar_TA.EventPreCountdown", 
        std::bind(&YourPluginName::OnRoundStart, this, std::placeholders::_1));
}

void YourPluginName::onUnload()
{
}

void YourPluginName::OnRoundStart(std::string eventName)
{
    std::vector<std::string> rumbleItems = {
        "SpecialPickup_BallFreeze_TA",
        "SpecialPickup_BallGrapple_TA",
        "SpecialPickup_BallLasso_TA",
        "SpecialPickup_BallSpring_TA",
        "SpecialPickup_Swapper_TA",
        "SpecialPickup_Tornado_TA",
        "SpecialPickup_Bumper_TA",
        "SpecialPickup_BoostModifier_TA",
        "SpecialPickup_BallVelcro_TA",
        "SpecialPickup_StrongHit_TA"
    };

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, rumbleItems.size() - 1);
    std::string chosenItem = rumbleItems[dis(gen)];

    gameWrapper->ExecuteUnrealCommand("ta giveitem " + chosenItem);
    
    cvarManager->log("Selected random Rumble item for this round: " + chosenItem);
}

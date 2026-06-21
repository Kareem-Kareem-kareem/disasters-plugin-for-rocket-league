#include "RLDisasters.h"
#include "bakkesmod/wrappers/gamewrapper.h"
#include "bakkesmod/wrappers/gameobject/carwrapper.h"
#include "bakkesmod/wrappers/gameevent/serverwrapper.h"
#include "bakkesmod/wrappers/GameObject/RumbleComponent/BallFreezePickup.h"

void RLDisasters::TickRumbleTracking()
{
    CarWrapper car = gameWrapper->GetLocalCar();
    if (car.IsNull()) return;

    // Retrieve the base rumble pickup component from the car
    RumblePickupComponentWrapper pickup = car.GetAttachedPickup();
    if (pickup.IsNull()) return;

    // Check if the current item is exactly a BallFreezePickup type
    if (pickup.IsA(BallFreezePickup::StaticClass())) 
    {
        // Safe downcast to access specific Freeze properties if needed
        BallFreezePickup freezeItem = BallFreezePickup(pickup.memory_address);
        if (freezeItem.IsNull()) return;

        // If the item is ready and not currently active, force execution
        if (!freezeItem.GetbIsActive()) 
        {
            freezeItem.Activate();
            cvarManager->log("RLDisasters: Forcefully activated Ball Freeze component.");
        }
    }
}

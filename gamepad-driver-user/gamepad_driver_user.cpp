//
//  gamepad_driver_user.cpp
//  gamepad-driver-user
//
//  Created by Noah Nübling on 30.08.23.
//

#include <os/log.h>

#include <DriverKit/IOUserServer.h>
#include <DriverKit/IOLib.h>

#include "gamepad_driver_user.h"

kern_return_t
IMPL(gamepad_driver_user, Start)
{
    kern_return_t ret;
    ret = Start(provider, SUPERDISPATCH);
    os_log(OS_LOG_DEFAULT, "Hello World");
    return ret;
}

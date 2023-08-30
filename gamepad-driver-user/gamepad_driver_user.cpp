//
//  gamepad_driver_user.cpp
//  gamepad-driver-user
//
//  Created by Noah Nübling on 30.08.23.
//



/// Imports from HIDKeyboardDriver sample app
#include <os/log.h>
#include <DriverKit/IOUserServer.h>
#include <DriverKit/IOLib.h>

#include <DriverKit/OSCollections.h>
#include <HIDDriverKit/HIDDriverKit.h>

/// Import Header
#include "gamepad_driver_user.h"


/// --- Lifecycle and iVars ---
/// This is mostly copied from the HIDKeyboardDriver sample project

struct gamepad_driver_user_IVars {
    
    OSArray *elements;
    struct {
        OSArray *elements;
    } keyboard;
};

#define _elements   ivars->elements
#define _keyboard   ivars->keyboard


bool gamepad_driver_user::init() {
    
    /// init
    /// Allocates memory for ivars struct
    /// And assigns allocated memory to inherited `ivars` variable. I think all drivers need to do it like that.
    
    if (!super::init()) {
        return false;
    }
    
    ivars = IONewZero(gamepad_driver_user_IVars, 1);
    if (!ivars) {
        return false;
    }
    
exit:
    return true;
}

void gamepad_driver_user::free() {
    
    /// free
    /// Frees memory of ivars struct
    
    if (ivars) {
        OSSafeReleaseNULL(_elements);
        OSSafeReleaseNULL(_keyboard.elements);
    }
    
    IOSafeDeleteNULL(ivars, gamepad_driver_user_IVars, 1);
    super::free();
}

kern_return_t
IMPL(gamepad_driver_user, Start)
{
    kern_return_t ret;
    ret = Start(provider, SUPERDISPATCH);
    os_log(OS_LOG_DEFAULT, "Hello World");
    return ret;
}

/// --- Report handling ---

kern_return_t gamepad_driver_user::handleReport(uint64_t timestamp, IOMemoryDescriptor* report, uint32_t reportLength, IOHIDReportType reportType, IOOptionBits options) {
    
    
    IOHIDDevice::handleReport(timestamp, report, reportLength, reportType, options);
    
    return KERN_SUCCESS;
}

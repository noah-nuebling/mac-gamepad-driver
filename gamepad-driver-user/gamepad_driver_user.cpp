//
//  gamepad_driver_user.cpp
//  gamepad-driver-user
//
//  Created by Noah Nübling on 30.08.23.
//


/// Imports from Karabiner VirtualHIDPointing
//#include "version.hpp"
#include <HIDDriverKit/IOHIDDeviceKeys.h>
#include <HIDDriverKit/IOHIDUsageTables.h>

/// Imports from HIDKeyboardDriver sample app
#include <os/log.h>
#include <DriverKit/IOUserServer.h>
#include <DriverKit/IOLib.h>

#include <DriverKit/OSCollections.h>
#include <HIDDriverKit/HIDDriverKit.h>
#include <USBDriverKit/IOUSBHostDevice.h>
#include <USBDriverKit/AppleUSBDescriptorParsing.h>

/// Other Imports
#include <cassert>

/// Import Header
#include "gamepad_driver_user.h"

/// MARK: --- Declarations ---

namespace Controller {
    enum Type {
        /// This is copied from 360Controller
        Xbox360 = 0,
        XboxOriginal = 1,
        XboxOne = 2,
        XboxOnePretend360 = 3,
        Xbox360Pretend360 = 4,
        unknown = 5,
    };
}

/// MARK: --- Lifecycle and iVars ---
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
    
    os_log(OS_LOG_DEFAULT, "Groot: Hoot Hoot");
    
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
    
    os_log(OS_LOG_DEFAULT, "Groot: Free");
    
    if (ivars) {
        OSSafeReleaseNULL(_elements);
        OSSafeReleaseNULL(_keyboard.elements);
    }
    
    IOSafeDeleteNULL(ivars, gamepad_driver_user_IVars, 1);
    super::free();
}

kern_return_t IMPL(gamepad_driver_user, Start) {
    
    /// Notes:
    /// - When something failed here, the 360Controller implementation used to set the `device` variable to NULL and then call ReleaseAll(). I don't know why. I don't think this is necessary here, since we're freeing (that's the same as releasing in this case right?) everything in free(). If we do need to release stuff when Start fails, we should probably do it in Stop().
    /// - Wrapping everything in { } to prevent weird cpp compiler errors with the goto fail statements.
    
    {
        /// Declare return
        IOReturn ret = kIOReturnSuccess;
        
        /// Get device
        ///   Note: Don't know what the 'Host' prefix stands for. Older code I saw uses IOUSBDevice instead of IOUSBHostDevice
        IOUSBHostDevice *device = OSDynamicCast(IOUSBHostDevice, provider);
        if (device == NULL) {
            os_log(OS_LOG_DEFAULT, "Start - Invalid provider");
            goto fail;
        }
        
        /// Get configuration descriptor
        uint8_t index = 0;
        const IOUSBConfigurationDescriptor *configurationDescriptor = device->CopyConfigurationDescriptor(index);
        if (configurationDescriptor == NULL) {
            os_log(OS_LOG_DEFAULT, "Start - No configuration descriptor available");
            goto fail;
        }
        
        /// Open device
        ret = device->Open(this, 0, NULL);
        if (ret != kIOReturnSuccess) {
            os_log(OS_LOG_DEFAULT, "Start - Unable to open device");
            goto fail;
        }
        
        /// Set Configuration
        /// Note: No idea what we're doing here. Copied this from 360Controller
        ret = device->SetConfiguration(configurationDescriptor->bConfigurationValue, true);
        if (ret != kIOReturnSuccess) {
            os_log(OS_LOG_DEFAULT, "Start - Unable to set configuration");
            goto fail;
        }
        
        /// Find interface
        ///     And determine controller type
        ///     360Controller uses old IOUSBDevice->FindNextInterface API. There the desired interface class and subclass are specified as integers. I don't know how to translate this to the modern implementation.
        
        Controller::Type controller = Controller::unknown;
        IOUSBHostInterface *controllerInterface = NULL;
        const IOUSBInterfaceDescriptor *controllerInterfaceDescriptor = NULL;
        
        /// Create iterator
        uintptr_t iterator;
        ret = device->CreateInterfaceIterator(&iterator);
        if (ret != kIOReturnSuccess) {
            os_log(OS_LOG_DEFAULT, "Start - Failed to create interface iterator");
        }
        
        /// Iterate
        /// Notes:
        /// - Why do we need to pass in the configurationDesc for GetInterfaceDescriptor()? I though the interfaces belong to a configuration anyways so why specify it?
        while (true) {
            
            /// Get next interface from iterator
            IOUSBHostInterface *interface;
            ret = device->CopyInterface(iterator, &interface);
            if (ret != kIOReturnSuccess) {
                os_log(OS_LOG_DEFAULT, "Start - Failed to copy an interface. Carrying on.");
            }
            
            /// Break if iterator finished
            if (interface == NULL) {
                break;
            }
            
            /// Get interface descriptor
            const IOUSBInterfaceDescriptor *interfaceDescriptor = interface->GetInterfaceDescriptor(configurationDescriptor);
            
            /// Check if interface is for known controller
            
            controller = Controller::unknown;
            
            if (interfaceDescriptor->bInterfaceSubClass == 93
                && interfaceDescriptor->bInterfaceProtocol == 1) {
                
                controller = Controller::Xbox360;
                
            } else if (interfaceDescriptor->bInterfaceSubClass == 66
                       && interfaceDescriptor->bInterfaceProtocol == 0) {
                
                controller = Controller::XboxOriginal;
                
            } else if (interfaceDescriptor->bInterfaceClass == 255
                       && interfaceDescriptor->bInterfaceSubClass == 71
                       && interfaceDescriptor->bInterfaceProtocol == 208) {
                
                controller = Controller::XboxOne;
                
            }
            
            /// Assign result and break if known controller interface was found
            if (controller != Controller::unknown) {
                controllerInterface = interface;
                controllerInterfaceDescriptor = interfaceDescriptor;
                break;
            }
        }
        
        /// Guard no interface found
        if (controllerInterface == NULL) {
            os_log(OS_LOG_DEFAULT, "Start - Unable to find controller interface");
            goto fail;
        }
        
        /// Log interface found
        os_log(OS_LOG_DEFAULT, "Start - Interface found! For controller of type %d", controller);
        
        /// Open interface
        /// NOTE: Not totally sure about the params here
        controllerInterface->Open(this, 0, NULL);
        
        /// Iterate over endpoints and create inPipe and outPipe
        
        IOUSBHostPipe *inPipe = NULL;
        IOUSBHostPipe *outPipe = NULL;
        const IOUSBEndpointDescriptor *inDescriptor = NULL;
        const IOUSBEndpointDescriptor *outDescriptor = NULL;
        
        const IOUSBEndpointDescriptor *endpointDescriptor = NULL;
        
        while (true) {
            
            /// Get next endpoint descriptor from iterator
            endpointDescriptor = IOUSBGetNextEndpointDescriptor(configurationDescriptor, controllerInterfaceDescriptor, (IOUSBDescriptorHeader *)endpointDescriptor);
            
            /// Break if iterator empty
            if (endpointDescriptor == NULL) {
                break;
            }
            
            /// Try to create input pipe from endpoint
            /// We're emulatirng the 360Controller logic here. Since the APIs are different now we have to do wacky stuff like `bEndpointAddress >> 7` Maybe we should use different APIs or different logic instead of this bitshifting stuff.
            /// See USB 2.0 spec section 9.6.6. for context on the bitshifting stuff
            
            if (inPipe == NULL) {
                
                bool isIn = (endpointDescriptor->bEndpointAddress & (1 << 7)) != 0;
                bool isInterrupt = (endpointDescriptor->bmAttributes & (1 << 0)) && (endpointDescriptor->bmAttributes & (1 << 1));
                uint8_t interval = endpointDescriptor->bInterval;
                uint16_t maxPacketSize = endpointDescriptor->wMaxPacketSize;
                
                os_log(OS_LOG_DEFAULT, "Start - Considering endpoint with attributes --- %d --- isIn: %d isInterrupt: %d interval: %d maxPacketSize: %d", endpointDescriptor->bmAttributes, isIn, isInterrupt, interval, maxPacketSize);
                
                if (isIn && isInterrupt) {
                    
                    /// Create inPipe
                    /// TODO: "Caller MUST release pipe"
                    inDescriptor = endpointDescriptor;
                    ret = controllerInterface->CopyPipe(endpointDescriptor->bEndpointAddress, &inPipe);
                    if (ret != kIOReturnSuccess) {
                        os_log(OS_LOG_DEFAULT, "Start - Failed to copy potential input pipe. Carrying on.");
                    }
                }
            }
            
            /// Try to create output pipe from endpoint
            
            if (outPipe == NULL) {
                
                bool isOut = (endpointDescriptor->bEndpointAddress & (1 << 7)) == 0;
                bool isInterrupt = (endpointDescriptor->bmAttributes & (1 << 0)) && (endpointDescriptor->bmAttributes & (1 << 1));
                uint8_t interval = endpointDescriptor->bInterval;
                uint16_t maxPacketSize = endpointDescriptor->wMaxPacketSize;
                
                os_log(OS_LOG_DEFAULT, "Start - Considering endpoint with attributes --- %d --- isOut: %d isInterrupt: %d interval: %d maxPacketSize: %d", endpointDescriptor->bmAttributes, isOut, isInterrupt, interval, maxPacketSize);
                
                if (isOut && isInterrupt) {
                    
                    /// Create outPipe
                    /// TODO: "Caller MUST release pipe"
                    outDescriptor = endpointDescriptor;
                    ret = controllerInterface->CopyPipe(endpointDescriptor->bEndpointAddress, &outPipe);
                    if (ret != kIOReturnSuccess) {
                        os_log(OS_LOG_DEFAULT, "Start - Failed to copy potential output pipe. Carrying on.");
                    }
                }
            }
            
            /// Break if both pipes are found
            if (inPipe != NULL && outPipe != NULL) {
                break;
            }
        }
        
        /// Destroy iterator
        ret = device->DestroyInterfaceIterator(iterator);
        if (ret != kIOReturnSuccess) {
            os_log(OS_LOG_DEFAULT, "Start - Failed to destroy interface iterator. Carrying on.");
        }
        
        /// Guard pipe creation success
        if (inPipe == NULL) {
            os_log(OS_LOG_DEFAULT, "Start - Input pipe couldn't be created.");
            goto fail;
        }
        if (outPipe == NULL) {
            os_log(OS_LOG_DEFAULT, "Start - Output pipe couldn't be created.");
            goto fail;
        }
        
        /// Get Device speed
        uint8_t deviceSpeed;
        ret = device->GetSpeed(&deviceSpeed);
        if (ret != kIOReturnSuccess) {
            os_log(OS_LOG_DEFAULT, "Start - Failed to get device speed");
            goto fail;
        }
        
        /// Get maxPacketSize for input endpoint
        uint16_t inMaxPacketSize = IOUSBGetEndpointMaxPacketSize(deviceSpeed, inDescriptor);
        
        /// Create buffer for input endpoint
        /// Notes:
        /// - We're passing 1 for the alignment param. The docs say 0 is the default, but the corresponding method which 360Controller uses (IOBufferMemoryDescriptor::inTaskWithOptions) has 1 as the default value. This is confusing. Not sure whether to pass 0 or 1.
        IOBufferMemoryDescriptor *inBuffer = NULL;
        ret = IOBufferMemoryDescriptor::Create(kIOMemoryDirectionIn, inMaxPacketSize, 1, &inBuffer);
        if (ret != kIOReturnSuccess) {
            os_log(OS_LOG_DEFAULT, "Start - Failed to create buffer for input endpoint");
        }
        
        /// Skipping Chatpad stuff from 360Controller
        /// ....
        
        /// Begin polling input
        bool success = true; //QueueRead();
        if (!success) {
            os_log(OS_LOG_DEFAULT, "Start - Failed to Start reading from device");
            goto fail;
        }
        
        /// Send initialization messages to Xbox One Controller
        /// Note: No idea what this does. Copied from 360Controller
        if (controller == Controller::XboxOne || controller == Controller::XboxOnePretend360) {
            uint8_t xoneInit0[] = { 0x01, 0x20, 0x00, 0x09, 0x00, 0x04, 0x20, 0x3a, 0x00, 0x00, 0x00, 0x80, 0x00 };
            uint8_t xoneInit1[] = { 0x05, 0x20, 0x00, 0x01, 0x00 };
            uint8_t xoneInit2[] = { 0x09, 0x00, 0x00, 0x09, 0x00, 0x0F, 0x00, 0x00,
                0x1D, 0x1D, 0xFF, 0x00, 0x00 };
            uint8_t xoneInit3[] = { 0x09, 0x00, 0x00, 0x09, 0x00, 0x0F, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00 };
//            QueueWrite(&xoneInit0, sizeof(xoneInit0));
//            QueueWrite(&xoneInit1, sizeof(xoneInit1));
//            QueueWrite(&xoneInit2, sizeof(xoneInit2));
//            QueueWrite(&xoneInit3, sizeof(xoneInit3));
        } else if (controller == Controller::Xbox360 || controller == Controller::Xbox360Pretend360 || controller == Controller::XboxOriginal) {
            
            /// Disable LED
            /// Notes: Copied from 360Controller. Do we need to do this on XboxOriginal controller?
//            Xbox360_Prepare(led,outLed);
//            led.pattern=ledOff;
//            QueueWrite(&led,sizeof(led));
            
        } else {
            os_log(OS_LOG_DEFAULT, "Start - Fail due to unhandled Controller Type");
            goto fail;
        }
        
        /// Create HIDDevice
        
        
        
        /// Call super
        ret = Start(provider, SUPERDISPATCH);
        if (ret != kIOReturnSuccess) goto fail;
        
        /// Register self with system
        ret = RegisterService();
        if (ret != kIOReturnSuccess) goto fail;
        
        /// Validate
        assert(ret == kIOReturnSuccess);
        
        /// Return & log success
        os_log(OS_LOG_DEFAULT, "Groot: Start");
        return ret;
        
    }
    
    /// Handle failed start
fail:
    os_log(OS_LOG_DEFAULT, "Groot: Failed to start");
    Stop(provider, SUPERDISPATCH);
    return kIOReturnError;
}

kern_return_t IMPL(gamepad_driver_user, Stop) {
    
    /// Initial implementation copied from Karabiner https://github.com/pqrs-org/Karabiner-DriverKit-VirtualHIDDevice/blob/151fefd3f5cdbe00874bd4c25cde0ded9665878f/src/DriverKit/Karabiner-DriverKit-VirtualHIDDevice/org_pqrs_Karabiner_DriverKit_VirtualHIDPointing.cpp#L130C1-L137C1
    
    os_log(OS_LOG_DEFAULT, "Groot: Stop");
    return Stop(provider, SUPERDISPATCH);
}

/// MARK: --- Read / Write ---

bool gamepad_driver_user::QueueRead(void) {
    
//    IOUSBCompletion complete;
//    IOReturn err;
//
//    if ((inPipe == NULL) || (inBuffer == NULL))
//        return false;
//    complete.target=this;
//    complete.action=ReadCompleteInternal;
//    complete.parameter=inBuffer;
//    err=inPipe->Read(inBuffer,0,0,inBuffer->getLength(),&complete);
//    if(err==kIOReturnSuccess) return true;
//    else {
//        IOLog("read - failed to start (0x%.8x)\n",err);
//        return false;
//    }
    return false;
}

bool gamepad_driver_user::QueueWrite(const void *bytes, uint32_t length) {
    
//    IOBufferMemoryDescriptor *outBuffer;
//    IOUSBCompletion complete;
//    IOReturn err;
//
//    outBuffer = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,kIODirectionOut,length);
//    if (outBuffer == NULL) {
//        IOLog("send - unable to allocate buffer\n");
//        return false;
//    }
//    outBuffer->writeBytes(0,bytes,length);
//    complete.target=this;
//    complete.action=WriteCompleteInternal;
//    complete.parameter=outBuffer;
//    err=outPipe->Write(outBuffer,0,0,length,&complete);
//    if(err==kIOReturnSuccess) return true;
//    else {
//        IOLog("send - failed to start (0x%.8x)\n",err);
//        return false;
//    }
    return false;
}

/// --- IOHIDDevice ---

/*
kern_return_t gamepad_driver_user::handleReport(uint64_t timestamp, IOMemoryDescriptor* report, uint32_t reportLength, IOHIDReportType reportType, IOOptionBits options) {
    
    
//    IOHIDDevice::handleReport(timestamp, report, reportLength, reportType, options);
    
    return KERN_SUCCESS;
}


kern_return_t gamepad_driver_user::getReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options, uint32_t completionTimeout, OSAction *action) {

    return KERN_SUCCESS;
}
kern_return_t gamepad_driver_user::setReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options, uint32_t completionTimeout, OSAction *action) {
    
    return KERN_SUCCESS;
}
//void gamepad_driver_user::CompleteReport(OSAction *action, IOReturn status, uint32_t actualByteCount) {
//    
//}
void gamepad_driver_user::setProperty(OSObject *key, OSObject *value) {
        
}
 */

/// --- IOUserHIDDevice ---

//bool gamepad_driver_user::handleStart(IOService *provider) {
//
//    /// vvv Code is from keyboard sample project Start() method. Probably doesn't make sense here.
//    kern_return_t ret = KERN_SUCCESS;
////    ret = Start(provider, SUPERDISPATCH);
//    os_log(OS_LOG_DEFAULT, "Hello World");
//
//    return ret;
//}
//
//OSDictionary *gamepad_driver_user::newDeviceDescription(void) {
//    return OSDictionaryCreate();
//}
//OSData *gamepad_driver_user::newReportDescriptor(void) {
//    uint8_t *bytes = NULL;
//    return OSDataCreate(bytes, 0);
//}


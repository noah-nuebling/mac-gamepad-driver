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
#include "ControlStruct.h"
#include "Xbox360ControllerClass.h"
#include "XboxOneControllerClass.h"

/// Import Header
#include "gamepad_driver_user.h"

/// MARK: --- Declarations ---

#define kIOSerialDeviceType   "Serial360Device"

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
    
    IOUSBHostPipe *inPipe;
    IOUSBHostPipe *outPipe;
    IOBufferMemoryDescriptor *inBuffer;
    IOBufferMemoryDescriptor *outBuffer;
    IODispatchQueue *queue;
    Controller::Type controller;
    Xbox360ControllerClass *padHandler;
    IOUSBHostDevice *provider;
};

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
    
    /// Release ivars
    if (ivars) {
        OSSafeReleaseNULL(ivars->inPipe);
        OSSafeReleaseNULL(ivars->outPipe);
        OSSafeReleaseNULL(ivars->inBuffer);
        OSSafeReleaseNULL(ivars->queue);
    }
    
    /// Free ivars struct
    IOSafeDeleteNULL(ivars, gamepad_driver_user_IVars, 1);
    
    /// Call super
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
        
        /// Store provider in ivar
        /// Note: This is weird hack to make CoolGetProvider work
        ivars->provider = device;
        
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
                ivars->controller = controller;
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
        
        ivars->inPipe = NULL;
        ivars->outPipe = NULL;
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
            
            if (ivars->inPipe == NULL) {
                
                bool isIn = (endpointDescriptor->bEndpointAddress & (1 << 7)) != 0;
                bool isInterrupt = (endpointDescriptor->bmAttributes & (1 << 0)) && (endpointDescriptor->bmAttributes & (1 << 1));
                uint8_t interval = endpointDescriptor->bInterval;
                uint16_t maxPacketSize = endpointDescriptor->wMaxPacketSize;
                
                os_log(OS_LOG_DEFAULT, "Start - Considering endpoint with attributes --- %d --- isIn: %d isInterrupt: %d interval: %d maxPacketSize: %d", endpointDescriptor->bmAttributes, isIn, isInterrupt, interval, maxPacketSize);
                
                if (isIn && isInterrupt) {
                    
                    /// Create inPipe
                    /// TODO: "Caller MUST release pipe"
                    inDescriptor = endpointDescriptor;
                    ret = controllerInterface->CopyPipe(endpointDescriptor->bEndpointAddress, &ivars->inPipe);
                    if (ret != kIOReturnSuccess) {
                        os_log(OS_LOG_DEFAULT, "Start - Failed to copy potential input pipe. Carrying on.");
                    }
                }
            }
            
            /// Try to create output pipe from endpoint
            
            if (ivars->outPipe == NULL) {
                
                bool isOut = (endpointDescriptor->bEndpointAddress & (1 << 7)) == 0;
                bool isInterrupt = (endpointDescriptor->bmAttributes & (1 << 0)) && (endpointDescriptor->bmAttributes & (1 << 1));
                uint8_t interval = endpointDescriptor->bInterval;
                uint16_t maxPacketSize = endpointDescriptor->wMaxPacketSize;
                
                os_log(OS_LOG_DEFAULT, "Start - Considering endpoint with attributes --- %d --- isOut: %d isInterrupt: %d interval: %d maxPacketSize: %d", endpointDescriptor->bmAttributes, isOut, isInterrupt, interval, maxPacketSize);
                
                if (isOut && isInterrupt) {
                    
                    /// Create outPipe
                    /// TODO: "Caller MUST release pipe"
                    outDescriptor = endpointDescriptor;
                    ret = controllerInterface->CopyPipe(endpointDescriptor->bEndpointAddress, &ivars->outPipe);
                    if (ret != kIOReturnSuccess) {
                        os_log(OS_LOG_DEFAULT, "Start - Failed to copy potential output pipe. Carrying on.");
                    }
                }
            }
            
            /// Break if both pipes are found
            if (ivars->inPipe != NULL && ivars->outPipe != NULL) {
                break;
            }
        }
        
        /// Destroy iterator
        ret = device->DestroyInterfaceIterator(iterator);
        if (ret != kIOReturnSuccess) {
            os_log(OS_LOG_DEFAULT, "Start - Failed to destroy interface iterator. Carrying on.");
        }
        
        /// Guard pipe creation success
        if (ivars->inPipe == NULL) {
            os_log(OS_LOG_DEFAULT, "Start - Input pipe couldn't be created.");
            goto fail;
        }
        if (ivars->outPipe == NULL) {
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
        /// - The 0 or 1 question is also important for the endpoint buffer!
        ivars->inBuffer = NULL;
        ret = IOBufferMemoryDescriptor::Create(kIOMemoryDirectionIn, inMaxPacketSize, 1, &ivars->inBuffer);
        if (ret != kIOReturnSuccess) {
            os_log(OS_LOG_DEFAULT, "Start - Failed to create buffer for input endpoint");
        }
        
        /// Get maxPacketSize for output endpoint
        uint16_t outMaxPacketSize = IOUSBGetEndpointMaxPacketSize(deviceSpeed, outDescriptor);
        
        /// Create buffer for output endpoint
        /// Notes: 360Controller created a fresh buffer for every write request. I don't know why, but I think this is probably a a bit better.
        ivars->outBuffer = NULL;
        ret = IOBufferMemoryDescriptor::Create(kIOMemoryDirectionOut, outMaxPacketSize, 1, &ivars->outBuffer);
        if (ret != kIOReturnSuccess) {
            os_log(OS_LOG_DEFAULT, "Start - Failed to create buffer for output endpoint");
        }
        
        /// Skipping Chatpad stuff from 360Controller
        /// ....
        
        /// Store dispatch queue
        ret = CopyDispatchQueue(kIOServiceDefaultQueueName, &ivars->queue);
        if (ret != kIOReturnSuccess) {
            os_log(OS_LOG_DEFAULT, "Start - Failed to get dispatch queue");
            goto fail;
        }
        
        /// Create and attach IOHIDDevice
        /// Note: Moved this up in the control flow compared to 360Controller
        PadConnect();
        
        /// Begin polling input
        bool success = QueueRead();
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
            QueueWrite(&xoneInit0, sizeof(xoneInit0));
            QueueWrite(&xoneInit1, sizeof(xoneInit1));
            QueueWrite(&xoneInit2, sizeof(xoneInit2));
            QueueWrite(&xoneInit3, sizeof(xoneInit3));
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

/// MARK: --- Write ---

bool gamepad_driver_user::QueueWrite(const void *bytes, uint32_t length) {
    
    /// Declare ret
    IOReturn ret = kIOReturnSuccess;
    
    /// Resize buffer
    ret = ivars->outBuffer->SetLength(length);
    if (ret != kIOReturnSuccess) {
        os_log(OS_LOG_DEFAULT, "Write - Failed to resize buffer. IOReturn: %.8x", ret);
        return false;
    }
    
    /// Get buffer address segment
    IOAddressSegment addressSegment;
    ret = ivars->outBuffer->GetAddressRange(&addressSegment);
    if (ret != kIOReturnSuccess) {
        os_log(OS_LOG_DEFAULT, "Write - Failed to get buffer address segment. IOReturn: %.8x", ret);
        return false;
    }
    
    /// Get raw buffer pointer
    void *rawBuffer = (void *)addressSegment.address;
    
    /// Write to buffer
    memcpy(rawBuffer, bytes, length);
    
    size_t referenceSize = 0;
    OSAction *completionHandler;
    ret = CreateActionWriteComplete(referenceSize, &completionHandler);
    if (ret != kIOReturnSuccess) {
        os_log(OS_LOG_DEFAULT, "Write - Failed to create action");
        return false;
    }
    
    /// Schedule async write to device
    uint32_t timeout = 0;
    ret = ivars->outPipe->AsyncIO(ivars->outBuffer, length, completionHandler, timeout);
    if (ret != kIOReturnSuccess) {
        os_log(OS_LOG_DEFAULT, "Write - Failed to write. IOReturn %.8x", ret);
        return false;
    }
    
    /// Cleanup
    /// TODO: Do this on early return to prevent leaks
    OSSafeReleaseNULL(completionHandler);
    
    /// Return success
    return true;
}

void IMPL(gamepad_driver_user, WriteComplete) {
    /// Type of this method:
    /// `void func(OSAction *action, IOReturn status, uint32_t actualByteCount, uint64_t completionTimestamp)`
    
    /// Log error
    if (status != kIOReturnSuccess) {
        os_log(OS_LOG_DEFAULT, "Write - Error writing: %.8x\n", status);
    } else {
        /// Log
        os_log(OS_LOG_DEFAULT, "Write - Wrote data");
    }
}

/// MARK: --- Read ---

bool gamepad_driver_user::QueueRead(void) {
    
    /// Log
    os_log(OS_LOG_DEFAULT, "Read - Requesting input");
    
    
    /// Declare return
    IOReturn ret;

    /// Define constants
    /// Docs "You must specify 0 when transferring data on an interrupt endpoint"
    uint32_t timeoutMs = 0;
    
    if (ivars->inPipe == NULL || ivars->inBuffer == NULL) {
        os_log(OS_LOG_DEFAULT, "Read - inPipe or inBuffer unavailable");
        return false;
    }
    
//    complete.target = this;
//    complete.action = ReadCompleteInternal;
//    complete.parameter = inBuffer;
//    ret = inPipe->Read(inBuffer, 0 ,0 , inBuffer->getLength(), &complete);
    
    /// Get inBuffer length
    uint64_t inBufferLength;
    ret = ivars->inBuffer->GetLength(&inBufferLength);
    if (ret != kIOReturnSuccess) {
        os_log(OS_LOG_DEFAULT, "Read - Failed to get inBuffer length.");
        return false;
    }
    
    /// Create OSAction
    /// The only docs of how this works I found in OSAction.iig: https://newosxbook.com/src.jl?tree=xnu&file=/iokit/DriverKit/OSAction.iig
    size_t referenceSize = 0;
    OSAction *completionHandler;
    ret = CreateActionReadComplete(referenceSize, &completionHandler);
    if (ret != kIOReturnSuccess) {
        os_log(OS_LOG_DEFAULT, "Read - Failed to create action");
        return false;
    }

    /// Schedule async read from device
    ret = ivars->inPipe->AsyncIO(ivars->inBuffer, (uint32_t)inBufferLength, completionHandler, timeoutMs);
    if (ret != kIOReturnSuccess) {
        os_log(OS_LOG_DEFAULT, "Read - Failed to read. IOReturn %d, inBufferIsNull: %d, inBufferLength: %d, actionIsNull: %d, timeout: %d", ret, ivars->inBuffer == NULL, (uint32_t)inBufferLength, completionHandler == NULL, timeoutMs);
        return false;
    }
    
    /// Cleanup
    /// TODO: We should do this also when the method fails at any point to prevent leaks
    OSSafeReleaseNULL(completionHandler);
    
    /// Return success
    return true;
}

void IMPL(gamepad_driver_user, ReadComplete) {
    /// Type of this method:
    /// `void func(OSAction *action, IOReturn status, uint32_t actualByteCount, uint64_t completionTimestamp)`
    
    /// Log
    os_log(OS_LOG_DEFAULT, "Read - Received data. Status %d bytecount: %d, timestamp: %llu", status, actualByteCount, completionTimestamp);
    
    /// Guard padHandler exists
    /// Notes:
    /// - This logic is copied from 360Controller I don't know if it's necessary here.
    /// - Comment from 360Controller: "avoid deadlock with release"
    if (ivars->padHandler == NULL) {
        os_log(OS_LOG_DEFAULT, "Read - Abort handling received data due to missing padHandler");
        return;
    }
    
    /// Enqueue workload
    /// Note: This logic is copied from 360Controller (although it is implemented with locks there). I don't know if it's necessary here
    
//    ivars->queue->DispatchSync(^{
        
        /// Declare return var
        IOReturn ret = kIOReturnSuccess;
        
        /// Init reread var.
        /// Note: In 360Controller it's set to this->isInactive() which is inherited from IOService in IOKit. But in DriverKit this doesn't seem to exist, and I couldn't see any replacement either. I assume they improved the IOService implementation so that  isInactive() is not necessary anymore
        bool doReadAgain = true;
        
        /// Log not responding
        if (status == kIOReturnNotResponding) {
            os_log(OS_LOG_DEFAULT, "Read - kIOReturnNotResponding");
        }
        
        /// Clear halted state of device on overrun
        /// Notes:
        /// - Coped from 360Controller. Don't know what this means
        /// - Note: Passing true to ClearStall. Docs say "it’s recommended that you specify YES, but you may safely specify NO for control endpoints"
        if (status == kIOReturnOverrun) {
            os_log(OS_LOG_DEFAULT, "Read - kIOReturnOverrun, clearing stall");
            if (ivars->inPipe) {
                ivars->inPipe->ClearStall(true);
            }
        }
        
        if (status == kIOReturnSuccess || status == kIOReturnOverrun) {
            
            /// Guard buffer exists
            /// Logic copied from 360Controller. Not sure if makes sense here.
            if (ivars->inBuffer != NULL) {
                    
                /// Get buffer address range
                IOAddressSegment bufferRange;
                ret = ivars->inBuffer->GetAddressRange(&bufferRange);
                if (ret != kIOReturnSuccess) {
                    os_log(OS_LOG_DEFAULT, "Read - Failed to get address range for buffer");
                    return;
                }
                
                /// Get report
                const XBOX360_IN_REPORT *report = (const XBOX360_IN_REPORT *)(IOVirtualAddress)bufferRange.address;
                
                /// Check report header
                /// Note: Copied logic from 360Controller. Not sure what we are doing.
                bool isValidReport = report->header.command == inReport && report->header.size == sizeof(XBOX360_IN_REPORT);
                bool isXboxOneReport = report->header.command == 0x20 || report->header.command == 0x07;
                
                if (isValidReport || isXboxOneReport) {
                    
                    os_log(OS_LOG_DEFAULT, "Read - Dispatching to padHandler");
                    ret = ivars->padHandler->handleReport(completionTimestamp, ivars->inBuffer, actualByteCount, kIOHIDReportTypeInput, 0);
                    if (ret != kIOReturnSuccess) {
                        os_log(OS_LOG_DEFAULT, "Read - failed to handle report. IOReturn: %.8x", ret);
                    }
                } else {
                    os_log(OS_LOG_DEFAULT, "Read - Not sending report since it's not valid. isValidReport: %d, isXboxOneReport: %d, command: %d, size: %d", isValidReport, isXboxOneReport, report->header.command, report->header.size);
                }
            }
            
        } else if (status == kIOReturnNotResponding) {
            os_log(OS_LOG_DEFAULT, "Read - kIOReturnNotResponding");
            doReadAgain = false;
        } else {
            os_log(OS_LOG_DEFAULT, "Read - Unhandled status %d", status);
            doReadAgain = false;
        }
            
        /// Queue another read
        os_log(OS_LOG_DEFAULT, "Read - Queueing another read");
        if (doReadAgain) QueueRead();
//    });
};


/// MARK: --- Support for main controller ---

void gamepad_driver_user::PadConnect(void) {
    
    
    /// Log
    os_log(OS_LOG_DEFAULT, "PadConnect - running");
    
    bool success;
    IOReturn ret;
    
    PadDisconnect();
    
    Controller::Type controller = ivars->controller;
    Xbox360ControllerClass *padHandlerClass = NULL;
    
//    if (controller == Controller::XboxOriginal) {
////        padHandler = new XboxOriginalControllerClass;
//    } else if (controller == Controller::XboxOne) {
//        padHandlerClass = XboxOneControllerClass::GetClass();
//    } else if (controller == Controller::XboxOnePretend360) {
////        padHandler = new XboxOnePretend360Class;
//    } else if (controller == Controller::Xbox360Pretend360) {
////        padHandler = new Xbox360Pretend360Class;
//    } else {
//        padHandlerClass = Xbox360ControllerClass::GetClass();
//    }
        
    /// Get IOCFPluginTypes
    OSDictionary *properties;
    ret = CopyProperties(&properties);
    if (ret != KERN_SUCCESS) {
        os_log(OS_LOG_DEFAULT, "PadConnect - Failed to copy properties");
        OSSafeReleaseNULL(properties);
        return;
    }
    OSObject *pluginTypes = properties->getObject("IOCFPlugInTypes");
    
    /// Create property dict for padHandler
    const OSObject *keys[] = {
        OSString::withCString(kIOSerialDeviceType),
        OSString::withCString("IOCFPlugInTypes"),
        OSString::withCString("IOKitDebug"),
    };
    const OSObject *objects[] = {
        OSNumber::withNumber((unsigned long long)1, 32),
        pluginTypes,
        OSNumber::withNumber((unsigned long long)65535, 32),
    };
    int count = sizeof(keys) / sizeof(keys[0]);
    
    OSDictionary *deviceProperties = OSDictionary::withObjects(objects, keys, count, count);
    
    /// Cleanup
    /// Note: maybe we should use goto exit pattern
    OSSafeReleaseNULL(properties);
    
    
    /// Init Create and attach padHandler
    /// I think for driverKit we need to use https://developer.apple.com/documentation/driverkit/ioservice/3325579-create
    /// See actual explanation here: https://stackoverflow.com/questions/61545594/how-should-newuserclient-be-implemented
    
    Xbox360ControllerClass *padHandler;
    ret = this->Create(this, "ControllerClassClientProperties", (IOService **)&padHandler);
    if (ret != kIOReturnSuccess || padHandler == NULL) {
        os_log(OS_LOG_DEFAULT, "PadConnect - Creating the ControllerClass client service failed. IOReturn: %.8x", ret);
        goto fail;
    }
    
    ret = padHandler->setProperties(deviceProperties);
    if (ret != kIOReturnSuccess) {
        os_log(OS_LOG_DEFAULT, "PadConnect - Setting properties failed");
        goto fail;
    }
        
    /// Start padHandler
    /// (Not sure if necessary)
//    padHandler->Start(this);
    
    /// Store padHandler
    ivars->padHandler = padHandler;
    
    /// Cleanup. (Not sure what we are doing)
    OSSafeReleaseNULL(properties);
    OSSafeReleaseNULL(deviceProperties);
    
    /// Return
    return;

    /// Cleanup after failure. Not sure what we are doing.
    fail:
    OSSafeReleaseNULL(properties);
    OSSafeReleaseNULL(deviceProperties);
    OSSafeReleaseNULL(padHandler);
}

void gamepad_driver_user::PadDisconnect(void) {
    
    if (ivars->padHandler != NULL) {
        ivars->padHandler->Terminate(0 /*kIOServiceRequired | kIOServiceSynchronous*/);
        ivars->padHandler->release();
        ivars->padHandler = NULL;
    }
}

kern_return_t IMPL(gamepad_driver_user, CoolGetProvider) {
    
    os_log(OS_LOG_DEFAULT, "CoolGetProvider()");
    
    IOService *p = this->GetProvider();
    IOUSBHostDevice *q = OSDynamicCast(IOUSBHostDevice, p);
    
    if (q == NULL) {
        os_log(OS_LOG_DEFAULT, "CoolGetProvider - failed to get provider normally - attempting to get it from ivars");
        q = ivars->provider;
    }
    
    if (q == NULL) {
        os_log(OS_LOG_DEFAULT, "CoolGetProvider - failed to get provider from ivars, too. Failure");
        return kIOReturnError;
    }
    
    
    os_log(OS_LOG_DEFAULT, "CoolGetProvider - success");
    *provider = q;
    
    return KERN_SUCCESS;
}

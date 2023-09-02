//
//  Controller.cpp
//  gamepad-driver-user
//
//  Created by Noah Nübling on 01.09.23.
//

/// Other imports
#include <HIDDriverKit/IOHIDDevice.h>
#include <HIDDriverKit/IOHIDDeviceKeys.h>
#include <DriverKit/DriverKit.h>
#include <USBDriverKit/IOUSBHostDevice.h>
#include "gamepad_driver_user.h"
#include "ControlStruct.h"

namespace HID_360 {
#include "xbox360hid.h"
}

/// Import Header
#include "Xbox360ControllerClass.h"

#pragma mark - Xbox360Controller

//OSDefineMetaClassAndStructors(Xbox360ControllerClass, IOHIDDevice)

static gamepad_driver_user *GetOwner(IOService *us) {
    
    os_log(OS_LOG_DEFAULT, "ControllerClass - GetOwner()");
    
    IOService *provider = us->GetProvider();

    if (provider == NULL) {
        os_log(OS_LOG_DEFAULT, "ControllerClass - GetOwner - failed");
        return NULL;
    }
    os_log(OS_LOG_DEFAULT, "ControllerClass - GetOwner - success");
    return OSDynamicCast(gamepad_driver_user, provider);
}

static IOUSBHostDevice* GetOwnerProvider(const IOService *us) {
    
    os_log(OS_LOG_DEFAULT, "ControllerClass - GetOwnerProvider()");
    
    IOReturn ret;
    
    gamepad_driver_user *prov = OSDynamicCast(gamepad_driver_user, us->GetProvider());

    if (prov == NULL) {
        os_log(OS_LOG_DEFAULT, "ControllerClass - GetOwnerProvider - Failed to get parent");
        return NULL;
    }
    
    IOUSBHostDevice *provprov;
    ret = prov->CoolGetProvider(&provprov);
    if (ret != kIOReturnSuccess || provprov == NULL) {
        os_log(OS_LOG_DEFAULT, "ControllerClass - GetOwnerProvider - Failed to get grandparent");
        return NULL;
    }
    os_log(OS_LOG_DEFAULT, "ControllerClass - GetOwnerProvider - success");
    return provprov;
}

bool Xbox360ControllerClass::handleStart(IOService *provider) {
    
    /// Log
    os_log(OS_LOG_DEFAULT, "ControllerClass - handleStart()");
    
    if (OSDynamicCast(gamepad_driver_user, provider) == NULL) {
        os_log(OS_LOG_DEFAULT, "ControllerClass - handleStart - No provider found");
        return false;
    }
    
    return true;
//    return IOHIDDevice::start(provider);
}

void Xbox360ControllerClass::setProperty(OSObject *key, OSObject *value) {
    
    OSDictionary *properties = OSDictionary::withCapacity(1);
    properties->setObject(key, value);
    
    setProperties(properties);
    
    OSSafeReleaseNULL(properties);
}

kern_return_t Xbox360ControllerClass::setProperties(OSDictionary *properties) {

    /// Log
    os_log(OS_LOG_DEFAULT, "ControllerClass - setProperty()");
    
    gamepad_driver_user *owner = GetOwner(this);
    if (owner == NULL) {
        return kIOReturnUnsupported;
    }
    return owner->SetProperties(properties);
}

OSData *Xbox360ControllerClass::newReportDescriptor(void) {
    
    /// Log
    os_log(OS_LOG_DEFAULT, "ControllerClass - newReportDescriptor()");
    
    /// Returns the HID descriptor for this device
    
    OSData *result = OSData::withBytes(&HID_360::ReportDescriptor, sizeof(HID_360::ReportDescriptor));
    
    return result;
}


kern_return_t Xbox360ControllerClass::setReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options, uint32_t completionTimeout, OSAction *action) {
    
    /// Notes:
    /// - Comment from 360Controller: Handles a message from the userspace IOHIDDeviceInterface122::setReport function
    /// - Why did the 360Controller author use those scopes inside the switch?? Kind of weird but leaving it there cause idk why they did that.

    /// Log
    os_log(OS_LOG_DEFAULT, "ControllerClass - setReport()");
    
    /// Map report memory into current process' address space
    /// Notes:
    /// - Not sure what we are doing here at all
    
    IOMemoryMap *map;
    uint64_t mappingOptions = kIOMemoryMapCacheModeDefault;
    report->CreateMapping(mappingOptions, 0, 0, 0, 0, &map);
    
    /// Get pointer to raw report data
    uint8_t *reportData = (uint8_t *)map->GetAddress();
    uint64_t reportDataLength = map->GetLength();
    
    /// Check whether rumble is enabled
    bool doRumble = true; // GetOwner(this)->rumbleType != 1;
    
    /// Return if rumble disabled
    /// Note: Why wouldn't we set leds if rumble is disabled?
    if (!doRumble) {
        return kIOReturnSuccess;
    }
    
    switch(reportData[0]) {
            
        case 0x00:
            
            /// Set force feedback
            
            if (reportData[1] != reportDataLength || reportData[1] != 0x04) {
                return kIOReturnUnsupported;
            }
        {
            XBOX360_OUT_RUMBLE rumble;

            Xbox360_Prepare(rumble, outRumble);
            rumble.big = reportData[2];
            rumble.little = reportData[3];
            GetOwner(this)->QueueWrite(&rumble, sizeof(rumble));
            
            // IOLog("Set rumble: big(%d) little(%d)\n", rumble.big, rumble.little);
        }
            return kIOReturnSuccess;
            
        case 0x01:
            
            /// Set LEDs
            
            if (reportData[1] != reportDataLength || reportData[1] != 0x03) {
                return kIOReturnUnsupported;
            }
        {
            XBOX360_OUT_LED led;
            Xbox360_Prepare(led, outLed);
            led.pattern = reportData[2];
            GetOwner(this)->QueueWrite(&led, sizeof(led));
            
            // IOLog("Set LED: %d\n", led.pattern);
        }
            return kIOReturnSuccess;
            
        default:
            
            /// Uknown escape
            
            os_log(OS_LOG_DEFAULT, "Unknown escape %d\n", reportData[0]);
            return kIOReturnUnsupported;
    }
}

kern_return_t Xbox360ControllerClass::getReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options, uint32_t completionTimeout, OSAction *action) {
    
    /// Log
    os_log(OS_LOG_DEFAULT, "ControllerClass - getReport()");
    
    /// Get report doesn't do anything yet ;)
    return kIOReturnUnsupported;
}

kern_return_t Xbox360ControllerClass::handleReport(uint64_t             timestamp,
                                                   IOMemoryDescriptor  *descriptor,
                                                   uint32_t             reportLength,
                                                   IOHIDReportType      reportType /*= kIOHIDReportTypeInput*/,
                                                   IOOptionBits         options /*= 0*/) {
    
    /// Log
    os_log(OS_LOG_DEFAULT, "ControllerClass - handlerReport()");
    
    /// Declare reusable ret
    kern_return_t ret = KERN_SUCCESS;
    
    if (reportLength >= sizeof(XBOX360_IN_REPORT)) {
        
        IOBufferMemoryDescriptor *desc = OSDynamicCast(IOBufferMemoryDescriptor, descriptor);
        
        if (desc != NULL) {
            
            /// Get addressSegment
            IOAddressSegment addressSegment;
            ret = desc->GetAddressRange(&addressSegment);
            if (ret != KERN_SUCCESS) {
                os_log(OS_LOG_DEFAULT, "handleReport - Failed to get AddressRange from reportDescriptor");
                return kIOReturnError; /** Not sure which error to return */
            }
            
            /// Get raw report
            
            XBOX360_IN_REPORT *report = (XBOX360_IN_REPORT *)addressSegment.address;
            
            /// Modify report according to settings
            
//            if (report->header.command == inReport && report->header.size == sizeof(XBOX360_IN_REPORT)) {
//                GetOwner(this)->fiddleReport(report->left, report->right);
//                if (!(GetOwner(this)->noMapping))
//                    remapButtons(report);
//                if (GetOwner(this)->swapSticks)
//                    remapAxes(report);
//            }
        }
    }
    
    /// Pass report to super method
    ret = IOHIDDevice::handleReport(timestamp, descriptor, reportLength, reportType, options);
    
    /// Return
    return ret;
}


OSString *Xbox360ControllerClass::copyDeviceString(uint8_t stringIndex, const char *fallback) {
    
    /// Helper method which returns the string for a specified index from the USB device's string list
    
    /// Log
    os_log(OS_LOG_DEFAULT, "ControllerClass - copyDeviceString() - arg stringIndex: %d", stringIndex);
    
    /// Declare return
    IOReturn ret;
    
    /// Get stringDescriptor from device
    const IOUSBStringDescriptor *stringDescriptor = GetOwnerProvider(this)->CopyStringDescriptor(stringIndex);
    
    /// Retrieve string from descriptor / fallback
    
    const char *cString = NULL;
    uint8_t cStringLength = 0;
    
    if (stringDescriptor == NULL) {
        
        if (fallback == NULL) {
            cString = "Unknown";
        } else {
            cString = fallback;
        }
        cStringLength = strlen(cString);
    } else {
        cString = (const char *)stringDescriptor->bString;
        cStringLength = stringDescriptor->bLength;
    }
    
    /// Create OSString
    OSString *result = OSString::withCString(cString, cStringLength);
    
    /// Free string descriptor
    IOFree((void *)stringDescriptor, sizeof(IOUSBStringDescriptor));
    
    /// Log
    os_log(OS_LOG_DEFAULT, "ControllerClass - copyDeviceString - result: %s", cString);
    
    /// Return
    return result;
    
}


/*


OSNumber* Xbox360ControllerClass::newPrimaryUsageNumber() const
{
    return OSNumber::withNumber(HID_360::ReportDescriptor[3], 8);
}

OSNumber* Xbox360ControllerClass::newPrimaryUsagePageNumber() const
{
    return OSNumber::withNumber(HID_360::ReportDescriptor[1], 8);
}

*/

/// MARK: --- Device Description ---

OSDictionary *Xbox360ControllerClass::newDeviceDescription(void) {
    
    /// Log
    os_log(OS_LOG_DEFAULT, "ControllerClass - newDeviceDescriptionn()");
    
    /// Get device descriptor
    const IOUSBDeviceDescriptor *deviceDescriptor = GetOwnerProvider(this)->CopyDeviceDescriptor();
    if (deviceDescriptor == NULL) {
        os_log(OS_LOG_DEFAULT, "ControllerClass - newDeviceDescription - Couldn't copy grandpa device descriptor");
        return OSDictionary::withCapacity(0);
    }
    
    
    /// Wrap values from device descriptor in OSContainers,
    ///     So they can be stored in the result dict
    /// Notes:
    /// - Hardcoding product string to "Xbox 360 Wired Controller" because that's what 360Controller did. We could also get the real value with `getDeviceString(deviceDescriptor->iProduct, "Unknown Product Name");`
    
    OSNumber *vendorID = OSNumber::withNumber(deviceDescriptor->idVendor, 16);
    OSNumber *productID = OSNumber::withNumber(deviceDescriptor->idProduct, 16);
    OSString *manufacturer = copyDeviceString(deviceDescriptor->iManufacturer, "Unknown Manufacturer");
    OSString *product = OSString::withCString("Xbox 360 Wired Controller");
    OSString *serialNumber = copyDeviceString(deviceDescriptor->iSerialNumber, "Unknown Serial Number");
    
    /// Free device descriptor
    IOFree((void *)deviceDescriptor, sizeof(IOUSBDeviceDescriptor));
    
    /// Hardcode additional values
    OSString *transport = OSString::withCString("USB");
    OSNumber *locationID = createLocationID();
    
    /// Create result dict
    OSDictionary *result = OSDictionary::withCapacity(0);
    
    /// Fill result dict
    
//    result->setObject(kIOHIDReportIntervalKey, );
    result->setObject(kIOHIDVendorIDKey, vendorID);
    result->setObject(kIOHIDProductIDKey, productID);
    result->setObject(kIOHIDTransportKey, transport);
//    result->setObject(kIOHIDVersionNumberKey, );
//    result->setObject(kIOHIDCountryCodeKey, );
    result->setObject(kIOHIDLocationIDKey, locationID);
    result->setObject(kIOHIDManufacturerKey, manufacturer);
    result->setObject(kIOHIDProductKey, product);
    result->setObject(kIOHIDSerialNumberKey, serialNumber);
//    result->setObject(kIOHIDRequestTimeoutKey, );
    
    /// Release result dict values
    
//    OSSafeReleaseNULL( );
    OSSafeReleaseNULL(vendorID);
    OSSafeReleaseNULL(productID);
    OSSafeReleaseNULL(transport);
//    OSSafeReleaseNULL( );
//    OSSafeReleaseNULL( );
    OSSafeReleaseNULL(locationID);
    OSSafeReleaseNULL(manufacturer);
    OSSafeReleaseNULL(product);
    OSSafeReleaseNULL(serialNumber);
//    OSSafeReleaseNULL( );
    
    /// Log
    os_log(OS_LOG_DEFAULT, "ControllerClass - newDeviceDescription - result has %d keys", result->getCount());
    
    /// Return result dict
    return result;
};

/*

OSString* Xbox360ControllerClass::newManufacturerString() const
{
    return getDeviceString(GetOwnerProvider(this)->GetManufacturerStringIndex());
}

OSNumber* Xbox360ControllerClass::newProductIDNumber() const
{
    return OSNumber::withNumber(GetOwnerProvider(this)->GetProductID(),16);
}

OSString* Xbox360ControllerClass::newProductString() const
{
    return OSString::withCString("Xbox 360 Wired Controller");
}

OSString* Xbox360ControllerClass::newSerialNumberString() const
{
    return getDeviceString(GetOwnerProvider(this)->GetSerialNumberStringIndex());
}

OSString* Xbox360ControllerClass::newTransportString() const
{
    return OSString::withCString("USB");
}

OSNumber* Xbox360ControllerClass::newVendorIDNumber() const
{
    return OSNumber::withNumber(GetOwnerProvider(this)->GetVendorID(),16);
}
 
 */

OSNumber *Xbox360ControllerClass::createLocationID() {
    
    /// Implementation taken from 360Controller
    
    /// Declare result and return
    kern_return_t ret = KERN_SUCCESS;
    uint32_t result = 0;

    /// Get grandparent
    IOUSBHostDevice *device = GetOwnerProvider(this);
    
    if (device) {
        
        /// Copy device properties dict
        OSDictionary *deviceProperties;
        ret = device->CopyProperties(&deviceProperties);
        
        if (ret != KERN_SUCCESS) {
            os_log(OS_LOG_DEFAULT, "createLocationID - Failed to get grandparent properties");
        }
        
        if (deviceProperties != NULL) {
            
            /// Get location ID from properties dict
            OSNumber *locationID = OSDynamicCast(OSNumber, deviceProperties->getObject("locationID"));
            if (locationID != NULL) {
                result = locationID->unsigned32BitValue();
            }
            
            /// Fall back to made up address
            if (result == 0) {
                
                OSNumber *USBAddress = OSDynamicCast(OSNumber, deviceProperties->getObject("USB Address"));
                OSNumber *productID = OSDynamicCast(OSNumber, deviceProperties->getObject("idProduct"));
                
                if (USBAddress != NULL) {
                    result |= USBAddress->unsigned8BitValue() << 24;
                }
                if (productID != NULL) {
                    result |= productID->unsigned8BitValue() << 16;
                }
            }
            
            /// Release device properties dict
            OSSafeReleaseNULL(deviceProperties);
        }
    }
    
    /// Return result
    
    if (result != 0) {
        return OSNumber::withNumber(result, 32);
    } else {
        return NULL;
    }
}



/*

void Xbox360ControllerClass::remapButtons(void *buffer) {
    
    XBOX360_IN_REPORT *report360 = (XBOX360_IN_REPORT*)buffer;
    UInt16 new_buttons = 0;

    new_buttons |= ((report360->buttons & 1) == 1) << GetOwner(this)->mapping[0];
    new_buttons |= ((report360->buttons & 2) == 2) << GetOwner(this)->mapping[1];
    new_buttons |= ((report360->buttons & 4) == 4) << GetOwner(this)->mapping[2];
    new_buttons |= ((report360->buttons & 8) == 8) << GetOwner(this)->mapping[3];
    new_buttons |= ((report360->buttons & 16) == 16) << GetOwner(this)->mapping[4];
    new_buttons |= ((report360->buttons & 32) == 32) << GetOwner(this)->mapping[5];
    new_buttons |= ((report360->buttons & 64) == 64) << GetOwner(this)->mapping[6];
    new_buttons |= ((report360->buttons & 128) == 128) << GetOwner(this)->mapping[7];
    new_buttons |= ((report360->buttons & 256) == 256) << GetOwner(this)->mapping[8];
    new_buttons |= ((report360->buttons & 512) == 512) << GetOwner(this)->mapping[9];
    new_buttons |= ((report360->buttons & 1024) == 1024) << GetOwner(this)->mapping[10];
    new_buttons |= ((report360->buttons & 4096) == 4096) << GetOwner(this)->mapping[11];
    new_buttons |= ((report360->buttons & 8192) == 8192) << GetOwner(this)->mapping[12];
    new_buttons |= ((report360->buttons & 16384) == 16384) << GetOwner(this)->mapping[13];
    new_buttons |= ((report360->buttons & 32768) == 32768) << GetOwner(this)->mapping[14];

//    IOLog("BUTTON PACKET - %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n", GetOwner(this)->mapping[0], GetOwner(this)->mapping[1], GetOwner(this)->mapping[2], GetOwner(this)->mapping[3], GetOwner(this)->mapping[4], GetOwner(this)->mapping[5], GetOwner(this)->mapping[6], GetOwner(this)->mapping[7], GetOwner(this)->mapping[8], GetOwner(this)->mapping[9], GetOwner(this)->mapping[10], GetOwner(this)->mapping[11], GetOwner(this)->mapping[12], GetOwner(this)->mapping[13], GetOwner(this)->mapping[14]);

    report360->buttons = new_buttons;
}

void Xbox360ControllerClass::remapAxes(void *buffer)
{
    XBOX360_IN_REPORT *report360 = (XBOX360_IN_REPORT*)buffer;

    XBOX360_HAT temp = report360->left;
    report360->left = report360->right;
    report360->right = temp;
}


*/

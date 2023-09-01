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


/// Import Header
#include "Xbox360ControllerClass.h"

#pragma mark - Xbox360Controller

//OSDefineMetaClassAndStructors(Xbox360ControllerClass, IOHIDDevice)

static gamepad_driver_user *GetOwner(IOService *us) {
    
    IOService *provider = us->GetProvider();

    if (provider == NULL) {
        return NULL;
    }
    return OSDynamicCast(gamepad_driver_user, provider);
}

static IOUSBHostDevice* GetOwnerProvider(const IOService *us)
{
    IOService *prov = us->GetProvider();

    if (prov == NULL) {
        return NULL;
    }
    IOService *provprov = prov->GetProvider();
    if (provprov == NULL) {
        return NULL;
    }
    return OSDynamicCast(IOUSBHostDevice, provprov);
}

bool Xbox360ControllerClass::handleStart(IOService *provider) {
    
    if (OSDynamicCast(gamepad_driver_user, provider) == NULL) {
        return false;
    }
    
    return true;
//    return IOHIDDevice::start(provider);
}

//kern_return_t Xbox360ControllerClass::setProperties(OSDictionary *properties) {
//    
//    gamepad_driver_user *owner = GetOwner(this);
//    if (owner == NULL)
//        return kIOReturnUnsupported;
//    return owner->SetProperties(properties);
//}

//void Xbox360ControllerClass::setProperty(OSObject *key, OSObject *value) {
//    
//    OSDictionary *properties = OSDictionary::withCapacity(1);
//    properties->setObject(key, value);
//    
//    setProperties(properties);
//}

//// Returns the HID descriptor for this device
//IOReturn Xbox360ControllerClass::newReportDescriptor(IOMemoryDescriptor **descriptor) const
//{
//    IOBufferMemoryDescriptor *buffer = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,kIODirectionOut,sizeof(HID_360::ReportDescriptor));
//
//    if (buffer == NULL) return kIOReturnNoResources;
//    buffer->writeBytes(0,HID_360::ReportDescriptor,sizeof(HID_360::ReportDescriptor));
//    *descriptor=buffer;
//    return kIOReturnSuccess;
//}
//
//// Handles a message from the userspace IOHIDDeviceInterface122::setReport function
//IOReturn Xbox360ControllerClass::setReport(IOMemoryDescriptor *report,IOHIDReportType reportType,IOOptionBits options)
//{
//    char data[2];
//
//    report->readBytes(0, data, 2);
//    if (GetOwner(this)->rumbleType == 1) // Don't Rumble
//        return kIOReturnSuccess;
//    switch(data[0]) {
//        case 0x00:  // Set force feedback
//            if((data[1]!=report->getLength()) || (data[1]!=0x04)) return kIOReturnUnsupported;
//        {
//            XBOX360_OUT_RUMBLE rumble;
//
//            Xbox360_Prepare(rumble,outRumble);
//            report->readBytes(2,data,2);
//            rumble.big=data[0];
//            rumble.little=data[1];
//            GetOwner(this)->QueueWrite(&rumble,sizeof(rumble));
//            // IOLog("Set rumble: big(%d) little(%d)\n", rumble.big, rumble.little);
//        }
//            return kIOReturnSuccess;
//        case 0x01:  // Set LEDs
//            if((data[1]!=report->getLength())||(data[1]!=0x03)) return kIOReturnUnsupported;
//        {
//            XBOX360_OUT_LED led;
//
//            report->readBytes(2,data,1);
//            Xbox360_Prepare(led,outLed);
//            led.pattern=data[0];
//            GetOwner(this)->QueueWrite(&led,sizeof(led));
//            // IOLog("Set LED: %d\n", led.pattern);
//        }
//            return kIOReturnSuccess;
//        default:
//            IOLog("Unknown escape %d\n", data[0]);
//            return kIOReturnUnsupported;
//    }
//}
//
//// Get report
//IOReturn Xbox360ControllerClass::getReport(IOMemoryDescriptor *report,IOHIDReportType reportType,IOOptionBits options)
//{
//    // Doesn't do anything yet ;)
//    return kIOReturnUnsupported;
//}
//
//IOReturn Xbox360ControllerClass::handleReport(IOMemoryDescriptor * descriptor, IOHIDReportType reportType, IOOptionBits options) {
//    if (descriptor->getLength() >= sizeof(XBOX360_IN_REPORT)) {
//        IOBufferMemoryDescriptor *desc = OSDynamicCast(IOBufferMemoryDescriptor, descriptor);
//        if (desc != NULL) {
//            XBOX360_IN_REPORT *report=(XBOX360_IN_REPORT*)desc->getBytesNoCopy();
//            if ((report->header.command==inReport) && (report->header.size==sizeof(XBOX360_IN_REPORT))) {
//                GetOwner(this)->fiddleReport(report->left, report->right);
//                if (!(GetOwner(this)->noMapping))
//                    remapButtons(report);
//                if (GetOwner(this)->swapSticks)
//                    remapAxes(report);
//            }
//        }
//    }
//    IOReturn ret = IOHIDDevice::handleReport(descriptor, reportType, options);
//    return ret;
//}


OSString *Xbox360ControllerClass::copyDeviceString(uint8_t stringIndex, const char *fallback) {
        
    /// Helper method which returns the string for a specified index from the USB device's string list
    
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
    
    /// Free string descriptor
    IOFree((void *)stringDescriptor, sizeof(IOUSBStringDescriptor));
    
    /// Create OSString
    OSString *result = OSString::withCString(cString, cStringLength);
    
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
    
    /// Get device descriptor
    const IOUSBDeviceDescriptor *deviceDescriptor = GetOwnerProvider(this)->CopyDeviceDescriptor();
    
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

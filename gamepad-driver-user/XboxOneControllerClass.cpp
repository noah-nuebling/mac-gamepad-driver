//
//  XboxOneControllerClass.cpp
//  gamepad-driver-user
//
//  Created by Noah Nübling on 01.09.23.
//

#include "XboxOneControllerClass.h"
#include "ControlStruct.h"

/*
 * Xbox One controller.
 * Does not pretend to be an Xbox 360 controller.
 */

typedef struct {
    uint8_t command;
    uint8_t reserved1;
    uint8_t counter;
    uint8_t size;
} PACKED XBOXONE_HEADER;

typedef struct {
    XBOXONE_HEADER header;
    uint16_t buttons;
    uint16_t trigL, trigR;
    XBOX360_HAT left, right;
} PACKED XBOXONE_IN_REPORT;

typedef struct {
    XBOXONE_HEADER header;
    uint16_t buttons;
    uint16_t trigL, trigR;
    XBOX360_HAT left, right;
    uint8_t unknown1[6];
    uint8_t triggersAsButtons; // 0x40 is RT. 0x80 is LT
    uint8_t unknown2[7];
} PACKED XBOXONE_IN_FIGHTSTICK_REPORT;

typedef struct {
    XBOXONE_HEADER header;
    uint16_t buttons;
    union {
        uint16_t steering; // leftX
        int16_t leftX;
    };
    union {
        uint16_t accelerator;
        uint16_t trigR;
    };
    union {
        uint16_t brake;
        uint16_t trigL;
    };
    union {
        uint8_t clutch;
        uint8_t leftY;
    };
    uint8_t unknown3[8];
} PACKED XBOXONE_IN_WHEEL_REPORT;

typedef struct {
    XBOXONE_HEADER header;
    uint16_t buttons;
    uint16_t trigL, trigR;
    XBOX360_HAT left, right;
    uint16_t true_buttons;
    uint16_t true_trigL, true_trigR;
    XBOX360_HAT true_left, true_right;
    uint8_t paddle;
} PACKED XBOXONE_ELITE_IN_REPORT;

typedef struct {
    XBOXONE_HEADER header;
    uint8_t state;
    uint8_t dummy;
} PACKED XBOXONE_IN_GUIDE_REPORT;

typedef struct {
    XBOXONE_HEADER header;
    uint8_t data[4];
    uint8_t zero[5];
} PACKED XBOXONE_OUT_GUIDE_REPORT;

typedef struct {
    XBOXONE_HEADER header;
    uint8_t mode; // So far always 0x00
    uint8_t rumbleMask; // So far always 0x0F
    uint8_t trigL, trigR;
    uint8_t little, big;
    uint8_t length; // Length of time to rumble
    uint8_t period; // Period of time between pulses. DO NOT INCLUDE WHEN SUBSTRUCTURE IS 0x09
    uint8_t extra;
} PACKED XBOXONE_OUT_RUMBLE;

typedef struct {
    XBOXONE_HEADER header; // 0x0a 0x20 0x04 0x03
    uint8_t zero;
    uint8_t command;
    uint8_t brightness; // 0x00 - 0x20
} PACKED XBOXONE_OUT_LED;

typedef enum {
    XONE_SYNC           = 0x0001, // Bit 00
    XONE_MENU           = 0x0004, // Bit 02
    XONE_VIEW           = 0x0008, // Bit 03
    XONE_A              = 0x0010, // Bit 04
    XONE_B              = 0x0020, // Bit 05
    XONE_X              = 0x0040, // Bit 06
    XONE_Y              = 0x0080, // Bit 07
    XONE_DPAD_UP        = 0x0100, // Bit 08
    XONE_DPAD_DOWN      = 0x0200, // Bit 09
    XONE_DPAD_LEFT      = 0x0400, // Bit 10
    XONE_DPAD_RIGHT     = 0x0800, // Bit 11
    XONE_LEFT_SHOULDER  = 0x1000, // Bit 12
    XONE_RIGHT_SHOULDER = 0x2000, // Bit 13
    XONE_LEFT_THUMB     = 0x4000, // Bit 14
    XONE_RIGHT_THUMB    = 0x8000, // Bit 15
} GAMEPAD_XONE;

typedef enum {
    XONE_PADDLE_UPPER_LEFT      = 0x0001, // Bit 00
    XONE_PADDLE_UPPER_RIGHT     = 0x0002, // Bit 01
    XONE_PADDLE_LOWER_LEFT      = 0x0004, // Bit 02
    XONE_PADDLE_LOWER_RIGHT     = 0x0008, // Bit 03
    XONE_PADDLE_PRESET_NUM      = 0x0010, // Bit 04
} GAMEPAD_XONE_ELITE_PADDLE;

typedef enum {
    XONE_LED_OFF_1           = 0x00,
    XONE_LED_SOLID           = 0x01,
    XONE_LED_BLINK_FAST      = 0x02,
    XONE_LED_BLINK_SLOW      = 0x03,
    XONE_LED_BLINK_VERY_SLOW = 0x04,
    XONE_LED_SOLD_1          = 0x05,
    XONE_LED_SOLD_2          = 0x06,
    XONE_LED_SOLD_3          = 0x07,
    XONE_LED_PHASE_SLOW      = 0x08,
    XONE_LED_PHASE_FAST      = 0x09,
    XONE_LED_REBOOT_1        = 0x0a,
    XONE_LED_OFF             = 0x0b,
    XONE_LED_FLICKER         = 0x0c,
    XONE_LED_SOLID_4         = 0x0d,
    XONE_LED_SOLID_5         = 0x0e,
    XONE_LED_REBOOT_2        = 0x0f,
} LED_XONE;

//OSDefineMetaClassAndStructors(XboxOneControllerClass, Xbox360ControllerClass)


/*

OSString* XboxOneControllerClass::newProductString() const
{
    return OSString::withCString("Xbox One Wired Controller");
}

UInt16 XboxOneControllerClass::convertButtonPacket(UInt16 buttons)
{
    UInt16 new_buttons = 0;

    new_buttons |= ((buttons & 4) == 4) << 4;
    new_buttons |= ((buttons & 8) == 8) << 5;
    new_buttons |= ((buttons & 16) == 16) << 12;
    new_buttons |= ((buttons & 32) == 32) << 13;
    new_buttons |= ((buttons & 64) == 64) << 14;
    new_buttons |= ((buttons & 128) == 128) << 15;
    new_buttons |= ((buttons & 256) == 256) << 0;
    new_buttons |= ((buttons & 512) == 512) << 1;
    new_buttons |= ((buttons & 1024) == 1024) << 2;
    new_buttons |= ((buttons & 2048) == 2048) << 3;
    new_buttons |= ((buttons & 4096) == 4096) << 8;
    new_buttons |= ((buttons & 8192) == 8192) << 9;
    new_buttons |= ((buttons & 16384) == 16384) << 6;
    new_buttons |= ((buttons & 32768) == 32768) << 7;

    new_buttons |= (isXboxOneGuideButtonPressed) << 10;

    return new_buttons;
}

void XboxOneControllerClass::convertFromXboxOne(void *buffer, uint8_t packetSize)
{
    XBOXONE_ELITE_IN_REPORT *reportXone = (XBOXONE_ELITE_IN_REPORT*)buffer;
    XBOX360_IN_REPORT *report360 = (XBOX360_IN_REPORT*)buffer;
    uint8_t trigL = 0, trigR = 0;
    XBOX360_HAT left, right;

    report360->header.command = 0x00;
    report360->header.size = 0x14;

    if (packetSize == 0x1a) // Fight Stick
    {
        if ((0x80 & reportXone->true_trigR) == 0x80) { trigL = 255; }
        if ((0x40 & reportXone->true_trigR) == 0x40) { trigR = 255; }
        
        left = reportXone->left;
        right = reportXone->right;
    }
    else if (packetSize == 0x11) // Racing Wheel
    {
        XBOXONE_IN_WHEEL_REPORT *wheelReport=(XBOXONE_IN_WHEEL_REPORT*)buffer;

        trigR = (wheelReport->accelerator / 1023.0) * 255; // UInt16 -> uint8_t
        trigL = (wheelReport->brake / 1023.0) * 255; // UInt16 -> uint8_t
        left.x = wheelReport->steering - 32768; // UInt16 -> SInt16
        left.y = wheelReport->clutch * 128; // Clutch is 0-255. Upconvert to half signed 16 range. (0 - 32640)
        right = {};
    }
    else // Traditional Controllers
    {
        trigL = (reportXone->trigL / 1023.0) * 255;
        trigR = (reportXone->trigR / 1023.0) * 255;
        
        left = reportXone->left;
        right = reportXone->right;
    }

    report360->buttons = convertButtonPacket(reportXone->buttons);
    report360->trigL = trigL;
    report360->trigR = trigR;
    report360->left = left;
    report360->right = right;
}

IOReturn XboxOneControllerClass::handleReport(IOMemoryDescriptor * descriptor, IOHIDReportType reportType, IOOptionBits options)
{
    if (descriptor->getLength() >= sizeof(XBOXONE_IN_GUIDE_REPORT)) {
        IOBufferMemoryDescriptor *desc = OSDynamicCast(IOBufferMemoryDescriptor, descriptor);
        if (desc != NULL) {
            XBOXONE_ELITE_IN_REPORT *report=(XBOXONE_ELITE_IN_REPORT*)desc->getBytesNoCopy();
            if ((report->header.command==0x07) && (report->header.size==(sizeof(XBOXONE_IN_GUIDE_REPORT)-4)))
            {
                XBOXONE_IN_GUIDE_REPORT *guideReport=(XBOXONE_IN_GUIDE_REPORT*)report;
                
                if (guideReport->header.reserved1 == 0x30) // 2016 Controller
                {
                    XBOXONE_OUT_GUIDE_REPORT outReport = {};
                    outReport.header.command = 0x01;
                    outReport.header.reserved1 = 0x20;
                    outReport.header.counter = guideReport->header.counter;
                    outReport.header.size = 0x09;
                    outReport.data[0] = 0x00;
                    outReport.data[1] = 0x07;
                    outReport.data[2] = 0x20;
                    outReport.data[3] = 0x02;
                    
                    GetOwner(this)->QueueWrite(&outReport, 13);
                }
                
                isXboxOneGuideButtonPressed = (bool)guideReport->state;
                XBOX360_IN_REPORT *oldReport = (XBOX360_IN_REPORT*)lastData;
                oldReport->buttons ^= (-isXboxOneGuideButtonPressed ^ oldReport->buttons) & (1 << GetOwner(this)->mapping[10]);
                memcpy(report, lastData, sizeof(XBOX360_IN_REPORT));
            }
            else if (report->header.command==0x20)
            {
                convertFromXboxOne(report, report->header.size);
                XBOX360_IN_REPORT *report360=(XBOX360_IN_REPORT*)report;
                if (!(GetOwner(this)->noMapping))
                    remapButtons(report360);
                GetOwner(this)->fiddleReport(report360->left, report360->right);

                if (GetOwner(this)->swapSticks)
                    remapAxes(report360);

                memcpy(lastData, report360, sizeof(XBOX360_IN_REPORT));
            }
        }
    }
    IOReturn ret = IOHIDDevice::handleReport(descriptor, reportType, options);
    return ret;
}

IOReturn XboxOneControllerClass::setReport(IOMemoryDescriptor *report,IOHIDReportType reportType,IOOptionBits options)
{
    //    IOLog("Xbox One Controller - setReport\n");
    unsigned char data[4];
    report->readBytes(0, &data, 4);
//        IOLog("Attempting to send: %d %d %d %d\n",((unsigned char*)data)[0], ((unsigned char*)data)[1], ((unsigned char*)data)[2], ((unsigned char*)data)[3]);
    uint8_t rumbleType;
    switch(data[0])//(header.command)
    {
        case 0x00:  // Set force feedback
            XBOXONE_OUT_RUMBLE rumble;
            rumble.header.command = 0x09;
            rumble.header.reserved1 = 0x00;
            rumble.header.counter = (GetOwner(this)->outCounter)++;
            rumble.header.size = 0x09;
            rumble.mode = 0x00;
            rumble.rumbleMask = 0x0F;
            rumble.length = 0xFF;
            rumble.period = 0x00;
            rumble.extra = 0x00;
//            IOLog("Data: %d %d %d %d, outCounter: %d\n", data[0], data[1], data[2], data[3], rumble.reserved2);

            rumbleType = GetOwner(this)->rumbleType;
            if (rumbleType == 0) // Default
            {
                rumble.trigL = 0x00;
                rumble.trigR = 0x00;
                rumble.little = data[2];
                rumble.big = data[3];
            }
            else if (rumbleType == 1) // None
            {
                return kIOReturnSuccess;
            }
            else if (rumbleType == 2) // Trigger
            {
                rumble.trigL = data[2] / 2.0;
                rumble.trigR = data[3] / 2.0;
                rumble.little = 0x00;
                rumble.big = 0x00;
            }
            else if (rumbleType == 3) // Both
            {
                rumble.trigL = data[2] / 2.0;
                rumble.trigR = data[3] / 2.0;
                rumble.little = data[2];
                rumble.big = data[3];
            }

            GetOwner(this)->QueueWrite(&rumble,13);
            return kIOReturnSuccess;
        case 0x01: // Unsupported LED
            return kIOReturnSuccess;
        default:
            IOLog("Unknown escape %d\n", data[0]);
            return kIOReturnUnsupported;
    }
}

*/

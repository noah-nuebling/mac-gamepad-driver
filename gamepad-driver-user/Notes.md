#  <#Title#>



## Apple Docs

- DriverKit Tutorial: https://developer.apple.com/documentation/driverkit/creating_a_driver_using_the_driverkit_sdk
- Installing Driver Tutorial: https://developer.apple.com/documentation/systemextensions/installing_system_extensions_and_drivers
- Testing & Debugging Driver Tutorial: https://developer.apple.com/documentation/driverkit/debugging_and_testing_system_extensions
- Debugging Workflow Demo: (starts at 25:00) https://developer.apple.com/videos/play/wwdc2019/702/
- Apple Game Controller Backward Compatibility: (They simulate Xbox 360 Controller) https://developer.apple.com/documentation/gamecontroller/understanding_game_controller_backward_compatibility
    - They provide and interface which sends Xbox 360 controller reports
- Tutorial on matching with devices: https://developer.apple.com/news/?id=zk5xdwbn
Demystifying code signing for DriverKit: https://developer.apple.com/news/?id=c63qcok4

## Examples

- HIDKeyboardDriver sample project: https://developer.apple.com/documentation/hiddriverkit/handling_keyboard_events_from_a_human_interface_device
- Karabiner IOUserHIDDevice subclass: https://github.com/pqrs-org/Karabiner-DriverKit-VirtualHIDDevice/blob/main/src/DriverKit/Karabiner-DriverKit-VirtualHIDDevice/org_pqrs_Karabiner_DriverKit_VirtualHIDPointing.iig

## Other

- IOUserHIDDevice: https://developer.apple.com/documentation/hiddriverkit/iouserhiddevice
- 360Controller impl: https://github.com/360Controller/360Controller/blob/master/360Controller/Controller.cpp
- Gamepad Tester: https://hardwaretester.com/gamepad
- Make sure you add the capabilites (aka entitlements?) for the driver to the app identifier under https://developer.apple.com/account/resources/identifiers/
- Microsoft explanation of USB Device Layout (Configuration > Interfaces > Endpoints > Pipes): https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-endpoints-and-their-pipes
- Explanation of Xbox 360 wired controller usb data: https://www.partsnotincluded.com/understanding-the-xbox-360-wired-controllers-usb-data/

## Debugging

To see logs use Terminal command like

```
log stream --predicate 'process="kernel"' 
```

or command like

```
log stream --predicate 'sender="com.nuebling.gamepad-driver-app.gamepad-driver-user.dext"'
```

For example of how to attach lldb see the Debugging Workflow Demo (mentioned above)

## Questions & Issues

- Should the IOProviderClass specified in the personalities be IOUSBHostDevice or IOUSBDevice? 360Controller uses IOUSBDevice. In the registry we find IOUSBHostDevice instances which inherit from IOUSBDevice. In IOKit docs there is IOUSBHostDevice which inherits from IOUSBHostDevice, but in the DriverKit docs, there is IOUSBHostDevice, and it inherits from IOService. And there is no IOUSBDevice. So I think we should use IOUSBHostDevice.
- The kernel fails to load the driver with error "Unsatisfied Entitlements: com.apple.developer.driverkit.transport.usb". Here's a thread on this issue: https://developer.apple.com/forums/thread/666632 

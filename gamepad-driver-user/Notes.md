#  <#Title#>



## Apple Docs

- DriverKit Tutorial: https://developer.apple.com/documentation/driverkit/creating_a_driver_using_the_driverkit_sdk
- Installing Driver Tutorial: https://developer.apple.com/documentation/systemextensions/installing_system_extensions_and_drivers
- Testing & Debugging Driver Tutorial: https://developer.apple.com/documentation/driverkit/debugging_and_testing_system_extensions
- Debugging Workflow Demo: (starts at 25:00) https://developer.apple.com/videos/play/wwdc2019/702/
- Apple Game Controller Backward Compatibility: (They simulate Xbox 360 Controller) https://developer.apple.com/documentation/gamecontroller/understanding_game_controller_backward_compatibility
    - They provide and interface which sends Xbox 360 controller reports

## Examples

- HIDKeyboardDriver sample project: https://developer.apple.com/documentation/hiddriverkit/handling_keyboard_events_from_a_human_interface_device
- Karabiner IOUserHIDDevice subclass: https://github.com/pqrs-org/Karabiner-DriverKit-VirtualHIDDevice/blob/main/src/DriverKit/Karabiner-DriverKit-VirtualHIDDevice/org_pqrs_Karabiner_DriverKit_VirtualHIDPointing.iig

## Other

- IOUserHIDDevice: https://developer.apple.com/documentation/hiddriverkit/iouserhiddevice
- 360Controller impl: https://github.com/360Controller/360Controller/blob/master/360Controller/Controller.cpp
- Gamepad Tester: https://hardwaretester.com/gamepad

## Debugging

To see logs use Terminal command like

```
log stream --predicate 'process="kernel"' 
```

or command like

```
log stream --predicate 'sender="[your driver bundle id].dext"'
```

For example of how to attach lldb see the Debugging Workflow Demo (mentioned above)

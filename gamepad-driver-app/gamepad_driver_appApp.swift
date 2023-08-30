//
//  gamepad_driver_appApp.swift
//  gamepad-driver-app
//
//  Created by Noah Nübling on 30.08.23.
//

import SwiftUI

@main
struct gamepad_driver_appApp: App {
    
    init() {
        ExtensionInstaller.shared.install()
    }
    
    var body: some Scene {
        WindowGroup {
            ContentView()
        }
    }
}

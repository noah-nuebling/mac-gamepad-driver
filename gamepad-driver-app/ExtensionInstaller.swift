//
//  Code.swift
//  gamepad-driver-app
//
//  Created by Noah Nübling on 30.08.23.
//

import Foundation
import SystemExtensions

class ExtensionInstaller: NSObject, OSSystemExtensionRequestDelegate {
    
    
    static let shared = ExtensionInstaller()
    
    func install() {
        /// Install Driver Kit Extension.
        /// Src: https://developer.apple.com/documentation/systemextensions/installing_system_extensions_and_drivers
        ///
        /// Create an activation request and assign a delegate to
        /// receive reports of success or failure.
        
        let driverBundleID = "com.nuebling.gamepad-driver-app.gamepad-driver-user"
        let request = OSSystemExtensionRequest.activationRequest(forExtensionWithIdentifier: driverBundleID, queue: DispatchQueue.main)
        request.delegate = self


        /// Submit the request to the system.
        let extensionManager = OSSystemExtensionManager.shared
        extensionManager.submitRequest(request)
    }
    
    
    /// vvv System Extension Request Delegate methods
    
    func request(_ request: OSSystemExtensionRequest, actionForReplacingExtension existing: OSSystemExtensionProperties, withExtension ext: OSSystemExtensionProperties) -> OSSystemExtensionRequest.ReplacementAction {
        
        print("Request needs action since there is an existing extension. Existing: \(existing) – extension: \(ext) – request: \(request)")
        
        
        let action = OSSystemExtensionRequest.ReplacementAction.replace
        
        print("...Returning action '\(action)'")
        return action
    }
    
    func requestNeedsUserApproval(_ request: OSSystemExtensionRequest) {
        
        print("Request needs user approval. Request: \(request)")
    }
    
    func request(_ request: OSSystemExtensionRequest, didFinishWithResult result: OSSystemExtensionRequest.Result) {
        print("Request finished with a result. Request: \(request) – Result: \(result)")
    }
    
    func request(_ request: OSSystemExtensionRequest, didFailWithError error: Error) {
        print("Request failed with an error. Request: \(request) – Error: \(error)")
    }
}

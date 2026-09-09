//
//  SleeperShellPlugin.swift
//  Sleeper Agent
//
//  The small native things that make the web UI stop feeling like a web page:
//  haptics, the idle timer, hiding the status bar for blackout mode, a durable
//  mirror of the settings, and the safe-area insets.
//
//  Written as one plugin rather than pulled in as three Capacitor packages
//  (@capacitor/haptics, @capacitor/preferences, @capacitor/app). Each of those
//  would add an SPM dependency to resolve for perhaps fifteen lines of work,
//  and this way the whole native surface of the app is in three files that can
//  be read end to end.
//
//  NOT YET COMPILED — see the header of SleeperEngine.swift.
//

import Foundation
import UIKit
import Capacitor

@objc(SleeperShellPlugin)
public class SleeperShellPlugin: CAPPlugin, CAPBridgedPlugin {

    public let identifier = "SleeperShellPlugin"
    public let jsName = "SleeperShell"
    public let pluginMethods: [CAPPluginMethod] = [
        CAPPluginMethod(name: "haptic",                 returnType: CAPPluginReturnPromise),
        CAPPluginMethod(name: "setIdleTimerDisabled",   returnType: CAPPluginReturnPromise),
        CAPPluginMethod(name: "setStatusBarHidden",     returnType: CAPPluginReturnPromise),
        CAPPluginMethod(name: "setBacking",             returnType: CAPPluginReturnPromise),
        CAPPluginMethod(name: "getBacking",             returnType: CAPPluginReturnPromise),
        CAPPluginMethod(name: "getEnvironment",         returnType: CAPPluginReturnPromise),
    ]

    /// A second home for the settings. WKWebView's local storage can be
    /// evicted under storage pressure; UserDefaults is not. localStorage stays
    /// the primary store so that the app can still read its settings
    /// synchronously as it boots, and this is the copy consulted when
    /// localStorage comes back empty.
    private static let backingKey = "brokild-sleep-noise-backup"

    // MARK: - Haptics

    private let impactLight = UIImpactFeedbackGenerator(style: .light)
    private let impactMedium = UIImpactFeedbackGenerator(style: .medium)
    private let selection = UISelectionFeedbackGenerator()

    /// Used sparingly and never while a fader is moving: a buzz on every value
    /// change of a mixer slider is intolerable, and this is a bedtime app.
    @objc func haptic(_ call: CAPPluginCall) {
        let kind = call.getString("kind") ?? "selection"
        DispatchQueue.main.async {
            switch kind {
            case "light":
                self.impactLight.prepare()
                self.impactLight.impactOccurred()
            case "medium":
                self.impactMedium.prepare()
                self.impactMedium.impactOccurred()
            default:
                self.selection.prepare()
                self.selection.selectionChanged()
            }
            call.resolve()
        }
    }

    // MARK: - Screen

    /// Held only by blackout mode, where a deliberately black screen is the
    /// point. Not used to keep the screen alive for audio's sake: that is what
    /// the audio background mode is for, and holding the display awake all
    /// night to play a sound would be a battery bug.
    @objc func setIdleTimerDisabled(_ call: CAPPluginCall) {
        let disabled = call.getBool("value") ?? false
        DispatchQueue.main.async {
            UIApplication.shared.isIdleTimerDisabled = disabled
            call.resolve(["value": disabled])
        }
    }

    /// The status bar is the last lit thing on screen once the page is black,
    /// so blackout mode takes it away too.
    @objc func setStatusBarHidden(_ call: CAPPluginCall) {
        let hidden = call.getBool("value") ?? false
        DispatchQueue.main.async {
            SleeperViewController.statusBarHidden = hidden
            self.bridge?.viewController?.setNeedsStatusBarAppearanceUpdate()
            call.resolve(["value": hidden])
        }
    }

    // MARK: - Durable settings mirror

    @objc func setBacking(_ call: CAPPluginCall) {
        guard let json = call.getString("json") else {
            call.reject("setBacking needs json"); return
        }
        // Bounded so a bug in the web app cannot grow UserDefaults without
        // limit; the real payload is well under a kilobyte.
        guard json.utf8.count <= 64 * 1024 else {
            call.reject("settings payload is implausibly large"); return
        }
        UserDefaults.standard.set(json, forKey: Self.backingKey)
        call.resolve(["ok": true])
    }

    @objc func getBacking(_ call: CAPPluginCall) {
        let json = UserDefaults.standard.string(forKey: Self.backingKey)
        call.resolve(["json": json ?? "", "present": json != nil])
    }

    // MARK: - Environment

    /// Safe-area insets and device shape. The CSS already uses env(safe-area-inset-*),
    /// which is the right mechanism; this is here so the blackout screen can
    /// also know whether there is a home indicator to keep clear of, and so a
    /// QA run can record what the app actually saw.
    @objc func getEnvironment(_ call: CAPPluginCall) {
        DispatchQueue.main.async {
            let window = UIApplication.shared.connectedScenes
                .compactMap { ($0 as? UIWindowScene)?.keyWindow }
                .first
            let insets = window?.safeAreaInsets ?? .zero
            let idiom = UIDevice.current.userInterfaceIdiom

            call.resolve([
                "safeArea": [
                    "top": insets.top,
                    "bottom": insets.bottom,
                    "left": insets.left,
                    "right": insets.right,
                ],
                "idiom": idiom == .pad ? "pad" : (idiom == .phone ? "phone" : "other"),
                "systemVersion": UIDevice.current.systemVersion,
                "scale": window?.screen.scale ?? 2.0,
                "hasHomeIndicator": insets.bottom > 0,
                "appVersion": Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "",
                "buildNumber": Bundle.main.infoDictionary?["CFBundleVersion"] as? String ?? "",
            ])
        }
    }
}

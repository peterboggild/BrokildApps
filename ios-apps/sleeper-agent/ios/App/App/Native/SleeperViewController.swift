//
//  SleeperViewController.swift
//  Sleeper Agent
//
//  Capacitor's bridge view controller, with one addition: the status bar can
//  be taken away. Blackout mode is meant to leave a genuinely dark screen on
//  the bedside table, and a status bar with a white clock in it is the last
//  lit thing left once the page itself is black.
//
//  Also pins the status bar to light content, since the app is dark at all
//  times and there is no light appearance to switch to.
//
//  NOT YET COMPILED — see the header of SleeperEngine.swift.
//

import UIKit
import Capacitor

class SleeperViewController: CAPBridgeViewController {

    /// Set by SleeperShellPlugin when blackout mode is entered or left.
    static var statusBarHidden = false

    override var prefersStatusBarHidden: Bool {
        SleeperViewController.statusBarHidden
    }

    override var preferredStatusBarStyle: UIStatusBarStyle {
        .lightContent
    }

    /// The app is dark; the fade is a courtesy so the bar does not blink out.
    override var preferredStatusBarUpdateAnimation: UIStatusBarAnimation {
        .fade
    }

    /// Portrait on the phone. Rolling over in bed should not rotate the
    /// screen, and the panel is designed as a single column. The iPad is left
    /// to the Info.plist, which allows it both ways.
    override var supportedInterfaceOrientations: UIInterfaceOrientationMask {
        UIDevice.current.userInterfaceIdiom == .pad ? .all : .portrait
    }
}

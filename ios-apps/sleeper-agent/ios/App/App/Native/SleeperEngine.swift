//
//  SleeperEngine.swift
//  Sleeper Agent
//
//  The native half of the audio path.
//
//  WHY THIS EXISTS
//  ---------------
//  The web app renders every sound into finite looping buffers — five 20 s
//  noise loops and, for the ambient synth, a 23 s chord bed and a 31 s melody
//  line. Everything downstream of those buffers is simple signal flow: a gain
//  per layer, one lowpass, a slow amplitude swell, a master envelope, and a
//  stop at a deadline.
//
//  In the browser that flow is Web Audio. Inside a WKWebView it cannot be
//  trusted for an eight-hour unattended session: the web content process can
//  be suspended or throttled when the app is backgrounded even while the host
//  app holds the `audio` background mode, and an AudioContext can be left in
//  an `interrupted` state it does not always recover from. The failure mode is
//  an entire silent night, discovered in the morning.
//
//  So JavaScript still renders the buffers — they are the product, and they
//  are covered by the offline DSP suite — and this class plays them. Once
//  playback has started the WebView may be frozen for the whole night without
//  consequence: the engine, the swell, the fades and the sleep timer all live
//  here, in a process iOS keeps alive precisely because it is playing audio.
//
//  NOT YET COMPILED
//  ----------------
//  There is no Apple toolchain in the environment this was written in. This
//  file has never been through swiftc. Expect to fix compile errors on the
//  first Mac build, and treat every statement about background behaviour as a
//  design intention until a device has confirmed it. See IOS_QA_CHECKLIST.md.
//

import Foundation
import AVFoundation
import MediaPlayer

/// One tick of the control loop. Everything time-varying — the swell, both
/// fades, the sleep timer — is driven from this single timer rather than from
/// a scattering of separate ramps, because there is then exactly one place
/// where time is handled and one place for it to go wrong.
private let kTickHz: Double = 40.0

/// Layer gains and the master volume are stepped rather than ramped by the
/// audio unit, so a step must be small enough to be inaudible on noise.
/// At 40 Hz a full 0→1 move takes this many seconds at most.
private let kSmoothingSeconds: Double = 0.08

final class SleeperEngine {

    static let shared = SleeperEngine()

    // MARK: - Graph

    private let engine = AVAudioEngine()

    /// The noise bus: the five faders sum here, and the tone control, the
    /// swell and the limiter act on the sum.
    private let noiseMixer = AVAudioMixerNode()
    private let toneEQ = AVAudioUnitEQ(numberOfBands: 1)
    private var limiter: AVAudioUnitEffect?

    /// The ambient synth joins after the tone control and the swell — both of
    /// those belong to the noise — but before the master envelope, so the
    /// sleep timer still fades and stops it.
    private let padMixer = AVAudioMixerNode()

    /// Master envelope × user volume.
    private let master = AVAudioMixerNode()

    private var format: AVAudioFormat!

    // MARK: - Sources

    private struct Layer {
        let player: AVAudioPlayerNode
        let buffer: AVAudioPCMBuffer
        var target: Float      // where the fader wants to be
        var current: Float     // where it actually is, stepped towards target
        var fading: Bool       // true while being taken out
    }

    private var layers: [String: Layer] = [:]
    private var padPlayers: [AVAudioPlayerNode] = []
    private var padTarget: Float = 0
    private var padCurrent: Float = 0

    // MARK: - Control state

    private(set) var isPlaying = false

    private var userVolume: Float = 0.55        // 0…1, already curved by the caller
    private var toneHz: Float = 3000
    private var swellOn = false
    private var swellDepth: Float = 0.15        // half-depth, as in the web app
    private var swellPeriod: Double = 16
    private var swellPhase: Double = 0

    private var fadeInSeconds: Double = 0
    private var fadeOutSeconds: Double = 0
    private var envelope: Float = 0             // master fade envelope, 0…1

    /// Absolute deadlines on the monotonic clock. Wall-clock time is not used:
    /// it can jump when the phone changes time zone overnight.
    private var startedAt: Double = 0
    private var endsAt: Double = 0              // 0 means no timer
    private var totalSeconds: Double = 0

    private var tick: DispatchSourceTimer?
    private let queue = DispatchQueue(label: "com.peterboggild.sleeperagent.engine")

    /// Set by the plugin so engine events can reach JavaScript.
    var onEvent: ((String, [String: Any]) -> Void)?

    private var nowPlayingTitle = "Sleeper Agent"
    private var nowPlayingSubtitle = ""

    /// True while an iOS interruption is in force, so the tick does not fight
    /// the system for the session.
    private var interrupted = false

    private init() {
        registerForSystemNotifications()
    }

    // MARK: - Session

    /// Configured on first play rather than at launch, so opening the app
    /// never takes the audio session or interrupts someone's music.
    private func activateSession() throws {
        let session = AVAudioSession.sharedInstance()
        // .playback: this app is the thing being listened to. No .mixWithOthers
        // (it would let another app's audio sit under the noise, which is not
        // what anyone wants at 2 a.m.) and no .duckOthers.
        try session.setCategory(.playback, mode: .default, options: [])
        try session.setActive(true, options: [])
    }

    private func deactivateSession() {
        do {
            try AVAudioSession.sharedInstance().setActive(
                false, options: [.notifyOthersOnDeactivation])
        } catch {
            log("could not deactivate the audio session: \(error.localizedDescription)")
        }
    }

    // MARK: - Buffer store

    /// Where the rendered loops live. JavaScript writes them here once, via the
    /// plugin, and every later night loads them straight off disk — which is
    /// also why starting is instant after the first run.
    static var cacheDirectory: URL {
        let base = FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask)[0]
        let dir = base.appendingPathComponent("sleeper-loops", isDirectory: true)
        if !FileManager.default.fileExists(atPath: dir.path) {
            try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        }
        return dir
    }

    static func url(forLoop id: String, sampleRate: Int) -> URL {
        // The id is fixed by the app, never user input, but it lands in a file
        // path so it is filtered anyway.
        let safe = id.filter { $0.isLetter || $0.isNumber || $0 == "-" || $0 == "_" }
        return cacheDirectory.appendingPathComponent("\(safe)-\(sampleRate).wav")
    }

    static func hasLoop(_ id: String, sampleRate: Int) -> Bool {
        FileManager.default.fileExists(atPath: url(forLoop: id, sampleRate: sampleRate).path)
    }

    /// The hardware rate, so JavaScript can render at it and nothing has to be
    /// resampled on the way in.
    static var hardwareSampleRate: Int {
        let r = AVAudioSession.sharedInstance().sampleRate
        return r > 0 ? Int(r.rounded()) : 48000
    }

    private func loadBuffer(id: String, sampleRate: Int) -> AVAudioPCMBuffer? {
        let url = Self.url(forLoop: id, sampleRate: sampleRate)
        guard let file = try? AVAudioFile(forReading: url) else {
            log("no cached loop for \(id) at \(sampleRate) Hz")
            return nil
        }
        let frames = AVAudioFrameCount(file.length)
        guard frames > 0,
              let buffer = AVAudioPCMBuffer(pcmFormat: file.processingFormat, frameCapacity: frames)
        else { return nil }
        do {
            try file.read(into: buffer)
            return buffer
        } catch {
            log("could not read \(id): \(error.localizedDescription)")
            return nil
        }
    }

    // MARK: - Building the graph

    private func buildGraph(sampleRate: Int) {
        format = AVAudioFormat(standardFormatWithSampleRate: Double(sampleRate), channels: 2)

        engine.attach(noiseMixer)
        engine.attach(toneEQ)
        engine.attach(padMixer)
        engine.attach(master)

        // One lowpass, matching the web app's tone control.
        let band = toneEQ.bands[0]
        band.filterType = .lowPass
        band.frequency = toneHz
        band.bypass = false
        toneEQ.globalGain = 0

        engine.connect(noiseMixer, to: toneEQ, format: format)

        // A peak limiter across the noise bus. Five faders open at once can sum
        // to more than the output can hold; one fader on its own never reaches
        // it. If the component is unavailable the graph is built without it
        // rather than failing to start — see the QA checklist for the
        // all-faders-at-maximum listening test that covers this case.
        if let lim = makeLimiter() {
            engine.attach(lim)
            engine.connect(toneEQ, to: lim, format: format)
            engine.connect(lim, to: master, format: format)
            limiter = lim
        } else {
            log("peak limiter unavailable; running without it")
            engine.connect(toneEQ, to: master, format: format)
        }

        engine.connect(padMixer, to: master, format: format)
        engine.connect(master, to: engine.mainMixerNode, format: format)

        master.outputVolume = 0          // the fade-in brings this up
        noiseMixer.outputVolume = 1
        padMixer.outputVolume = 1
    }

    private func makeLimiter() -> AVAudioUnitEffect? {
        var desc = AudioComponentDescription()
        desc.componentType = kAudioUnitType_Effect
        desc.componentSubType = kAudioUnitSubType_PeakLimiter
        desc.componentManufacturer = kAudioUnitManufacturer_Apple
        desc.componentFlags = 0
        desc.componentFlagsMask = 0
        guard AudioComponentFindNext(nil, &desc) != nil else { return nil }
        return AVAudioUnitEffect(audioComponentDescription: desc)
    }

    private func teardownGraph() {
        for (_, layer) in layers {
            layer.player.stop()
            engine.detach(layer.player)
        }
        layers.removeAll()
        for p in padPlayers {
            p.stop()
            engine.detach(p)
        }
        padPlayers.removeAll()

        if engine.isRunning { engine.stop() }
        engine.detach(noiseMixer)
        engine.detach(toneEQ)
        engine.detach(padMixer)
        engine.detach(master)
        if let lim = limiter { engine.detach(lim); limiter = nil }
    }

    // MARK: - Transport

    struct LayerRequest {
        let id: String
        let level: Float
    }

    struct StartRequest {
        let sampleRate: Int
        let layers: [LayerRequest]
        let padIDs: [String]        // [] for none; otherwise the bed and melody loops
        let padLevel: Float
        let volume: Float
        let toneHz: Float
        let swellOn: Bool
        let swellDepth: Float
        let swellPeriod: Double
        let fadeInSeconds: Double
        let fadeOutSeconds: Double
        /// 0 for an unlimited session.
        let durationSeconds: Double
        let title: String
        let subtitle: String
    }

    /// Errors are returned rather than thrown past the plugin boundary so the
    /// JavaScript side can show the user something specific.
    enum StartError: LocalizedError {
        case sessionFailed(String)
        case engineFailed(String)
        case noLoops

        var errorDescription: String? {
            switch self {
            case .sessionFailed(let m): return "The audio session could not be started: \(m)"
            case .engineFailed(let m):  return "The audio engine could not be started: \(m)"
            case .noLoops:              return "No rendered loops were available to play."
            }
        }
    }

    func start(_ req: StartRequest) throws {
        try queue.sync {
            if isPlaying { stopLocked(fadeSeconds: 0, notify: false) }

            do { try activateSession() }
            catch { throw StartError.sessionFailed(error.localizedDescription) }

            userVolume = clamp(req.volume, 0, 1)
            toneHz = clamp(req.toneHz, 60, 20000)
            swellOn = req.swellOn
            swellDepth = clamp(req.swellDepth, 0, 0.45)
            swellPeriod = max(4, req.swellPeriod)
            swellPhase = 0
            fadeInSeconds = max(0, req.fadeInSeconds)
            fadeOutSeconds = max(0, req.fadeOutSeconds)
            padTarget = clamp(req.padLevel, 0, 2)
            padCurrent = 0
            nowPlayingTitle = req.title
            nowPlayingSubtitle = req.subtitle

            buildGraph(sampleRate: req.sampleRate)

            // Noise faders.
            for l in req.layers where l.level > 0 {
                guard let buffer = loadBuffer(id: l.id, sampleRate: req.sampleRate) else { continue }
                let player = AVAudioPlayerNode()
                engine.attach(player)
                engine.connect(player, to: noiseMixer, format: buffer.format)
                player.volume = 0            // the fade-in brings it up
                layers[l.id] = Layer(player: player, buffer: buffer,
                                     target: clamp(l.level, 0, 2), current: 0, fading: false)
            }

            // The ambient synth: two co-prime loops started together.
            if req.padLevel > 0 {
                for id in req.padIDs {
                    guard let buffer = loadBuffer(id: id, sampleRate: req.sampleRate) else { continue }
                    let player = AVAudioPlayerNode()
                    engine.attach(player)
                    engine.connect(player, to: padMixer, format: buffer.format)
                    player.volume = 1
                    padPlayers.append(player)
                    player.scheduleBuffer(buffer, at: nil, options: .loops, completionHandler: nil)
                }
            }

            // An empty mix is legal — every fader down with the theme off — and
            // the timer still has to run, so this is not an error. But if the
            // caller asked for layers and none of them loaded, something is
            // wrong and it should say so.
            if layers.isEmpty && padPlayers.isEmpty && !req.layers.isEmpty {
                teardownGraph()
                throw StartError.noLoops
            }

            engine.prepare()
            do { try engine.start() }
            catch {
                teardownGraph()
                throw StartError.engineFailed(error.localizedDescription)
            }

            for (_, layer) in layers {
                layer.player.scheduleBuffer(layer.buffer, at: nil, options: .loops, completionHandler: nil)
                layer.player.play()
            }
            for p in padPlayers { p.play() }

            envelope = fadeInSeconds > 0 ? 0 : 1
            master.outputVolume = envelope * userVolume
            padMixer.outputVolume = padCurrent

            startedAt = now()
            totalSeconds = req.durationSeconds
            endsAt = req.durationSeconds > 0 ? startedAt + req.durationSeconds : 0
            isPlaying = true
            interrupted = false

            startTick()
            setupRemoteCommands()
            updateNowPlaying()
        }
    }

    func stop(fadeSeconds: Double) {
        queue.sync { stopLocked(fadeSeconds: fadeSeconds, notify: false) }
    }

    /// The sleep timer reaching its end. Distinguished from a manual stop so
    /// JavaScript can show "Timer finished — sleep well" and release the
    /// system media slot.
    private func finish() {
        stopLocked(fadeSeconds: 0, notify: false)
        emit("finished", [:])
    }

    private func stopLocked(fadeSeconds: Double, notify: Bool) {
        guard isPlaying || engine.isRunning else { return }

        // A short fade even on a "hard" stop: cutting a 0.18 RMS noise bed dead
        // is an audible click, and this app is used with the lights off.
        let fade = max(fadeSeconds, 0.08)
        let steps = Int(fade * kTickHz)
        if steps > 1 {
            let from = envelope
            for i in 1...steps {
                envelope = from * (1 - Float(i) / Float(steps))
                master.outputVolume = envelope * userVolume
                Thread.sleep(forTimeInterval: 1.0 / kTickHz)
            }
        }
        envelope = 0
        master.outputVolume = 0

        stopTick()
        teardownGraph()
        isPlaying = false
        endsAt = 0
        clearNowPlaying()
        deactivateSession()
        if notify { emit("stopped", [:]) }
    }

    // MARK: - Live parameter changes

    /// A fader moved. Crossing zero starts or retires the layer; anything else
    /// just moves its gain, which the tick smooths.
    func setLayer(id: String, level: Float, sampleRate: Int) {
        queue.sync {
            guard isPlaying else { return }
            let want = clamp(level, 0, 2)

            if want <= 0 {
                if var layer = layers[id] {
                    layer.target = 0
                    layer.fading = true
                    layers[id] = layer
                }
                return
            }

            if var layer = layers[id] {
                layer.target = want
                layer.fading = false
                layers[id] = layer
                return
            }

            // Coming up off zero: attach and start it under a fade so it
            // arrives rather than appearing.
            guard let buffer = loadBuffer(id: id, sampleRate: sampleRate) else {
                log("cannot raise \(id): no cached loop")
                return
            }
            let player = AVAudioPlayerNode()
            engine.attach(player)
            engine.connect(player, to: noiseMixer, format: buffer.format)
            player.volume = 0
            player.scheduleBuffer(buffer, at: nil, options: .loops, completionHandler: nil)
            if engine.isRunning { player.play() }
            layers[id] = Layer(player: player, buffer: buffer, target: want, current: 0, fading: false)
        }
    }

    func setPadLevel(_ level: Float) {
        queue.sync { padTarget = clamp(level, 0, 2) }
    }

    func setVolume(_ v: Float) {
        queue.sync {
            userVolume = clamp(v, 0, 1)
            master.outputVolume = envelope * userVolume
        }
    }

    func setTone(hz: Float) {
        queue.sync {
            toneHz = clamp(hz, 60, 20000)
            if engine.isRunning { toneEQ.bands[0].frequency = toneHz }
        }
    }

    func setSwell(on: Bool, depth: Float, period: Double) {
        queue.sync {
            swellOn = on
            swellDepth = clamp(depth, 0, 0.45)
            swellPeriod = max(4, period)
            if !on { noiseMixer.outputVolume = 1 }
        }
    }

    func setNowPlaying(title: String, subtitle: String) {
        queue.sync {
            nowPlayingTitle = title
            nowPlayingSubtitle = subtitle
            if isPlaying { updateNowPlaying() }
        }
    }

    /// Remaining milliseconds, for the countdown JavaScript paints. Read
    /// rather than pushed, so a frozen WebView simply catches up when it wakes.
    func state() -> [String: Any] {
        queue.sync {
            let remaining = endsAt > 0 ? max(0, endsAt - now()) : 0
            return [
                "playing": isPlaying,
                "remainingMs": Int(remaining * 1000),
                "unlimited": endsAt == 0,
                "interrupted": interrupted,
                "openLayers": Array(layers.keys),
            ]
        }
    }

    // MARK: - The control loop

    private func startTick() {
        stopTick()
        let t = DispatchSource.makeTimerSource(queue: queue)
        t.schedule(deadline: .now(), repeating: 1.0 / kTickHz, leeway: .milliseconds(10))
        t.setEventHandler { [weak self] in self?.onTick() }
        tick = t
        t.resume()
    }

    private func stopTick() {
        tick?.cancel()
        tick = nil
    }

    private func onTick() {
        guard isPlaying, !interrupted else { return }
        let t = now()
        let dt = 1.0 / kTickHz

        // --- master envelope: fade in, then hold, then fade out before the end
        if endsAt > 0 {
            let remaining = endsAt - t
            if remaining <= 0 {
                finish()
                return
            }
            if fadeOutSeconds > 0 && remaining <= fadeOutSeconds {
                envelope = Float(remaining / fadeOutSeconds)
            } else if fadeInSeconds > 0 {
                envelope = min(1, Float((t - startedAt) / fadeInSeconds))
            } else {
                envelope = 1
            }
        } else if fadeInSeconds > 0 {
            envelope = min(1, Float((t - startedAt) / fadeInSeconds))
        } else {
            envelope = 1
        }
        master.outputVolume = envelope * userVolume

        // --- swell: a slow rise and fall over the whole noise bus. Half-depth,
        // so the gain swings between 1-2d and 1 exactly as in the web app.
        if swellOn {
            swellPhase += dt / swellPeriod
            if swellPhase > 1 { swellPhase -= 1 }
            let s = Float(sin(2 * Double.pi * swellPhase))
            noiseMixer.outputVolume = (1 - swellDepth) + swellDepth * s
        }

        // --- fader gains, stepped towards their targets
        let step = Float(dt / kSmoothingSeconds)
        var retired: [String] = []
        for (id, var layer) in layers {
            if abs(layer.current - layer.target) > 0.0005 {
                let d = layer.target - layer.current
                layer.current += max(-step, min(step, d))
                layer.player.volume = layer.current
                layers[id] = layer
            } else if layer.current != layer.target {
                layer.current = layer.target
                layer.player.volume = layer.current
                layers[id] = layer
            }
            if layer.fading && layer.current <= 0.0005 { retired.append(id) }
        }
        for id in retired {
            if let layer = layers.removeValue(forKey: id) {
                layer.player.stop()
                engine.detach(layer.player)
            }
        }

        if abs(padCurrent - padTarget) > 0.0005 {
            let d = padTarget - padCurrent
            padCurrent += max(-step, min(step, d))
            padMixer.outputVolume = padCurrent
        }

        // The lock screen only needs refreshing about once a second.
        if Int((t - startedAt) * kTickHz) % Int(kTickHz) == 0 { updateNowPlaying() }
    }

    // MARK: - Now Playing and remote commands

    private func updateNowPlaying() {
        var info: [String: Any] = [
            MPMediaItemPropertyTitle: nowPlayingTitle,
            MPMediaItemPropertyArtist: nowPlayingSubtitle.isEmpty ? "Sleeper Agent" : nowPlayingSubtitle,
            // Marked live so iOS shows no scrubber: there is nothing to seek
            // in a generated noise bed, and a draggable bar that does nothing
            // reads as a broken control.
            MPNowPlayingInfoPropertyIsLiveStream: true,
            MPNowPlayingInfoPropertyPlaybackRate: isPlaying ? 1.0 : 0.0,
        ]
        if endsAt > 0 {
            info[MPNowPlayingInfoPropertyElapsedPlaybackTime] = now() - startedAt
        }
        MPNowPlayingInfoCenter.default().nowPlayingInfo = info
        MPNowPlayingInfoCenter.default().playbackState = isPlaying ? .playing : .paused
    }

    private func clearNowPlaying() {
        MPNowPlayingInfoCenter.default().nowPlayingInfo = nil
        MPNowPlayingInfoCenter.default().playbackState = .stopped
    }

    private var remoteCommandsWired = false

    private func setupRemoteCommands() {
        guard !remoteCommandsWired else { return }
        remoteCommandsWired = true
        let c = MPRemoteCommandCenter.shared()

        c.playCommand.addTarget { [weak self] _ in
            self?.emit("remotePlay", [:])
            return .success
        }
        c.pauseCommand.addTarget { [weak self] _ in
            self?.emit("remotePause", [:])
            return .success
        }
        c.togglePlayPauseCommand.addTarget { [weak self] _ in
            self?.emit("remoteToggle", [:])
            return .success
        }
        c.playCommand.isEnabled = true
        c.pauseCommand.isEnabled = true
        c.togglePlayPauseCommand.isEnabled = true

        // Everything else is explicitly off. A lock screen offering
        // next/previous track for a noise generator is a control that does
        // nothing, which is worse than no control.
        for unwanted: MPRemoteCommand in [
            c.nextTrackCommand, c.previousTrackCommand,
            c.seekForwardCommand, c.seekBackwardCommand,
            c.skipForwardCommand, c.skipBackwardCommand,
            c.changePlaybackPositionCommand, c.changeRepeatModeCommand,
            c.changeShuffleModeCommand, c.ratingCommand, c.likeCommand,
            c.dislikeCommand, c.bookmarkCommand,
        ] {
            unwanted.isEnabled = false
        }
    }

    // MARK: - System events

    private func registerForSystemNotifications() {
        let nc = NotificationCenter.default

        nc.addObserver(self, selector: #selector(handleInterruption(_:)),
                       name: AVAudioSession.interruptionNotification, object: nil)
        nc.addObserver(self, selector: #selector(handleRouteChange(_:)),
                       name: AVAudioSession.routeChangeNotification, object: nil)
        nc.addObserver(self, selector: #selector(handleMediaServicesReset(_:)),
                       name: AVAudioSession.mediaServicesWereResetNotification, object: nil)
        nc.addObserver(self, selector: #selector(handleEngineConfigurationChange(_:)),
                       name: .AVAudioEngineConfigurationChange, object: nil)
    }

    /// A call, Siri, an alarm. iOS stops the audio for us; the question is only
    /// what happens afterwards.
    ///
    /// Deliberately conservative: playback resumes only when iOS itself says
    /// the interruption ended with `shouldResume`. Sound restarting on its own
    /// in the small hours is worse than sound not restarting, and the app is
    /// one tap away on the lock screen either way.
    @objc private func handleInterruption(_ note: Notification) {
        guard let info = note.userInfo,
              let raw = info[AVAudioSessionInterruptionTypeKey] as? UInt,
              let type = AVAudioSession.InterruptionType(rawValue: raw)
        else { return }

        switch type {
        case .began:
            queue.sync {
                interrupted = true
                for (_, l) in layers { l.player.pause() }
                for p in padPlayers { p.pause() }
            }
            emit("interrupted", ["resumable": false])

        case .ended:
            let opts = AVAudioSession.InterruptionOptions(
                rawValue: info[AVAudioSessionInterruptionOptionKey] as? UInt ?? 0)
            let shouldResume = opts.contains(.shouldResume)
            queue.sync {
                interrupted = false
                guard isPlaying else { return }
                if shouldResume {
                    do {
                        try AVAudioSession.sharedInstance().setActive(true)
                        if !engine.isRunning { try engine.start() }
                        for (_, l) in layers { l.player.play() }
                        for p in padPlayers { p.play() }
                    } catch {
                        log("could not resume after interruption: \(error.localizedDescription)")
                    }
                } else {
                    // Left paused on purpose. The timer is not advanced while
                    // paused, so the night is not silently shortened.
                    startedAt = now() - (totalSeconds - (endsAt > 0 ? endsAt - now() : 0))
                }
            }
            emit("interruptionEnded", ["resumed": shouldResume])

        @unknown default:
            break
        }
    }

    /// AirPods pulled out, headphones unplugged, Bluetooth switched away.
    /// iOS pauses for `.oldDeviceUnavailable`, which is the behaviour anyone
    /// expects; the engine is simply told so the UI can agree with it.
    @objc private func handleRouteChange(_ note: Notification) {
        guard let raw = note.userInfo?[AVAudioSessionRouteChangeReasonKey] as? UInt,
              let reason = AVAudioSession.RouteChangeReason(rawValue: raw)
        else { return }

        switch reason {
        case .oldDeviceUnavailable:
            queue.sync {
                for (_, l) in layers { l.player.pause() }
                for p in padPlayers { p.pause() }
                interrupted = true
            }
            emit("routeLost", [:])

        case .newDeviceAvailable, .categoryChange, .override:
            emit("routeChanged", [:])

        default:
            break
        }
    }

    /// The audio server restarted. Everything is invalid and has to be rebuilt
    /// from scratch; a long overnight session is exactly the window in which
    /// this happens, and not handling it is a silent night.
    @objc private func handleMediaServicesReset(_ note: Notification) {
        queue.sync {
            guard isPlaying else { return }
            log("media services were reset — rebuilding")
            stopTick()
            teardownGraph()
            isPlaying = false
        }
        emit("needsRestart", ["reason": "mediaServicesReset"])
    }

    /// The engine's own configuration changed under it, usually because the
    /// route did. Players have to be restarted or the output is silence.
    @objc private func handleEngineConfigurationChange(_ note: Notification) {
        queue.sync {
            guard isPlaying, !interrupted else { return }
            do {
                if !engine.isRunning { try engine.start() }
                for (_, l) in layers where !l.player.isPlaying { l.player.play() }
                for p in padPlayers where !p.isPlaying { p.play() }
            } catch {
                log("could not restart after a configuration change: \(error.localizedDescription)")
                emit("needsRestart", ["reason": "configurationChange"])
            }
        }
    }

    // MARK: - Helpers

    /// The monotonic clock. Wall time is avoided on purpose: a phone that
    /// changes time zone or corrects its clock mid-session must not shorten or
    /// extend the night.
    private func now() -> Double { CACurrentMediaTime() }

    private func clamp(_ v: Float, _ lo: Float, _ hi: Float) -> Float { min(hi, max(lo, v)) }

    private func emit(_ name: String, _ data: [String: Any]) {
        onEvent?(name, data)
    }

    private func log(_ s: String) {
        #if DEBUG
        NSLog("[SleeperEngine] %@", s)
        #endif
    }
}

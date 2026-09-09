//
//  SleeperAudioPlugin.swift
//  Sleeper Agent
//
//  The bridge between the web UI and SleeperEngine.
//
//  Rendered loops cross the bridge once, in chunks, and are cached on disk as
//  16-bit WAV. Sixteen bits is ample: the material is noise, which dithers
//  itself, normalised to 0.18 RMS with a 0.9 peak ceiling. Chunking keeps a
//  20-second stereo loop — about 3.8 MB, 5.1 MB once base64'd — from having to
//  exist as one string in the WebView and one message on the bridge.
//
//  After the first run nothing crosses the bridge at all: the files are still
//  in Caches and the engine loads them directly, which is also why starting is
//  instant on every later night.
//
//  NOT YET COMPILED — see the header of SleeperEngine.swift.
//

import Foundation
import Capacitor
import AVFoundation

@objc(SleeperAudioPlugin)
public class SleeperAudioPlugin: CAPPlugin, CAPBridgedPlugin {

    public let identifier = "SleeperAudioPlugin"
    public let jsName = "SleeperAudio"
    public let pluginMethods: [CAPPluginMethod] = [
        CAPPluginMethod(name: "getInfo",       returnType: CAPPluginReturnPromise),
        CAPPluginMethod(name: "prepareBegin",  returnType: CAPPluginReturnPromise),
        CAPPluginMethod(name: "prepareChunk",  returnType: CAPPluginReturnPromise),
        CAPPluginMethod(name: "prepareEnd",    returnType: CAPPluginReturnPromise),
        CAPPluginMethod(name: "start",         returnType: CAPPluginReturnPromise),
        CAPPluginMethod(name: "stop",          returnType: CAPPluginReturnPromise),
        CAPPluginMethod(name: "setLayer",      returnType: CAPPluginReturnPromise),
        CAPPluginMethod(name: "setPadLevel",   returnType: CAPPluginReturnPromise),
        CAPPluginMethod(name: "setVolume",     returnType: CAPPluginReturnPromise),
        CAPPluginMethod(name: "setTone",       returnType: CAPPluginReturnPromise),
        CAPPluginMethod(name: "setSwell",      returnType: CAPPluginReturnPromise),
        CAPPluginMethod(name: "setNowPlaying", returnType: CAPPluginReturnPromise),
        CAPPluginMethod(name: "getState",      returnType: CAPPluginReturnPromise),
        CAPPluginMethod(name: "clearCache",    returnType: CAPPluginReturnPromise),
    ]

    /// Loops being written, keyed by id. A partial write lands on a `.part`
    /// file and is only renamed into place when it is complete, so an
    /// interrupted transfer can never leave a truncated loop in the cache to
    /// be loaded on some later night.
    private final class Transfer {
        let url: URL
        let partURL: URL
        let handle: FileHandle
        let channels: Int
        let sampleRate: Int
        var bytes: Int = 0

        init(url: URL, partURL: URL, handle: FileHandle, channels: Int, sampleRate: Int) {
            self.url = url
            self.partURL = partURL
            self.handle = handle
            self.channels = channels
            self.sampleRate = sampleRate
        }
    }

    private var transfers: [String: Transfer] = [:]
    private let lock = NSLock()

    override public func load() {
        SleeperEngine.shared.onEvent = { [weak self] name, data in
            self?.notifyListeners(name, data: data)
        }
    }

    // MARK: - Discovery

    /// Tells JavaScript the hardware sample rate to render at, and which loops
    /// are already on disk so it can skip generating them.
    @objc func getInfo(_ call: CAPPluginCall) {
        let sampleRate = SleeperEngine.hardwareSampleRate
        let ids = call.getArray("ids", String.self) ?? []
        let cached = ids.filter { SleeperEngine.hasLoop($0, sampleRate: sampleRate) }
        call.resolve([
            "sampleRate": sampleRate,
            "cached": cached,
            "native": true,
        ])
    }

    // MARK: - Loop transfer

    @objc func prepareBegin(_ call: CAPPluginCall) {
        guard let id = call.getString("id"), !id.isEmpty else {
            call.reject("prepareBegin needs an id"); return
        }
        let sampleRate = call.getInt("sampleRate") ?? SleeperEngine.hardwareSampleRate
        let channels = call.getInt("channels") ?? 2

        guard channels == 1 || channels == 2 else {
            call.reject("channels must be 1 or 2"); return
        }
        guard sampleRate >= 8000 && sampleRate <= 192000 else {
            call.reject("implausible sample rate \(sampleRate)"); return
        }

        let url = SleeperEngine.url(forLoop: id, sampleRate: sampleRate)
        let partURL = url.appendingPathExtension("part")

        lock.lock()
        defer { lock.unlock() }

        // Abandon any earlier attempt at this id.
        if let old = transfers.removeValue(forKey: id) {
            try? old.handle.close()
            try? FileManager.default.removeItem(at: old.partURL)
        }

        let header = Self.wavHeader(channels: channels, sampleRate: sampleRate, dataBytes: 0)
        do {
            try? FileManager.default.removeItem(at: partURL)
            try header.write(to: partURL, options: .atomic)
            let handle = try FileHandle(forWritingTo: partURL)
            handle.seekToEndOfFile()
            transfers[id] = Transfer(url: url, partURL: partURL, handle: handle,
                                     channels: channels, sampleRate: sampleRate)
            call.resolve(["ok": true])
        } catch {
            call.reject("could not open the loop cache for writing: \(error.localizedDescription)")
        }
    }

    @objc func prepareChunk(_ call: CAPPluginCall) {
        guard let id = call.getString("id"),
              let b64 = call.getString("data")
        else { call.reject("prepareChunk needs id and data"); return }

        guard let data = Data(base64Encoded: b64) else {
            call.reject("chunk was not valid base64"); return
        }

        lock.lock()
        defer { lock.unlock() }
        guard let t = transfers[id] else {
            call.reject("no transfer in progress for \(id)"); return
        }
        do {
            try t.handle.write(contentsOf: data)
            t.bytes += data.count
            call.resolve(["bytes": t.bytes])
        } catch {
            call.reject("could not write the loop chunk: \(error.localizedDescription)")
        }
    }

    @objc func prepareEnd(_ call: CAPPluginCall) {
        guard let id = call.getString("id") else {
            call.reject("prepareEnd needs an id"); return
        }

        lock.lock()
        defer { lock.unlock() }
        guard let t = transfers.removeValue(forKey: id) else {
            call.reject("no transfer in progress for \(id)"); return
        }

        do {
            try t.handle.close()

            // Patch the two length fields now that the payload size is known.
            let patch = try FileHandle(forUpdating: t.partURL)
            try patch.seek(toOffset: 4)
            try patch.write(contentsOf: Self.le32(UInt32(36 + t.bytes)))   // RIFF size
            try patch.seek(toOffset: 40)
            try patch.write(contentsOf: Self.le32(UInt32(t.bytes)))        // data size
            try patch.close()

            // Prove it is loadable before it is allowed into the cache. A loop
            // that fails to open at 3 a.m. is not something to discover then.
            guard let probe = try? AVAudioFile(forReading: t.partURL), probe.length > 0 else {
                try? FileManager.default.removeItem(at: t.partURL)
                call.reject("the written loop could not be read back")
                return
            }

            try? FileManager.default.removeItem(at: t.url)
            try FileManager.default.moveItem(at: t.partURL, to: t.url)
            call.resolve(["ok": true, "bytes": t.bytes, "frames": probe.length])
        } catch {
            try? FileManager.default.removeItem(at: t.partURL)
            call.reject("could not finish the loop: \(error.localizedDescription)")
        }
    }

    @objc func clearCache(_ call: CAPPluginCall) {
        let dir = SleeperEngine.cacheDirectory
        let files = (try? FileManager.default.contentsOfDirectory(at: dir, includingPropertiesForKeys: nil)) ?? []
        for f in files { try? FileManager.default.removeItem(at: f) }
        call.resolve(["removed": files.count])
    }

    // MARK: - Transport

    @objc func start(_ call: CAPPluginCall) {
        let sampleRate = call.getInt("sampleRate") ?? SleeperEngine.hardwareSampleRate

        var layers: [SleeperEngine.LayerRequest] = []
        for entry in call.getArray("layers", JSObject.self) ?? [] {
            guard let id = entry["id"] as? String else { continue }
            let level = Float(truncating: (entry["level"] as? NSNumber) ?? 0)
            layers.append(.init(id: id, level: level))
        }

        let req = SleeperEngine.StartRequest(
            sampleRate: sampleRate,
            layers: layers,
            padIDs: call.getArray("padIds", String.self) ?? [],
            padLevel: floatValue(call, "padLevel", 0),
            volume: floatValue(call, "volume", 0.3),
            toneHz: floatValue(call, "toneHz", 3000),
            swellOn: call.getBool("swellOn") ?? false,
            swellDepth: floatValue(call, "swellDepth", 0),
            swellPeriod: call.getDouble("swellPeriod") ?? 16,
            fadeInSeconds: call.getDouble("fadeInSeconds") ?? 0,
            fadeOutSeconds: call.getDouble("fadeOutSeconds") ?? 0,
            durationSeconds: call.getDouble("durationSeconds") ?? 0,
            title: call.getString("title") ?? "Sleeper Agent",
            subtitle: call.getString("subtitle") ?? ""
        )

        do {
            try SleeperEngine.shared.start(req)
            call.resolve(SleeperEngine.shared.state())
        } catch {
            call.reject(error.localizedDescription)
        }
    }

    @objc func stop(_ call: CAPPluginCall) {
        SleeperEngine.shared.stop(fadeSeconds: call.getDouble("fadeSeconds") ?? 0.4)
        call.resolve(["ok": true])
    }

    @objc func setLayer(_ call: CAPPluginCall) {
        guard let id = call.getString("id") else { call.reject("setLayer needs an id"); return }
        SleeperEngine.shared.setLayer(
            id: id,
            level: floatValue(call, "level", 0),
            sampleRate: call.getInt("sampleRate") ?? SleeperEngine.hardwareSampleRate)
        call.resolve()
    }

    @objc func setPadLevel(_ call: CAPPluginCall) {
        SleeperEngine.shared.setPadLevel(floatValue(call, "level", 0))
        call.resolve()
    }

    @objc func setVolume(_ call: CAPPluginCall) {
        SleeperEngine.shared.setVolume(floatValue(call, "value", 0.3))
        call.resolve()
    }

    @objc func setTone(_ call: CAPPluginCall) {
        SleeperEngine.shared.setTone(hz: floatValue(call, "hz", 3000))
        call.resolve()
    }

    @objc func setSwell(_ call: CAPPluginCall) {
        SleeperEngine.shared.setSwell(
            on: call.getBool("on") ?? false,
            depth: floatValue(call, "depth", 0),
            period: call.getDouble("period") ?? 16)
        call.resolve()
    }

    @objc func setNowPlaying(_ call: CAPPluginCall) {
        SleeperEngine.shared.setNowPlaying(
            title: call.getString("title") ?? "Sleeper Agent",
            subtitle: call.getString("subtitle") ?? "")
        call.resolve()
    }

    @objc func getState(_ call: CAPPluginCall) {
        call.resolve(SleeperEngine.shared.state())
    }

    // MARK: - Helpers

    private func floatValue(_ call: CAPPluginCall, _ key: String, _ fallback: Float) -> Float {
        if let d = call.getDouble(key) { return Float(d) }
        if let i = call.getInt(key) { return Float(i) }
        return fallback
    }

    private static func le16(_ v: UInt16) -> Data {
        Data([UInt8(v & 0xff), UInt8((v >> 8) & 0xff)])
    }

    private static func le32(_ v: UInt32) -> Data {
        Data([UInt8(v & 0xff), UInt8((v >> 8) & 0xff),
              UInt8((v >> 16) & 0xff), UInt8((v >> 24) & 0xff)])
    }

    /// A canonical 44-byte PCM WAV header. The two size fields are patched in
    /// prepareEnd once the payload length is known.
    private static func wavHeader(channels: Int, sampleRate: Int, dataBytes: Int) -> Data {
        let bitsPerSample = 16
        let blockAlign = channels * bitsPerSample / 8
        let byteRate = sampleRate * blockAlign

        var d = Data()
        d.append("RIFF".data(using: .ascii)!)
        d.append(le32(UInt32(36 + dataBytes)))
        d.append("WAVE".data(using: .ascii)!)
        d.append("fmt ".data(using: .ascii)!)
        d.append(le32(16))                              // fmt chunk size
        d.append(le16(1))                               // PCM
        d.append(le16(UInt16(channels)))
        d.append(le32(UInt32(sampleRate)))
        d.append(le32(UInt32(byteRate)))
        d.append(le16(UInt16(blockAlign)))
        d.append(le16(UInt16(bitsPerSample)))
        d.append("data".data(using: .ascii)!)
        d.append(le32(UInt32(dataBytes)))
        return d
    }
}

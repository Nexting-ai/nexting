import Foundation
@preconcurrency import CoreBluetooth

private let serviceUUID = CBUUID(string: "6EADC0DE-0001-4A21-9C5E-1B7F3D9E42A0")
private let downlinkUUID = CBUUID(string: "6EADC0DE-0002-4A21-9C5E-1B7F3D9E42A0")
private let uplinkUUID = CBUUID(string: "6EADC0DE-0003-4A21-9C5E-1B7F3D9E42A0")
private let deviceInfoUUID = CBUUID(string: "6EADC0DE-0004-4A21-9C5E-1B7F3D9E42A0")

private let maximumMessageBytes = 4096
private let answerRetryInterval = 1.0
private let deviceInfoData = Data(
    "{\"protocol\":\"nexting-device\",\"spec\":\"0.2.0-experimental.1\",\"wire\":[1],\"profiles\":[\"approval/1\"],\"model\":\"macos-device-simulator\",\"fw\":\"0.2.0\",\"max_message_bytes\":4096,\"max_summary_bytes\":240,\"display_name\":\"Mac Device Simulator\",\"button_count\":2,\"approval_button_count\":2,\"custom_button_count\":0}"
        .utf8
)

private struct PendingApproval {
    let requestID: String
    let summary: String
    let deadlineMs: Int64
    var lockedChoice: NextingDeviceChoice?
}

private func log(_ message: String) {
    print(message)
    fflush(stdout)
}

private func monotonicNowMs() -> Int64 {
    Int64((ProcessInfo.processInfo.systemUptime * 1_000).rounded(.down))
}

private func answerWire(requestID: String, choice: NextingDeviceChoice) -> Data? {
    NextingDeviceCodec.encode(.answer(requestId: requestID, choice: choice))
}

private final class DeviceSimulator: NSObject, CBPeripheralManagerDelegate {
    private var manager: CBPeripheralManager!
    private var uplink: CBMutableCharacteristic?
    private var subscribedCentral: CBCentral?
    private var downlinkBuffers: [UUID: Data] = [:]
    private var discardingOversize: Set<UUID> = []
    private var pending: PendingApproval?
    private var retryTimer: Timer?
    private var outboundFrame: Data?
    private var outboundFrameOffset = 0
    private var outboundIsRetry = false

    override init() {
        super.init()
        manager = CBPeripheralManager(delegate: self, queue: .main)
    }

    func peripheralManagerDidUpdateState(_ peripheral: CBPeripheralManager) {
        switch peripheral.state {
        case .poweredOn:
            publishService()
        case .poweredOff:
            resetConnectionState()
            log("Bluetooth is off.")
        case .unauthorized:
            resetConnectionState()
            log("Bluetooth access is not authorized. Allow your terminal in System Settings → Privacy & Security → Bluetooth.")
        default:
            log("Waiting for Bluetooth (state \(peripheral.state.rawValue))…")
        }
    }

    private func publishService() {
        let downlink = CBMutableCharacteristic(
            type: downlinkUUID,
            properties: [.write, .writeWithoutResponse],
            value: nil,
            permissions: [.writeable, .writeEncryptionRequired]
        )
        uplink = CBMutableCharacteristic(
            type: uplinkUUID,
            properties: [.notify, .notifyEncryptionRequired],
            value: nil,
            permissions: []
        )
        let deviceInfo = CBMutableCharacteristic(
            type: deviceInfoUUID,
            properties: [.read],
            value: deviceInfoData,
            permissions: [.readable]
        )
        let service = CBMutableService(type: serviceUUID, primary: true)
        service.characteristics = [downlink, uplink!, deviceInfo]
        manager.removeAllServices()
        manager.add(service)
    }

    func peripheralManager(_ peripheral: CBPeripheralManager,
                           didAdd service: CBService,
                           error: Error?) {
        guard error == nil else {
            log("Could not publish the GATT service: \(error!.localizedDescription)")
            return
        }
        peripheral.startAdvertising([
            CBAdvertisementDataServiceUUIDsKey: [serviceUUID],
            CBAdvertisementDataLocalNameKey: "Nexting-Device-Sim",
        ])
        log("Advertising Nexting Device Protocol Experimental 0.2 as Nexting-Device-Sim.")
        log("After the App presents an approval, press a for Allow or d for Deny.")
    }

    func peripheralManager(_ peripheral: CBPeripheralManager,
                           central: CBCentral,
                           didSubscribeTo characteristic: CBCharacteristic) {
        guard characteristic.uuid == uplinkUUID else { return }
        if subscribedCentral?.identifier != central.identifier {
            resetConnectionState()
        }
        subscribedCentral = central
        downlinkBuffers[central.identifier] = Data()
        log("Nexting App subscribed to the answer channel.")
    }

    func peripheralManager(_ peripheral: CBPeripheralManager,
                           central: CBCentral,
                           didUnsubscribeFrom characteristic: CBCharacteristic) {
        guard characteristic.uuid == uplinkUUID else { return }
        resetConnectionState(for: central)
        log("Nexting App disconnected; volatile approval state was cleared.")
    }

    func peripheralManager(_ peripheral: CBPeripheralManager,
                           didReceiveWrite requests: [CBATTRequest]) {
        for request in requests {
            guard request.characteristic.uuid == downlinkUUID else {
                peripheral.respond(to: request, withResult: .requestNotSupported)
                continue
            }
            guard let subscribedCentral,
                  subscribedCentral.identifier == request.central.identifier else {
                peripheral.respond(to: request, withResult: .insufficientAuthorization)
                continue
            }
            guard let value = request.value else {
                peripheral.respond(to: request, withResult: .invalidAttributeValueLength)
                continue
            }
            consumeDownlink(value, from: request.central)
            peripheral.respond(to: request, withResult: .success)
        }
    }

    private func consumeDownlink(_ data: Data, from central: CBCentral) {
        let identifier = central.identifier
        var incoming = data
        var buffer = downlinkBuffers[central.identifier] ?? Data()

        while !incoming.isEmpty {
            if discardingOversize.contains(identifier) {
                guard let newlineIndex = incoming.firstIndex(of: 0x0A) else { return }
                incoming.removeSubrange(incoming.startIndex ... newlineIndex)
                discardingOversize.remove(identifier)
                continue
            }

            if let newlineIndex = incoming.firstIndex(of: 0x0A) {
                let fragment = incoming[..<newlineIndex]
                if buffer.count + fragment.count >= maximumMessageBytes {
                    buffer.removeAll(keepingCapacity: true)
                    log("Dropped an oversized downlink message.")
                } else {
                    buffer.append(contentsOf: fragment)
                    let frame = buffer
                    buffer.removeAll(keepingCapacity: true)
                    handleDownlinkFrame(frame)
                }
                incoming.removeSubrange(incoming.startIndex ... newlineIndex)
            } else {
                if buffer.count + incoming.count >= maximumMessageBytes {
                    buffer.removeAll(keepingCapacity: true)
                    discardingOversize.insert(identifier)
                    log("Dropping downlink bytes until the next frame boundary.")
                } else {
                    buffer.append(incoming)
                }
                incoming.removeAll()
            }
        }
        downlinkBuffers[central.identifier] = buffer
    }

    private func handleDownlinkFrame(_ frame: Data) {
        guard let message = NextingDeviceCodec.decode(frame) else {
            log("Ignored an invalid downlink message.")
            return
        }
        switch message {
        case let .present(requestID, summary, ttlMilliseconds):
            if let previous = pending {
                log("Replacing local request \(previous.requestID.prefix(8)) with \(requestID.prefix(8)).")
            }
            retryTimer?.invalidate()
            retryTimer = nil
            clearOutbound()
            pending = PendingApproval(
                requestID: requestID,
                summary: summary,
                deadlineMs: monotonicNowMs() + Int64(ttlMilliseconds),
                lockedChoice: nil
            )
            log("🔴 \(summary) [id \(requestID.prefix(8))…] — press a/d")
        case let .resolved(requestID, reason):
            guard pending?.requestID == requestID else {
                log("Ignored resolution for an unknown request.")
                return
            }
            clearPending()
            log("⚫️ Request resolved: \(reason.rawValue).")
        default:
            log("Ignored a downlink message type that devices do not consume.")
        }
    }

    func handleKey(_ character: Character) {
        let choice: NextingDeviceChoice
        switch character {
        case "a", "A": choice = .allow
        case "d", "D": choice = .deny
        default: return
        }
        guard var approval = pending, approval.deadlineMs > monotonicNowMs() else {
            clearPending()
            log("No live approval is waiting; key ignored.")
            return
        }
        guard approval.lockedChoice == nil else {
            log("An answer is already waiting for host resolution; key ignored.")
            return
        }
        approval.lockedChoice = choice
        pending = approval
        startLockedAnswer(isRetry: false)
    }

    @objc private func retryLockedAnswer() {
        guard let approval = pending, approval.deadlineMs > monotonicNowMs() else {
            clearPending()
            log("Local approval TTL expired.")
            return
        }
        guard outboundFrame == nil else { return }
        startLockedAnswer(isRetry: true)
    }

    private func startLockedAnswer(isRetry: Bool) {
        guard let approval = pending,
              let choice = approval.lockedChoice,
              subscribedCentral != nil,
              uplink != nil,
              outboundFrame == nil
        else { return }
        retryTimer?.invalidate()
        retryTimer = nil
        guard let wire = answerWire(requestID: approval.requestID, choice: choice) else {
            clearPending()
            return
        }
        outboundFrame = wire
        outboundFrameOffset = 0
        outboundIsRetry = isRetry
        flushOutboundFrame()
    }

    private func flushOutboundFrame() {
        guard let approval = pending, approval.deadlineMs > monotonicNowMs() else {
            clearPending()
            log("Local approval TTL expired before its answer was sent.")
            return
        }
        guard let frame = outboundFrame,
              let uplink,
              let central = subscribedCentral
        else { return }

        let maximumChunkBytes = central.maximumUpdateValueLength
        guard maximumChunkBytes > 0 else {
            log("Central reported an invalid notification limit; answer remains queued.")
            return
        }

        while outboundFrameOffset < frame.count {
            let chunkEnd = min(outboundFrameOffset + maximumChunkBytes, frame.count)
            let chunk = frame.subdata(in: outboundFrameOffset ..< chunkEnd)
            guard manager.updateValue(
                chunk,
                for: uplink,
                onSubscribedCentrals: [central]
            ) else {
                log("Answer notification queue is full; waiting to resume.")
                return
            }
            outboundFrameOffset = chunkEnd
        }

        let wasRetry = outboundIsRetry
        let requestID = pending?.requestID
        let choice = pending?.lockedChoice
        outboundFrame = nil
        outboundFrameOffset = 0
        outboundIsRetry = false
        if let requestID, let choice {
            log("\(wasRetry ? "Retried" : "Sent") \(choice.rawValue) for \(requestID.prefix(8))…")
            retryTimer = Timer.scheduledTimer(
                timeInterval: answerRetryInterval,
                target: self,
                selector: #selector(retryLockedAnswer),
                userInfo: nil,
                repeats: false
            )
        }
    }

    func peripheralManagerIsReady(toUpdateSubscribers peripheral: CBPeripheralManager) {
        flushOutboundFrame()
    }

    private func clearPending() {
        retryTimer?.invalidate()
        retryTimer = nil
        pending = nil
        clearOutbound()
    }

    private func clearOutbound() {
        outboundFrame = nil
        outboundFrameOffset = 0
        outboundIsRetry = false
    }

    private func resetConnectionState(for central: CBCentral? = nil) {
        if let central {
            downlinkBuffers.removeValue(forKey: central.identifier)
            discardingOversize.remove(central.identifier)
            if subscribedCentral?.identifier == central.identifier {
                subscribedCentral = nil
                clearPending()
            }
        } else {
            subscribedCentral = nil
            downlinkBuffers.removeAll()
            discardingOversize.removeAll()
            clearPending()
        }
    }
}

private let simulator = DeviceSimulator()
private let standardInput = DispatchSource.makeReadSource(
    fileDescriptor: FileHandle.standardInput.fileDescriptor,
    queue: .main
)
standardInput.setEventHandler {
    let data = FileHandle.standardInput.availableData
    for byte in data {
        simulator.handleKey(Character(UnicodeScalar(byte)))
    }
}
standardInput.resume()

log("Starting the Nexting device simulator…")
RunLoop.main.run()

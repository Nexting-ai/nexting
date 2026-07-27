@preconcurrency import CoreBluetooth
import Foundation

public let NextingDeviceServiceUUID = CBUUID(
    string: "6EADC0DE-0001-4A21-9C5E-1B7F3D9E42A0"
)
public let NextingDeviceDownlinkUUID = CBUUID(
    string: "6EADC0DE-0002-4A21-9C5E-1B7F3D9E42A0"
)
public let NextingDeviceUplinkUUID = CBUUID(
    string: "6EADC0DE-0003-4A21-9C5E-1B7F3D9E42A0"
)
public let NextingDeviceInfoUUID = CBUUID(
    string: "6EADC0DE-0004-4A21-9C5E-1B7F3D9E42A0"
)

enum NextingDeviceSetupDecision: Equatable {
    case accept(NextingDeviceInfo)
    case recover
}

enum NextingDeviceSetupTransition {
    static func deviceInfoRead(
        data: Data?,
        hadError: Bool
    ) -> NextingDeviceSetupDecision {
        guard !hadError, let data,
              let info = NextingDeviceInfo.decode(data) else { return .recover }
        return .accept(info)
    }
}

enum NextingDeviceTransportPolicy {
    static let hostMaxMessageBytes = 4_096

    static func accepts(
        frameByteCount: Int,
        deviceInfo: NextingDeviceInfo?
    ) -> Bool {
        let limit = min(
            hostMaxMessageBytes,
            deviceInfo?.maxMessageBytes ?? hostMaxMessageBytes
        )
        return frameByteCount > 0 && frameByteCount <= limit
    }

    static func boundedSummary(
        _ summary: String,
        deviceInfo: NextingDeviceInfo?
    ) -> String {
        let limit = min(
            NextingDeviceCodec.maxSummaryBytes,
            deviceInfo?.maxSummaryBytes ?? NextingDeviceCodec.maxSummaryBytes
        )
        guard summary.utf8.count > limit else { return summary }
        var result = ""
        var byteCount = 0
        for character in summary {
            let addition = String(character)
            let additionBytes = addition.utf8.count
            guard byteCount + additionBytes <= limit else { break }
            result.append(character)
            byteCount += additionBytes
        }
        return result
    }

    static func boundedPresentSummary(
        requestId: String,
        summary: String,
        ttlMs: Int,
        deviceInfo: NextingDeviceInfo?
    ) -> String? {
        var candidate = boundedSummary(summary, deviceInfo: deviceInfo)
        while let wire = NextingDeviceCodec.encode(.present(
            requestId: requestId,
            summary: candidate,
            ttlMs: ttlMs
        )) {
            if accepts(
                frameByteCount: wire.count,
                deviceInfo: deviceInfo
            ) { return candidate }
            guard !candidate.isEmpty else { return nil }
            candidate.removeLast()
        }
        return nil
    }
}

struct NextingDeviceWriteQueue {
    let maxPendingBytes: Int
    let maxPendingFrames: Int
    private var frames: [Data] = []
    private var headIndex = 0
    private var frameOffset = 0
    private var inFlightLength: Int?
    private(set) var pendingByteCount = 0

    init(maxPendingBytes: Int, maxPendingFrames: Int = 64) {
        precondition(maxPendingBytes > 0, "maxPendingBytes must be positive")
        precondition(maxPendingFrames > 0, "maxPendingFrames must be positive")
        self.maxPendingBytes = maxPendingBytes
        self.maxPendingFrames = maxPendingFrames
    }

    var hasInFlightChunk: Bool { inFlightLength != nil }

    mutating func enqueue(frame: Data) -> Bool {
        guard !frame.isEmpty,
              frames.count - headIndex < maxPendingFrames,
              frame.count <= maxPendingBytes - pendingByteCount else { return false }
        frames.append(frame)
        pendingByteCount += frame.count
        return true
    }

    mutating func nextChunk(maxBytes: Int) -> Data? {
        guard maxBytes > 0, inFlightLength == nil,
              headIndex < frames.count else { return nil }
        let frame = frames[headIndex]
        let length = min(maxBytes, frame.count - frameOffset)
        guard length > 0 else { return nil }
        inFlightLength = length
        return frame.subdata(in: frameOffset ..< frameOffset + length)
    }

    mutating func acknowledgeInFlightChunk() -> Bool {
        guard let length = inFlightLength,
              headIndex < frames.count else { return false }
        inFlightLength = nil
        frameOffset += length
        pendingByteCount -= length
        if frameOffset == frames[headIndex].count {
            headIndex += 1
            frameOffset = 0
            compactCompletedFrames()
        }
        return true
    }

    mutating func removeAll() {
        frames.removeAll(keepingCapacity: true)
        headIndex = 0
        frameOffset = 0
        inFlightLength = nil
        pendingByteCount = 0
    }

    private mutating func compactCompletedFrames() {
        if headIndex == frames.count {
            frames.removeAll(keepingCapacity: true)
            headIndex = 0
        } else if headIndex >= 32, headIndex * 2 >= frames.count {
            frames.removeFirst(headIndex)
            headIndex = 0
        }
    }
}

public enum NextingDeviceSendRejection: Equatable, Sendable {
    case notReady
    case messageTooLarge(actualBytes: Int, limitBytes: Int)
    case queueFull(limitBytes: Int)
}

public final class NextingDeviceCentral: NSObject, NextingDeviceRelayTransport {
    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var downlink: CBCharacteristic?
    private var uplink: CBCharacteristic?
    private var deviceInfoCharacteristic: CBCharacteristic?
    private var batteryLevelCharacteristic: CBCharacteristic?
    private var decoder = NextingDeviceLineDecoder()
    private var writeQueue: NextingDeviceWriteQueue
    private var shouldScan = false
    private var recoveryState = NextingDeviceDiscoveryRecoveryState()
    private var recoveringPeripheralID: UUID?
    private let authorizationPredicate: (NextingDevicePeripheralIdentity) -> Bool

    public var onMessage: ((NextingDevicePeripheralIdentity, NextingDeviceMessage) -> Void)?
    public var onConnectedChange: ((Bool) -> Void)?
    /// Discovery is informational only. A discovered peripheral is never
    /// connected unless `authorizationPredicate` also accepts it.
    public var onDiscovered: ((NextingDevicePeripheralIdentity, Int) -> Void)?
    public var onBatteryLevel: ((Int?) -> Void)?
    public var onSendRejected: ((NextingDeviceSendRejection) -> Void)?

    public private(set) var isReady = false {
        didSet {
            if isReady != oldValue { onConnectedChange?(isReady) }
        }
    }

    public private(set) var connectedIdentity: NextingDevicePeripheralIdentity?
    public private(set) var connectedDeviceInfo: NextingDeviceInfo?
    public private(set) var connectedBatteryLevel: Int?

    public init(
        authorizationPredicate: @escaping (NextingDevicePeripheralIdentity) -> Bool,
        maxPendingWriteBytes: Int = 16_384
    ) {
        self.authorizationPredicate = authorizationPredicate
        writeQueue = NextingDeviceWriteQueue(
            maxPendingBytes: maxPendingWriteBytes
        )
        super.init()
        central = CBCentralManager(delegate: self, queue: nil)
    }

    @discardableResult
    public func send(_ data: Data) -> Bool {
        guard isReady, peripheral != nil, downlink != nil,
              let connectedDeviceInfo else {
            onSendRejected?(.notReady)
            return false
        }
        let messageLimit = min(
            NextingDeviceTransportPolicy.hostMaxMessageBytes,
            connectedDeviceInfo.maxMessageBytes
        )
        guard NextingDeviceTransportPolicy.accepts(
            frameByteCount: data.count,
            deviceInfo: connectedDeviceInfo
        ) else {
            onSendRejected?(.messageTooLarge(
                actualBytes: data.count,
                limitBytes: messageLimit
            ))
            return false
        }
        guard writeQueue.enqueue(frame: data) else {
            onSendRejected?(.queueFull(limitBytes: writeQueue.maxPendingBytes))
            return false
        }
        pumpWriteQueue()
        return true
    }

    public func startScan() {
        shouldScan = true
        recoveryState.beginExplicitScanCycle()
        guard peripheral == nil, recoveringPeripheralID == nil else { return }
        central.stopScan()
        resumeScanning()
    }

    public func disconnect() {
        shouldScan = false
        central.stopScan()
        if let peripheral { central.cancelPeripheralConnection(peripheral) }
        recoveringPeripheralID = nil
        clearConnectionState()
    }

    private func pumpWriteQueue() {
        guard let peripheral, let downlink else { return }
        let maximum = max(1, peripheral.maximumWriteValueLength(for: .withResponse))
        guard let next = writeQueue.nextChunk(maxBytes: maximum) else { return }
        peripheral.writeValue(next, for: downlink, type: .withResponse)
    }

    private func resumeScanning() {
        guard shouldScan, central.state == .poweredOn,
              peripheral == nil, recoveringPeripheralID == nil else { return }
        central.scanForPeripherals(withServices: [NextingDeviceServiceUUID])
    }

    private func clearConnectionState() {
        isReady = false
        downlink = nil
        uplink = nil
        deviceInfoCharacteristic = nil
        batteryLevelCharacteristic = nil
        peripheral = nil
        connectedIdentity = nil
        connectedDeviceInfo = nil
        connectedBatteryLevel = nil
        onBatteryLevel?(nil)
        decoder.reset()
        writeQueue.removeAll()
    }

    private func recoverFromSetupFailure(_ peripheral: CBPeripheral) {
        guard self.peripheral === peripheral,
              connectedIdentity?.id == peripheral.identifier,
              recoveringPeripheralID == nil else { return }
        recoveryState.quarantine(peripheral.identifier)
        recoveringPeripheralID = peripheral.identifier
        isReady = false
        downlink = nil
        uplink = nil
        deviceInfoCharacteristic = nil
        decoder.reset()
        writeQueue.removeAll()
        central.cancelPeripheralConnection(peripheral)
    }
}

extension NextingDeviceCentral: CBCentralManagerDelegate {
    public func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn {
            resumeScanning()
        } else {
            recoveringPeripheralID = nil
            clearConnectionState()
        }
    }

    public func centralManager(
        _ central: CBCentralManager,
        didDiscover peripheral: CBPeripheral,
        advertisementData: [String: Any],
        rssi RSSI: NSNumber
    ) {
        let name = advertisementData[CBAdvertisementDataLocalNameKey] as? String
            ?? peripheral.name
        let identity = NextingDevicePeripheralIdentity(
            id: peripheral.identifier,
            name: name
        )
        onDiscovered?(identity, RSSI.intValue)
        guard self.peripheral == nil, shouldScan,
              recoveryState.canAttempt(peripheral.identifier),
              authorizationPredicate(identity) else { return }
        central.stopScan()
        self.peripheral = peripheral
        connectedIdentity = identity
        peripheral.delegate = self
        central.connect(peripheral)
    }

    public func centralManager(
        _ central: CBCentralManager,
        didConnect peripheral: CBPeripheral
    ) {
        guard shouldScan, self.peripheral === peripheral,
              let identity = connectedIdentity,
              identity.id == peripheral.identifier,
              authorizationPredicate(identity) else {
            central.cancelPeripheralConnection(peripheral)
            return
        }
        peripheral.discoverServices([
            NextingDeviceServiceUUID,
            NextingDeviceBattery.serviceUUID,
        ])
    }

    public func centralManager(
        _ central: CBCentralManager,
        didDisconnectPeripheral peripheral: CBPeripheral,
        error: Error?
    ) {
        guard self.peripheral === peripheral
                || connectedIdentity?.id == peripheral.identifier
                || recoveringPeripheralID == peripheral.identifier else { return }
        let shouldResume = recoveryState.connectionDidEnd(
            peripheral.identifier,
            wasReady: isReady,
            shouldScan: shouldScan
        )
        recoveringPeripheralID = nil
        clearConnectionState()
        if shouldResume { resumeScanning() }
    }

    public func centralManager(
        _ central: CBCentralManager,
        didFailToConnect peripheral: CBPeripheral,
        error: Error?
    ) {
        guard self.peripheral === peripheral
                || connectedIdentity?.id == peripheral.identifier else { return }
        recoveryState.quarantine(peripheral.identifier)
        recoveringPeripheralID = nil
        clearConnectionState()
        resumeScanning()
    }
}

extension NextingDeviceCentral: CBPeripheralDelegate {
    public func peripheral(
        _ peripheral: CBPeripheral,
        didDiscoverServices error: Error?
    ) {
        guard self.peripheral === peripheral,
              connectedIdentity?.id == peripheral.identifier else { return }
        guard error == nil,
              let service = peripheral.services?.first(where: {
                  $0.uuid == NextingDeviceServiceUUID
              }) else {
            recoverFromSetupFailure(peripheral)
            return
        }
        peripheral.discoverCharacteristics(
            [
                NextingDeviceDownlinkUUID,
                NextingDeviceUplinkUUID,
                NextingDeviceInfoUUID,
            ],
            for: service
        )
        if let batteryService = peripheral.services?.first(where: {
            $0.uuid == NextingDeviceBattery.serviceUUID
        }) {
            peripheral.discoverCharacteristics(
                [NextingDeviceBattery.levelCharacteristicUUID],
                for: batteryService
            )
        }
    }

    public func peripheral(
        _ peripheral: CBPeripheral,
        didDiscoverCharacteristicsFor service: CBService,
        error: Error?
    ) {
        guard self.peripheral === peripheral,
              connectedIdentity?.id == peripheral.identifier else { return }

        if service.uuid == NextingDeviceBattery.serviceUUID {
            guard error == nil,
                  let batteryLevel = service.characteristics?.first(where: {
                      $0.uuid == NextingDeviceBattery.levelCharacteristicUUID
                  }),
                  batteryLevel.properties.contains(.read) else { return }
            batteryLevelCharacteristic = batteryLevel
            peripheral.readValue(for: batteryLevel)
            if batteryLevel.properties.contains(.notify) {
                peripheral.setNotifyValue(true, for: batteryLevel)
            }
            return
        }
        guard service.uuid == NextingDeviceServiceUUID else { return }
        let characteristics = service.characteristics ?? []
        guard error == nil,
              let downlink = characteristics.first(where: {
                  $0.uuid == NextingDeviceDownlinkUUID
              }),
              downlink.properties.contains(.write),
              let uplink = characteristics.first(where: {
                  $0.uuid == NextingDeviceUplinkUUID
              }),
              uplink.properties.contains(.notify),
              let deviceInfo = characteristics.first(where: {
                  $0.uuid == NextingDeviceInfoUUID
              }),
              deviceInfo.properties.contains(.read) else {
            recoverFromSetupFailure(peripheral)
            return
        }
        self.downlink = downlink
        self.uplink = uplink
        deviceInfoCharacteristic = deviceInfo
        peripheral.readValue(for: deviceInfo)
    }

    public func peripheral(
        _ peripheral: CBPeripheral,
        didUpdateValueFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        guard self.peripheral === peripheral,
              connectedIdentity?.id == peripheral.identifier else { return }

        if characteristic.uuid == NextingDeviceBattery.levelCharacteristicUUID,
           batteryLevelCharacteristic === characteristic {
            let level = error == nil
                ? characteristic.value.flatMap(NextingDeviceBattery.decodeLevel)
                : nil
            connectedBatteryLevel = level
            onBatteryLevel?(level)
            return
        }

        if characteristic.uuid == NextingDeviceInfoUUID,
           deviceInfoCharacteristic === characteristic {
            switch NextingDeviceSetupTransition.deviceInfoRead(
                data: characteristic.value,
                hadError: error != nil
            ) {
            case .recover:
                recoverFromSetupFailure(peripheral)
                return
            case let .accept(info):
                connectedDeviceInfo = info
                decoder = NextingDeviceLineDecoder(
                    maxMessageBytes: min(
                        info.maxMessageBytes,
                        NextingDeviceTransportPolicy.hostMaxMessageBytes
                    )
                )
                guard let uplink else {
                    recoverFromSetupFailure(peripheral)
                    return
                }
                peripheral.setNotifyValue(true, for: uplink)
                return
            }
        }

        guard characteristic.uuid == NextingDeviceUplinkUUID,
              uplink === characteristic,
              error == nil,
              let data = characteristic.value,
              let identity = connectedIdentity else { return }
        for message in decoder.push(data) {
            onMessage?(identity, message)
        }
    }

    public func peripheral(
        _ peripheral: CBPeripheral,
        didUpdateNotificationStateFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        guard self.peripheral === peripheral,
              connectedIdentity?.id == peripheral.identifier,
              characteristic.uuid == NextingDeviceUplinkUUID,
              uplink === characteristic else { return }
        guard error == nil, downlink != nil,
              connectedDeviceInfo != nil,
              characteristic.isNotifying else {
            recoverFromSetupFailure(peripheral)
            return
        }
        isReady = true
    }

    public func peripheral(
        _ peripheral: CBPeripheral,
        didWriteValueFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        guard self.peripheral === peripheral,
              characteristic.uuid == NextingDeviceDownlinkUUID,
              downlink === characteristic,
              writeQueue.hasInFlightChunk else { return }
        guard error == nil else {
            recoverFromSetupFailure(peripheral)
            return
        }
        _ = writeQueue.acknowledgeInFlightChunk()
        pumpWriteQueue()
    }
}

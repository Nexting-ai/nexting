import CoreBluetooth
import Foundation

public enum NextingDeviceBattery {
    public static let serviceUUIDString = "180F"
    public static let levelCharacteristicUUIDString = "2A19"
    public static var serviceUUID: CBUUID { CBUUID(string: serviceUUIDString) }
    public static var levelCharacteristicUUID: CBUUID {
        CBUUID(string: levelCharacteristicUUIDString)
    }

    public static func decodeLevel(_ data: Data) -> Int? {
        guard data.count == 1, let raw = data.first else { return nil }
        return min(Int(raw), 100)
    }
}

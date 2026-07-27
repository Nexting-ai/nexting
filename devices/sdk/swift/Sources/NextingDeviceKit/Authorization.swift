import Foundation

public struct NextingDevicePeripheralIdentity: Equatable, Hashable, Sendable {
    public let id: UUID
    public let name: String?

    public init(id: UUID, name: String?) {
        self.id = id
        self.name = name
    }
}

public final class NextingDeviceAuthorizationStore {
    private static let authorizedIDsKey = "nexting_device_authorized_peripheral_uuids"
    private let defaults: UserDefaults

    public init(defaults: UserDefaults = .standard) {
        self.defaults = defaults
    }

    public func isAuthorized(_ id: UUID) -> Bool {
        authorizedIDs.contains(id.uuidString)
    }

    public func authorize(_ id: UUID) {
        var ids = authorizedIDs
        ids.insert(id.uuidString)
        defaults.set(ids.sorted(), forKey: Self.authorizedIDsKey)
    }

    public func revoke(_ id: UUID) {
        var ids = authorizedIDs
        ids.remove(id.uuidString)
        defaults.set(ids.sorted(), forKey: Self.authorizedIDsKey)
    }

    private var authorizedIDs: Set<String> {
        Set(defaults.stringArray(forKey: Self.authorizedIDsKey) ?? [])
    }
}

public struct NextingDeviceAuthorizationPolicy {
    public static let debugTestDeviceName = "Nexting-Device-Sim"

    private let store: NextingDeviceAuthorizationStore
    private let allowDebugTestDevice: Bool

    public init(
        store: NextingDeviceAuthorizationStore,
        allowDebugTestDevice: Bool
    ) {
        self.store = store
        self.allowDebugTestDevice = allowDebugTestDevice
    }

    public func isAuthorized(_ identity: NextingDevicePeripheralIdentity) -> Bool {
        store.isAuthorized(identity.id)
            || (allowDebugTestDevice && identity.name == Self.debugTestDeviceName)
    }
}

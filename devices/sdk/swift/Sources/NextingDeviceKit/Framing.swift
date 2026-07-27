import Foundation

public struct NextingDeviceLineDecoder: Sendable {
    private let maxMessageBytes: Int
    private var buffer = Data()
    private var discardingOversizedMessage = false

    public init(maxMessageBytes: Int = 4096) {
        precondition(
            (1 ... NextingDeviceCodec.maxMessageBytes).contains(maxMessageBytes),
            "maxMessageBytes must be within the host protocol limit"
        )
        self.maxMessageBytes = maxMessageBytes
        buffer.reserveCapacity(min(maxMessageBytes, 4096))
    }

    public mutating func push<Bytes: DataProtocol>(
        _ chunk: Bytes
    ) -> [NextingDeviceMessage] {
        var messages: [NextingDeviceMessage] = []

        guard chunk.count <= maxMessageBytes else {
            reset()
            return [.error(requestId: nil, code: .messageTooLarge)]
        }

        for byte in chunk {
            if discardingOversizedMessage {
                if byte == 0x0A { discardingOversizedMessage = false }
                continue
            }

            if byte == 0x0A {
                let message = NextingDeviceCodec.decode(buffer)
                    ?? .error(requestId: nil, code: .badMessage)
                messages.append(message)
                buffer.removeAll(keepingCapacity: true)
                continue
            }

            if buffer.count >= maxMessageBytes - 1 {
                buffer.removeAll(keepingCapacity: true)
                discardingOversizedMessage = true
                messages.append(.error(requestId: nil, code: .messageTooLarge))
                continue
            }

            buffer.append(byte)
        }

        return messages
    }

    public mutating func reset() {
        buffer.removeAll(keepingCapacity: true)
        discardingOversizedMessage = false
    }
}

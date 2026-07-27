package ai.nexting.devices

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

class ProtocolTest {
    @Test
    fun `approval messages round trip`() {
        val present = DeviceMessage.Present("cc:42", "Allow shell command?", 60_000)
        assertEquals(present, DeviceMessageCodec.decode(DeviceMessageCodec.encode(present)!!))
        val answer = DeviceMessage.Answer("cc:42", DeviceChoice.ALLOW)
        assertEquals(answer, DeviceMessageCodec.decode(DeviceMessageCodec.encode(answer)!!))
    }

    @Test
    fun `status messages preserve slots and labels`() {
        val status = DeviceMessage.Status(listOf(
            DeviceAgentStatus(0, DeviceAgentState.WORKING, "Claude Code"),
            DeviceAgentStatus(1, DeviceAgentState.NEEDS_INPUT),
        ))
        assertEquals(status, DeviceMessageCodec.decode(DeviceMessageCodec.encode(status)!!))
    }

    @Test
    fun `invalid limits fail closed`() {
        assertNull(DeviceMessageCodec.encode(DeviceMessage.Present("", "x", 1)))
        assertNull(DeviceMessageCodec.encode(DeviceMessage.Present("x", "x", 0)))
        assertNull(DeviceMessageCodec.decode(
            """{"v":1,"t":"answer","id":"x","ch":"maybe"}""".encodeToByteArray(),
        ))
        assertNull(DeviceMessageCodec.decode(
            """{"v":1,"t":"status","agents":[{"slot":0,"state":"idle"},{"slot":0,"state":"working"}]}"""
                .encodeToByteArray(),
        ))
    }
}

class DeviceLineDecoderTest {
    @Test
    fun `fragmented frame decodes once`() {
        val decoder = DeviceLineDecoder()
        val wire = DeviceMessageCodec.encode(
            DeviceMessage.Answer("request-1", DeviceChoice.ALLOW),
        )!!
        assertEquals(emptyList(), decoder.push(wire.copyOfRange(0, 7)))
        assertEquals(
            listOf(DeviceMessage.Answer("request-1", DeviceChoice.ALLOW)),
            decoder.push(wire.copyOfRange(7, wire.size)),
        )
    }

    @Test
    fun `oversize frame discards through newline and recovers in same chunk`() {
        val decoder = DeviceLineDecoder(maximumMessageBytes = 64)
        val valid = DeviceMessageCodec.encode(
            DeviceMessage.Answer("r2", DeviceChoice.DENY),
        )!!
        val hostile = ByteArray(80) { 'x'.code.toByte() } + byteArrayOf('\n'.code.toByte()) + valid
        assertEquals(
            listOf(DeviceMessage.Answer("r2", DeviceChoice.DENY)),
            decoder.push(hostile),
        )
    }

    @Test
    fun `reset drops a partial frame`() {
        val decoder = DeviceLineDecoder()
        decoder.push("""{"v":1,"t":"answer"""".encodeToByteArray())
        decoder.reset()
        assertEquals(emptyList(), decoder.push(byteArrayOf('\n'.code.toByte())))
    }
}

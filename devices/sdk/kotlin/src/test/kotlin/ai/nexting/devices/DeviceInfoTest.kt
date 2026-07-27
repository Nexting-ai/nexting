package ai.nexting.devices

import java.nio.file.Files
import java.nio.file.Path
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive

class DeviceInfoTest {
    private val vectors = Json.parseToJsonElement(
        Files.readString(
            Path.of("../../protocol/vectors/device-info-v1.json").toAbsolutePath().normalize(),
        ),
    ).jsonObject

    @Test
    fun `valid shared vectors normalize identically`() {
        val valid = vectors.getValue("valid").jsonArray
        valid.forEach { raw ->
            val item = raw.jsonObject
            val expected = item.getValue("decoded").jsonObject
            val info = assertNotNull(
                DeviceInfoCodec.decode(item.getValue("wire").jsonPrimitive.content.encodeToByteArray()),
                item.getValue("name").jsonPrimitive.content,
            )
            assertEquals(expected.getValue("model").jsonPrimitive.content, info.model)
            assertEquals(
                expected.getValue("statusSlots").jsonPrimitive.content.toInt(),
                info.capabilities.statusSlots,
            )
            assertEquals(
                expected.getValue("batteryService").jsonPrimitive.content.toBoolean(),
                info.capabilities.batteryService,
            )
        }
    }

    @Test
    fun `invalid core is rejected`() {
        vectors.getValue("invalidCore").jsonArray.forEach { raw ->
            val item = raw.jsonObject
            assertNull(
                DeviceInfoCodec.decode(item.getValue("wire").jsonPrimitive.content.encodeToByteArray()),
                item.getValue("name").jsonPrimitive.content,
            )
        }
    }

    @Test
    fun `battery level is bounded and optional`() {
        assertEquals(0, DeviceBattery.decodeLevel(byteArrayOf(0)))
        assertEquals(55, DeviceBattery.decodeLevel(byteArrayOf(55)))
        assertEquals(100, DeviceBattery.decodeLevel(byteArrayOf(100)))
        assertEquals(100, DeviceBattery.decodeLevel(byteArrayOf(255.toByte())))
        assertNull(DeviceBattery.decodeLevel(byteArrayOf()))
        assertNull(DeviceBattery.decodeLevel(byteArrayOf(1, 2)))
    }
}

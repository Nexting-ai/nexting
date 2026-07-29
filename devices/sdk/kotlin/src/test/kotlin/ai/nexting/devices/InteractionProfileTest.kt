package ai.nexting.devices

import java.nio.file.Files
import java.nio.file.Path
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

class InteractionProfileTest {
    private val vectorDirectory: Path = Path.of(System.getProperty("user.dir"))
        .resolve("../../protocol/vectors")
        .normalize()

    @Test
    fun `all interaction vectors use the same strict Kotlin codec`() {
        val profiles = listOf("navigation", "keys", "rotary", "voice", "text", "usage", "config")
        profiles.forEach { profile ->
            val document = Json.parseToJsonElement(
                Files.readString(vectorDirectory.resolve("$profile-v1.json")),
            ).jsonObject
            document["valid"]!!.jsonArray.forEach { raw ->
                val vector = raw.jsonObject
                val name = vector["name"]!!.jsonPrimitive.content
                val wire = vector["wire"]!!.jsonPrimitive.content.encodeToByteArray()
                val message = assertNotNull(DeviceMessageCodec.decode(wire), name)
                assertContentEquals(wire, DeviceMessageCodec.encode(message), name)
            }
            document["invalid"]!!.jsonArray.forEach { raw ->
                val vector = raw.jsonObject
                assertNull(
                    DeviceMessageCodec.decode(
                        vector["wire"]!!.jsonPrimitive.content.encodeToByteArray(),
                    ),
                    vector["name"]!!.jsonPrimitive.content,
                )
            }
        }
    }

    @Test
    fun `interaction profile negotiation is explicit`() {
        val info = DeviceInfoCodec.decode(
            """{"protocol":"nexting-device","spec":"0.2.0-experimental.2","wire":[1],"profiles":["approval/1","navigation/1","keys/1"],"model":"multi-pad","fw":"0.2.0","max_message_bytes":4096,"max_summary_bytes":240}"""
                .encodeToByteArray(),
        )
        assertNotNull(info)
        assertTrue(info.supportsNavigationV1)
        assertTrue(info.supportsKeysV1)
        assertFalse(info.supportsConfigV1)
    }
}

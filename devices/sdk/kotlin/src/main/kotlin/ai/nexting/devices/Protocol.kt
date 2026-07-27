package ai.nexting.devices

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put

enum class DeviceChoice(val wireValue: String) {
    ALLOW("allow"),
    DENY("deny"),
}

enum class DeviceResolutionReason(val wireValue: String) {
    ANSWERED("answered"),
    EXPIRED("expired"),
    CANCELLED("cancelled"),
    REPLACED("replaced"),
}

enum class DeviceErrorCode(val wireValue: String) {
    BAD_MESSAGE("bad_message"),
    MESSAGE_TOO_LARGE("message_too_large"),
    UNSUPPORTED_VERSION("unsupported_version"),
    UNSUPPORTED_PROFILE("unsupported_profile"),
    UNKNOWN_REQUEST("unknown_request"),
    NOT_AUTHORIZED("not_authorized"),
    BUSY("busy"),
}

enum class DeviceAgentState(val wireValue: String) {
    IDLE("idle"),
    THINKING("thinking"),
    WORKING("working"),
    COMPLETE("complete"),
    NEEDS_INPUT("needs_input"),
    ERROR("error"),
}

data class DeviceAgentStatus(
    val slot: Int,
    val state: DeviceAgentState,
    val label: String? = null,
)

sealed interface DeviceMessage {
    data class Present(
        val requestId: String,
        val summary: String,
        val ttlMilliseconds: Int,
    ) : DeviceMessage

    data class Answer(
        val requestId: String,
        val choice: DeviceChoice,
    ) : DeviceMessage

    data class Resolved(
        val requestId: String,
        val reason: DeviceResolutionReason,
    ) : DeviceMessage

    data class Error(
        val requestId: String?,
        val code: DeviceErrorCode,
    ) : DeviceMessage

    data class Status(
        val agents: List<DeviceAgentStatus>,
    ) : DeviceMessage
}

object DeviceMessageCodec {
    const val WIRE_VERSION = 1
    const val APPROVAL_PROFILE = "approval/1"
    const val STATUS_PROFILE = "status/1"
    const val MAX_REQUEST_ID_BYTES = 64
    const val MAX_SUMMARY_BYTES = 240
    const val MAX_TTL_MILLISECONDS = 300_000
    const val MAX_MESSAGE_BYTES = 4_096
    const val MAX_STATUS_AGENTS = 8
    const val MAX_STATUS_LABEL_BYTES = 64

    private val json = Json {
        ignoreUnknownKeys = false
        isLenient = false
    }
    private val requestId = Regex("[A-Za-z0-9._:-]{1,64}")

    fun encode(message: DeviceMessage): ByteArray? {
        val objectValue = when (message) {
            is DeviceMessage.Present -> {
                if (
                    !validId(message.requestId) ||
                    !validSummary(message.summary) ||
                    message.ttlMilliseconds !in 1..MAX_TTL_MILLISECONDS
                ) return null
                buildJsonObject {
                    put("v", WIRE_VERSION)
                    put("t", "present")
                    put("id", message.requestId)
                    put("sum", message.summary)
                    put("opt", buildJsonArray {
                        add(JsonPrimitive("allow"))
                        add(JsonPrimitive("deny"))
                    })
                    put("ttl", message.ttlMilliseconds)
                }
            }
            is DeviceMessage.Answer -> {
                if (!validId(message.requestId)) return null
                buildJsonObject {
                    put("v", WIRE_VERSION)
                    put("t", "answer")
                    put("id", message.requestId)
                    put("ch", message.choice.wireValue)
                }
            }
            is DeviceMessage.Resolved -> {
                if (!validId(message.requestId)) return null
                buildJsonObject {
                    put("v", WIRE_VERSION)
                    put("t", "resolved")
                    put("id", message.requestId)
                    put("r", message.reason.wireValue)
                }
            }
            is DeviceMessage.Error -> {
                if (message.requestId != null && !validId(message.requestId)) return null
                buildJsonObject {
                    put("v", WIRE_VERSION)
                    put("t", "error")
                    message.requestId?.let { put("id", it) }
                    put("code", message.code.wireValue)
                }
            }
            is DeviceMessage.Status -> {
                if (!validAgents(message.agents)) return null
                buildJsonObject {
                    put("v", WIRE_VERSION)
                    put("t", "status")
                    put("agents", buildJsonArray {
                        message.agents.forEach { agent ->
                            add(buildJsonObject {
                                put("slot", agent.slot)
                                put("state", agent.state.wireValue)
                                agent.label?.let { put("label", it) }
                            })
                        }
                    })
                }
            }
        }
        val encoded = (objectValue.toString() + "\n").encodeToByteArray()
        return encoded.takeIf { it.size <= MAX_MESSAGE_BYTES }
    }

    fun decode(bytes: ByteArray): DeviceMessage? {
        if (bytes.isEmpty() || bytes.size > MAX_MESSAGE_BYTES) return null
        val text = runCatching { bytes.decodeToString(throwOnInvalidSequence = true) }
            .getOrNull() ?: return null
        val body = if (text.endsWith("\n")) text.dropLast(1) else text
        if (body.isEmpty() || '\n' in body || '\r' in body || '\u0000' in body) return null
        val root = runCatching { json.parseToJsonElement(body).jsonObject }.getOrNull()
            ?: return null
        if (root.canonicalInt("v") != WIRE_VERSION) return null
        return when (root.string("t")) {
            "present" -> decodePresent(root)
            "answer" -> decodeAnswer(root)
            "resolved" -> decodeResolved(root)
            "error" -> decodeError(root)
            "status" -> decodeStatus(root)
            else -> null
        }
    }

    private fun decodePresent(root: JsonObject): DeviceMessage? {
        val id = root.string("id")?.takeIf(::validId) ?: return null
        val summary = root.string("sum")?.takeIf(::validSummary) ?: return null
        val options = runCatching {
            root["opt"]!!.jsonArray.map { it.jsonPrimitive.content }
        }.getOrNull() ?: return null
        if (options != listOf("allow", "deny")) return null
        val ttl = root.canonicalInt("ttl")
            ?.takeIf { it in 1..MAX_TTL_MILLISECONDS } ?: return null
        return DeviceMessage.Present(id, summary, ttl)
    }

    private fun decodeAnswer(root: JsonObject): DeviceMessage? {
        val id = root.string("id")?.takeIf(::validId) ?: return null
        val choice = DeviceChoice.entries.firstOrNull {
            it.wireValue == root.string("ch")
        } ?: return null
        return DeviceMessage.Answer(id, choice)
    }

    private fun decodeResolved(root: JsonObject): DeviceMessage? {
        val id = root.string("id")?.takeIf(::validId) ?: return null
        val reason = DeviceResolutionReason.entries.firstOrNull {
            it.wireValue == root.string("r")
        } ?: return null
        return DeviceMessage.Resolved(id, reason)
    }

    private fun decodeError(root: JsonObject): DeviceMessage? {
        val id = if ("id" in root) {
            root.string("id")?.takeIf(::validId) ?: return null
        } else {
            null
        }
        val code = DeviceErrorCode.entries.firstOrNull {
            it.wireValue == root.string("code")
        } ?: return null
        return DeviceMessage.Error(id, code)
    }

    private fun decodeStatus(root: JsonObject): DeviceMessage? {
        val rawAgents = root["agents"] as? JsonArray ?: return null
        val agents = rawAgents.map { raw ->
            val item = raw as? JsonObject ?: return null
            val slot = item.canonicalInt("slot") ?: return null
            val state = DeviceAgentState.entries.firstOrNull {
                it.wireValue == item.string("state")
            } ?: return null
            val label = if ("label" in item) item.string("label") ?: return null else null
            DeviceAgentStatus(slot, state, label)
        }
        if (!validAgents(agents)) return null
        return DeviceMessage.Status(agents)
    }

    private fun validId(value: String): Boolean =
        value.encodeToByteArray().size <= MAX_REQUEST_ID_BYTES &&
            requestId.matches(value)

    private fun validSummary(value: String): Boolean =
        value.encodeToByteArray().size <= MAX_SUMMARY_BYTES && '\u0000' !in value

    private fun validAgents(agents: List<DeviceAgentStatus>): Boolean =
        agents.size <= MAX_STATUS_AGENTS &&
            agents.map { it.slot }.distinct().size == agents.size &&
            agents.all { agent ->
                agent.slot in 0 until MAX_STATUS_AGENTS &&
                    (agent.label == null || (
                        agent.label.isNotEmpty() &&
                            agent.label.encodeToByteArray().size <= MAX_STATUS_LABEL_BYTES &&
                            agent.label.none { it.code < 0x20 || it.code == 0x7f }
                        ))
            }

    private fun JsonObject.string(key: String): String? {
        val primitive = this[key] as? JsonPrimitive ?: return null
        if (!primitive.isString || primitive == JsonNull) return null
        return primitive.content
    }

    private fun JsonObject.canonicalInt(key: String): Int? {
        val primitive = this[key] as? JsonPrimitive ?: return null
        if (primitive.isString || !Regex("0|[1-9][0-9]*").matches(primitive.content)) {
            return null
        }
        return primitive.intOrNull
    }
}

/**
 * Bounded newline framing for BLE notification fragments.
 *
 * The complete frame limit includes LF. Oversize input is discarded through
 * its next LF, after which later frames in the same chunk can decode normally.
 */
class DeviceLineDecoder(
    private val maximumMessageBytes: Int = DeviceMessageCodec.MAX_MESSAGE_BYTES,
) {
    private val buffer = ArrayList<Byte>(minOf(maximumMessageBytes, 512))
    private var discardingOversize = false

    init {
        require(maximumMessageBytes in 1..DeviceMessageCodec.MAX_MESSAGE_BYTES)
    }

    fun push(chunk: ByteArray): List<DeviceMessage> {
        val messages = mutableListOf<DeviceMessage>()
        for (byte in chunk) {
            if (discardingOversize) {
                if (byte == LF) {
                    discardingOversize = false
                    buffer.clear()
                }
                continue
            }
            if (byte == LF) {
                val frame = ByteArray(buffer.size + 1)
                buffer.forEachIndexed { index, value -> frame[index] = value }
                frame[frame.lastIndex] = LF
                buffer.clear()
                DeviceMessageCodec.decode(frame)?.let(messages::add)
                continue
            }
            // Reserve one byte for the required terminating LF.
            if (buffer.size + 2 > maximumMessageBytes) {
                buffer.clear()
                discardingOversize = true
                continue
            }
            buffer.add(byte)
        }
        return messages
    }

    fun reset() {
        buffer.clear()
        discardingOversize = false
    }

    private companion object {
        const val LF: Byte = 0x0a
    }
}

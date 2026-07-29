package ai.nexting.devices

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.longOrNull
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

enum class NavigationDirection(val wireValue: String) {
    PREVIOUS("prev"), NEXT("next"), UP("up"), DOWN("down"), LEFT("left"), RIGHT("right"),
}

enum class NavigationResolution(val wireValue: String) {
    SELECTED("selected"), CANCELLED("cancelled"), EXPIRED("expired"), REPLACED("replaced"),
}

enum class ControlGesture(val wireValue: String) {
    PRESS("press"), RELEASE("release"), HOLD("hold"), DOUBLE("double"),
}

enum class KeyLight(val wireValue: String) {
    OFF("off"), DIM("dim"), SOLID("solid"), PULSE("pulse"),
}

data class Rgb(val red: Int, val green: Int, val blue: Int)

data class KeyPresentation(
    val slot: Int,
    val label: String,
    val enabled: Boolean,
    val light: KeyLight,
    val rgb: Rgb? = null,
)

data class RotaryControl(
    val slot: Int,
    val label: String,
    val value: Int,
    val minimum: Int,
    val maximum: Int,
    val wrap: Boolean,
)

enum class VoiceEvent(val wireValue: String) {
    START("start"), STOP("stop"), CANCEL("cancel"),
}

enum class VoiceState(val wireValue: String) {
    IDLE("idle"), LISTENING("listening"), TRANSCRIBING("transcribing"),
    SUBMITTED("submitted"), ERROR("error"),
}

data class UsageSnapshot(
    val model: String,
    val inputTokens: Long,
    val outputTokens: Long,
    val cachedTokens: Long? = null,
    val contextUsed: Long? = null,
    val contextLimit: Long? = null,
)

sealed interface ConfigValue {
    data class BooleanValue(val value: Boolean) : ConfigValue
    data class IntegerValue(val value: Int) : ConfigValue
    data class StringValue(val value: String) : ConfigValue
}

data class ConfigEntry(val key: String, val value: ConfigValue)

enum class ConfigStatus(val wireValue: String) {
    APPLIED("applied"), REJECTED("rejected"),
}

enum class ConfigError(val wireValue: String) {
    UNKNOWN_KEY("unknown_key"), INVALID_VALUE("invalid_value"),
    STORAGE_ERROR("storage_error"), UNSUPPORTED("unsupported"),
}

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

    data class NavigationPresent(
        val requestId: String,
        val items: List<String>,
        val cursor: Int,
        val ttlMilliseconds: Int,
    ) : DeviceMessage

    data class NavigationMove(
        val requestId: String,
        val direction: NavigationDirection,
        val sequence: Long,
    ) : DeviceMessage

    data class NavigationSelect(
        val requestId: String,
        val index: Int,
        val sequence: Long,
    ) : DeviceMessage

    data class NavigationResolved(
        val requestId: String,
        val reason: NavigationResolution,
    ) : DeviceMessage

    data class Keymap(val revision: Long, val keys: List<KeyPresentation>) : DeviceMessage
    data class KeyEvent(val slot: Int, val event: ControlGesture, val sequence: Long) : DeviceMessage
    data class RotaryMap(val revision: Long, val controls: List<RotaryControl>) : DeviceMessage
    data class RotaryEvent(val slot: Int, val delta: Int, val sequence: Long) : DeviceMessage
    data class RotaryPress(val slot: Int, val event: ControlGesture, val sequence: Long) : DeviceMessage
    data class VoiceControl(val event: VoiceEvent, val sequence: Long) : DeviceMessage
    data class VoiceStatus(val state: VoiceState, val label: String? = null) : DeviceMessage
    data class Text(val channel: Int, val title: String? = null, val content: String) : DeviceMessage
    data class Usage(val snapshot: UsageSnapshot) : DeviceMessage
    data object UsageClear : DeviceMessage
    data class Config(val revision: Long, val entries: List<ConfigEntry>) : DeviceMessage
    data class ConfigResult(
        val revision: Long,
        val status: ConfigStatus,
        val code: ConfigError? = null,
    ) : DeviceMessage

    val requiredProfile: String
        get() = when (this) {
            is Present, is Answer, is Resolved, is Error ->
                DeviceMessageCodec.APPROVAL_PROFILE
            is Status -> DeviceMessageCodec.STATUS_PROFILE
            is NavigationPresent, is NavigationMove, is NavigationSelect,
            is NavigationResolved -> DeviceMessageCodec.NAVIGATION_PROFILE
            is Keymap, is KeyEvent -> DeviceMessageCodec.KEYS_PROFILE
            is RotaryMap, is RotaryEvent, is RotaryPress ->
                DeviceMessageCodec.ROTARY_PROFILE
            is VoiceControl, is VoiceStatus -> DeviceMessageCodec.VOICE_PROFILE
            is Text -> DeviceMessageCodec.TEXT_PROFILE
            is Usage, UsageClear -> DeviceMessageCodec.USAGE_PROFILE
            is Config, is ConfigResult -> DeviceMessageCodec.CONFIG_PROFILE
        }

    val interactionSequence: Pair<String, Long>?
        get() = when (this) {
            is NavigationMove -> "navigation:$requestId" to sequence
            is NavigationSelect -> "navigation:$requestId" to sequence
            is KeyEvent -> "key:$slot" to sequence
            is RotaryEvent -> "rotary:$slot" to sequence
            is RotaryPress -> "rotary:$slot" to sequence
            is VoiceControl -> "voice" to sequence
            else -> null
        }
}

object DeviceMessageCodec {
    const val WIRE_VERSION = 1
    const val APPROVAL_PROFILE = "approval/1"
    const val STATUS_PROFILE = "status/1"
    const val NAVIGATION_PROFILE = "navigation/1"
    const val KEYS_PROFILE = "keys/1"
    const val ROTARY_PROFILE = "rotary/1"
    const val VOICE_PROFILE = "voice/1"
    const val TEXT_PROFILE = "text/1"
    const val USAGE_PROFILE = "usage/1"
    const val CONFIG_PROFILE = "config/1"
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
            is DeviceMessage.NavigationPresent -> {
                if (
                    !validId(message.requestId) ||
                    !validNavigationItems(message.items) ||
                    message.cursor !in message.items.indices ||
                    message.ttlMilliseconds !in 1..MAX_TTL_MILLISECONDS
                ) return null
                buildJsonObject {
                    put("v", WIRE_VERSION)
                    put("t", "nav_present")
                    put("id", message.requestId)
                    put("items", buildJsonArray {
                        message.items.forEach { add(JsonPrimitive(it)) }
                    })
                    put("cursor", message.cursor)
                    put("ttl", message.ttlMilliseconds)
                }
            }
            is DeviceMessage.NavigationMove -> {
                if (!validId(message.requestId) || !validU32(message.sequence)) return null
                buildJsonObject {
                    put("v", WIRE_VERSION)
                    put("t", "nav_move")
                    put("id", message.requestId)
                    put("dir", message.direction.wireValue)
                    put("seq", message.sequence)
                }
            }
            is DeviceMessage.NavigationSelect -> {
                if (
                    !validId(message.requestId) || message.index !in 0..7 ||
                    !validU32(message.sequence)
                ) return null
                buildJsonObject {
                    put("v", WIRE_VERSION)
                    put("t", "nav_select")
                    put("id", message.requestId)
                    put("index", message.index)
                    put("seq", message.sequence)
                }
            }
            is DeviceMessage.NavigationResolved -> {
                if (!validId(message.requestId)) return null
                buildJsonObject {
                    put("v", WIRE_VERSION)
                    put("t", "nav_resolved")
                    put("id", message.requestId)
                    put("r", message.reason.wireValue)
                }
            }
            is DeviceMessage.Keymap -> {
                if (!validU32(message.revision) || !validKeys(message.keys)) return null
                buildJsonObject {
                    put("v", WIRE_VERSION)
                    put("t", "keymap")
                    put("rev", message.revision)
                    put("keys", buildJsonArray {
                        message.keys.forEach { key ->
                            add(buildJsonObject {
                                put("slot", key.slot)
                                put("label", key.label)
                                put("enabled", key.enabled)
                                put("light", key.light.wireValue)
                                key.rgb?.let { rgb ->
                                    put("rgb", buildJsonArray {
                                        add(JsonPrimitive(rgb.red))
                                        add(JsonPrimitive(rgb.green))
                                        add(JsonPrimitive(rgb.blue))
                                    })
                                }
                            })
                        }
                    })
                }
            }
            is DeviceMessage.KeyEvent -> {
                if (
                    message.slot !in 0..63 || !validU32(message.sequence)
                ) return null
                buildJsonObject {
                    put("v", WIRE_VERSION)
                    put("t", "key_event")
                    put("slot", message.slot)
                    put("event", message.event.wireValue)
                    put("seq", message.sequence)
                }
            }
            is DeviceMessage.RotaryMap -> {
                if (
                    !validU32(message.revision) ||
                    !validRotaryControls(message.controls)
                ) return null
                buildJsonObject {
                    put("v", WIRE_VERSION)
                    put("t", "rotary_map")
                    put("rev", message.revision)
                    put("controls", buildJsonArray {
                        message.controls.forEach { control ->
                            add(buildJsonObject {
                                put("slot", control.slot)
                                put("label", control.label)
                                put("value", control.value)
                                put("min", control.minimum)
                                put("max", control.maximum)
                                put("wrap", control.wrap)
                            })
                        }
                    })
                }
            }
            is DeviceMessage.RotaryEvent -> {
                if (
                    message.slot !in 0..15 || message.delta !in -127..127 ||
                    message.delta == 0 || !validU32(message.sequence)
                ) return null
                buildJsonObject {
                    put("v", WIRE_VERSION)
                    put("t", "rotary_event")
                    put("slot", message.slot)
                    put("delta", message.delta)
                    put("seq", message.sequence)
                }
            }
            is DeviceMessage.RotaryPress -> {
                if (message.slot !in 0..15 || !validU32(message.sequence)) return null
                buildJsonObject {
                    put("v", WIRE_VERSION)
                    put("t", "rotary_press")
                    put("slot", message.slot)
                    put("event", message.event.wireValue)
                    put("seq", message.sequence)
                }
            }
            is DeviceMessage.VoiceControl -> {
                if (!validU32(message.sequence)) return null
                buildJsonObject {
                    put("v", WIRE_VERSION)
                    put("t", "voice_event")
                    put("event", message.event.wireValue)
                    put("seq", message.sequence)
                }
            }
            is DeviceMessage.VoiceStatus -> {
                if (message.label != null && !validText(message.label, 1, 64)) return null
                buildJsonObject {
                    put("v", WIRE_VERSION)
                    put("t", "voice_state")
                    put("state", message.state.wireValue)
                    message.label?.let { put("label", it) }
                }
            }
            is DeviceMessage.Text -> {
                if (
                    message.channel !in 0..7 ||
                    !validText(message.content, 0, 1_024, allowLayout = true) ||
                    (message.title != null && !validText(message.title, 1, 64))
                ) return null
                buildJsonObject {
                    put("v", WIRE_VERSION)
                    put("t", "text")
                    put("channel", message.channel)
                    message.title?.let { put("title", it) }
                    put("content", message.content)
                }
            }
            is DeviceMessage.Usage -> {
                if (!validUsage(message.snapshot)) return null
                buildJsonObject {
                    put("v", WIRE_VERSION)
                    put("t", "usage")
                    put("model", message.snapshot.model)
                    put("input_tokens", message.snapshot.inputTokens)
                    put("output_tokens", message.snapshot.outputTokens)
                    message.snapshot.cachedTokens?.let { put("cached_tokens", it) }
                    message.snapshot.contextUsed?.let { put("context_used", it) }
                    message.snapshot.contextLimit?.let { put("context_limit", it) }
                }
            }
            DeviceMessage.UsageClear -> buildJsonObject {
                put("v", WIRE_VERSION)
                put("t", "usage_clear")
            }
            is DeviceMessage.Config -> {
                if (
                    !validU32(message.revision) ||
                    !validConfigEntries(message.entries)
                ) return null
                buildJsonObject {
                    put("v", WIRE_VERSION)
                    put("t", "config")
                    put("rev", message.revision)
                    put("entries", buildJsonArray {
                        message.entries.forEach { entry ->
                            add(buildJsonObject {
                                put("key", entry.key)
                                when (val value = entry.value) {
                                    is ConfigValue.BooleanValue -> put("value", value.value)
                                    is ConfigValue.IntegerValue -> put("value", value.value)
                                    is ConfigValue.StringValue -> put("value", value.value)
                                }
                            })
                        }
                    })
                }
            }
            is DeviceMessage.ConfigResult -> {
                if (
                    !validU32(message.revision) ||
                    (message.status == ConfigStatus.APPLIED && message.code != null) ||
                    (message.status == ConfigStatus.REJECTED && message.code == null)
                ) return null
                buildJsonObject {
                    put("v", WIRE_VERSION)
                    put("t", "config_result")
                    put("rev", message.revision)
                    put("status", message.status.wireValue)
                    message.code?.let { put("code", it.wireValue) }
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
            "nav_present" -> decodeNavigationPresent(root)
            "nav_move" -> decodeNavigationMove(root)
            "nav_select" -> decodeNavigationSelect(root)
            "nav_resolved" -> decodeNavigationResolved(root)
            "keymap" -> decodeKeymap(root)
            "key_event" -> decodeKeyEvent(root)
            "rotary_map" -> decodeRotaryMap(root)
            "rotary_event" -> decodeRotaryEvent(root)
            "rotary_press" -> decodeRotaryPress(root)
            "voice_event" -> decodeVoiceEvent(root)
            "voice_state" -> decodeVoiceState(root)
            "text" -> decodeText(root)
            "usage" -> decodeUsage(root)
            "usage_clear" -> decodeUsageClear(root)
            "config" -> decodeConfig(root)
            "config_result" -> decodeConfigResult(root)
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

    private fun decodeNavigationPresent(root: JsonObject): DeviceMessage? {
        if (!root.only("v", "t", "id", "items", "cursor", "ttl")) return null
        val id = root.string("id")?.takeIf(::validId) ?: return null
        val items = (root["items"] as? JsonArray)?.map {
            val value = it as? JsonPrimitive ?: return null
            if (!value.isString) return null
            value.content
        } ?: return null
        if (!validNavigationItems(items)) return null
        val cursor = root.canonicalInt("cursor")?.takeIf(items.indices::contains) ?: return null
        val ttl = root.canonicalInt("ttl")
            ?.takeIf { it in 1..MAX_TTL_MILLISECONDS } ?: return null
        return DeviceMessage.NavigationPresent(id, items, cursor, ttl)
    }

    private fun decodeNavigationMove(root: JsonObject): DeviceMessage? {
        if (!root.only("v", "t", "id", "dir", "seq")) return null
        val id = root.string("id")?.takeIf(::validId) ?: return null
        val direction = NavigationDirection.entries.firstOrNull {
            it.wireValue == root.string("dir")
        } ?: return null
        val sequence = root.u32("seq") ?: return null
        return DeviceMessage.NavigationMove(id, direction, sequence)
    }

    private fun decodeNavigationSelect(root: JsonObject): DeviceMessage? {
        if (!root.only("v", "t", "id", "index", "seq")) return null
        val id = root.string("id")?.takeIf(::validId) ?: return null
        val index = root.canonicalInt("index")?.takeIf { it in 0..7 } ?: return null
        val sequence = root.u32("seq") ?: return null
        return DeviceMessage.NavigationSelect(id, index, sequence)
    }

    private fun decodeNavigationResolved(root: JsonObject): DeviceMessage? {
        if (!root.only("v", "t", "id", "r")) return null
        val id = root.string("id")?.takeIf(::validId) ?: return null
        val reason = NavigationResolution.entries.firstOrNull {
            it.wireValue == root.string("r")
        } ?: return null
        return DeviceMessage.NavigationResolved(id, reason)
    }

    private fun decodeKeymap(root: JsonObject): DeviceMessage? {
        if (!root.only("v", "t", "rev", "keys")) return null
        val revision = root.u32("rev") ?: return null
        val rawKeys = root["keys"] as? JsonArray ?: return null
        val keys = rawKeys.map { raw ->
            val item = raw as? JsonObject ?: return null
            if (!item.only("slot", "label", "enabled", "light", "rgb")) return null
            val slot = item.canonicalInt("slot") ?: return null
            val label = item.string("label") ?: return null
            val enabled = item.boolean("enabled") ?: return null
            val light = KeyLight.entries.firstOrNull {
                it.wireValue == item.string("light")
            } ?: return null
            val rgb = if ("rgb" in item) {
                val values = item["rgb"] as? JsonArray ?: return null
                if (values.size != 3) return null
                Rgb(
                    (values[0] as? JsonPrimitive)?.canonicalInt() ?: return null,
                    (values[1] as? JsonPrimitive)?.canonicalInt() ?: return null,
                    (values[2] as? JsonPrimitive)?.canonicalInt() ?: return null,
                )
            } else {
                null
            }
            KeyPresentation(slot, label, enabled, light, rgb)
        }
        if (!validKeys(keys)) return null
        return DeviceMessage.Keymap(revision, keys)
    }

    private fun decodeKeyEvent(root: JsonObject): DeviceMessage? {
        if (!root.only("v", "t", "slot", "event", "seq")) return null
        val slot = root.canonicalInt("slot")?.takeIf { it in 0..63 } ?: return null
        val event = ControlGesture.entries.firstOrNull {
            it.wireValue == root.string("event")
        } ?: return null
        return DeviceMessage.KeyEvent(slot, event, root.u32("seq") ?: return null)
    }

    private fun decodeRotaryMap(root: JsonObject): DeviceMessage? {
        if (!root.only("v", "t", "rev", "controls")) return null
        val revision = root.u32("rev") ?: return null
        val rawControls = root["controls"] as? JsonArray ?: return null
        val controls = rawControls.map { raw ->
            val item = raw as? JsonObject ?: return null
            if (!item.only("slot", "label", "value", "min", "max", "wrap")) return null
            RotaryControl(
                slot = item.canonicalInt("slot") ?: return null,
                label = item.string("label") ?: return null,
                value = item.signedInt("value") ?: return null,
                minimum = item.signedInt("min") ?: return null,
                maximum = item.signedInt("max") ?: return null,
                wrap = item.boolean("wrap") ?: return null,
            )
        }
        if (!validRotaryControls(controls)) return null
        return DeviceMessage.RotaryMap(revision, controls)
    }

    private fun decodeRotaryEvent(root: JsonObject): DeviceMessage? {
        if (!root.only("v", "t", "slot", "delta", "seq")) return null
        val slot = root.canonicalInt("slot")?.takeIf { it in 0..15 } ?: return null
        val delta = root.signedInt("delta")
            ?.takeIf { it in -127..127 && it != 0 } ?: return null
        return DeviceMessage.RotaryEvent(slot, delta, root.u32("seq") ?: return null)
    }

    private fun decodeRotaryPress(root: JsonObject): DeviceMessage? {
        if (!root.only("v", "t", "slot", "event", "seq")) return null
        val slot = root.canonicalInt("slot")?.takeIf { it in 0..15 } ?: return null
        val event = ControlGesture.entries.firstOrNull {
            it.wireValue == root.string("event")
        } ?: return null
        return DeviceMessage.RotaryPress(slot, event, root.u32("seq") ?: return null)
    }

    private fun decodeVoiceEvent(root: JsonObject): DeviceMessage? {
        if (!root.only("v", "t", "event", "seq")) return null
        val event = VoiceEvent.entries.firstOrNull {
            it.wireValue == root.string("event")
        } ?: return null
        return DeviceMessage.VoiceControl(event, root.u32("seq") ?: return null)
    }

    private fun decodeVoiceState(root: JsonObject): DeviceMessage? {
        if (!root.only("v", "t", "state", "label")) return null
        val state = VoiceState.entries.firstOrNull {
            it.wireValue == root.string("state")
        } ?: return null
        val label = if ("label" in root) {
            root.string("label")?.takeIf { validText(it, 1, 64) } ?: return null
        } else {
            null
        }
        return DeviceMessage.VoiceStatus(state, label)
    }

    private fun decodeText(root: JsonObject): DeviceMessage? {
        if (!root.only("v", "t", "channel", "title", "content")) return null
        val channel = root.canonicalInt("channel")?.takeIf { it in 0..7 } ?: return null
        val title = if ("title" in root) {
            root.string("title")?.takeIf { validText(it, 1, 64) } ?: return null
        } else {
            null
        }
        val content = root.string("content")
            ?.takeIf { validText(it, 0, 1_024, allowLayout = true) } ?: return null
        return DeviceMessage.Text(channel, title, content)
    }

    private fun decodeUsage(root: JsonObject): DeviceMessage? {
        if (
            !root.only(
                "v", "t", "model", "input_tokens", "output_tokens",
                "cached_tokens", "context_used", "context_limit",
            )
        ) return null
        val snapshot = UsageSnapshot(
            model = root.string("model") ?: return null,
            inputTokens = root.safeCounter("input_tokens") ?: return null,
            outputTokens = root.safeCounter("output_tokens") ?: return null,
            cachedTokens = if ("cached_tokens" in root) {
                root.safeCounter("cached_tokens") ?: return null
            } else null,
            contextUsed = if ("context_used" in root) {
                root.safeCounter("context_used") ?: return null
            } else null,
            contextLimit = if ("context_limit" in root) {
                root.safeCounter("context_limit") ?: return null
            } else null,
        )
        if (!validUsage(snapshot)) return null
        return DeviceMessage.Usage(snapshot)
    }

    private fun decodeUsageClear(root: JsonObject): DeviceMessage? =
        DeviceMessage.UsageClear.takeIf { root.keys == setOf("v", "t") }

    private fun decodeConfig(root: JsonObject): DeviceMessage? {
        if (!root.only("v", "t", "rev", "entries")) return null
        val revision = root.u32("rev") ?: return null
        val rawEntries = root["entries"] as? JsonArray ?: return null
        val entries = rawEntries.map { raw ->
            val item = raw as? JsonObject ?: return null
            if (item.keys != setOf("key", "value")) return null
            ConfigEntry(
                item.string("key") ?: return null,
                configValue(item["value"]) ?: return null,
            )
        }
        if (!validConfigEntries(entries)) return null
        return DeviceMessage.Config(revision, entries)
    }

    private fun decodeConfigResult(root: JsonObject): DeviceMessage? {
        if (!root.only("v", "t", "rev", "status", "code")) return null
        val revision = root.u32("rev") ?: return null
        val status = ConfigStatus.entries.firstOrNull {
            it.wireValue == root.string("status")
        } ?: return null
        val code = if ("code" in root) {
            ConfigError.entries.firstOrNull {
                it.wireValue == root.string("code")
            } ?: return null
        } else {
            null
        }
        if (
            status == ConfigStatus.APPLIED && code != null ||
            status == ConfigStatus.REJECTED && code == null
        ) return null
        return DeviceMessage.ConfigResult(revision, status, code)
    }

    private fun JsonObject.only(vararg allowed: String): Boolean =
        keys.all(allowed.toSet()::contains)

    private fun validText(
        value: String,
        minimumBytes: Int,
        maximumBytes: Int,
        allowLayout: Boolean = false,
    ): Boolean {
        val size = value.encodeToByteArray().size
        return size in minimumBytes..maximumBytes &&
            value.none {
                it.code == 0x7f ||
                    it.code < 0x20 && !(allowLayout && (it == '\n' || it == '\t'))
            }
    }

    private fun validNavigationItems(items: List<String>): Boolean =
        items.size in 2..8 &&
            items.distinct().size == items.size &&
            items.all { validText(it, 1, 64) }

    private fun validU32(value: Long): Boolean = value in 0..0xffff_ffffL

    private fun validKeys(keys: List<KeyPresentation>): Boolean =
        keys.size <= 64 &&
            keys.map { it.slot }.distinct().size == keys.size &&
            keys.all { key ->
                key.slot in 0..63 &&
                    validText(key.label, 1, 32) &&
                    (key.rgb == null ||
                        listOf(key.rgb.red, key.rgb.green, key.rgb.blue).all { it in 0..255 })
            }

    private fun validRotaryControls(controls: List<RotaryControl>): Boolean =
        controls.size <= 16 &&
            controls.map { it.slot }.distinct().size == controls.size &&
            controls.all {
                it.slot in 0..15 &&
                    validText(it.label, 1, 32) &&
                    it.minimum in -1_000_000..1_000_000 &&
                    it.maximum in -1_000_000..1_000_000 &&
                    it.value in it.minimum..it.maximum
            }

    private fun validCounter(value: Long): Boolean =
        value in 0..9_007_199_254_740_991L

    private fun validUsage(snapshot: UsageSnapshot): Boolean =
        validText(snapshot.model, 1, 64) &&
            validCounter(snapshot.inputTokens) &&
            validCounter(snapshot.outputTokens) &&
            (snapshot.cachedTokens == null || validCounter(snapshot.cachedTokens)) &&
            (snapshot.contextUsed == null) == (snapshot.contextLimit == null) &&
            (
                snapshot.contextUsed == null ||
                    validCounter(snapshot.contextUsed) &&
                    validCounter(snapshot.contextLimit!!) &&
                    snapshot.contextUsed <= snapshot.contextLimit
                )

    private val configKey = Regex("[A-Za-z0-9][A-Za-z0-9._-]{0,47}")

    private fun validConfigEntries(entries: List<ConfigEntry>): Boolean =
        entries.size <= 32 &&
            entries.map { it.key }.distinct().size == entries.size &&
            entries.all { entry ->
                configKey.matches(entry.key) &&
                    when (val value = entry.value) {
                        is ConfigValue.BooleanValue -> true
                        is ConfigValue.IntegerValue -> value.value in -1_000_000..1_000_000
                        is ConfigValue.StringValue -> validText(value.value, 0, 128)
                    }
            }

    private fun configValue(raw: kotlinx.serialization.json.JsonElement?): ConfigValue? {
        val primitive = raw as? JsonPrimitive ?: return null
        if (primitive.isString) return ConfigValue.StringValue(primitive.content)
        primitive.booleanOrNull?.let { return ConfigValue.BooleanValue(it) }
        val value = primitive.content
            .takeIf { Regex("-?(0|[1-9][0-9]*)").matches(it) }
            ?.toIntOrNull() ?: return null
        return ConfigValue.IntegerValue(value)
    }

    private fun JsonObject.boolean(key: String): Boolean? {
        val primitive = this[key] as? JsonPrimitive ?: return null
        return primitive.booleanOrNull
    }

    private fun JsonObject.u32(key: String): Long? {
        val primitive = this[key] as? JsonPrimitive ?: return null
        if (primitive.isString || !Regex("0|[1-9][0-9]*").matches(primitive.content)) {
            return null
        }
        return primitive.longOrNull?.takeIf(::validU32)
    }

    private fun JsonObject.signedInt(key: String): Int? {
        val primitive = this[key] as? JsonPrimitive ?: return null
        if (
            primitive.isString ||
            !Regex("-?(0|[1-9][0-9]*)").matches(primitive.content) ||
            primitive.content == "-0"
        ) return null
        return primitive.intOrNull
    }

    private fun JsonPrimitive.canonicalInt(): Int? {
        if (isString || !Regex("0|[1-9][0-9]*").matches(content)) return null
        return intOrNull
    }

    private fun JsonObject.safeCounter(key: String): Long? {
        val primitive = this[key] as? JsonPrimitive ?: return null
        if (primitive.isString || !Regex("0|[1-9][0-9]*").matches(primitive.content)) {
            return null
        }
        return primitive.longOrNull?.takeIf(::validCounter)
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

package ai.nexting.devices

import java.nio.ByteBuffer
import java.nio.charset.CodingErrorAction
import java.nio.charset.StandardCharsets
import java.util.UUID
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.booleanOrNull
import kotlinx.serialization.json.intOrNull
import kotlinx.serialization.json.longOrNull

data class DeviceIdentity(
    val deviceId: UUID?,
    val manufacturer: String?,
    val displayName: String?,
    val serialNumber: String?,
)

data class DeviceDisplay(
    val type: String,
    val width: Int,
    val height: Int,
)

data class DeviceCapabilities(
    val buttonCount: Int?,
    val approvalButtonCount: Int?,
    val customButtonCount: Int?,
    val rotaryCount: Int?,
    val rotaryPressCount: Int?,
    val statusSlots: Int,
    val batteryService: Boolean,
    val display: DeviceDisplay?,
    val haptics: List<String>,
)

data class DeviceVendorFact(
    val key: String,
    val label: String,
    val value: String,
)

data class DeviceVendorInfo(
    val namespace: String,
    val facts: List<DeviceVendorFact>,
)

data class DeviceInfo(
    val protocolName: String,
    val spec: String,
    val wireVersions: List<Int>,
    val profiles: List<String>,
    val model: String,
    val firmwareVersion: String,
    val maxMessageBytes: Long,
    val maxSummaryBytes: Int,
    val identity: DeviceIdentity,
    val capabilities: DeviceCapabilities,
    val vendor: DeviceVendorInfo?,
) {
    val supportsApprovalV1: Boolean
        get() = protocolName == "nexting-device" &&
            wireVersions.contains(1) &&
            profiles.contains("approval/1")

    val supportsStatusV1: Boolean
        get() = supportsApprovalV1 &&
            profiles.contains("status/1") &&
            capabilities.statusSlots > 0

    fun supportsProfile(profile: String): Boolean =
        supportsApprovalV1 && profiles.contains(profile)

    val supportsNavigationV1: Boolean
        get() = supportsProfile(DeviceMessageCodec.NAVIGATION_PROFILE)
    val supportsKeysV1: Boolean
        get() = supportsProfile(DeviceMessageCodec.KEYS_PROFILE)
    val supportsRotaryV1: Boolean
        get() = supportsProfile(DeviceMessageCodec.ROTARY_PROFILE)
    val supportsVoiceV1: Boolean
        get() = supportsProfile(DeviceMessageCodec.VOICE_PROFILE)
    val supportsTextV1: Boolean
        get() = supportsProfile(DeviceMessageCodec.TEXT_PROFILE)
    val supportsUsageV1: Boolean
        get() = supportsProfile(DeviceMessageCodec.USAGE_PROFILE)
    val supportsConfigV1: Boolean
        get() = supportsProfile(DeviceMessageCodec.CONFIG_PROFILE)
}

object DeviceBattery {
    const val SERVICE_UUID = "0000180f-0000-1000-8000-00805f9b34fb"
    const val LEVEL_CHARACTERISTIC_UUID = "00002a19-0000-1000-8000-00805f9b34fb"

    fun decodeLevel(bytes: ByteArray): Int? {
        if (bytes.size != 1) return null
        return (bytes[0].toInt() and 0xff).coerceAtMost(100)
    }
}

object DeviceInfoCodec {
    const val MAX_ENCODED_BYTES = 4_096
    private const val MAX_VENDOR_BYTES = 1_024
    private val json = Json {
        ignoreUnknownKeys = true
        isLenient = false
    }
    private val canonicalUnsigned = Regex("0|[1-9][0-9]*")
    private val uuid = Regex(
        "[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[1-5][0-9a-fA-F]{3}-" +
            "[89abAB][0-9a-fA-F]{3}-[0-9a-fA-F]{12}",
    )
    private val namespace = Regex(
        "[a-z0-9](?:[a-z0-9-]{0,62}\\.)+[a-z0-9][a-z0-9-]{0,62}",
    )
    private val factKey = Regex("[A-Za-z0-9][A-Za-z0-9._-]{0,31}")
    private val inertMarkup = Regex(
        "(?:</?[a-z]|https?://|www\\.|[`*_#\\[\\]()])",
        RegexOption.IGNORE_CASE,
    )

    fun decode(bytes: ByteArray): DeviceInfo? {
        if (bytes.size > MAX_ENCODED_BYTES) return null
        val text = decodeUtf8(bytes) ?: return null
        val root = runCatching { json.parseToJsonElement(text) }.getOrNull()
            as? JsonObject ?: return null
        if (!boundedDepth(root, 0)) return null

        val protocolName = root.requiredString("protocol") ?: return null
        val spec = root.requiredString("spec") ?: return null
        val model = root.requiredString("model") ?: return null
        val firmware = root.requiredString("fw") ?: return null
        if (
            protocolName != "nexting-device" ||
            !validText(spec) ||
            !validText(model) ||
            !validText(firmware)
        ) return null

        val wire = root.requiredArray("wire")
            ?.mapNotNull { canonicalInt(it, 1, 65_535) }
            ?: return null
        if (wire.isEmpty() || wire.size > 4 || wire.distinct().size != wire.size || 1 !in wire) {
            return null
        }

        val profiles = root.requiredArray("profiles")
            ?.mapNotNull {
                (it as? JsonPrimitive)
                    ?.takeIf(JsonPrimitive::isString)
                    ?.content
            }
            ?: return null
        if (
            profiles.isEmpty() ||
            profiles.size > 16 ||
            profiles.distinct().size != profiles.size ||
            profiles.any { !validText(it, 32) } ||
            "approval/1" !in profiles
        ) return null

        val maxMessageBytes = canonicalLong(root["max_message_bytes"], 512, UInt.MAX_VALUE.toLong())
            ?: return null
        val maxSummaryBytes = canonicalInt(root["max_summary_bytes"], 1, 240)
            ?: return null

        val statusSlots = optionalInt(root, "statusSlots", 0, 8) ?: return null
        val buttonCount = optionalInt(root, "button_count", 0, 1_024) ?: return null
        val approvalButtonCount =
            optionalInt(root, "approval_button_count", 0, 1_024) ?: return null
        val customButtonCount =
            optionalInt(root, "custom_button_count", 0, 1_024) ?: return null
        val rotaryCount = optionalInt(root, "rotary_count", 0, 64) ?: return null
        val rotaryPressCount =
            optionalInt(root, "rotary_press_count", 0, 64) ?: return null
        if (
            (statusSlots.value ?: 0) > 0 && "status/1" !in profiles ||
            !fits(approvalButtonCount.value, buttonCount.value) ||
            !fits(customButtonCount.value, buttonCount.value) ||
            !fits(rotaryPressCount.value, rotaryCount.value)
        ) return null

        val batteryService = when (val raw = root["battery_service"]) {
            null -> false
            is JsonPrimitive -> raw.booleanOrNull ?: return null
            else -> return null
        }
        val display = decodeDisplay(root["display"]) ?: if ("display" in root) return null else null
        val haptics = decodeHaptics(root["haptics"]) ?: if ("haptics" in root) return null else emptyList()

        val rawDeviceId = root.optionalString("device_id") ?: if ("device_id" in root) return null else null
        val deviceId = if (rawDeviceId != null) {
            if (!uuid.matches(rawDeviceId)) return null
            runCatching { UUID.fromString(rawDeviceId) }.getOrNull() ?: return null
        } else {
            null
        }
        val manufacturer = validatedOptionalText(root, "manufacturer") ?: if ("manufacturer" in root) return null else null
        val displayName = validatedOptionalText(root, "display_name") ?: if ("display_name" in root) return null else null
        val serialNumber = validatedOptionalText(root, "serial_number") ?: if ("serial_number" in root) return null else null

        return DeviceInfo(
            protocolName = protocolName,
            spec = spec,
            wireVersions = wire,
            profiles = profiles,
            model = model,
            firmwareVersion = firmware,
            maxMessageBytes = maxMessageBytes,
            maxSummaryBytes = maxSummaryBytes,
            identity = DeviceIdentity(deviceId, manufacturer, displayName, serialNumber),
            capabilities = DeviceCapabilities(
                buttonCount = buttonCount.value,
                approvalButtonCount = approvalButtonCount.value,
                customButtonCount = customButtonCount.value,
                rotaryCount = rotaryCount.value,
                rotaryPressCount = rotaryPressCount.value,
                statusSlots = statusSlots.value ?: 0,
                batteryService = batteryService,
                display = display,
                haptics = haptics,
            ),
            vendor = decodeVendor(root["vendor"]),
        )
    }

    private fun decodeUtf8(bytes: ByteArray): String? = runCatching {
        StandardCharsets.UTF_8.newDecoder()
            .onMalformedInput(CodingErrorAction.REPORT)
            .onUnmappableCharacter(CodingErrorAction.REPORT)
            .decode(ByteBuffer.wrap(bytes))
            .toString()
    }.getOrNull()

    private fun boundedDepth(value: JsonElement, depth: Int): Boolean {
        if (depth > 8) return false
        return when (value) {
            is JsonObject -> value.values.all { boundedDepth(it, depth + 1) }
            is JsonArray -> value.all { boundedDepth(it, depth + 1) }
            else -> true
        }
    }

    private fun validText(value: String, maximumBytes: Int = 64): Boolean =
        value.isNotEmpty() &&
            value.encodeToByteArray().size <= maximumBytes &&
            value.none { it.code < 0x20 || it.code == 0x7f }

    private fun validatedOptionalText(root: JsonObject, key: String): String? =
        root.optionalString(key)?.takeIf(::validText)

    private fun canonicalInt(value: JsonElement?, minimum: Int, maximum: Int): Int? {
        val primitive = value as? JsonPrimitive ?: return null
        if (primitive.isString || !canonicalUnsigned.matches(primitive.content)) return null
        return primitive.intOrNull?.takeIf { it in minimum..maximum }
    }

    private fun canonicalLong(value: JsonElement?, minimum: Long, maximum: Long): Long? {
        val primitive = value as? JsonPrimitive ?: return null
        if (primitive.isString || !canonicalUnsigned.matches(primitive.content)) return null
        return primitive.longOrNull?.takeIf { it in minimum..maximum }
    }

    private data class OptionalInt(val value: Int?)

    private fun optionalInt(
        root: JsonObject,
        key: String,
        minimum: Int,
        maximum: Int,
    ): OptionalInt? {
        val raw = root[key] ?: return OptionalInt(null)
        return canonicalInt(raw, minimum, maximum)?.let { OptionalInt(it) }
    }

    private fun fits(value: Int?, total: Int?): Boolean =
        value == null || total == null || value <= total

    private fun decodeDisplay(raw: JsonElement?): DeviceDisplay? {
        val display = raw as? JsonObject ?: return null
        val type = display.requiredString("type")?.takeIf { validText(it, 32) } ?: return null
        val width = canonicalInt(display["width"], 1, 4_096) ?: return null
        val height = canonicalInt(display["height"], 1, 4_096) ?: return null
        return DeviceDisplay(type, width, height)
    }

    private fun decodeHaptics(raw: JsonElement?): List<String>? {
        val values = raw as? JsonArray ?: return null
        val decoded = values.mapNotNull {
            (it as? JsonPrimitive)
                ?.takeIf(JsonPrimitive::isString)
                ?.content
        }
        if (
            decoded.isEmpty() ||
            decoded.size > 8 ||
            decoded.size != values.size ||
            decoded.distinct().size != decoded.size ||
            decoded.any { !validText(it, 32) }
        ) return null
        return decoded
    }

    private fun decodeVendor(raw: JsonElement?): DeviceVendorInfo? {
        val vendor = raw as? JsonObject ?: return null
        if (vendor.toString().encodeToByteArray().size > MAX_VENDOR_BYTES) return null
        val vendorNamespace = vendor.requiredString("namespace")
            ?.takeIf { validText(it, 128) && namespace.matches(it) }
            ?: return null
        val rawFacts = vendor["facts"] as? JsonArray ?: return null
        if (rawFacts.size !in 1..16) return null
        val seen = mutableSetOf<String>()
        val facts = rawFacts.map { rawFact ->
            val fact = rawFact as? JsonObject ?: return null
            val key = fact.requiredString("key")
                ?.takeIf { factKey.matches(it) && seen.add(it) }
                ?: return null
            val label = fact.requiredString("label")
                ?.takeIf { validInertText(it, 64) }
                ?: return null
            val value = vendorValue(fact["value"])
                ?.takeIf { validInertText(it, 128) }
                ?: return null
            DeviceVendorFact(key, label, value)
        }
        return DeviceVendorInfo(vendorNamespace, facts)
    }

    private fun vendorValue(raw: JsonElement?): String? {
        val primitive = raw as? JsonPrimitive ?: return null
        if (primitive == JsonNull || primitive.booleanOrNull != null) return null
        if (primitive.isString) return primitive.content
        if (!canonicalUnsigned.matches(primitive.content)) return null
        return primitive.longOrNull?.toString()
    }

    private fun validInertText(value: String, maximumBytes: Int): Boolean =
        validText(value, maximumBytes) && !inertMarkup.containsMatchIn(value)

    private fun JsonObject.requiredString(key: String): String? {
        val primitive = this[key] as? JsonPrimitive ?: return null
        return primitive.contentOrNull.takeIf { primitive.isString }
    }

    private fun JsonObject.optionalString(key: String): String? =
        if (key !in this) null else requiredString(key)

    private fun JsonObject.requiredArray(key: String): JsonArray? = this[key] as? JsonArray

    private val JsonPrimitive.contentOrNull: String?
        get() = if (this == JsonNull) null else content
}

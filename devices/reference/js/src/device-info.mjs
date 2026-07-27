export const MAX_DEVICE_INFO_BYTES = 4_096;
export const MAX_DEVICE_INFO_STRING_BYTES = 64;
export const MAX_VENDOR_FACTS = 16;
export const MAX_VENDOR_BYTES = 1_024;

const encoder = new TextEncoder();
const decoder = new TextDecoder("utf-8", { fatal: true });
const controlCharacterPattern = /[\u0000-\u001f\u007f]/;
const uuidPattern =
  /^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;
const namespacePattern = /^[a-z0-9](?:[a-z0-9-]{0,62}\.)+[a-z0-9][a-z0-9-]{0,62}$/;
const factKeyPattern = /^[A-Za-z0-9][A-Za-z0-9._-]{0,31}$/;
const inertTextPattern = /(?:<\/?[a-z]|https?:\/\/|www\.|[`*_#[\]()])/i;

const knownFields = new Set([
  "protocol",
  "spec",
  "wire",
  "profiles",
  "model",
  "fw",
  "max_message_bytes",
  "max_summary_bytes",
  "statusSlots",
  "device_id",
  "manufacturer",
  "display_name",
  "serial_number",
  "button_count",
  "approval_button_count",
  "custom_button_count",
  "rotary_count",
  "rotary_press_count",
  "battery_service",
  "display",
  "haptics",
  "vendor",
]);

const topLevelIntegerFields = new Set([
  "max_message_bytes",
  "max_summary_bytes",
  "statusSlots",
  "button_count",
  "approval_button_count",
  "custom_button_count",
  "rotary_count",
  "rotary_press_count",
]);

function byteLength(value) {
  return encoder.encode(value).byteLength;
}

function normalizeText(raw) {
  try {
    if (typeof raw === "string") {
      if (byteLength(raw) > MAX_DEVICE_INFO_BYTES) return null;
      return raw;
    }
    if (raw instanceof Uint8Array) {
      if (raw.byteLength > MAX_DEVICE_INFO_BYTES) return null;
      return decoder.decode(raw);
    }
  } catch {
    return null;
  }
  return null;
}

function scanJSONString(text, start) {
  let index = start + 1;
  while (index < text.length) {
    const value = text[index];
    if (value === '"') return index + 1;
    if (value === "\\") {
      index += text[index + 1] === "u" ? 6 : 2;
      continue;
    }
    index += 1;
  }
  return null;
}

function canonicalUnsignedIntegerAfterKey(text, afterKey) {
  let index = afterKey;
  while (/\s/.test(text[index] ?? "")) index += 1;
  if (text[index] !== ":") return false;
  index += 1;
  while (/\s/.test(text[index] ?? "")) index += 1;
  if (text[index] === "0") index += 1;
  else {
    if (text[index] < "1" || text[index] > "9") return false;
    while (text[index] >= "0" && text[index] <= "9") index += 1;
  }
  while (/\s/.test(text[index] ?? "")) index += 1;
  return text[index] === "," || text[index] === "}";
}

function safeTopLevelEncoding(text) {
  let objectDepth = 0;
  let arrayDepth = 0;
  let expectingKey = false;
  let rootStarted = false;
  const seen = new Set();

  for (let index = 0; index < text.length;) {
    const value = text[index];
    if (value === '"') {
      const end = scanJSONString(text, index);
      if (end === null) return false;
      if (objectDepth === 1 && arrayDepth === 0 && expectingKey) {
        let key;
        try {
          key = JSON.parse(text.slice(index, end));
        } catch {
          return false;
        }
        if (knownFields.has(key)) {
          if (seen.has(key)) return false;
          seen.add(key);
        }
        if (
          topLevelIntegerFields.has(key) &&
          !canonicalUnsignedIntegerAfterKey(text, end)
        ) {
          return false;
        }
        expectingKey = false;
      }
      index = end;
      continue;
    }
    if (value === "{") {
      objectDepth += 1;
      if (!rootStarted) {
        rootStarted = true;
        expectingKey = true;
      }
    } else if (value === "}") {
      objectDepth -= 1;
    } else if (value === "[") {
      arrayDepth += 1;
    } else if (value === "]") {
      arrayDepth -= 1;
    } else if (value === "," && objectDepth === 1 && arrayDepth === 0) {
      expectingKey = true;
    }
    if (objectDepth < 0 || arrayDepth < 0 || objectDepth + arrayDepth > 9) {
      return false;
    }
    index += 1;
  }
  return rootStarted && objectDepth === 0 && arrayDepth === 0;
}

function isObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function validText(value, maxBytes = MAX_DEVICE_INFO_STRING_BYTES) {
  return (
    typeof value === "string" &&
    value.length > 0 &&
    byteLength(value) <= maxBytes &&
    !controlCharacterPattern.test(value)
  );
}

function validUniqueTextArray(value, maxItems, maxBytes) {
  return (
    Array.isArray(value) &&
    value.length >= 1 &&
    value.length <= maxItems &&
    value.every((item) => validText(item, maxBytes)) &&
    new Set(value).size === value.length
  );
}

function optionalCount(value, maximum) {
  if (value === undefined) return null;
  return Number.isInteger(value) && value >= 0 && value <= maximum
    ? value
    : undefined;
}

function normalizeDisplay(value) {
  if (value === undefined) return null;
  if (
    !isObject(value) ||
    !validText(value.type, 32) ||
    !Number.isInteger(value.width) ||
    value.width < 1 ||
    value.width > 4_096 ||
    !Number.isInteger(value.height) ||
    value.height < 1 ||
    value.height > 4_096
  ) {
    return undefined;
  }
  return { type: value.type, width: value.width, height: value.height };
}

function normalizeHaptics(value) {
  if (value === undefined) return [];
  if (!validUniqueTextArray(value, 8, 32)) return undefined;
  return [...value];
}

function normalizeVendor(value) {
  if (value === undefined) return null;
  if (!isObject(value)) return null;
  let encoded;
  try {
    encoded = JSON.stringify(value);
  } catch {
    return null;
  }
  if (byteLength(encoded) > MAX_VENDOR_BYTES) return null;
  if (
    !validText(value.namespace, 128) ||
    !namespacePattern.test(value.namespace) ||
    !Array.isArray(value.facts) ||
    value.facts.length < 1 ||
    value.facts.length > MAX_VENDOR_FACTS
  ) {
    return null;
  }

  const facts = [];
  const keys = new Set();
  for (const fact of value.facts) {
    if (!isObject(fact) || !factKeyPattern.test(fact.key ?? "")) return null;
    if (keys.has(fact.key)) return null;
    keys.add(fact.key);
    if (!validText(fact.label, 64) || inertTextPattern.test(fact.label)) return null;
    const normalizedValue = Number.isSafeInteger(fact.value)
      ? String(fact.value)
      : fact.value;
    if (
      !validText(normalizedValue, 128) ||
      inertTextPattern.test(normalizedValue)
    ) {
      return null;
    }
    facts.push({ key: fact.key, label: fact.label, value: normalizedValue });
  }
  return { namespace: value.namespace, facts };
}

export function decodeDeviceInfo(raw) {
  const text = normalizeText(raw);
  if (text === null || !safeTopLevelEncoding(text)) return null;

  let value;
  try {
    value = JSON.parse(text);
  } catch {
    return null;
  }
  if (!isObject(value)) return null;
  if (
    value.protocol !== "nexting-device" ||
    !validText(value.spec) ||
    !validUniqueTextArray(value.profiles, 16, 32) ||
    !value.profiles.includes("approval/1") ||
    !Array.isArray(value.wire) ||
    value.wire.length < 1 ||
    value.wire.length > 4 ||
    !value.wire.every(
      (item) => Number.isInteger(item) && item >= 1 && item <= 65_535,
    ) ||
    new Set(value.wire).size !== value.wire.length ||
    !value.wire.includes(1) ||
    !validText(value.model) ||
    !validText(value.fw) ||
    !Number.isInteger(value.max_message_bytes) ||
    value.max_message_bytes < 512 ||
    value.max_message_bytes > 4_294_967_295 ||
    !Number.isInteger(value.max_summary_bytes) ||
    value.max_summary_bytes < 1 ||
    value.max_summary_bytes > 240
  ) {
    return null;
  }

  const statusSlots = optionalCount(value.statusSlots, 8);
  const buttonCount = optionalCount(value.button_count, 1_024);
  const approvalButtonCount = optionalCount(value.approval_button_count, 1_024);
  const customButtonCount = optionalCount(value.custom_button_count, 1_024);
  const rotaryCount = optionalCount(value.rotary_count, 64);
  const rotaryPressCount = optionalCount(value.rotary_press_count, 64);
  const display = normalizeDisplay(value.display);
  const haptics = normalizeHaptics(value.haptics);
  if (
    [
      statusSlots,
      buttonCount,
      approvalButtonCount,
      customButtonCount,
      rotaryCount,
      rotaryPressCount,
      display,
      haptics,
    ].includes(undefined) ||
    (buttonCount !== null &&
      [approvalButtonCount, customButtonCount].some(
        (count) => count !== null && count > buttonCount,
      )) ||
    (rotaryCount !== null &&
      rotaryPressCount !== null &&
      rotaryPressCount > rotaryCount) ||
    (statusSlots > 0 && !value.profiles.includes("status/1")) ||
    (value.battery_service !== undefined &&
      typeof value.battery_service !== "boolean")
  ) {
    return null;
  }

  const optionalIdentity = [
    ["device_id", value.device_id, 36],
    ["manufacturer", value.manufacturer, 64],
    ["display_name", value.display_name, 64],
    ["serial_number", value.serial_number, 64],
  ];
  for (const [field, item, maxBytes] of optionalIdentity) {
    if (item !== undefined && !validText(item, maxBytes)) return null;
    if (field === "device_id" && item !== undefined && !uuidPattern.test(item)) {
      return null;
    }
  }

  return {
    protocolName: value.protocol,
    spec: value.spec,
    wireVersions: [...value.wire],
    profiles: [...value.profiles],
    model: value.model,
    firmwareVersion: value.fw,
    limits: {
      maxMessageBytes: value.max_message_bytes,
      maxSummaryBytes: value.max_summary_bytes,
    },
    identity: {
      deviceId: value.device_id ?? null,
      manufacturer: value.manufacturer ?? null,
      displayName: value.display_name ?? null,
      serialNumber: value.serial_number ?? null,
    },
    capabilities: {
      buttonCount,
      approvalButtonCount,
      customButtonCount,
      rotaryCount,
      rotaryPressCount,
      statusSlots: statusSlots ?? 0,
      batteryService: value.battery_service ?? false,
      display,
      haptics,
    },
    vendor: normalizeVendor(value.vendor),
  };
}

export {
  MAX_DEVICE_INFO_BYTES,
  MAX_DEVICE_INFO_STRING_BYTES,
  MAX_VENDOR_BYTES,
  MAX_VENDOR_FACTS,
  decodeDeviceInfo,
  supportsProfile,
} from "./device-info.mjs";

export const WIRE_VERSION = 1;
export const PROFILE = "approval/1";
export const STATUS_PROFILE = "status/1";
export const NAVIGATION_PROFILE = "navigation/1";
export const KEYS_PROFILE = "keys/1";
export const ROTARY_PROFILE = "rotary/1";
export const VOICE_PROFILE = "voice/1";
export const TEXT_PROFILE = "text/1";
export const USAGE_PROFILE = "usage/1";
export const CONFIG_PROFILE = "config/1";
export const INTERACTION_PROFILES = [
  NAVIGATION_PROFILE,
  KEYS_PROFILE,
  ROTARY_PROFILE,
  VOICE_PROFILE,
  TEXT_PROFILE,
  USAGE_PROFILE,
  CONFIG_PROFILE,
];
export const CHOICES = ["allow", "deny"];
export const STATUS_STATES = [
  "idle",
  "thinking",
  "working",
  "complete",
  "needs_input",
  "error",
];
export const RESOLUTION_REASONS = [
  "answered",
  "expired",
  "cancelled",
  "replaced",
];
export const ERROR_CODES = [
  "bad_message",
  "message_too_large",
  "unsupported_version",
  "unsupported_profile",
  "unknown_request",
  "not_authorized",
  "busy",
];

export const MAX_REQUEST_ID_BYTES = 64;
export const MAX_SUMMARY_BYTES = 240;
export const MAX_TTL_MS = 300_000;
export const MAX_MESSAGE_BYTES = 4_096;
export const MAX_STATUS_AGENTS = 8;
export const MAX_STATUS_LABEL_BYTES = 64;
export const MAX_SEQUENCE = 4_294_967_295;
export const MAX_SAFE_COUNTER = Number.MAX_SAFE_INTEGER;

const NAV_DIRECTIONS = ["prev", "next", "up", "down", "left", "right"];
const NAV_RESOLUTION_REASONS = ["selected", "cancelled", "expired", "replaced"];
const GESTURES = ["press", "release", "hold", "double"];
const LIGHT_STATES = ["off", "dim", "solid", "pulse"];
const VOICE_EVENTS = ["start", "stop", "cancel"];
const VOICE_STATES = ["idle", "listening", "transcribing", "submitted", "error"];
const CONFIG_STATUSES = ["applied", "rejected"];
const CONFIG_ERROR_CODES = [
  "unknown_key",
  "invalid_value",
  "storage_error",
  "unsupported",
];

const idPattern = /^[A-Za-z0-9._:-]+$/;
const encoder = new TextEncoder();
const decoder = new TextDecoder("utf-8", { fatal: true });
const knownWireFields = new Set([
  "v",
  "t",
  "id",
  "sum",
  "opt",
  "ttl",
  "ch",
  "r",
  "code",
  "agents",
  "items",
  "cursor",
  "dir",
  "seq",
  "index",
  "rev",
  "keys",
  "slot",
  "event",
  "controls",
  "delta",
  "state",
  "label",
  "channel",
  "title",
  "content",
  "model",
  "input_tokens",
  "output_tokens",
  "cached_tokens",
  "context_used",
  "context_limit",
  "entries",
  "status",
]);
const canonicalUnsignedFields = new Set([
  "v",
  "ttl",
  "seq",
  "rev",
  "cursor",
  "index",
  "slot",
  "channel",
  "input_tokens",
  "output_tokens",
  "cached_tokens",
  "context_used",
  "context_limit",
]);

// eslint-disable-next-line no-control-regex -- the protocol bans control characters on purpose
const controlCharacterPattern = /[\u0000-\u001f\u007f]/;

function byteLength(value) {
  return encoder.encode(value).byteLength;
}

function validId(value) {
  return (
    typeof value === "string" &&
    value.length > 0 &&
    byteLength(value) <= MAX_REQUEST_ID_BYTES &&
    idPattern.test(value)
  );
}

function validSummary(value) {
  return (
    typeof value === "string" &&
    !value.includes("\0") &&
    byteLength(value) <= MAX_SUMMARY_BYTES
  );
}

function validTTL(value) {
  return Number.isInteger(value) && value >= 1 && value <= MAX_TTL_MS;
}

function validFixedOptions(value) {
  return (
    Array.isArray(value) &&
    value.length === 2 &&
    value[0] === CHOICES[0] &&
    value[1] === CHOICES[1]
  );
}

function validStatusLabel(value) {
  return (
    typeof value === "string" &&
    value.length > 0 &&
    byteLength(value) <= MAX_STATUS_LABEL_BYTES &&
    !controlCharacterPattern.test(value)
  );
}

function validStatusAgents(value) {
  if (!Array.isArray(value) || value.length > MAX_STATUS_AGENTS) return false;
  const seenSlots = new Set();
  for (const agent of value) {
    if (!agent || typeof agent !== "object" || Array.isArray(agent)) return false;
    if (!Number.isInteger(agent.slot) || agent.slot < 0 || agent.slot > 7) {
      return false;
    }
    if (seenSlots.has(agent.slot)) return false;
    seenSlots.add(agent.slot);
    if (!STATUS_STATES.includes(agent.state)) return false;
    if (agent.label !== undefined && !validStatusLabel(agent.label)) return false;
  }
  return true;
}

function isObject(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function hasOnlyFields(value, fields) {
  return isObject(value) && Object.keys(value).every((field) => fields.includes(field));
}

function validBoundedText(value, minimum, maximum, allowNewlineAndTab = false) {
  if (
    typeof value !== "string" ||
    byteLength(value) < minimum ||
    byteLength(value) > maximum
  ) return false;
  const pattern = allowNewlineAndTab
    ? /[\u0000-\u0008\u000b-\u001f\u007f]/
    : controlCharacterPattern;
  return !pattern.test(value);
}

function validU32(value) {
  return Number.isInteger(value) && value >= 0 && value <= MAX_SEQUENCE;
}

function validSafeCounter(value) {
  return Number.isSafeInteger(value) && value >= 0;
}

function validUniqueSlots(value, maximumItems, maximumSlot, validator) {
  if (!Array.isArray(value) || value.length > maximumItems) return false;
  const slots = new Set();
  for (const item of value) {
    if (
      !isObject(item) ||
      !Number.isInteger(item.slot) ||
      item.slot < 0 ||
      item.slot > maximumSlot ||
      slots.has(item.slot) ||
      !validator(item)
    ) return false;
    slots.add(item.slot);
  }
  return true;
}

function validNavigationItems(value) {
  return (
    Array.isArray(value) &&
    value.length >= 2 &&
    value.length <= 8 &&
    value.every((item) => validBoundedText(item, 1, 64)) &&
    new Set(value).size === value.length
  );
}

function validKeyPresentations(value) {
  return validUniqueSlots(value, 64, 63, (item) =>
    hasOnlyFields(item, ["slot", "label", "enabled", "light", "rgb"]) &&
    validBoundedText(item.label, 1, 32) &&
    typeof item.enabled === "boolean" &&
    LIGHT_STATES.includes(item.light) &&
    (item.rgb === undefined ||
      (Array.isArray(item.rgb) &&
        item.rgb.length === 3 &&
        item.rgb.every((component) =>
          Number.isInteger(component) && component >= 0 && component <= 255))),
  );
}

function validRotaryControls(value) {
  return validUniqueSlots(value, 16, 15, (item) =>
    hasOnlyFields(item, ["slot", "label", "value", "min", "max", "wrap"]) &&
    validBoundedText(item.label, 1, 32) &&
    [item.value, item.min, item.max].every((number) =>
      Number.isInteger(number) && number >= -1_000_000 && number <= 1_000_000) &&
    item.min <= item.value &&
    item.value <= item.max &&
    typeof item.wrap === "boolean",
  );
}

const configKeyPattern = /^[A-Za-z0-9][A-Za-z0-9._-]{0,47}$/;

function validConfigValue(value) {
  return (
    typeof value === "boolean" ||
    (Number.isInteger(value) && value >= -1_000_000 && value <= 1_000_000) ||
    validBoundedText(value, 0, 128)
  );
}

function validConfigEntries(value) {
  if (!Array.isArray(value) || value.length > 32) return false;
  const keys = new Set();
  for (const entry of value) {
    if (
      !hasOnlyFields(entry, ["key", "value"]) ||
      typeof entry.key !== "string" ||
      !configKeyPattern.test(entry.key) ||
      keys.has(entry.key) ||
      !validConfigValue(entry.value)
    ) return false;
    keys.add(entry.key);
  }
  return true;
}

function normalizeRaw(raw) {
  let text;
  try {
    if (typeof raw === "string") {
      if (raw.length > MAX_MESSAGE_BYTES) return null;
      const rawByteLength = byteLength(raw);
      if (
        rawByteLength > MAX_MESSAGE_BYTES ||
        (rawByteLength === MAX_MESSAGE_BYTES && !raw.endsWith("\n"))
      ) {
        return null;
      }
      text = raw;
    } else if (raw instanceof Uint8Array) {
      if (
        raw.byteLength > MAX_MESSAGE_BYTES ||
        (raw.byteLength === MAX_MESSAGE_BYTES && raw[raw.byteLength - 1] !== 0x0a)
      ) return null;
      text = decoder.decode(raw);
    }
    else return null;
  } catch {
    return null;
  }

  if (text.endsWith("\n")) text = text.slice(0, -1);
  if (text.length === 0 || text.includes("\n") || text.includes("\r")) return null;
  return text;
}

function scanJSONString(text, start) {
  let index = start + 1;
  while (index < text.length) {
    const value = text[index];
    if (value === '"') return index + 1;
    if (value === "\0") return null;
    const codeUnit = value.charCodeAt(0);
    if (codeUnit >= 0xd800 && codeUnit <= 0xdbff) {
      const low = text.charCodeAt(index + 1);
      if (low < 0xdc00 || low > 0xdfff) return null;
      index += 2;
      continue;
    }
    if (codeUnit >= 0xdc00 && codeUnit <= 0xdfff) return null;
    if (value !== "\\") {
      index += 1;
      continue;
    }
    if (index + 1 >= text.length) return null;
    if (text[index + 1] === "u") {
      if (index + 5 >= text.length) return null;
      const highHex = text.slice(index + 2, index + 6);
      if (!/^[0-9a-f]{4}$/i.test(highHex)) return null;
      const high = Number.parseInt(highHex, 16);
      if (high === 0) return null;
      if (high >= 0xd800 && high <= 0xdbff) {
        if (text.slice(index + 6, index + 8) !== "\\u") return null;
        const lowHex = text.slice(index + 8, index + 12);
        if (!/^[0-9a-f]{4}$/i.test(lowHex)) return null;
        const low = Number.parseInt(lowHex, 16);
        if (low < 0xdc00 || low > 0xdfff) return null;
        index += 12;
      } else {
        if (high >= 0xdc00 && high <= 0xdfff) return null;
        index += 6;
      }
    } else {
      index += 2;
    }
  }
  return null;
}

function hasCanonicalUnsignedIntegerValue(text, afterKey) {
  let index = afterKey;
  while (/\s/.test(text[index] ?? "")) index += 1;
  if (text[index] !== ":") return false;
  index += 1;
  while (/\s/.test(text[index] ?? "")) index += 1;
  const start = index;
  if (text[index] < "0" || text[index] > "9") return false;
  if (text[index] === "0") index += 1;
  else while (text[index] >= "0" && text[index] <= "9") index += 1;
  if (index === start) return false;
  while (/\s/.test(text[index] ?? "")) index += 1;
  return text[index] === "," || text[index] === "}";
}

function rawObjectIsSafe(text) {
  let objectDepth = 0;
  let arrayDepth = 0;
  let expectingTopLevelKey = false;
  let rootStarted = false;
  const seenKnownFields = new Set();

  for (let index = 0; index < text.length; ) {
    const value = text[index];
    if (value === '"') {
      const end = scanJSONString(text, index);
      if (end === null) return false;
      if (objectDepth === 1 && arrayDepth === 0 && expectingTopLevelKey) {
        let key;
        try {
          key = JSON.parse(text.slice(index, end));
        } catch {
          return false;
        }
        if (knownWireFields.has(key)) {
          if (seenKnownFields.has(key)) return false;
          seenKnownFields.add(key);
        }
        if (
          canonicalUnsignedFields.has(key) &&
          !hasCanonicalUnsignedIntegerValue(text, end)
        ) return false;
        expectingTopLevelKey = false;
      }
      index = end;
      continue;
    }
    if (value === "{") {
      objectDepth += 1;
      if (!rootStarted) {
        rootStarted = true;
        expectingTopLevelKey = true;
      }
    } else if (value === "}") {
      objectDepth -= 1;
    } else if (value === "[") {
      arrayDepth += 1;
    } else if (value === "]") {
      arrayDepth -= 1;
    } else if (value === "," && objectDepth === 1 && arrayDepth === 0) {
      expectingTopLevelKey = true;
    }
    if (objectDepth + arrayDepth > 9) return false;
    index += 1;
  }

  return rootStarted && objectDepth === 0 && arrayDepth === 0;
}

export function encode(message) {
  if (!message || typeof message !== "object") return null;

  let wire;
  switch (message.type) {
    case "present":
      if (
        !validId(message.requestId) ||
        !validSummary(message.summary) ||
        !validFixedOptions(message.options) ||
        !validTTL(message.ttlMs)
      ) {
        return null;
      }
      wire = {
        v: WIRE_VERSION,
        t: "present",
        id: message.requestId,
        sum: message.summary,
        opt: CHOICES,
        ttl: message.ttlMs,
      };
      break;
    case "answer":
      if (!validId(message.requestId) || !CHOICES.includes(message.choice)) return null;
      wire = {
        v: WIRE_VERSION,
        t: "answer",
        id: message.requestId,
        ch: message.choice,
      };
      break;
    case "resolved":
      if (
        !validId(message.requestId) ||
        !RESOLUTION_REASONS.includes(message.reason)
      ) {
        return null;
      }
      wire = {
        v: WIRE_VERSION,
        t: "resolved",
        id: message.requestId,
        r: message.reason,
      };
      break;
    case "error": {
      const hasId = message.requestId !== null && message.requestId !== undefined;
      if ((hasId && !validId(message.requestId)) || !ERROR_CODES.includes(message.code)) {
        return null;
      }
      wire = { v: WIRE_VERSION, t: "error" };
      if (hasId) wire.id = message.requestId;
      wire.code = message.code;
      break;
    }
    case "status": {
      if (!validStatusAgents(message.agents)) return null;
      wire = {
        v: WIRE_VERSION,
        t: "status",
        agents: message.agents.map((agent) => {
          const wireAgent = { slot: agent.slot, state: agent.state };
          if (agent.label !== undefined) wireAgent.label = agent.label;
          return wireAgent;
        }),
      };
      break;
    }
    case "navPresent":
      if (
        !validId(message.requestId) ||
        !validNavigationItems(message.items) ||
        !Number.isInteger(message.cursor) ||
        message.cursor < 0 ||
        message.cursor >= message.items.length ||
        !validTTL(message.ttlMs)
      ) return null;
      wire = {
        v: WIRE_VERSION,
        t: "nav_present",
        id: message.requestId,
        items: message.items,
        cursor: message.cursor,
        ttl: message.ttlMs,
      };
      break;
    case "navMove":
      if (
        !validId(message.requestId) ||
        !NAV_DIRECTIONS.includes(message.direction) ||
        !validU32(message.sequence)
      ) return null;
      wire = {
        v: WIRE_VERSION,
        t: "nav_move",
        id: message.requestId,
        dir: message.direction,
        seq: message.sequence,
      };
      break;
    case "navSelect":
      if (
        !validId(message.requestId) ||
        !Number.isInteger(message.index) ||
        message.index < 0 ||
        message.index > 7 ||
        !validU32(message.sequence)
      ) return null;
      wire = {
        v: WIRE_VERSION,
        t: "nav_select",
        id: message.requestId,
        index: message.index,
        seq: message.sequence,
      };
      break;
    case "navResolved":
      if (
        !validId(message.requestId) ||
        !NAV_RESOLUTION_REASONS.includes(message.reason)
      ) return null;
      wire = {
        v: WIRE_VERSION,
        t: "nav_resolved",
        id: message.requestId,
        r: message.reason,
      };
      break;
    case "keymap":
      if (!validU32(message.revision) || !validKeyPresentations(message.keys)) {
        return null;
      }
      wire = {
        v: WIRE_VERSION,
        t: "keymap",
        rev: message.revision,
        keys: message.keys,
      };
      break;
    case "keyEvent":
      if (
        !Number.isInteger(message.slot) ||
        message.slot < 0 ||
        message.slot > 63 ||
        !GESTURES.includes(message.event) ||
        !validU32(message.sequence)
      ) return null;
      wire = {
        v: WIRE_VERSION,
        t: "key_event",
        slot: message.slot,
        event: message.event,
        seq: message.sequence,
      };
      break;
    case "rotaryMap":
      if (!validU32(message.revision) || !Array.isArray(message.controls)) return null;
      {
        const controls = message.controls.map((item) => ({
          slot: item.slot,
          label: item.label,
          value: item.value,
          min: item.minimum,
          max: item.maximum,
          wrap: item.wrap,
        }));
        if (!validRotaryControls(controls)) return null;
        wire = {
          v: WIRE_VERSION,
          t: "rotary_map",
          rev: message.revision,
          controls,
        };
      }
      break;
    case "rotaryEvent":
      if (
        !Number.isInteger(message.slot) ||
        message.slot < 0 ||
        message.slot > 15 ||
        !Number.isInteger(message.delta) ||
        message.delta === 0 ||
        message.delta < -127 ||
        message.delta > 127 ||
        !validU32(message.sequence)
      ) return null;
      wire = {
        v: WIRE_VERSION,
        t: "rotary_event",
        slot: message.slot,
        delta: message.delta,
        seq: message.sequence,
      };
      break;
    case "rotaryPress":
      if (
        !Number.isInteger(message.slot) ||
        message.slot < 0 ||
        message.slot > 15 ||
        !GESTURES.includes(message.event) ||
        !validU32(message.sequence)
      ) return null;
      wire = {
        v: WIRE_VERSION,
        t: "rotary_press",
        slot: message.slot,
        event: message.event,
        seq: message.sequence,
      };
      break;
    case "voiceEvent":
      if (!VOICE_EVENTS.includes(message.event) || !validU32(message.sequence)) {
        return null;
      }
      wire = {
        v: WIRE_VERSION,
        t: "voice_event",
        event: message.event,
        seq: message.sequence,
      };
      break;
    case "voiceState":
      if (
        !VOICE_STATES.includes(message.state) ||
        (message.label !== undefined &&
          !validBoundedText(message.label, 1, 64))
      ) return null;
      wire = { v: WIRE_VERSION, t: "voice_state", state: message.state };
      if (message.label !== undefined) wire.label = message.label;
      break;
    case "text":
      if (
        !Number.isInteger(message.channel) ||
        message.channel < 0 ||
        message.channel > 7 ||
        (message.title !== undefined &&
          !validBoundedText(message.title, 1, 64)) ||
        !validBoundedText(message.content, 0, 1024, true)
      ) return null;
      wire = { v: WIRE_VERSION, t: "text", channel: message.channel };
      if (message.title !== undefined) wire.title = message.title;
      wire.content = message.content;
      break;
    case "usage":
      if (
        !validBoundedText(message.model, 1, 64) ||
        !validSafeCounter(message.inputTokens) ||
        !validSafeCounter(message.outputTokens) ||
        (message.cachedTokens !== undefined &&
          !validSafeCounter(message.cachedTokens)) ||
        ((message.contextUsed === undefined) !==
          (message.contextLimit === undefined)) ||
        (message.contextUsed !== undefined &&
          (!validSafeCounter(message.contextUsed) ||
            !validSafeCounter(message.contextLimit) ||
            message.contextUsed > message.contextLimit))
      ) return null;
      wire = {
        v: WIRE_VERSION,
        t: "usage",
        model: message.model,
        input_tokens: message.inputTokens,
        output_tokens: message.outputTokens,
      };
      if (message.cachedTokens !== undefined) {
        wire.cached_tokens = message.cachedTokens;
      }
      if (message.contextUsed !== undefined) {
        wire.context_used = message.contextUsed;
        wire.context_limit = message.contextLimit;
      }
      break;
    case "usageClear":
      wire = { v: WIRE_VERSION, t: "usage_clear" };
      break;
    case "config":
      if (!validU32(message.revision) || !validConfigEntries(message.entries)) {
        return null;
      }
      wire = {
        v: WIRE_VERSION,
        t: "config",
        rev: message.revision,
        entries: message.entries,
      };
      break;
    case "configResult":
      if (
        !validU32(message.revision) ||
        !CONFIG_STATUSES.includes(message.status) ||
        (message.status === "applied" && message.code !== undefined) ||
        (message.status === "rejected" &&
          !CONFIG_ERROR_CODES.includes(message.code))
      ) return null;
      wire = {
        v: WIRE_VERSION,
        t: "config_result",
        rev: message.revision,
        status: message.status,
      };
      if (message.code !== undefined) wire.code = message.code;
      break;
    default:
      return null;
  }

  return `${JSON.stringify(wire)}\n`;
}

export function decode(raw) {
  const text = normalizeRaw(raw);
  if (text === null || !rawObjectIsSafe(text)) return null;

  let wire;
  try {
    wire = JSON.parse(text);
  } catch {
    return null;
  }
  if (!wire || typeof wire !== "object" || Array.isArray(wire)) return null;
  if (wire.v !== WIRE_VERSION || typeof wire.t !== "string") return null;

  switch (wire.t) {
    case "present":
      if (
        !validId(wire.id) ||
        !validSummary(wire.sum) ||
        !validFixedOptions(wire.opt) ||
        !validTTL(wire.ttl)
      ) {
        return null;
      }
      return {
        type: "present",
        requestId: wire.id,
        summary: wire.sum,
        options: [...CHOICES],
        ttlMs: wire.ttl,
      };
    case "answer":
      if (!validId(wire.id) || !CHOICES.includes(wire.ch)) return null;
      return { type: "answer", requestId: wire.id, choice: wire.ch };
    case "resolved":
      if (!validId(wire.id) || !RESOLUTION_REASONS.includes(wire.r)) return null;
      return { type: "resolved", requestId: wire.id, reason: wire.r };
    case "error": {
      const hasId = Object.hasOwn(wire, "id");
      if ((hasId && !validId(wire.id)) || !ERROR_CODES.includes(wire.code)) return null;
      return {
        type: "error",
        requestId: hasId ? wire.id : null,
        code: wire.code,
      };
    }
    case "status": {
      if (!validStatusAgents(wire.agents)) return null;
      return {
        type: "status",
        agents: wire.agents.map((agent) => {
          const decoded = { slot: agent.slot, state: agent.state };
          if (agent.label !== undefined) decoded.label = agent.label;
          return decoded;
        }),
      };
    }
    case "nav_present":
      if (
        !hasOnlyFields(wire, ["v", "t", "id", "items", "cursor", "ttl"]) ||
        !validId(wire.id) ||
        !validNavigationItems(wire.items) ||
        !Number.isInteger(wire.cursor) ||
        wire.cursor < 0 ||
        wire.cursor >= wire.items.length ||
        !validTTL(wire.ttl)
      ) return null;
      return {
        type: "navPresent",
        requestId: wire.id,
        items: [...wire.items],
        cursor: wire.cursor,
        ttlMs: wire.ttl,
      };
    case "nav_move":
      if (
        !hasOnlyFields(wire, ["v", "t", "id", "dir", "seq"]) ||
        !validId(wire.id) ||
        !NAV_DIRECTIONS.includes(wire.dir) ||
        !validU32(wire.seq)
      ) return null;
      return {
        type: "navMove",
        requestId: wire.id,
        direction: wire.dir,
        sequence: wire.seq,
      };
    case "nav_select":
      if (
        !hasOnlyFields(wire, ["v", "t", "id", "index", "seq"]) ||
        !validId(wire.id) ||
        !Number.isInteger(wire.index) ||
        wire.index < 0 ||
        wire.index > 7 ||
        !validU32(wire.seq)
      ) return null;
      return {
        type: "navSelect",
        requestId: wire.id,
        index: wire.index,
        sequence: wire.seq,
      };
    case "nav_resolved":
      if (
        !hasOnlyFields(wire, ["v", "t", "id", "r"]) ||
        !validId(wire.id) ||
        !NAV_RESOLUTION_REASONS.includes(wire.r)
      ) return null;
      return {
        type: "navResolved",
        requestId: wire.id,
        reason: wire.r,
      };
    case "keymap":
      if (
        !hasOnlyFields(wire, ["v", "t", "rev", "keys"]) ||
        !validU32(wire.rev) ||
        !validKeyPresentations(wire.keys)
      ) return null;
      return {
        type: "keymap",
        revision: wire.rev,
        keys: wire.keys.map((key) => ({ ...key, ...(key.rgb ? { rgb: [...key.rgb] } : {}) })),
      };
    case "key_event":
      if (
        !hasOnlyFields(wire, ["v", "t", "slot", "event", "seq"]) ||
        !Number.isInteger(wire.slot) ||
        wire.slot < 0 ||
        wire.slot > 63 ||
        !GESTURES.includes(wire.event) ||
        !validU32(wire.seq)
      ) return null;
      return {
        type: "keyEvent",
        slot: wire.slot,
        event: wire.event,
        sequence: wire.seq,
      };
    case "rotary_map":
      if (
        !hasOnlyFields(wire, ["v", "t", "rev", "controls"]) ||
        !validU32(wire.rev) ||
        !validRotaryControls(wire.controls)
      ) return null;
      return {
        type: "rotaryMap",
        revision: wire.rev,
        controls: wire.controls.map((control) => ({
          slot: control.slot,
          label: control.label,
          value: control.value,
          minimum: control.min,
          maximum: control.max,
          wrap: control.wrap,
        })),
      };
    case "rotary_event":
      if (
        !hasOnlyFields(wire, ["v", "t", "slot", "delta", "seq"]) ||
        !Number.isInteger(wire.slot) ||
        wire.slot < 0 ||
        wire.slot > 15 ||
        !Number.isInteger(wire.delta) ||
        wire.delta === 0 ||
        wire.delta < -127 ||
        wire.delta > 127 ||
        !validU32(wire.seq)
      ) return null;
      return {
        type: "rotaryEvent",
        slot: wire.slot,
        delta: wire.delta,
        sequence: wire.seq,
      };
    case "rotary_press":
      if (
        !hasOnlyFields(wire, ["v", "t", "slot", "event", "seq"]) ||
        !Number.isInteger(wire.slot) ||
        wire.slot < 0 ||
        wire.slot > 15 ||
        !GESTURES.includes(wire.event) ||
        !validU32(wire.seq)
      ) return null;
      return {
        type: "rotaryPress",
        slot: wire.slot,
        event: wire.event,
        sequence: wire.seq,
      };
    case "voice_event":
      if (
        !hasOnlyFields(wire, ["v", "t", "event", "seq"]) ||
        !VOICE_EVENTS.includes(wire.event) ||
        !validU32(wire.seq)
      ) return null;
      return {
        type: "voiceEvent",
        event: wire.event,
        sequence: wire.seq,
      };
    case "voice_state": {
      if (
        !hasOnlyFields(wire, ["v", "t", "state", "label"]) ||
        !VOICE_STATES.includes(wire.state) ||
        (wire.label !== undefined && !validBoundedText(wire.label, 1, 64))
      ) return null;
      const decoded = { type: "voiceState", state: wire.state };
      if (wire.label !== undefined) decoded.label = wire.label;
      return decoded;
    }
    case "text": {
      if (
        !hasOnlyFields(wire, ["v", "t", "channel", "title", "content"]) ||
        !Number.isInteger(wire.channel) ||
        wire.channel < 0 ||
        wire.channel > 7 ||
        (wire.title !== undefined && !validBoundedText(wire.title, 1, 64)) ||
        !validBoundedText(wire.content, 0, 1024, true)
      ) return null;
      const decoded = { type: "text", channel: wire.channel };
      if (wire.title !== undefined) decoded.title = wire.title;
      decoded.content = wire.content;
      return decoded;
    }
    case "usage": {
      if (
        !hasOnlyFields(wire, [
          "v",
          "t",
          "model",
          "input_tokens",
          "output_tokens",
          "cached_tokens",
          "context_used",
          "context_limit",
        ]) ||
        !validBoundedText(wire.model, 1, 64) ||
        !validSafeCounter(wire.input_tokens) ||
        !validSafeCounter(wire.output_tokens) ||
        (wire.cached_tokens !== undefined &&
          !validSafeCounter(wire.cached_tokens)) ||
        ((wire.context_used === undefined) !==
          (wire.context_limit === undefined)) ||
        (wire.context_used !== undefined &&
          (!validSafeCounter(wire.context_used) ||
            !validSafeCounter(wire.context_limit) ||
            wire.context_used > wire.context_limit))
      ) return null;
      const decoded = {
        type: "usage",
        model: wire.model,
        inputTokens: wire.input_tokens,
        outputTokens: wire.output_tokens,
      };
      if (wire.cached_tokens !== undefined) decoded.cachedTokens = wire.cached_tokens;
      if (wire.context_used !== undefined) {
        decoded.contextUsed = wire.context_used;
        decoded.contextLimit = wire.context_limit;
      }
      return decoded;
    }
    case "usage_clear":
      if (!hasOnlyFields(wire, ["v", "t"])) return null;
      return { type: "usageClear" };
    case "config":
      if (
        !hasOnlyFields(wire, ["v", "t", "rev", "entries"]) ||
        !validU32(wire.rev) ||
        !validConfigEntries(wire.entries)
      ) return null;
      return {
        type: "config",
        revision: wire.rev,
        entries: wire.entries.map((entry) => ({ ...entry })),
      };
    case "config_result": {
      if (
        !hasOnlyFields(wire, ["v", "t", "rev", "status", "code"]) ||
        !validU32(wire.rev) ||
        !CONFIG_STATUSES.includes(wire.status) ||
        (wire.status === "applied" && wire.code !== undefined) ||
        (wire.status === "rejected" && !CONFIG_ERROR_CODES.includes(wire.code))
      ) return null;
      const decoded = {
        type: "configResult",
        revision: wire.rev,
        status: wire.status,
      };
      if (wire.code !== undefined) decoded.code = wire.code;
      return decoded;
    }
    default:
      return null;
  }
}

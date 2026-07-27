export {
  MAX_DEVICE_INFO_BYTES,
  MAX_DEVICE_INFO_STRING_BYTES,
  MAX_VENDOR_BYTES,
  MAX_VENDOR_FACTS,
  decodeDeviceInfo,
} from "./device-info.mjs";

export const WIRE_VERSION = 1;
export const PROFILE = "approval/1";
export const STATUS_PROFILE = "status/1";
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
          (key === "v" || key === "ttl") &&
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
    default:
      return null;
  }
}

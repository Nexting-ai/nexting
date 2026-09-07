import { hasUniqueObjectKeys } from "./strict-json.mjs";

export const COMPATIBILITY_TUNNEL_VERSION = 1;

const descriptorFields = [
  "version",
  "integration_id",
  "display_name",
  "discovery",
  "envelope",
  "channels",
  "requirements",
];
const channelNames = ["control_down", "control_up", "audio_up", "flow_down"];
const normalizedNames = {
  control_down: "controlDown",
  control_up: "controlUp",
  audio_up: "audioUp",
  flow_down: "flowDown",
};
const uuidPattern =
  /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
const integrationPattern = /^[a-z0-9][a-z0-9-]{0,31}(?:\.[a-z0-9][a-z0-9-]{0,31})+$/;

function isRecord(value) {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function exactFields(value, fields) {
  return isRecord(value) &&
    Object.keys(value).length === fields.length &&
    Object.keys(value).every((field) => fields.includes(field));
}

function boundedText(value, maximumBytes) {
  return (
    typeof value === "string" &&
    value.length > 0 &&
    new TextEncoder().encode(value).length <= maximumBytes &&
    !/[\u0000-\u001f\u007f]/.test(value)
  );
}

export function parseTransportDescriptor(value) {
  if (typeof value === "string") {
    if (!hasUniqueObjectKeys(value, 8192)) {
      return null;
    }
    try {
      value = JSON.parse(value);
    } catch {
      return null;
    }
  }
  if (
    !exactFields(value, descriptorFields) ||
    value.version !== COMPATIBILITY_TUNNEL_VERSION ||
    typeof value.integration_id !== "string" ||
    !integrationPattern.test(value.integration_id) ||
    !boundedText(value.display_name, 64) ||
    !exactFields(value.discovery, ["service_uuid"]) ||
    !uuidPattern.test(value.discovery.service_uuid) ||
    !exactFields(value.envelope, ["strategy", "max_payload_bytes"]) ||
    value.envelope.strategy !== "opcode-prefix-v1" ||
    !Number.isInteger(value.envelope.max_payload_bytes) ||
    value.envelope.max_payload_bytes < 1 ||
    value.envelope.max_payload_bytes > 511 ||
    !exactFields(value.channels, channelNames) ||
    !exactFields(value.requirements, ["bonded", "encrypted", "min_att_payload"]) ||
    value.requirements.bonded !== true ||
    value.requirements.encrypted !== true ||
    !Number.isInteger(value.requirements.min_att_payload) ||
    value.requirements.min_att_payload < 64 ||
    value.requirements.min_att_payload > 512
  ) {
    return null;
  }

  const channels = {};
  const opcodes = new Set();
  for (const name of channelNames) {
    const channel = value.channels[name];
    const expectedProperty = name.endsWith("_up") ? "notify" : "write";
    if (
      !exactFields(channel, ["characteristic_uuid", "property", "opcode"]) ||
      !uuidPattern.test(channel.characteristic_uuid) ||
      channel.property !== expectedProperty ||
      !Number.isInteger(channel.opcode) ||
      channel.opcode < 0 ||
      channel.opcode > 255 ||
      opcodes.has(channel.opcode)
    ) {
      return null;
    }
    opcodes.add(channel.opcode);
    channels[normalizedNames[name]] = {
      characteristicUuid: channel.characteristic_uuid.toLowerCase(),
      property: channel.property,
      opcode: channel.opcode,
    };
  }

  return {
    version: value.version,
    integrationId: value.integration_id,
    displayName: value.display_name,
    serviceUuid: value.discovery.service_uuid.toLowerCase(),
    strategy: value.envelope.strategy,
    maxPayloadBytes: value.envelope.max_payload_bytes,
    channels,
    requirements: {
      bonded: true,
      encrypted: true,
      minAttPayload: value.requirements.min_att_payload,
    },
  };
}

export function encodeTunnelPacket(descriptor, channelName, payload) {
  const channel = descriptor?.channels?.[channelName];
  if (
    !channel ||
    channel.property !== "write" ||
    !(payload instanceof Uint8Array) ||
    payload.length > descriptor.maxPayloadBytes
  ) {
    throw new RangeError("invalid tunnel downlink packet");
  }
  const packet = new Uint8Array(payload.length + 1);
  packet[0] = channel.opcode;
  packet.set(payload, 1);
  return packet;
}

export function parseTunnelPacket(descriptor, packet) {
  if (
    !(packet instanceof Uint8Array) ||
    packet.length < 1 ||
    packet.length - 1 > descriptor?.maxPayloadBytes
  ) {
    return null;
  }
  const match = Object.entries(descriptor.channels).find(
    ([, channel]) => channel.property === "notify" && channel.opcode === packet[0],
  );
  return match ? { channel: match[0], payload: packet.slice(1) } : null;
}

import { decode, MAX_MESSAGE_BYTES } from "./protocol.mjs";

const encoder = new TextEncoder();

function asBytes(chunk) {
  if (typeof chunk === "string") return encoder.encode(chunk);
  if (chunk instanceof Uint8Array) return chunk;
  return null;
}

export function createLineDecoder({ maxMessageBytes = 4096 } = {}) {
  if (
    !Number.isInteger(maxMessageBytes) ||
    maxMessageBytes < 1 ||
    maxMessageBytes > MAX_MESSAGE_BYTES
  ) {
    throw new RangeError(`maxMessageBytes must be an integer from 1 to ${MAX_MESSAGE_BYTES}`);
  }

  let buffer = [];
  let discarding = false;

  return {
    push(chunk) {
      if (
        (typeof chunk === "string" && chunk.length > maxMessageBytes) ||
        (chunk instanceof Uint8Array && chunk.byteLength > maxMessageBytes)
      ) {
        buffer = [];
        discarding = false;
        return [{ type: "error", requestId: null, code: "message_too_large" }];
      }
      const bytes = asBytes(chunk);
      if (bytes === null) {
        return [{ type: "error", requestId: null, code: "bad_message" }];
      }
      if (bytes.byteLength > maxMessageBytes) {
        buffer = [];
        discarding = false;
        return [{ type: "error", requestId: null, code: "message_too_large" }];
      }

      const messages = [];
      for (const byte of bytes) {
        if (discarding) {
          if (byte === 0x0a) discarding = false;
          continue;
        }

        if (byte === 0x0a) {
          const message = decode(Uint8Array.from(buffer));
          buffer = [];
          messages.push(
            message ?? { type: "error", requestId: null, code: "bad_message" },
          );
          continue;
        }

        if (buffer.length >= maxMessageBytes - 1) {
          buffer = [];
          discarding = true;
          messages.push({
            type: "error",
            requestId: null,
            code: "message_too_large",
          });
          continue;
        }

        buffer.push(byte);
      }
      return messages;
    },

    reset() {
      buffer = [];
      discarding = false;
    },
  };
}

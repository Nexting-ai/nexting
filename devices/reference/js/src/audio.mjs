export const DEVICE_AUDIO_PROFILE = "device-audio/1";
export const DEVICE_AUDIO_WIRE_VERSION = 1;
export const DEVICE_AUDIO_CODEC_IMA_ADPCM = 1;
export const DEVICE_AUDIO_SAMPLE_RATE = 16_000;
export const DEVICE_AUDIO_FRAME_MS = 20;
export const DEVICE_AUDIO_SAMPLES_PER_FRAME = 320;
export const DEVICE_AUDIO_PAYLOAD_BYTES = 160;
export const DEVICE_AUDIO_MAX_FRAMES = 6_000;
export const DEVICE_AUDIO_MAX_SAMPLES = 1_920_000;
export const AUDIO_FRAME_HEADER_BYTES = 28;
export const AUDIO_FRAME_BYTES =
  AUDIO_FRAME_HEADER_BYTES + DEVICE_AUDIO_PAYLOAD_BYTES + 4;
export const AUDIO_FRAGMENT_HEADER_BYTES = 12;
export const MAX_AUDIO_FRAGMENTS = 4;

const stepTable = [
  7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37,
  41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173,
  190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
  724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
  2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894,
  6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289,
  16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
];
const indexTable = [-1, -1, -1, -1, 2, 4, 6, 8];

function clamp(value, minimum, maximum) {
  return Math.max(minimum, Math.min(maximum, value));
}

function isU32(value) {
  return Number.isInteger(value) && value >= 0 && value <= 0xffff_ffff;
}

function isValidFramePosition(sequence, sampleIndex) {
  return (
    isU32(sequence) &&
    sequence < DEVICE_AUDIO_MAX_FRAMES &&
    isU32(sampleIndex) &&
    sampleIndex === sequence * DEVICE_AUDIO_SAMPLES_PER_FRAME
  );
}

function updatePredictor(predictor, step, code) {
  let difference = step >> 3;
  if (code & 4) difference += step;
  if (code & 2) difference += step >> 1;
  if (code & 1) difference += step >> 2;
  return clamp(predictor + (code & 8 ? -difference : difference), -32768, 32767);
}

export function crc32c(bytes) {
  if (!(bytes instanceof Uint8Array)) throw new TypeError("bytes must be Uint8Array");
  let crc = 0xffff_ffff;
  for (const byte of bytes) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit += 1) {
      crc = (crc >>> 1) ^ (crc & 1 ? 0x82f6_3b78 : 0);
    }
  }
  return (crc ^ 0xffff_ffff) >>> 0;
}

export function encodeImaAdpcmBlock(samples, stepIndex = 0) {
  if (!(samples instanceof Int16Array) || samples.length !== 320) {
    throw new RangeError("IMA ADPCM blocks require exactly 320 PCM16 samples");
  }
  if (!Number.isInteger(stepIndex) || stepIndex < 0 || stepIndex > 88) {
    throw new RangeError("stepIndex must be between 0 and 88");
  }

  const predictor = samples[0];
  let currentPredictor = predictor;
  let currentIndex = stepIndex;
  const payload = new Uint8Array(DEVICE_AUDIO_PAYLOAD_BYTES);
  for (let sampleIndex = 1; sampleIndex < samples.length; sampleIndex += 1) {
    const step = stepTable[currentIndex];
    let difference = samples[sampleIndex] - currentPredictor;
    let code = 0;
    if (difference < 0) {
      code = 8;
      difference = -difference;
    }
    let threshold = step;
    if (difference >= threshold) {
      code |= 4;
      difference -= threshold;
    }
    threshold >>= 1;
    if (difference >= threshold) {
      code |= 2;
      difference -= threshold;
    }
    threshold >>= 1;
    if (difference >= threshold) code |= 1;

    currentPredictor = updatePredictor(currentPredictor, step, code);
    currentIndex = clamp(currentIndex + indexTable[code & 7], 0, 88);
    const codeIndex = sampleIndex - 1;
    const byteIndex = codeIndex >> 1;
    if ((codeIndex & 1) === 0) payload[byteIndex] = code;
    else payload[byteIndex] |= code << 4;
  }
  return { predictor, stepIndex, payload };
}

export function decodeImaAdpcmBlock({ predictor, stepIndex, payload, sampleCount }) {
  if (
    !Number.isInteger(predictor) ||
    predictor < -32768 ||
    predictor > 32767 ||
    !Number.isInteger(stepIndex) ||
    stepIndex < 0 ||
    stepIndex > 88 ||
    !(payload instanceof Uint8Array) ||
    !Number.isInteger(sampleCount) ||
    sampleCount < 1 ||
    sampleCount > 320 ||
    payload.length !== Math.ceil((sampleCount - 1) / 2)
  ) {
    throw new RangeError("invalid IMA ADPCM block");
  }
  if ((sampleCount - 1) % 2 === 1 && (payload.at(-1) & 0xf0) !== 0) {
    throw new RangeError("unused ADPCM high nibble must be zero");
  }

  const samples = new Int16Array(sampleCount);
  samples[0] = predictor;
  let currentPredictor = predictor;
  let currentIndex = stepIndex;
  for (let sampleIndex = 1; sampleIndex < sampleCount; sampleIndex += 1) {
    const codeIndex = sampleIndex - 1;
    const packed = payload[codeIndex >> 1];
    const code = codeIndex & 1 ? packed >> 4 : packed & 0x0f;
    currentPredictor = updatePredictor(
      currentPredictor,
      stepTable[currentIndex],
      code,
    );
    currentIndex = clamp(currentIndex + indexTable[code & 7], 0, 88);
    samples[sampleIndex] = currentPredictor;
  }
  return samples;
}

export function encodeAudioFrame({
  epoch,
  streamId,
  sequence,
  sampleIndex,
  samples,
  stepIndex = 0,
}) {
  if (
    !isU32(epoch) ||
    !isU32(streamId) ||
    !isValidFramePosition(sequence, sampleIndex)
  ) {
    throw new RangeError("audio frame counters must be unsigned 32-bit integers");
  }
  const block = encodeImaAdpcmBlock(samples, stepIndex);
  const frame = new Uint8Array(AUDIO_FRAME_BYTES);
  const view = new DataView(frame.buffer);
  frame[0] = DEVICE_AUDIO_WIRE_VERSION;
  frame[1] = 1;
  frame[2] = DEVICE_AUDIO_CODEC_IMA_ADPCM;
  frame[3] = 0;
  view.setUint32(4, epoch, true);
  view.setUint32(8, streamId, true);
  view.setUint32(12, sequence, true);
  view.setUint32(16, sampleIndex, true);
  view.setUint16(20, samples.length, true);
  view.setInt16(22, block.predictor, true);
  frame[24] = block.stepIndex;
  frame[25] = 0;
  view.setUint16(26, block.payload.length, true);
  frame.set(block.payload, AUDIO_FRAME_HEADER_BYTES);
  view.setUint32(AUDIO_FRAME_BYTES - 4, crc32c(frame.subarray(0, -4)), true);
  return frame;
}

export function decodeAudioFrame(frame) {
  if (!(frame instanceof Uint8Array) || frame.byteLength !== AUDIO_FRAME_BYTES) {
    return null;
  }
  const view = new DataView(frame.buffer, frame.byteOffset, frame.byteLength);
  const sequence = view.getUint32(12, true);
  const sampleIndex = view.getUint32(16, true);
  if (
    frame[0] !== DEVICE_AUDIO_WIRE_VERSION ||
    frame[1] !== 1 ||
    frame[2] !== DEVICE_AUDIO_CODEC_IMA_ADPCM ||
    frame[3] !== 0 ||
    frame[24] > 88 ||
    frame[25] !== 0 ||
    view.getUint16(20, true) !== DEVICE_AUDIO_SAMPLES_PER_FRAME ||
    view.getUint16(26, true) !== DEVICE_AUDIO_PAYLOAD_BYTES ||
    !isValidFramePosition(sequence, sampleIndex) ||
    (frame[AUDIO_FRAME_BYTES - 5] & 0xf0) !== 0 ||
    view.getUint32(AUDIO_FRAME_BYTES - 4, true) !==
      crc32c(frame.subarray(0, -4))
  ) {
    return null;
  }
  const payload = frame.slice(AUDIO_FRAME_HEADER_BYTES, -4);
  try {
    return {
      epoch: view.getUint32(4, true),
      streamId: view.getUint32(8, true),
      sequence,
      sampleIndex,
      sampleCount: view.getUint16(20, true),
      codec: "ima_adpcm",
      predictor: view.getInt16(22, true),
      stepIndex: frame[24],
      payload,
      samples: decodeImaAdpcmBlock({
        predictor: view.getInt16(22, true),
        stepIndex: frame[24],
        payload,
        sampleCount: view.getUint16(20, true),
      }),
    };
  } catch {
    return null;
  }
}

export function fragmentAudioFrame(
  frame,
  { streamId, sequence, maxPacketBytes },
) {
  if (
    !(frame instanceof Uint8Array) ||
    frame.length !== AUDIO_FRAME_BYTES ||
    !isU32(streamId) ||
    !isU32(sequence) ||
    !Number.isInteger(maxPacketBytes) ||
    maxPacketBytes <= AUDIO_FRAGMENT_HEADER_BYTES
  ) {
    throw new RangeError("invalid fragmentation arguments");
  }
  const decoded = decodeAudioFrame(frame);
  if (
    decoded === null ||
    decoded.streamId !== streamId ||
    decoded.sequence !== sequence
  ) {
    throw new RangeError("fragment identity must match the logical frame");
  }
  const chunkBytes = maxPacketBytes - AUDIO_FRAGMENT_HEADER_BYTES;
  const count = Math.ceil(frame.length / chunkBytes);
  if (count > MAX_AUDIO_FRAGMENTS) {
    throw new RangeError("logical audio frame exceeds four fragments");
  }
  return Array.from({ length: count }, (_, fragmentIndex) => {
    const payload = frame.subarray(
      fragmentIndex * chunkBytes,
      Math.min(frame.length, (fragmentIndex + 1) * chunkBytes),
    );
    const packet = new Uint8Array(AUDIO_FRAGMENT_HEADER_BYTES + payload.length);
    const view = new DataView(packet.buffer);
    packet[0] = DEVICE_AUDIO_WIRE_VERSION;
    packet[1] = 2;
    packet[2] = fragmentIndex;
    packet[3] = count;
    view.setUint32(4, streamId, true);
    view.setUint32(8, sequence, true);
    packet.set(payload, AUDIO_FRAGMENT_HEADER_BYTES);
    return packet;
  });
}

export function parseAudioFragment(packet) {
  if (
    !(packet instanceof Uint8Array) ||
    packet.length <= AUDIO_FRAGMENT_HEADER_BYTES ||
    packet.length > AUDIO_FRAGMENT_HEADER_BYTES + AUDIO_FRAME_BYTES
  ) {
    return null;
  }
  const view = new DataView(packet.buffer, packet.byteOffset, packet.byteLength);
  const fragmentIndex = packet[2];
  const fragmentCount = packet[3];
  if (
    packet[0] !== DEVICE_AUDIO_WIRE_VERSION ||
    packet[1] !== 2 ||
    fragmentCount < 1 ||
    fragmentCount > MAX_AUDIO_FRAGMENTS ||
    fragmentIndex >= fragmentCount
  ) {
    return null;
  }
  return {
    streamId: view.getUint32(4, true),
    sequence: view.getUint32(8, true),
    fragmentIndex,
    fragmentCount,
    payload: packet.slice(AUDIO_FRAGMENT_HEADER_BYTES),
  };
}

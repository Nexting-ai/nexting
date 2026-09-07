const INITIAL_CREDITS = 8;
const MAX_QUEUE_FRAMES = 10;
const CREDIT_STALL_MS = 500;
const HOLD_THRESHOLD_MS = 450;
const MAX_DURATION_MS = 120_000;
const MAX_FRAMES = 6000;

export class DeviceAudioSimulator {
  constructor({ now = () => Date.now() } = {}) {
    this.now = now;
    this.state = "unavailable";
    this.events = [];
    this.sentFrames = [];
    this.frameQueue = [];
    this.credits = 0;
    this.sequence = 0;
    this.epoch = null;
    this.streamId = 0;
    this.captureStartedAt = null;
    this.creditStallStartedAt = null;
    this.lastAckSequence = -1;
    this.startupBufferMs = 200;
  }

  get queuedFrames() {
    return this.frameQueue.length;
  }

  get audioInRamBytes() {
    return this.frameQueue.reduce(
      (total, record) => total + record.bytes.length,
      0,
    );
  }

  configure({ authorized, encrypted, attPayload, epoch }) {
    this.cancelActive("host", false);
    if (
      authorized !== true ||
      encrypted !== true ||
      !Number.isInteger(attPayload) ||
      attPayload < 64 ||
      !Number.isInteger(epoch) ||
      epoch < 0 ||
      epoch > 0xffff_ffff
    ) {
      this.state = "unavailable";
      return false;
    }
    this.epoch = epoch;
    this.credits = INITIAL_CREDITS;
    this.state = "ready";
    return true;
  }

  handleHostControl(message) {
    if (
      message?.type === "audioCancel" &&
      message.epoch === this.epoch &&
      message.streamId === this.streamId &&
      this.isActive()
    ) {
      this.cancelActive("host");
      return true;
    }
    return false;
  }

  physicalVoiceDown() {
    if (this.state === "latchedRecording") {
      this.drain("submitted");
      return true;
    }
    if (this.state !== "ready") return false;
    this.streamId = (this.streamId + 1) >>> 0;
    this.captureStartedAt = this.now();
    this.sequence = 0;
    this.credits = INITIAL_CREDITS;
    this.sentFrames = [];
    this.frameQueue = [];
    this.creditStallStartedAt = null;
    this.lastAckSequence = -1;
    this.state = "capturing";
    this.events.push({
      type: "audioBegin",
      epoch: this.epoch,
      streamId: this.streamId,
      localPhysicalInput: true,
    });
    return true;
  }

  physicalVoiceUp() {
    if (this.state !== "capturing") return false;
    const heldMs = this.now() - this.captureStartedAt;
    if (heldMs >= HOLD_THRESHOLD_MS) this.drain("submitted");
    else this.state = "latchedRecording";
    return true;
  }

  localSubmit() {
    if (!this.isCapturing()) return false;
    this.drain("submitted");
    return true;
  }

  localReject() {
    if (!this.isActive()) return false;
    this.cancelActive("rejected");
    return true;
  }

  disconnect() {
    const active = this.isActive();
    this.cancelActive("disconnect", active, "unavailable");
    this.state = "unavailable";
  }

  captureFrame(frame) {
    if (!this.isCapturing() || !(frame instanceof Uint8Array)) return false;
    if (this.sequence >= MAX_FRAMES) {
      this.drain("max_duration");
      return false;
    }
    const record = { sequence: this.sequence, bytes: frame.slice() };
    this.sequence = (this.sequence + 1) >>> 0;
    if (this.credits > 0) {
      this.sendRecord(record);
      return true;
    }
    if (this.frameQueue.length >= MAX_QUEUE_FRAMES) {
      this.cancelActive("transport");
      return false;
    }
    this.frameQueue.push(record);
    if (this.creditStallStartedAt === null) this.creditStallStartedAt = this.now();
    return true;
  }

  grantCredit({ epoch, streamId, ackSequence, credits }) {
    const highestSentSequence = this.sentFrames.at(-1)?.sequence ?? -1;
    if (
      !this.isActive() ||
      epoch !== this.epoch ||
      streamId !== this.streamId ||
      !Number.isInteger(ackSequence) ||
      ackSequence <= this.lastAckSequence ||
      ackSequence > highestSentSequence ||
      !Number.isInteger(credits) ||
      credits < 1 ||
      credits > 32 ||
      this.credits + credits > INITIAL_CREDITS
    ) {
      return false;
    }
    this.lastAckSequence = ackSequence;
    this.credits += credits;
    while (this.credits > 0 && this.frameQueue.length > 0) {
      this.sendRecord(this.frameQueue.shift());
    }
    this.creditStallStartedAt =
      this.frameQueue.length === 0 ? null : this.now();
    if (this.state === "draining" && this.frameQueue.length === 0) {
      this.finishDrain(this.pendingEndReason);
    }
    return true;
  }

  tick() {
    if (
      this.isCapturing() &&
      this.captureStartedAt !== null &&
      this.now() - this.captureStartedAt >= MAX_DURATION_MS
    ) {
      this.drain("max_duration");
      return;
    }
    if (
      this.isActive() &&
      this.creditStallStartedAt !== null &&
      this.now() - this.creditStallStartedAt >= CREDIT_STALL_MS
    ) {
      this.cancelActive("timeout");
    }
  }

  sendRecord(record) {
    this.credits -= 1;
    this.sentFrames.push({
      sequence: record.sequence,
      byteLength: record.bytes.length,
    });
    record.bytes.fill(0);
  }

  drain(reason) {
    if (!this.isCapturing()) return;
    this.state = "draining";
    this.pendingEndReason = reason;
    if (this.frameQueue.length === 0) this.finishDrain(reason);
  }

  finishDrain(reason) {
    this.state = "waitingResult";
    this.events.push({
      type: "audioEnd",
      epoch: this.epoch,
      streamId: this.streamId,
      lastSequence: Math.max(0, this.sequence - 1),
      sampleCount: this.sequence * 320,
      reason,
    });
    this.clearRam();
  }

  cancelActive(reason, emit = true, finalState = "ready") {
    if (emit) {
      this.events.push({
        type: "audioCancel",
        epoch: this.epoch,
        streamId: this.streamId,
        reason,
      });
    }
    this.clearRam();
    this.captureStartedAt = null;
    this.state = finalState;
  }

  clearRam() {
    for (const record of this.frameQueue) record.bytes.fill(0);
    this.frameQueue = [];
    this.creditStallStartedAt = null;
  }

  isCapturing() {
    return this.state === "capturing" || this.state === "latchedRecording";
  }

  isActive() {
    return this.isCapturing() || this.state === "draining";
  }
}

export class DeviceAudioReceiver {
  constructor({ maxFrames = MAX_FRAMES } = {}) {
    if (!Number.isInteger(maxFrames) || maxFrames < 1 || maxFrames > MAX_FRAMES) {
      throw new RangeError("maxFrames must represent at most 120 seconds");
    }
    this.maxFrames = maxFrames;
    this.expectedSequence = 0;
    this.missingFrames = 0;
    this.samples = [];
    this.terminated = false;
  }

  accept(frame) {
    if (
      this.terminated ||
      !Number.isInteger(frame?.sequence) ||
      frame.sequence < 0 ||
      !(frame.samples instanceof Int16Array) ||
      frame.samples.length !== 320
    ) {
      return { accepted: false, terminated: this.terminated, reason: "bad_frame" };
    }
    if (frame.sequence < this.expectedSequence) {
      return { accepted: false, duplicate: true };
    }
    if (frame.sequence >= this.maxFrames) {
      this.terminate();
      return { accepted: false, terminated: true, reason: "max_duration" };
    }
    const gap = frame.sequence - this.expectedSequence;
    if (gap >= 3 || this.missingFrames + gap >= 10) {
      this.terminate();
      return { accepted: false, terminated: true, reason: "missing_frames" };
    }
    for (let missing = 0; missing < gap; missing += 1) {
      this.samples.push(...new Int16Array(320));
    }
    this.missingFrames += gap;
    this.samples.push(...frame.samples);
    this.expectedSequence = frame.sequence + 1;
    return { accepted: true, insertedSilenceFrames: gap };
  }

  terminate() {
    this.samples.fill(0);
    this.samples = [];
    this.terminated = true;
  }
}

import { CHOICES, encode } from "./protocol.mjs";

export function createApprovalRelay({
  sendToDevice,
  answerPrompt,
  now = () => performance.now(),
}) {
  if (
    typeof sendToDevice !== "function" ||
    typeof answerPrompt !== "function"
  ) {
    throw new TypeError("sendToDevice and answerPrompt are required");
  }

  let pending = null;

  function finish(reason, shouldSend = true) {
    if (pending === null) return;
    const { requestId } = pending;
    pending = null;
    if (shouldSend) {
      sendToDevice(encode({ type: "resolved", requestId, reason }));
    }
  }

  return {
    get hasPending() {
      return pending !== null;
    },

    get pendingRequestId() {
      return pending?.requestId ?? null;
    },

    present({ requestId, summary, ttlMs }) {
      const message = {
        type: "present",
        requestId,
        summary,
        options: [...CHOICES],
        ttlMs,
      };
      const wire = encode(message);
      if (wire === null) return false;

      if (pending !== null) finish("replaced");
      pending = {
        requestId,
        deadlineMs: now() + ttlMs,
        lockedChoice: null,
        answerInFlight: false,
      };
      sendToDevice(wire);
      return true;
    },

    onDeviceAnswer({ requestId, choice, authorized }) {
      if (authorized !== true)
        return { accepted: false, reason: "unauthorized" };
      if (pending === null) return { accepted: false, reason: "no_pending" };
      if (requestId !== pending.requestId) {
        return { accepted: false, reason: "stale_or_unknown" };
      }
      if (!CHOICES.includes(choice))
        return { accepted: false, reason: "bad_choice" };
      if (now() >= pending.deadlineMs) {
        finish("expired");
        return { accepted: false, reason: "expired" };
      }
      if (pending.answerInFlight) {
        return { accepted: false, reason: "answer_in_flight" };
      }
      if (pending.lockedChoice !== null && pending.lockedChoice !== choice) {
        return { accepted: false, reason: "choice_locked" };
      }

      pending.lockedChoice ??= choice;
      pending.answerInFlight = true;
      answerPrompt(choice);
      return { accepted: true, choice };
    },

    phoneAnswerStarted(requestId) {
      if (pending === null || pending.requestId !== requestId) return false;
      if (now() >= pending.deadlineMs) {
        finish("expired");
        return false;
      }
      if (pending.answerInFlight) return false;
      pending.answerInFlight = true;
      return true;
    },

    answerSucceeded(requestId) {
      if (pending?.requestId === requestId && pending.answerInFlight) {
        finish("answered");
      }
    },

    answerFailed(requestId) {
      if (pending?.requestId !== requestId || !pending.answerInFlight) return;
      pending.answerInFlight = false;
      if (now() >= pending.deadlineMs) finish("expired");
    },

    cancel(requestId) {
      if (
        pending !== null &&
        (requestId === undefined || pending.requestId === requestId)
      ) {
        finish("cancelled");
      }
    },

    tick() {
      if (
        pending !== null &&
        !pending.answerInFlight &&
        now() >= pending.deadlineMs
      ) {
        finish("expired");
      }
    },

    disconnect() {
      finish("cancelled", false);
    },
  };
}

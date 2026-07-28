# Connect a Host to the official Codex App Server

This guide explains how a Host can project a small, safe subset of official
Codex approvals onto a Nexting `approval/1` device. It is an integration
pattern, not a new wire profile. [`SPEC.md`](../SPEC.md) remains the normative
device contract.

## Choose the correct official surface

OpenAI publishes two related integration layers:

- [Codex SDK](https://learn.chatgpt.com/docs/codex-sdk) is the higher-level
  TypeScript and Python interface for server-side automation that starts,
  resumes, and runs Codex threads.
- [Codex App Server](https://learn.chatgpt.com/docs/app-server) is the
  bidirectional JSON-RPC interface for rich clients that need authentication,
  conversation history, approvals, and streamed Agent events.

A physical approval surface needs the second layer. It must observe the
server-initiated request, preserve its request identity inside the trusted
Host, return the exact official decision, and wait for
`serverRequest/resolved`.

Keep App Server local. Use its default stdio transport, a local Unix socket, or
an authenticated localhost/loopback connection. Do not expose an unauthenticated
non-loopback listener. The device still talks only to the authorized Host over
encrypted BLE; it never connects to App Server, Codex, or a Nexting cloud
endpoint.

## Exact projection

| Official Codex request | Hardware eligibility | Allow result | Deny result |
| --- | --- | --- | --- |
| `item/commandExecution/requestApproval` | Only when `availableDecisions` is explicitly and exactly `["accept","decline"]`, `command` is meaningful, and no elevated field below is present | `{ "decision": "accept" }` | `{ "decision": "decline" }` |
| `item/fileChange/requestApproval` | Only when `reason` is meaningful, `grantRoot` is absent, and the Host offers exactly one-time accept/decline | `{ "decision": "accept" }` | `{ "decision": "decline" }` |
| `item/permissions/requestApproval` | Never in `approval/1` | Phone/desktop only | Phone/desktop only |
| `item/tool/requestUserInput` | Never in `approval/1` | Phone/desktop only | Phone/desktop only |
| `mcpServer/elicitation/request` | Never in `approval/1` | Phone/desktop only | Phone/desktop only |

Reject a command request from hardware when any of these fields is present:

- `networkApprovalContext`: this is a managed network prompt and needs
  host/protocol-specific UI;
- `additionalPermissions`: the user must review the requested permission set;
- `proposedExecpolicyAmendment`: accepting can change future command policy.

Reject a file-change request when `grantRoot` is present. Never convert
`acceptForSession`, `cancel`, an exec-policy amendment, a permissions subset,
or form content into the green Allow key.

## Host state machine

The Host, not the device, owns the official App Server request. The safe flow
is:

1. Receive the official JSON-RPC request and retain its exact request ID,
   method, thread, turn, and params in trusted memory.
2. Apply the eligibility rules above. A missing field, unknown version,
   malformed decision list, or unhelpful summary means phone/desktop only.
3. Call `NextingDeviceRelayCoordinator.present(context:summary:ttlMs:)` with
   the retained request as private `context`. Send only the opaque public
   request ID, bounded summary, fixed choices, and relative TTL over BLE.
4. When the coordinator invokes `answerPrompt`, atomically claim the same
   pending request against phone UI. Map Allow to
   `{ "decision": "accept" }` and Deny to
   `{ "decision": "decline" }`.
5. Send one JSON-RPC response using the retained App Server request ID.
6. Call `answerSucceeded(requestId:)` only after the owning App Server path
   accepts or authoritatively settles the response. Call
   `answerFailed(requestId:)` after a retryable delivery failure.
7. When App Server emits `serverRequest/resolved` first, cancel the matching
   device request. Never clear a newer request by ID mismatch.

Pseudocode for the policy boundary:

```text
onCodexRequest(request):
  if request.method == commandApproval
     and request.availableDecisions == ["accept", "decline"]
     and meaningful(request.command)
     and absent(request.networkApprovalContext)
     and absent(request.additionalPermissions)
     and absent(request.proposedExecpolicyAmendment):
       present(context=request, summary=request.command, ttlMs=30000)

  else if request.method == fileChangeApproval
     and meaningful(request.reason)
     and absent(request.grantRoot):
       present(context=request, summary=request.reason, ttlMs=30000)

  else:
       renderOnTrustedScreenOnly(request)
```

## What this repository provides

The public package provides BLE, codecs, authorization primitives, the
`NextingDeviceRelayCoordinator`, answer-claim building blocks, protocol
vectors, and reference firmware. It deliberately does not publish a Codex Agent adapter,
accounts, cloud routing, or Nexting's private Bridge.

A third-party Host supplies its own official App Server client, prompt
rendering, eligibility/risk policy, enrollment UI, and authoritative answer
path. The public SDK helps that Host talk safely to a nearby device; it does
not grant access to the user's Codex session.

## Verify the boundary

At minimum, test these negative cases before shipping:

- missing or reordered `availableDecisions`;
- `acceptForSession` or `cancel`;
- `networkApprovalContext`, `additionalPermissions`, or
  `proposedExecpolicyAmendment`;
- file approval with `grantRoot` or no meaningful `reason`;
- unknown request version or method;
- stale, expired, replaced, duplicate, and mismatched request IDs;
- phone and hardware answers arriving in the same event-loop turn;
- App Server settling with `serverRequest/resolved` before the BLE answer.

Then run the shared device verification from
[Public interfaces](interfaces.md#verify-the-interfaces).

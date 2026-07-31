# Contributing

Nexting Devices opens the small, security-sensitive boundary between an authorized physical control surface and the trusted Nexting App. Contributions are welcome when they keep that boundary portable, testable, and fail closed.

Experimental 0.2 is published as the `devices/` subtree of
`Nexting-ai/nexting`. Before preparing a change, read [the foundation
blueprint](docs/foundation-development.md), [public interface
catalog](docs/interfaces.md), [protocol specification](SPEC.md), [security
model](SECURITY.md), and [conformance rules](docs/conformance.md). Coding
Agents must also follow [`AGENTS.md`](AGENTS.md).

## Keep the public boundary clean

Changes may cover the protocol, shared vectors, reference implementation, public SDKs, simulator, developer-reference firmware, conformance tests, and documentation in this repository.

Do not copy or reconstruct private Nexting App UI, Mac Bridge or Agent adapters, account or cloud routes, production PIN/Ring firmware, hardware designs, manufacturing data, OTA signing, factory provisioning, device keys, credentials, or internal identifiers. The exported repository must build without the private monorepo.

If a useful feature requires one of those systems, describe the public interface it needs; do not move the private implementation across the boundary.

## Change the contract before its implementations

For wire, validation, or state-machine changes:

1. Explain the product behavior and failure behavior.
2. Update `SPEC.md` when normative behavior changes.
3. Add valid or invalid cases to `protocol/vectors/approval-v1.json`.
4. Make the JavaScript reference test fail for the intended reason.
5. Implement the reference behavior, then port the same result to Swift and C.
6. Keep platform firmware thin; shared protocol behavior belongs in the portable C core.
7. Update `CHANGELOG.md` and compatibility notes.

Unknown, malformed, unauthorized, stale, duplicate, replaced, or expired input must not produce approval. A new profile or transport needs an explicit versioned contract; it must not be hidden behind optional fields in `approval/1`.

## Run evidence appropriate to the change

| Change                      | Minimum evidence                                                                             |
| --------------------------- | -------------------------------------------------------------------------------------------- |
| Protocol or vectors         | JavaScript, Swift, Kotlin, and C sanitizer suites                                            |
| Swift host SDK              | Swift tests and build; consuming product integration tests when Host-facing behavior changes |
| Public documentation system | Documentation contract, links, public-boundary, naming, and diff checks                      |
| C device core               | CMake build and CTest with ASan/UBSan                                                        |
| BLE simulator               | Private integration contract and warnings-as-errors `swiftc` build                           |
| Reference firmware          | Firmware contract plus every affected pinned board build                                     |
| Hardware claim              | Complete dated real-iPhone checklist for the exact board and firmware commit                 |
| Documentation only          | Link, public-boundary, naming, and diff checks                                               |

The exact commands and current known failures live in [`docs/development.md`](docs/development.md). Do not delete, skip, or weaken a failing security assertion to make a branch green.

## Use hardware labels literally

- `Core tested`: shared portable behavior passed desktop tests.
- `Build verified`: a pinned toolchain produced an artifact for one exact target.
- `Board verified`: the complete real-iPhone checklist has a dated evidence record.
- `Nexting Compatible`: unavailable during Experimental 0.2.

A successful compile is not proof of pairing, race handling, retry behavior, bond revocation, or radio delivery. Record evidence using [`docs/board-verification.md`](docs/board-verification.md).

## Before review

- Keep the change focused and explain its user-visible effect.
- Include the failing test that motivated a behavior fix.
- List commands actually run and distinguish simulated, compiled, and real-board evidence.
- Run `git diff --check`, `npm run check:boundary`, and `npm run check:naming`.
- Confirm no secrets, private paths, generated build products, or production material entered the tree.
- Report security vulnerabilities privately as described in [`SECURITY.md`](SECURITY.md), not in a public issue.

Code contributions use Apache-2.0. Specification and documentation contributions use CC BY 4.0. The licenses do not grant the Nexting name, logo, or a compatibility badge.

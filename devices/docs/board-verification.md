# Board verification checklist

Use this checklist for each exact board, firmware commit, and App version. Attach serial logs and phone evidence to the port record.

## Record

- Board, revision, chip:
- Firmware commit and build command:
- Zephyr/SDK version:
- App commit/version and iPhone/iOS:
- Pairing method and whether the bond was fresh:
- Tester and date:

## Required behavior

| Case                      | Expected result                                                                                                                                                          | Pass / evidence |
| ------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | --------------- |
| Device Info               | All required fields are readable; wire/profile/bounds match the binary.                                                                                                  |                 |
| Optional Device Info      | Every advertised button/rotary/display/haptics/battery capability matches the physical unit; absent capabilities create no empty App rows.                               |                 |
| Vendor facts              | Bounded inert facts render only in the vendor section and cannot replace system-owned rows or create actions.                                                            |                 |
| Account metadata          | Name, optional user number, and notes edit and sync without changing immutable hardware identity.                                                                        |                 |
| Cross-platform table      | iOS and Android show the same ordered continuous key/value information table for the same Device Info.                                                                   |                 |
| Encryption                | Plain approval write and unencrypted notification subscription are rejected or trigger pairing.                                                                          |                 |
| Bond required             | A peer that requests an encrypted but non-bonding session is rejected; a completed bond survives reconnect and board reboot.                                             |                 |
| Pairing failure recovery  | A rejected or failed security upgrade disconnects that peer, then the board becomes discoverable again without rebooting.                                                |                 |
| Unauthorized device       | Release App does not connect or disclose a summary before explicit authorization.                                                                                        |                 |
| Present                   | One current summary appears and Pending output turns on.                                                                                                                 |                 |
| Device Allow              | One Allow wins, App continues, matching `answered` clears the device.                                                                                                    |                 |
| Device Deny               | One Deny wins, App continues, matching `answered` clears the device.                                                                                                     |                 |
| Phone first               | Phone choice wins; device clears; late button press is ignored.                                                                                                          |                 |
| Device first              | Device choice wins; later phone tap cannot consume the prompt again.                                                                                                     |                 |
| Repeat                    | Holding/bouncing the button never changes or double-consumes the first choice.                                                                                           |                 |
| Retry                     | Dropping the first notification causes the exact same answer to retry after one second.                                                                                  |                 |
| Answer fragmentation      | At a small negotiated ATT MTU, a maximum-ID Answer arrives as one ordered newline frame with no truncation, duplication, or interleaving.                                |                 |
| Replacement               | New Present replaces old state; old ID cannot answer.                                                                                                                    |                 |
| Expiry                    | At the TTL boundary, LED clears and a press cannot answer.                                                                                                               |                 |
| Cancellation              | Host cancellation clears Pending immediately.                                                                                                                            |                 |
| Disconnect                | Connection loss clears current request, answer cache, LED, and partial frame.                                                                                            |                 |
| Reconnect                 | Device restores no approval; host may re-present with only remaining TTL.                                                                                                |                 |
| Reboot                    | Bond may persist; approval and partial frame never persist.                                                                                                              |                 |
| Fragmentation             | A valid message split through a multi-byte UTF-8 character decodes once.                                                                                                 |                 |
| Oversize                  | Input above the advertised limit is discarded through newline and recovery works.                                                                                        |                 |
| Malformed input           | Invalid UTF-8, JSON, version, profile, option, and ID fail closed.                                                                                                       |                 |
| Short reset chord         | Pressing both buttons for less than three seconds neither answers nor clears bonds.                                                                                      |                 |
| Local bond reset          | Holding both buttons for three seconds clears volatile state and all board bonds, disconnects the phone, shows the one-second LED confirmation, and resumes advertising. |                 |
| Bond-reset failure        | Force or instrument `bt_unpair` failure; the board logs the failure, does not show the success LED, and keeps connectable advertising stopped.                           |                 |
| Fresh pairing after reset | The old phone bond cannot silently resume; after forgetting it phone-side, a new encrypted bond succeeds.                                                                |                 |
| App revocation            | Removing App authorization stops future summaries and answers independently of the board's local bond reset.                                                             |                 |
| Agent isolation           | A Claude Code answer can reach only Claude Code; a Codex answer can reach only Codex; both race with phone input through one single-consumption gate.                    |                 |

## Label decision

`Board verified` requires every required behavior to pass. Record any caveat rather than weakening the checklist. Experimental reference boards remain non-MITM Just Works devices and must not be described as production certified.

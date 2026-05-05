# Pinclaw — Wearable AI Voice Agent

Pinclaw is a clip-on wearable AI device. Press a button, speak naturally, and your personal AI agent schedules, remembers, researches, and acts on your behalf — hands-free, screen-free.

**Website:** [pinclaw.ai](https://pinclaw.ai)

## What It Does

- **One-tap voice dispatch** — 3 seconds to assign a task, 5x faster than pulling out your phone
- **Autonomous agent** — schedules meetings, drafts emails, sets reminders, books reservations in the background
- **Context memory** — remembers people, conversations, and threads across weeks
- **Deep iPhone integration** — Calendar, Reminders, Contacts, Health, HomeKit, all voice-controlled
- **Open ecosystem** — works with OpenClaw, Hermes, Ollama, or any OpenAI-compatible backend

## Hardware

Purpose-built hardware — not a phone app or simulator.

- Custom clip-on form factor
- Beamforming mic array with noise suppression
- BLE 5.0 audio streaming (Opus codec)
- One-button push-to-talk interaction
- 12-hour battery life

## Repository Structure

```
pinclaw/
├── firmware/    # Device firmware (BLE audio + button control)
├── hardware/    # Hardware design files (3D models, wiring diagrams)
└── plugin/      # OpenClaw plugin for local AI integration
```

Each subdirectory is a git submodule linking to its own repository.

## Pricing

| Plan | Price | What You Get |
|------|-------|-------------|
| Hardware | $99 (pre-order) | Pinclaw clip device |
| Pinclaw Pro | $29/mo or $279/yr | Managed AI agent, latest models, zero setup |
| Bring Your Own Agent | Free | Connect your own AI backend |

## License

MIT

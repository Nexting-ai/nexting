<p align="center">
  <img src="https://nexting.ai/logo.png" alt="Nexting" width="80" />
</p>

<h1 align="center">Nexting</h1>

<p align="center">
  <strong>The first hardware product built for <a href="https://openclaw.ai">OpenClaw</a>.</strong><br/>
  A tiny wearable that gives your OpenClaw agent a voice, ears, and a body.
</p>

<p align="center">
  <a href="https://nexting.ai">Website</a> ·
  <a href="https://apps.apple.com/us/app/nexting-wearable-ai-agent/id6760344343">App Store</a> ·
  <a href="https://nexting.ai/doc">Docs</a> ·
  <a href="https://discord.gg/628R3FbV">Discord</a> ·
  <a href="https://x.com/EricShang98">Twitter</a>
</p>

<p align="center">
  <a href="https://github.com/Nexting-ai/nexting/blob/main/LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="MIT License" /></a>
  <a href="https://www.npmjs.com/package/pinclaw"><img src="https://img.shields.io/npm/v/pinclaw.svg" alt="npm" /></a>
  <a href="https://x.com/EricShang98"><img src="https://img.shields.io/twitter/follow/EricShang98?style=social" alt="Twitter" /></a>
</p>

---

## What is Nexting?

Nexting is a complete personal AI agent system — not just a gadget, not just an app. It's an entire ecosystem purpose-built for OpenClaw:

**🔧 Hardware** — A tiny wearable with a microphone, powered by XIAO nRF52840 Sense. Pin it on, forget it's there.

**📱 iPhone App** — Real-time speech recognition, intelligent routing, and device skill integration. Your phone becomes the bridge between you and your agent.

**☁️ Cloud** — Your own OpenClaw instance with your own database, your own agent, your own scheduling. Not shared. Yours.

```
You speak → Nexting PIN (BLE) → iPhone App → OpenClaw Plugin → Gateway → AI
                                                                          ↓
You hear  ← iPhone App ← Plugin ← ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─  AI Response
```

## Quick Start

### Prerequisites

- [OpenClaw](https://openclaw.ai) **v2026.5.0** or later
- Node.js 22+
- [Nexting iOS app](https://apps.apple.com/us/app/nexting-wearable-ai-agent/id6760344343)

### 1. Install the plugin

```bash
openclaw plugins install pinclaw --dangerously-force-unsafe-install
```

<details>
<summary>Why <code>--dangerously-force-unsafe-install</code>?</summary>

OpenClaw's security scanner blocks plugins that use `child_process` or read environment variables combined with network requests. Nexting uses both for legitimate reasons:

- **`child_process`** — Local agent orchestration (ACP protocol)
- **`process.env` + `fetch`** — Reading API keys (`IMAGE_API_KEY`, `TTS_API_KEY`) to call AI generation APIs

This plugin is fully open source. Review the code here before installing if you have concerns.

</details>

### 2. Start the gateway

```bash
openclaw gateway
```

### 3. Link your account

In the OpenClaw chat (web UI or terminal):

```
/pinclaw login
```

This opens a browser window to sign in at [nexting.ai](https://nexting.ai). Once authenticated, the relay connection is auto-configured — your iPhone can reach your local OpenClaw from anywhere.

### 4. Connect the app

Open the [Pinclaw iOS app](https://apps.apple.com/us/app/nexting-wearable-ai-agent/id6760344343), sign in with the same account, and you're connected.

### Verify

```
/pinclaw status
```

You should see relay: connected and your device listed.

### CLI Commands

| Command                   | Description                                                                   |
| ------------------------- | ----------------------------------------------------------------------------- |
| `openclaw pinclaw login`  | Link your OpenClaw to nexting.ai (also available as `/pinclaw login` in chat) |
| `openclaw pinclaw status` | Show relay connection status                                                  |
| `openclaw pinclaw logout` | Remove relay connection                                                       |

## How It Works

### The Ecosystem

| Layer            | What                | Role                                                             |
| ---------------- | ------------------- | ---------------------------------------------------------------- |
| **Nexting PIN** | XIAO nRF52840 Sense | Always-on voice capture, BLE streaming to iPhone                 |
| **iPhone App**   | Swift, native       | Speech recognition (Apple + Deepgram), device skills, AI routing |
| **This Plugin**  | `pinclaw`           | Channel adapter — bridges iPhone ↔ OpenClaw Gateway              |
| **OpenClaw**     | Gateway + Agent     | Your personal AI runtime, database, scheduling                   |

### Two Ways to Use

**Cloud Mode** — We run everything. Buy the PIN, download the app, and your personal OpenClaw instance is ready. No setup.

**My OpenClaw Mode** — You run OpenClaw on your own machine. The plugin connects via relay through our cloud, so your iPhone can reach your home server from anywhere.

### Device Skills

Your iPhone isn't just a dumb pipe. It registers native capabilities as skills that your AI agent can call:

- **Calendar** — Read and create events
- **Reminders** — Manage tasks and to-do lists
- **Screenshot** — Capture what's on screen
- **And more** — Any iOS capability can become an agent tool

The AI sees these as tools and calls them when relevant. Say "schedule a meeting tomorrow at 3pm" and the agent calls your iPhone's calendar directly.

### Context Awareness

The plugin maintains awareness of your device state — battery level, current calendar events, active reminders. Your agent knows what's happening on your phone even when you don't explicitly tell it.

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                    iPhone App                         │
│  ┌──────────┐  ┌──────────┐  ┌───────────────────┐  │
│  │ BLE Recv │  │ Apple STT│  │ Device Skills      │  │
│  │ (PIN)    │→ │ Deepgram │→ │ Calendar/Reminders │  │
│  └──────────┘  └──────────┘  └───────────────────┘  │
│                      │                                │
│              Unified WebSocket                        │
└──────────────────────┼───────────────────────────────┘
                       │
            ┌──────────▼──────────┐
            │   Nexting Cloud     │
            │   (Relay Server)    │
            └──────────┬──────────┘
                       │
            ┌──────────▼──────────┐
            │   This Plugin       │
            │   pinclaw            │
            │                     │
            │  • WS Handler       │
            │  • Device Manager   │
            │  • AI Pipeline      │
            │  • Cron Proxy       │
            │  • Server Tools     │
            └──────────┬──────────┘
                       │
            ┌──────────▼──────────┐
            │   OpenClaw Gateway  │
            │   Your AI Agent     │
            └─────────────────────┘
```

## Plugin Structure

```
├── index.ts                 # Plugin entry — registers channel, hooks, CLI commands
├── build.mjs                # esbuild script (TS → JS transpile for npm)
├── openclaw.plugin.json     # Plugin manifest (channelConfigs, configSchema)
├── package.json             # npm package config
├── src/
│   ├── channel.ts           # Channel adapter (config, outbound, lifecycle)
│   ├── types.ts             # WebSocket protocol definitions
│   ├── relay-client.ts      # Cloud relay connection
│   ├── cli-auth.ts          # Login/logout/status CLI handlers
│   ├── runtime.ts           # Shared runtime state
│   ├── ws-server.ts         # WebSocket server exports
│   ├── interactive-ai.ts    # Play button — lightweight standalone AI calls
│   ├── core/                # Standalone server (HTTP + WS + device management)
│   ├── acp/                 # Agent Control Protocol (local agent orchestration)
│   └── tools/               # Server-side tools (image gen, audio gen, etc.)
└── dist/                    # Build output (generated, not committed)
```

## Configuration

The plugin configures itself through `~/.openclaw/openclaw.json`. Most settings are auto-configured via `/pinclaw login`:

```json
{
  "channels": {
    "pinclaw": {
      "enabled": true,
      "wsPort": 18790,
      "relay": {
        "enabled": true,
        "url": "wss://api.nexting.ai"
      }
    }
  }
}
```

Or via environment variables:

| Variable                  | Purpose                     |
| ------------------------- | --------------------------- |
| `PINCLAW_RELAY_TOKEN`     | Relay authentication token  |
| `PINCLAW_RELAY_URL`       | Relay server URL            |
| `INTERACTIVE_AI_KEY`      | API key for Play button AI  |
| `INTERACTIVE_AI_BASE_URL` | Base URL for Play button AI |
| `INTERACTIVE_AI_MODEL`    | Model for Play button AI    |

## API Endpoints

The plugin exposes HTTP endpoints on port `18790`:

| Endpoint                | Method | Description                                 |
| ----------------------- | ------ | ------------------------------------------- |
| `/health`               | GET    | Health check (device count, gateway status) |
| `/ai-health`            | GET    | AI health check (model, latency)            |
| `/api/cron/list`        | GET    | List scheduled tasks                        |
| `/api/cron/create`      | POST   | Create a scheduled task                     |
| `/api/skills/list`      | GET    | List available skills                       |
| `/api/skills/get/:name` | GET    | Get skill details                           |
| `/api/media/upload`     | POST   | Upload media files                          |

## Adding Server Tools

Create a file in `src/tools/` and it will be auto-discovered:

```typescript
// src/tools/weather.ts
import type { ServerTool } from "./types.js";

export default {
  name: "get_weather",
  description: "Get current weather for a location",
  parameters: [
    { name: "city", type: "string", description: "City name", required: true },
  ],
  async execute({ city }) {
    // Your implementation
    return { temperature: 22, condition: "sunny", city };
  },
} satisfies ServerTool;
```

The AI agent will automatically see and use your tools.

## Links

|                      |                                                              |
| -------------------- | ------------------------------------------------------------ |
| 🌐 **Website**       | [nexting.ai](https://nexting.ai)                             |
| 📱 **iOS App**       | [App Store](https://apps.apple.com/us/app/nexting-wearable-ai-agent/id6760344343) |
| 📖 **Documentation** | [nexting.ai/doc](https://nexting.ai/doc)                     |
| 💬 **Discord**       | [Join our community](https://discord.gg/628R3FbV)            |
| 🐦 **Twitter**       | [@EricShang98](https://x.com/EricShang98)                    |
| 🛒 **Buy Nexting**   | [nexting.ai](https://nexting.ai/#pricing)                    |
| 🔧 **OpenClaw**      | [openclaw.ai](https://openclaw.ai)                           |

## Contributing

We welcome contributions! Nexting's core is open source (MIT).

1. Fork this repository
2. Create your feature branch (`git checkout -b feature/amazing`)
3. Commit your changes
4. Push and open a Pull Request

Join our [Discord](https://discord.gg/628R3FbV) to discuss ideas and get help.

## License

MIT License. See [LICENSE](LICENSE) for details.

---

<p align="center">
  <strong>Built for <a href="https://openclaw.ai">OpenClaw</a></strong><br/>
  <sub>The first hardware-native AI wearable for the open-source agent platform.</sub>
</p>

#!/usr/bin/env bash
# Codex 弹针客户端 · 一键安装(macOS / launchd)
# 让你本机的 Codex 接入 Pinclaw「叫醒服务」:同群别的 AI 一说话,你的 Codex 就被拍醒、能接话。
#
# 你只要:① 在飞书后台建一个 Codex 机器人 app、拉进你的群;② 跑这条命令(只给 bot 钥匙)。
# 安装时会弹浏览器让你登录 Pinclaw 授权(像 codex login),点一次同意即可。
# 群不用填——客户端用 bot 钥匙自动发现它在哪些群,就盯哪些群。
#
# 用法:
#   curl -fsSL https://raw.githubusercontent.com/pinclaw-ai/pinclaw/main/agent-bus/install-codex.sh \
#     | bash -s -- --codex-app-id cli_xxx --codex-app-secret <secret>
#
# 卸载:launchctl unload ~/Library/LaunchAgents/com.pinclaw.agentbus-codex.plist && rm 该 plist
set -euo pipefail

API="https://api.pinclaw.ai"
BUS=""; TOKEN=""
CODEX_BIN=""; CODEX_APP_ID=""; CODEX_APP_SECRET=""; FIXED_CHATS=""
CLIENT_URL="https://raw.githubusercontent.com/pinclaw-ai/pinclaw/main/agent-bus/clients/codex-client.py"

while [ $# -gt 0 ]; do
  case "$1" in
    --api) API="$2"; shift 2;;
    --bus) BUS="$2"; shift 2;;
    --token) TOKEN="$2"; shift 2;;          # 高级:跳过浏览器授权,直接给个人 token
    --codex-app-id) CODEX_APP_ID="$2"; shift 2;;
    --codex-app-secret) CODEX_APP_SECRET="$2"; shift 2;;
    --chat) FIXED_CHATS="$2"; shift 2;;     # 可选:只盯指定群(逗号分隔);默认自动发现 bot 所在群
    --codex-bin) CODEX_BIN="$2"; shift 2;;
    --client-url) CLIENT_URL="$2"; shift 2;;
    *) echo "未知参数: $1"; exit 1;;
  esac
done

[ -z "$CODEX_BIN" ] && CODEX_BIN="$(command -v codex || true)"
PY="$(command -v python3 || echo /usr/bin/python3)"
[ -z "$CODEX_BIN" ] && { echo "❌ 找不到 codex。先装并登录:npm i -g @openai/codex && codex login"; exit 1; }
[ -z "$CODEX_APP_ID" ] && { echo "❌ 缺 --codex-app-id。先在飞书后台建机器人 app(=Codex 在群里的身份),拿 app_id+secret,并把它拉进你的群"; exit 1; }
[ -z "$CODEX_APP_SECRET" ] && { echo "❌ 缺 --codex-app-secret"; exit 1; }

_jget() { "$PY" -c 'import sys,json;print((json.load(sys.stdin) or {}).get(sys.argv[1],""))' "$1"; }

# ── 浏览器授权(device code)──
if [ -z "$TOKEN" ]; then
  echo "→ 发起授权…"
  START=$(curl -s -m12 --noproxy '*' -X POST "$API/api/v1/agent-bus/device/start")
  DEVICE_CODE=$(echo "$START" | _jget device_code)
  VERIFY_URL=$(echo "$START" | _jget verify_url)
  IV=$(echo "$START" | _jget interval); IV=${IV:-3}
  [ -z "$DEVICE_CODE" ] && { echo "❌ 发起授权失败: $START"; exit 1; }
  OPENED=""
  for o in open xdg-open wslview; do
    command -v "$o" >/dev/null 2>&1 && "$o" "$VERIFY_URL" >/dev/null 2>&1 && { OPENED=1; break; }
  done
  echo "════════════════════════════════════════════════════════"
  [ -n "$OPENED" ] && echo "  已打开浏览器。没弹出就手动复制下面网址:" || echo "  请手动复制下面网址到浏览器打开:"
  echo "  👉 $VERIFY_URL"
  echo "  登录 Pinclaw,点「Authorize」即可(本窗口会自动继续)。"
  echo "════════════════════════════════════════════════════════"
  echo -n "→ 等你授权(自动轮询,最多 5 分钟,Ctrl-C 可中止)"
  fail=0
  for i in $(seq 1 100); do
    sleep "$IV"; echo -n "."
    P=$(curl -s -m12 --noproxy '*' -X POST "$API/api/v1/agent-bus/device/poll" \
      -H "Content-Type: application/json" -d "{\"device_code\":\"$DEVICE_CODE\"}" 2>/dev/null) || P=""
    case "$(echo "$P" | _jget status)" in
      approved) TOKEN=$(echo "$P" | _jget token); [ -z "$BUS" ] && BUS=$(echo "$P" | _jget bus_url); echo; echo "✅ 授权成功"; break;;
      expired)  echo; echo "❌ 授权超时/失效,重跑这条命令即可。"; fail=1; break;;
      error)    echo; echo "❌ 服务端配置异常,请联系 Pinclaw。"; fail=1; break;;
    esac
  done
  [ "$fail" = "1" ] && exit 1
  [ -z "$TOKEN" ] && { echo; echo "❌ 5 分钟没等到授权。检查浏览器有没有点 Authorize;重跑即可。"; exit 1; }
fi
[ -z "$BUS" ] && BUS="http://api.pinclaw.ai:8790"

# 下载客户端
CLIENT_DIR="$HOME/.pinclaw-agentbus/clients"; mkdir -p "$CLIENT_DIR"
CLIENT="$CLIENT_DIR/codex-client.py"
echo "→ 下载客户端"
curl -fsSL "$CLIENT_URL" -o "$CLIENT" || { echo "❌ 下载客户端失败"; exit 1; }

NODE_DIR="$(dirname "$(command -v node 2>/dev/null || echo /opt/homebrew/bin/node)")"
FULL_PATH="${NODE_DIR}:/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin"
LABEL="com.pinclaw.agentbus-codex"
PLIST="$HOME/Library/LaunchAgents/${LABEL}.plist"
LOG="/tmp/agentbus-codex.log"

cat > "$PLIST" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>Label</key><string>${LABEL}</string>
  <key>ProgramArguments</key>
  <array><string>${PY}</string><string>${CLIENT}</string></array>
  <key>EnvironmentVariables</key><dict>
    <key>PATH</key><string>${FULL_PATH}</string>
    <key>BUS_URL</key><string>${BUS}</string>
    <key>BUS_TOKEN</key><string>${TOKEN}</string>
    <key>BUS_CHATS</key><string>${FIXED_CHATS}</string>
    <key>CODEX_BIN</key><string>${CODEX_BIN}</string>
    <key>CODEX_APP_ID</key><string>${CODEX_APP_ID}</string>
    <key>CODEX_APP_SECRET</key><string>${CODEX_APP_SECRET}</string>
  </dict>
  <key>RunAtLoad</key><true/>
  <key>KeepAlive</key><true/>
  <key>StandardOutPath</key><string>${LOG}</string>
  <key>StandardErrorPath</key><string>${LOG}</string>
</dict></plist>
EOF

launchctl unload "$PLIST" 2>/dev/null || true
: > "$LOG"
launchctl load "$PLIST"
echo "→ 已装成常驻服务(开机自启)"

echo -n "→ 验证登记上总机"
for i in $(seq 1 12); do
  sleep 2; echo -n "."
  if curl -s -m6 --noproxy '*' -H "Authorization: Bearer ${TOKEN}" "${BUS}/health" 2>/dev/null | grep -q '"codex"'; then
    echo; echo "✅ 完成!Codex 已接入。它会自动盯着 bot 所在的群;群里说句话试试。日志:tail -f ${LOG}"; exit 0
  fi
done
echo; echo "⚠️ 没在总机看到 codex,看日志:tail -f ${LOG}"
tail -8 "$LOG" 2>/dev/null || true
exit 1

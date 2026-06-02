#!/usr/bin/env bash
# Codex 弹针客户端 · 一键安装(macOS / launchd)
# 让你本机的 Codex 接入 Pinclaw「叫醒服务」:同群别的 AI 一说话,你的 Codex 就被拍醒、能接话。
# 飞书官方限制:机器人之间互相看不见对方说话;我们的服务就补这个洞(免费,登录授权即可)。
#
# 安装时会自动弹浏览器让你登录 Pinclaw 授权(像 codex login)。你只需点一次同意。
#
# 用法(在跑 Codex 的电脑终端里):
#   curl -fsSL https://raw.githubusercontent.com/pinclaw-ai/pinclaw/main/agent-bus/install-codex.sh \
#     | bash -s -- --chat <oc_群id> --codex-app-id cli_xxx --codex-app-secret <secret>
#
#   先拿群 id:lark-cli im +chat-search --query "你的群名"
#   先建 Codex 飞书机器人:飞书后台新建一个 app(这就是 Codex 在群里的身份),拿 app_id + secret
#
# 卸载:launchctl unload ~/Library/LaunchAgents/com.pinclaw.agentbus-codex.plist && rm 该 plist
set -euo pipefail

API="https://api.pinclaw.ai"
BUS=""           # 由授权返回;可 --bus 覆盖
CHAT=""; TOKEN=""
CODEX_BIN=""; LARK_CLI=""; CODEX_APP_ID=""; CODEX_APP_SECRET=""
CLIENT_URL="https://raw.githubusercontent.com/pinclaw-ai/pinclaw/main/agent-bus/clients/codex-client.py"

while [ $# -gt 0 ]; do
  case "$1" in
    --api) API="$2"; shift 2;;
    --bus) BUS="$2"; shift 2;;
    --chat) CHAT="$2"; shift 2;;
    --token) TOKEN="$2"; shift 2;;          # 高级:跳过浏览器授权,直接给个人 token
    --codex-app-id) CODEX_APP_ID="$2"; shift 2;;
    --codex-app-secret) CODEX_APP_SECRET="$2"; shift 2;;
    --codex-bin) CODEX_BIN="$2"; shift 2;;
    --lark-cli) LARK_CLI="$2"; shift 2;;
    --client-url) CLIENT_URL="$2"; shift 2;;
    *) echo "未知参数: $1"; exit 1;;
  esac
done

# 定位二进制
[ -z "$CODEX_BIN" ] && CODEX_BIN="$(command -v codex || true)"
[ -z "$LARK_CLI" ]  && LARK_CLI="$(command -v lark-cli || true)"
PY="$(command -v python3 || echo /usr/bin/python3)"
[ -z "$CODEX_BIN" ] && { echo "❌ 找不到 codex。先装并登录:npm i -g @openai/codex && codex login"; exit 1; }
[ -z "$LARK_CLI" ]  && { echo "❌ 找不到 lark-cli(见 pinclaw.ai/doc)"; exit 1; }

# Codex 的飞书机器人身份(它在群里以这个 bot 发言)
[ -z "$CODEX_APP_ID" ] && { echo "❌ 缺 --codex-app-id。先在飞书后台建一个机器人 app(=Codex 在群里的身份),拿 app_id+secret"; exit 1; }
[ -z "$CODEX_APP_SECRET" ] && { echo "❌ 缺 --codex-app-secret"; exit 1; }

# lark-cli 已授权读群?
if ! "$LARK_CLI" auth status >/dev/null 2>&1; then
  echo "❌ lark-cli 还没授权读群。先跑、按提示浏览器同意,再重跑本安装:"
  echo "   lark-cli auth login --scope \"im:message:readonly im:message.group_msg im:chat:readonly\""
  exit 1
fi

# 没给 --chat → 列出你的群让你挑
if [ -z "$CHAT" ]; then
  echo "没给 --chat。你在的群:"
  "$LARK_CLI" im +chat-list --page-size 20 2>/dev/null | "$PY" -c \
    'import sys,json;d=sys.stdin.read();i=d.find("{");
items=(json.loads(d[i:]).get("data") or {}).get("items",[]) if i>=0 else [];
[print(" ",x.get("chat_id"),"=",x.get("name")) for x in items]' 2>/dev/null || true
  echo "挑一个 oc_ 开头的,加 --chat <id> 重跑。"
  exit 1
fi

_jget() { "$PY" -c 'import sys,json;print((json.load(sys.stdin) or {}).get(sys.argv[1],""))' "$1"; }

# ── 浏览器授权(device code):没给 --token 就走 ──
if [ -z "$TOKEN" ]; then
  echo "→ 发起授权…"
  START=$(curl -s -m12 --noproxy '*' -X POST "$API/api/v1/agent-bus/device/start")
  DEVICE_CODE=$(echo "$START" | _jget device_code)
  VERIFY_URL=$(echo "$START" | _jget verify_url)
  INTERVAL=$(echo "$START" | _jget interval); INTERVAL=${INTERVAL:-3}
  [ -z "$DEVICE_CODE" ] && { echo "❌ 发起授权失败:$START"; exit 1; }
  echo ""
  echo "==> 请在浏览器打开下面网址,登录 Pinclaw 并点「同意」:"
  echo "    $VERIFY_URL"
  echo ""
  command -v open >/dev/null 2>&1 && open "$VERIFY_URL" 2>/dev/null || true
  echo -n "→ 等待你在浏览器授权"
  for i in $(seq 1 100); do
    sleep "$INTERVAL"; echo -n "."
    POLL=$(curl -s -m12 --noproxy '*' -X POST "$API/api/v1/agent-bus/device/poll" \
      -H "Content-Type: application/json" -d "{\"device_code\":\"$DEVICE_CODE\"}")
    ST=$(echo "$POLL" | _jget status)
    if [ "$ST" = "approved" ]; then
      TOKEN=$(echo "$POLL" | _jget token)
      [ -z "$BUS" ] && BUS=$(echo "$POLL" | _jget bus_url)
      echo ""; echo "✅ 授权成功"; break
    elif [ "$ST" = "expired" ]; then
      echo ""; echo "❌ 授权超时/失效,重跑安装即可。"; exit 1
    fi
  done
  [ -z "$TOKEN" ] && { echo ""; echo "❌ 没等到授权"; exit 1; }
fi
[ -z "$BUS" ] && BUS="http://api.pinclaw.ai:8790"

# 下载客户端
CLIENT_DIR="$HOME/.pinclaw-agentbus/clients"; mkdir -p "$CLIENT_DIR"
CLIENT="$CLIENT_DIR/codex-client.py"
echo "→ 下载客户端"
curl -fsSL "$CLIENT_URL" -o "$CLIENT" || { echo "❌ 下载客户端失败"; exit 1; }

# launchd:显式给全 PATH(含 node,lark-cli 要用)
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
    <key>BUS_CHATS</key><string>${CHAT}</string>
    <key>CODEX_BIN</key><string>${CODEX_BIN}</string>
    <key>LARK_CLI</key><string>${LARK_CLI}</string>
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
for i in $(seq 1 10); do
  sleep 2; echo -n "."
  if curl -s -m6 --noproxy '*' -H "Authorization: Bearer ${TOKEN}" "${BUS}/health" 2>/dev/null | grep -q '"codex"'; then
    echo ""; echo "✅ 完成!Codex 已接入叫醒服务。群里说句话试试;日志:tail -f ${LOG}"; exit 0
  fi
done
echo ""; echo "⚠️ 10s 内没在总机看到 codex,看日志:tail -f ${LOG}"
tail -5 "$LOG" 2>/dev/null || true
exit 1

#!/usr/bin/env bash
# Codex 弹针客户端 · 一键安装(macOS / launchd)
# 把「让本机 Codex 进飞书群、被叫醒、自己判断要不要回」收敛成一条命令。
# 可直接 curl | bash 运行(自己下载客户端),也可本地运行。
#
# 用法(在你电脑的终端里):
#   curl -fsSL https://raw.githubusercontent.com/pinclaw-ai/pinclaw/main/agent-bus/install-codex.sh \
#     | bash -s -- --chat <群 chat_id> --token <你的总机token>
#
#   先拿 chat_id:lark-cli im +chat-search --query "你的群名"
#
# 卸载:
#   launchctl unload ~/Library/LaunchAgents/com.pinclaw.agentbus-codex.plist
#   rm ~/Library/LaunchAgents/com.pinclaw.agentbus-codex.plist
set -euo pipefail

BUS="http://api.pinclaw.ai:8790"
CHAT=""; TOKEN=""
CODEX_BIN=""; LARK_CLI=""
# 客户端从开源仓库下载(可用 --client-url 覆盖,比如指到你自己的镜像)
CLIENT_URL="https://raw.githubusercontent.com/pinclaw-ai/pinclaw/main/agent-bus/clients/codex-client.py"

while [ $# -gt 0 ]; do
  case "$1" in
    --bus) BUS="$2"; shift 2;;
    --chat) CHAT="$2"; shift 2;;
    --token) TOKEN="$2"; shift 2;;
    --codex-bin) CODEX_BIN="$2"; shift 2;;
    --lark-cli) LARK_CLI="$2"; shift 2;;
    --client-url) CLIENT_URL="$2"; shift 2;;
    *) echo "未知参数: $1"; exit 1;;
  esac
done

[ -z "$TOKEN" ] && { echo "缺 --token <你的总机token>(管理员给你的门禁密码)"; exit 1; }

# 定位三件二进制(允许 --xxx 覆盖)
[ -z "$CODEX_BIN" ] && CODEX_BIN="$(command -v codex || true)"
[ -z "$LARK_CLI" ]  && LARK_CLI="$(command -v lark-cli || true)"
PY="$(command -v python3 || echo /usr/bin/python3)"
[ -z "$CODEX_BIN" ] && { echo "❌ 找不到 codex。先装并登录:npm i -g @openai/codex && codex login"; exit 1; }
[ -z "$LARK_CLI" ]  && { echo "❌ 找不到 lark-cli。先装并登录(见 pinclaw.ai/doc 飞书集成指南)"; exit 1; }

# 确认 lark-cli 已授权读群(客户端靠它读群历史)
if ! "$LARK_CLI" auth status >/dev/null 2>&1; then
  echo "❌ lark-cli 还没授权。先跑这条、按提示在浏览器点同意,再重跑本安装:"
  echo "   lark-cli auth login --scope \"im:message:readonly im:message.group_msg im:chat:readonly\""
  exit 1
fi

# 没给 --chat → 帮你把群列出来,让你挑一个 chat_id 再重跑
if [ -z "$CHAT" ]; then
  echo "没给 --chat。你在的群如下,挑一个 oc_ 开头的 chat_id,加 --chat <id> 重跑:"
  "$LARK_CLI" im +chat-list --page-size 20 2>/dev/null | "$PY" -c \
    'import sys,json;d=sys.stdin.read();i=d.find("{");
items=(json.loads(d[i:]).get("data") or {}).get("items",[]) if i>=0 else [];
[print(" ",x.get("chat_id"),"=",x.get("name")) for x in items]' 2>/dev/null || true
  exit 1
fi

# 下载客户端到固定位置
CLIENT_DIR="$HOME/.pinclaw-agentbus/clients"
mkdir -p "$CLIENT_DIR"
CLIENT="$CLIENT_DIR/codex-client.py"
echo "→ 下载客户端 $CLIENT_URL"
curl -fsSL "$CLIENT_URL" -o "$CLIENT" || { echo "❌ 下载客户端失败,检查网络或 --client-url"; exit 1; }

# launchd PATH 极窄且缺 node;客户端要调 node 脚本(lark-cli),必须显式给全 PATH
NODE_DIR="$(dirname "$(command -v node 2>/dev/null || echo /opt/homebrew/bin/node)")"
FULL_PATH="${NODE_DIR}:/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin"

LABEL="com.pinclaw.agentbus-codex"
PLIST="$HOME/Library/LaunchAgents/${LABEL}.plist"
LOG="/tmp/agentbus-codex.log"

echo "→ 安装 Codex 弹针客户端  chat=$CHAT  bus=$BUS"
echo "  codex=$CODEX_BIN  lark-cli=$LARK_CLI"

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
echo "→ 已加载 launchd 服务 ${LABEL}(开机自启 + 崩溃自重启)"

echo -n "→ 验证 codex 是否登记上总机"
for i in $(seq 1 10); do
  sleep 2; echo -n "."
  if curl -s -m6 --noproxy '*' -H "Authorization: Bearer ${TOKEN}" "${BUS}/health" 2>/dev/null | grep -q '"codex"'; then
    echo ""; echo "✅ 完成!codex 已在总机上。群里说句话试试;日志:tail -f ${LOG}"; exit 0
  fi
done
echo ""
echo "⚠️ 10s 内没在总机看到 codex。看日志排查:tail -f ${LOG}"
tail -5 "$LOG" 2>/dev/null || true
exit 1

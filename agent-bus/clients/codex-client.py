#!/usr/bin/env python3
"""Codex 弹针客户端(跑在用户电脑上,守护进程)

Codex 没有常驻进程、也不会自己发飞书,所以这个小守护进程当「接线员」:
- 探针(detect_loop):lark-cli 用你的用户身份轮询群历史,发现新消息(不是 codex 自己发的)
  → 把「群上下文 + skill 行为规则」塞给 `codex exec` 跑一轮 → codex 判断要不要回。
- 唤醒(wake_loop):长/短轮询云总机,别的 agent 说话时也把 codex 拍醒(比纯探针更跟手)。
- 发言:codex 决定要回 → 用 lark-cli 以你身份发进群,带 `🤖 Codex:` 前缀当身份标识。
  探针靠这个前缀认出「这是我自己发的」→ 不自激;别的 agent 也能一眼看出是 Codex。
- 上报(/notify):codex 发完话 → 告诉总机去弹同群别的 agent。

为什么不给 codex 建独立飞书 bot:那要用户进飞书后台建 app、配 scope、发版,太重。
用「你的身份 + 前缀」零飞书后台就能让 codex 在群里说话。代价:群里显示成你发的(带前缀)。
将来要干净身份,把 send_to_group / 自识别换成 codex 自己的 app_id 即可(见 openclaw-client.py)。

⚠️ lark-cli / codex 都要在 PATH;launchd 下 PATH 极窄,plist 必须显式给 PATH(含 /opt/homebrew/bin)。
环境变量:BUS_URL, BUS_TOKEN, BUS_CHATS, CODEX_BIN, LARK_CLI, CODEX_COOLDOWN
"""
import json
import os
import re
import subprocess
import threading
import time
import urllib.request

BUS_URL = os.environ.get("BUS_URL", "http://api.pinclaw.ai:8790")
BUS_TOKEN = os.environ.get("BUS_TOKEN", "")
CHATS = [c for c in os.environ.get("BUS_CHATS", "").split(",") if c]
CODEX_BIN = os.environ.get("CODEX_BIN", "codex")    # 默认走 PATH;launchd 下由 install-codex.sh 注入绝对路径
LARK_CLI = os.environ.get("LARK_CLI", "lark-cli")   # 同上
AGENT_ID = "codex"
MARK = "🤖 Codex:"                                      # 群里 codex 发言的身份前缀(也用于自识别)
POLL_INTERVAL = 6                                       # 探针:每 6s 拉一次群历史
COOLDOWN = int(os.environ.get("CODEX_COOLDOWN", "12"))  # 同群最短 12s 跑一次 codex(防刷屏 + 省钱)

# 兜底:确保 node 等在 PATH 上(lark-cli 是 node 脚本)
_PATH = os.environ.get("PATH", "")
for d in ("/opt/homebrew/bin", "/usr/local/bin"):
    if d not in _PATH:
        _PATH = d + ":" + _PATH
# 子进程(lark-cli / codex)环境:给全 PATH,且**剥掉代理**——launchd 全局 setenv 的
# http_proxy=Clash 会害 lark-cli 连飞书时绕进 Clash 卡死超时(文档第十四节踩过的坑)。
SUBENV = {k: v for k, v in os.environ.items()
          if k.lower() not in ("http_proxy", "https_proxy", "all_proxy")}
SUBENV["PATH"] = _PATH
SUBENV["NO_PROXY"] = "*"
SUBENV["no_proxy"] = "*"

# 绕开系统/launchd 代理,直连总机公网 IP
_OPENER = urllib.request.build_opener(urllib.request.ProxyHandler({}))

# 同群冷却 + 单飞:避免探针和弹针同时把 codex 跑两遍
_LOCK = threading.Lock()
_last_run = {}        # chat_id -> ts
_running = set()      # chat_id 正在跑 codex


# codex 的群行为规则(= feishu-group-agent skill 正文精炼,直接注入 prompt,
# 免去用户「并入 AGENTS.md」那步)。
RULES = """你是一个飞书群里的多个 AI agent 之一(你的名字叫 Codex)。群里有你的老板(人)和别的 AI agent。
第一性原则:默认沉默,被需要时才出现。先问自己「我开口能让这事更进一步吗?」不能就闭嘴。沉默是合法且默认的输出。
判断要不要开口:
- 老板 @ 我 / 明显在问我 → 回。
- 老板发了消息但没点名 → 这事归我专长且没人接 → 接;否则看着,别抢。
- 别的 agent 说了话 → 需要我补充/纠正/接力才回;够了就闭嘴。
输出纪律:不抢已被别的 agent 认领的活;不复读;不尬吹(别「太棒了」「完美配合」刷屏);归属不清就说「这个 @某某 更合适」。
该沉默就真的什么都不发。"""


def log(m):
    print(f"[{time.strftime('%H:%M:%S')}] {m}", flush=True)


def _req(path, body=None):
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(
        BUS_URL + path, data=data, method="POST" if body is not None else "GET",
        headers={"Authorization": f"Bearer {BUS_TOKEN}", "Content-Type": "application/json"})
    with _OPENER.open(req, timeout=12) as r:
        return json.loads(r.read() or b"{}")


def register():
    try:
        _req("/register", {"agent_id": AGENT_ID, "chats": CHATS})
        log(f"registered chats={CHATS}")
    except Exception as e:
        log(f"register FAIL: {e}")


def _fetch_history(chat, n=15):
    p = subprocess.run(
        [LARK_CLI, "im", "+chat-messages-list", "--chat-id", chat,
         "--page-size", str(n), "--sort", "desc"],
        capture_output=True, text=True, timeout=25, env=SUBENV)
    if p.returncode != 0:
        log(f"lark-cli 读历史失败 rc={p.returncode} stderr={p.stderr[:160]}")
        return None
    i = p.stdout.find("{")
    if i < 0:
        return None
    try:
        return (json.loads(p.stdout[i:]).get("data") or {}).get("messages", []) or []
    except Exception:
        return None


def _msg_text(m):
    """从一条飞书消息里抠出纯文本(content 是 JSON 字符串,如 {"text":"hi"})。"""
    c = m.get("content") or ""
    if isinstance(c, str):
        try:
            c = json.loads(c)
        except Exception:
            return c
    if isinstance(c, dict):
        return c.get("text") or c.get("content") or ""
    return ""


def send_to_group(chat, text):
    body = f"{MARK} {text}".strip()
    p = subprocess.run(
        [LARK_CLI, "im", "+messages-send", "--chat-id", chat, "--text", body],
        capture_output=True, text=True, timeout=30, env=SUBENV)
    if p.returncode != 0:
        log(f"发群失败 rc={p.returncode} stderr={p.stderr[:160]}")
        return False
    log(f"已发群: {text[:60]}")
    try:
        _req("/notify", {"chat_id": chat, "from": AGENT_ID, "text": text})  # 弹同群别的 agent
    except Exception as e:
        log(f"notify FAIL: {e}")
    return True


def _run_codex(chat, history_lines, trigger):
    """把群上下文 + 规则交给 codex 跑一轮,让它产出回复或 [SILENT]。"""
    convo = "\n".join(history_lines[-15:])
    prompt = (
        f"{RULES}\n\n"
        f"=== 群最近的对话(旧→新)===\n{convo}\n\n"
        f"=== 刚发生 ===\n{trigger}\n\n"
        "按上面的规则决定:要不要在群里回一句。\n"
        "把你最终要发进群的内容放在 <REPLY> 和 </REPLY> 之间(纯文本、简洁、像群聊一句话)。\n"
        "如果你判断该沉默,就输出 <REPLY>[SILENT]</REPLY>,别的什么都不要写。"
    )
    try:
        p = subprocess.run(
            [CODEX_BIN, "exec", prompt],
            capture_output=True, text=True, timeout=180, env=SUBENV)
    except Exception as e:
        log(f"codex 跑失败: {e}")
        return None
    out = p.stdout or ""
    mt = re.search(r"<REPLY>(.*?)</REPLY>", out, re.S)
    reply = (mt.group(1).strip() if mt else out.strip())
    if not reply or reply == "[SILENT]" or "[SILENT]" in reply[:12]:
        log("codex 判断沉默")
        return None
    return reply


def maybe_respond(chat, trigger):
    """冷却 + 单飞地跑一轮 codex,决定回不回。探针和弹针都走这里。"""
    now = time.time()
    with _LOCK:
        if chat in _running:
            return
        if now - _last_run.get(chat, 0) < COOLDOWN:
            return
        _last_run[chat] = now
        _running.add(chat)
    try:
        msgs = _fetch_history(chat) or []
        lines = []
        for m in reversed(msgs):                       # 旧→新
            who = "agent/bot" if (m.get("sender") or {}).get("sender_type") == "app" else "人"
            t = _msg_text(m)
            if t:
                lines.append(f"[{who}] {t}")
        reply = _run_codex(chat, lines, trigger)
        if reply:
            send_to_group(chat, reply)
    finally:
        with _LOCK:
            _running.discard(chat)


def wake_loop():
    """短轮询云总机:别的 agent 说话 → 总机弹 codex → 拍醒跑一轮。"""
    while True:
        try:
            res = _req(f"/poll?agent={AGENT_ID}")
        except Exception as e:
            log(f"poll FAIL: {e}")
            time.sleep(3)
            continue
        for w in res.get("wakes", []):
            frm, txt = w.get("from", "someone"), w.get("text", "")
            log(f"收到弹针: from={frm} text={txt[:40]}")
            threading.Thread(
                target=maybe_respond,
                args=(w.get("chat_id", ""), f"群里「{frm}」刚说:{txt}"),
                daemon=True).start()
        time.sleep(3)


def detect_loop():
    """本地探针:轮询群历史,发现新消息(非 codex 自己发的)→ 拍醒 codex。
    这条让 codex 不靠总机也能对「人」的消息反应(人 @ 不到非 bot 的 codex,只能靠拉)。"""
    seen = {c: None for c in CHATS}
    while True:
        for chat in CHATS:
            msgs = _fetch_history(chat)
            if msgs is None:
                continue
            if seen[chat] is None:                     # 首轮只播种,不回溯历史
                seen[chat] = {m.get("message_id") for m in msgs if m.get("message_id")}
                continue
            fresh = []
            for m in reversed(msgs):                   # 旧→新
                mid = m.get("message_id")
                if not mid or mid in seen[chat]:
                    continue
                seen[chat].add(mid)
                t = _msg_text(m)
                if t.startswith(MARK):                 # 自己发的,跳过(防自激)
                    continue
                fresh.append(t)
            if fresh:
                last = fresh[-1]
                log(f"探到新消息 → 拍醒 codex: {last[:40]}")
                threading.Thread(
                    target=maybe_respond,
                    args=(chat, f"群里有人刚说:{last}"),
                    daemon=True).start()
            if len(seen[chat]) > 200:
                seen[chat] = set(list(seen[chat])[-100:])
        time.sleep(POLL_INTERVAL)


if __name__ == "__main__":
    log(f"codex-client 启动 BUS={BUS_URL} chats={CHATS} codex={CODEX_BIN}")
    if BUS_TOKEN and CHATS:
        register()
        threading.Thread(target=wake_loop, daemon=True).start()
    detect_loop()

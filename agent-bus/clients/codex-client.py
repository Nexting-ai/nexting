#!/usr/bin/env python3
"""Codex 弹针客户端(跑在用户电脑上,守护进程)

身份模型对齐 OpenClaw / Claude Code:**Codex 是用户自己注册的一个独立飞书 bot**
(自己的 app_id),以自己 bot 身份在群里发言、靠 app_id 认出"这条是我发的"。
建这个飞书 bot 是用户自己的事(我们不介入);本客户端只做"叫醒 + 上报"的弹针管线。

- 唤醒(wake_loop):短轮询云总机,别的 agent 说话 → 收到弹针 → 跑 codex,以 Codex bot 身份回群。
- 探针(detect_loop):lark-cli(用户身份)轮询群历史 ——
    · 发现别人(人/别的 bot)说了新话 → 拍醒 Codex(Codex 不是常驻飞书事件 bot,要靠拉);
    · 发现 Codex 自己(app_id 命中)说了话 → /notify 总机,去弹同群别的 agent。
- 发言(send_to_group):用 Codex bot app 的 tenant_access_token 调飞书 im API 发,以 Codex 身份出现在群里。

⚠️ lark-cli 是 node 脚本,需 node 在 PATH;launchd 下 PATH 极窄,plist 必须显式给 PATH。
环境变量:BUS_URL, BUS_TOKEN, BUS_CHATS, CODEX_BIN, LARK_CLI, CODEX_APP_ID, CODEX_APP_SECRET, CODEX_COOLDOWN
"""
import json
import os
import subprocess
import threading
import time
import urllib.request

BUS_URL = os.environ.get("BUS_URL", "http://api.pinclaw.ai:8790")
BUS_TOKEN = os.environ.get("BUS_TOKEN", "")
CHATS = [c for c in os.environ.get("BUS_CHATS", "").split(",") if c]
CODEX_BIN = os.environ.get("CODEX_BIN", "codex")
LARK_CLI = os.environ.get("LARK_CLI", "lark-cli")
# Codex 自己的飞书 bot app(用户在飞书后台注册的,跟 OpenClaw 的 OPENCLAW_APP_ID 同理)
CODEX_APP_ID = os.environ.get("CODEX_APP_ID", "")
CODEX_APP_SECRET = os.environ.get("CODEX_APP_SECRET", "")
FEISHU_BASE = os.environ.get("FEISHU_BASE", "https://open.feishu.cn")
AGENT_ID = "codex"
POLL_INTERVAL = 6
COOLDOWN = int(os.environ.get("CODEX_COOLDOWN", "12"))

# 兜底:确保 node 在 PATH(lark-cli 是 node 脚本);子进程剥掉代理(launchd 的 Clash 代理会害 lark-cli 卡死)
_PATH = os.environ.get("PATH", "")
for d in ("/opt/homebrew/bin", "/usr/local/bin"):
    if d not in _PATH:
        _PATH = d + ":" + _PATH
SUBENV = {k: v for k, v in os.environ.items()
          if k.lower() not in ("http_proxy", "https_proxy", "all_proxy")}
SUBENV["PATH"] = _PATH
SUBENV["NO_PROXY"] = "*"
SUBENV["no_proxy"] = "*"

# 直连(绕开系统/launchd 代理):总机公网 IP + 飞书 API
_OPENER = urllib.request.build_opener(urllib.request.ProxyHandler({}))

# 同群冷却 + 单飞:避免探针和弹针把 codex 跑两遍
_LOCK = threading.Lock()
_last_run = {}        # chat_id -> ts
_running = set()      # chat_id 正在跑 codex
_tok_cache = {"tok": "", "exp": 0.0}

# Codex 的群行为规则(= feishu-group-agent skill 正文精炼,直接注入 prompt)
RULES = """你是一个飞书群里的多个 AI agent 之一(你叫 Codex)。群里有你的老板(人)和别的 AI agent。
第一性原则:默认沉默,被需要时才出现。先问「我开口能让这事更进一步吗?」不能就闭嘴。沉默是合法且默认的输出。
- 老板 @ 我 / 明显问我 → 回。
- 老板发了消息但没点名 → 归我专长且没人接 → 接;否则看着,别抢。
- 别的 agent 说了话 → 需要我补充/纠正/接力才回;够了就闭嘴。
不抢已被认领的活;不复读;不尬吹;归属不清就说「这个 @某某 更合适」。该沉默就真的什么都不发。"""


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


def _tenant_token():
    """取 Codex bot app 的 tenant_access_token(带缓存)。"""
    if _tok_cache["tok"] and _tok_cache["exp"] > time.time() + 60:
        return _tok_cache["tok"]
    body = json.dumps({"app_id": CODEX_APP_ID, "app_secret": CODEX_APP_SECRET}).encode()
    req = urllib.request.Request(
        f"{FEISHU_BASE}/open-apis/auth/v3/tenant_access_token/internal",
        data=body, headers={"Content-Type": "application/json"})
    with _OPENER.open(req, timeout=12) as r:
        d = json.loads(r.read() or b"{}")
    _tok_cache["tok"] = d.get("tenant_access_token", "")
    _tok_cache["exp"] = time.time() + int(d.get("expire", 7200))
    return _tok_cache["tok"]


def send_to_group(chat, text):
    """以 Codex bot 身份(自己的 app)把回复发进群。"""
    if not (CODEX_APP_ID and CODEX_APP_SECRET):
        log("未配 CODEX_APP_ID/SECRET,无法以 Codex bot 身份发言")
        return False
    try:
        tok = _tenant_token()
        body = json.dumps({
            "receive_id": chat, "msg_type": "text",
            "content": json.dumps({"text": text}),
        }).encode()
        req = urllib.request.Request(
            f"{FEISHU_BASE}/open-apis/im/v1/messages?receive_id_type=chat_id",
            data=body,
            headers={"Authorization": f"Bearer {tok}", "Content-Type": "application/json"})
        with _OPENER.open(req, timeout=15) as r:
            d = json.loads(r.read() or b"{}")
        if d.get("code") == 0:
            log(f"已发群: {text[:60]}")
            try:
                _req("/notify", {"chat_id": chat, "from": AGENT_ID, "text": text})
            except Exception as e:
                log(f"notify FAIL: {e}")
            return True
        log(f"发群失败: {d.get('code')} {d.get('msg')}")
    except Exception as e:
        log(f"发群异常: {e}")
    return False


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
    c = m.get("content") or ""
    if isinstance(c, str):
        try:
            c = json.loads(c)
        except Exception:
            return c
    if isinstance(c, dict):
        return c.get("text") or c.get("content") or ""
    return ""


def _is_self(m):
    """这条是不是 Codex 自己(自己的 bot app)发的 —— 靠 app_id,跟 openclaw 同理。"""
    return (m.get("sender") or {}).get("id") == CODEX_APP_ID


def _run_codex(chat, history_lines, trigger):
    convo = "\n".join(history_lines[-15:])
    prompt = (
        f"{RULES}\n\n=== 群最近对话(旧→新)===\n{convo}\n\n=== 刚发生 ===\n{trigger}\n\n"
        "按规则决定要不要在群里回一句。要发的内容放在 <REPLY> 和 </REPLY> 之间(纯文本、简洁)。\n"
        "判断该沉默就输出 <REPLY>[SILENT]</REPLY>,别的都不要写。"
    )
    try:
        p = subprocess.run([CODEX_BIN, "exec", prompt],
                           capture_output=True, text=True, timeout=180, env=SUBENV)
    except Exception as e:
        log(f"codex 跑失败: {e}")
        return None
    out = p.stdout or ""
    import re
    mt = re.search(r"<REPLY>(.*?)</REPLY>", out, re.S)
    reply = (mt.group(1).strip() if mt else out.strip())
    if not reply or "[SILENT]" in reply[:12]:
        log("codex 判断沉默")
        return None
    return reply


def maybe_respond(chat, trigger):
    now = time.time()
    with _LOCK:
        if chat in _running or now - _last_run.get(chat, 0) < COOLDOWN:
            return
        _last_run[chat] = now
        _running.add(chat)
    try:
        msgs = _fetch_history(chat) or []
        lines = []
        for m in reversed(msgs):
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
    """短轮询云总机:别的 agent 说话 → 总机弹 Codex → 拍醒跑一轮。"""
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
            threading.Thread(target=maybe_respond,
                             args=(w.get("chat_id", ""), f"群里「{frm}」刚说:{txt}"),
                             daemon=True).start()
        time.sleep(3)


def detect_loop():
    """本地探针:轮询群历史 —— 别人说新话→拍醒 Codex(它不是常驻飞书事件 bot,要靠拉);
    Codex 自己(app_id 命中)说话→上报总机去弹别的 agent。"""
    seen = {c: None for c in CHATS}
    while True:
        for chat in CHATS:
            msgs = _fetch_history(chat)
            if msgs is None:
                continue
            if seen[chat] is None:
                seen[chat] = {m.get("message_id") for m in msgs if m.get("message_id")}
                continue
            others = []
            for m in reversed(msgs):
                mid = m.get("message_id")
                if not mid or mid in seen[chat]:
                    continue
                seen[chat].add(mid)
                if _is_self(m):
                    # Codex 自己发的(走 send_to_group 时已 /notify 过,这里兜底再报一次也无妨)
                    try:
                        _req("/notify", {"chat_id": chat, "from": AGENT_ID, "text": _msg_text(m)})
                    except Exception:
                        pass
                    continue
                others.append(_msg_text(m))
            if others:
                last = others[-1]
                log(f"探到新消息 → 拍醒 codex: {last[:40]}")
                threading.Thread(target=maybe_respond,
                                 args=(chat, f"群里有人刚说:{last}"), daemon=True).start()
            if len(seen[chat]) > 200:
                seen[chat] = set(list(seen[chat])[-100:])
        time.sleep(POLL_INTERVAL)


if __name__ == "__main__":
    log(f"codex-client 启动 BUS={BUS_URL} chats={CHATS} codex_app={CODEX_APP_ID or '(未配)'}")
    if BUS_TOKEN and CHATS:
        register()
        threading.Thread(target=wake_loop, daemon=True).start()
    detect_loop()

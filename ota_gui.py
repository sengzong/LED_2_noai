#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ota_gui.py - STM32N647 OTA 小软件(带描述, 多槽位) —— 无依赖(纯标准库 + 浏览器)
=============================================================================
运行:  python ota_gui.py
然后浏览器打开: http://127.0.0.1:17800
在页面拖入 appli.bin + 填描述 + 点发送。后端监听 8080, 等板子连接后发送容器:
[ magic u32 "OFW2" | ver u32 | desc_len u32 | img_len u32 | desc | img ]
"""
import base64
import json
import struct
import threading
import socket
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

MAGIC = 0x3257464F   # "OFW2"
VER   = 2
BOARD_PORT = 8080
WEB_PORT   = 17800

_state = {"phase": "idle", "sent": 0, "total": 0, "file": "", "desc": "", "log": []}
_lock = threading.Lock()


def _log(msg):
    with _lock:
        _state["log"].append(msg)
        if len(_state["log"]) > 200:
            _state["log"] = _state["log"][-200:]


def hex_to_bin(text):
    """Intel HEX(文本) -> (原始镜像字节, 基址)。支持 00/01/02/04/05 记录。"""
    buf_lo = None
    buf_hi = None
    upper = 0
    rows = []
    for line in text.splitlines():
        line = line.strip()
        if not line or line[0] != ":":
            continue
        b = bytes.fromhex(line[1:])
        c, a, t = b[0], (b[1] << 8) | b[2], b[3]
        d = b[4:4 + c]
        if t == 0x00:
            addr = upper + a
            if buf_lo is None or addr < buf_lo:
                buf_lo = addr
            e = addr + c
            if buf_hi is None or e > buf_hi:
                buf_hi = e
            rows.append((addr, d))
        elif t == 0x04:
            upper = ((d[0] << 8) | d[1]) << 16
        elif t == 0x02:
            upper = ((d[0] << 8) | d[1]) << 4
        elif t == 0x01:
            break
    if buf_lo is None:
        raise ValueError("HEX 里没有数据记录")
    buf = bytearray(buf_hi - buf_lo)
    for addr, d in rows:
        buf[addr - buf_lo: addr - buf_lo + len(d)] = d
    return bytes(buf), buf_lo


def _send_worker(filename, desc, data):
    with _lock:
        _state["phase"] = "listen"
        _state["sent"] = 0
        _state["total"] = 0
        _state["file"] = filename
        _state["desc"] = desc
    _log(f"监听 :{BOARD_PORT}，等待板子连接…")
    try:
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind(("0.0.0.0", BOARD_PORT))
        srv.listen(1)
        conn, addr = srv.accept()
        srv.close()
        _log(f"板子已连接 {addr[0]}:{addr[1]}")
    except OSError as e:
        _log(f"!! 监听失败: {e}")
        with _lock: _state["phase"] = "idle"
        return

    desc_b = desc.encode("utf-8")
    hdr = struct.pack("<IIII", MAGIC, VER, len(desc_b), len(data))
    payload = hdr + desc_b + data
    with _lock: _state["total"] = len(payload)
    with _lock: _state["phase"] = "sending"

    sent = 0
    try:
        while sent < len(payload):
            n = conn.send(payload[sent: sent + 65536])
            if n == 0:
                break
            sent += n
            with _lock: _state["sent"] = sent
    except OSError as e:
        _log(f"!! 发送中断: {e}")
    finally:
        conn.close()
    with _lock:
        _state["phase"] = "done"
        _state["sent"] = sent
    _log(f"发送完成 {sent:,}/{len(payload):,} B")


class Handler(BaseHTTPRequestHandler):
    def _send_json(self, obj):
        body = json.dumps(obj).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if urlparse(self.path).path == "/api/state":
            with _lock:
                s = dict(_state)
                s["log"] = list(_state["log"])
            self._send_json(s)
        else:
            body = HTML.encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

    def do_POST(self):
        ln = int(self.headers.get("Content-Length", 0))
        j = json.loads(self.rfile.read(ln))
        try:
            data = base64.b64decode(j.get("data", ""))
        except Exception:
            self._send_json({"ok": False, "err": "base64"})
            return
        fn = j.get("file", "")
        desc = j.get("desc", "").strip()
        base = None
        if fn.lower().endswith(".hex"):
            try:
                data, base = hex_to_bin(data.decode("utf-8", "replace"))
                _log(f"HEX 解析 → 原始镜像 {len(data):,} B @0x{base:08X}")
            except Exception as e:
                self._send_json({"ok": False, "err": f"hex: {e}"})
                return
        if not data or not desc:
            self._send_json({"ok": False, "err": "empty"})
            return
        t = threading.Thread(target=_send_worker, args=(fn, desc, data), daemon=True)
        t.start()
        self._send_json({"ok": True})

    def log_message(self, *a):
        pass


HTML = r"""<!DOCTYPE html><html lang="zh"><head><meta charset="utf-8">
<title>STM32N647 OTA</title>
<style>
 :root{--fg:#111;--bg:#f4f6f8;--ac:#1a7f37;--bd:#cfd6dd}
 *{box-sizing:border-box;font-family:system-ui,Segoe UI,Microsoft YaHei,sans-serif}
 body{background:var(--bg);color:var(--fg);margin:0;padding:24px;display:flex;justify-content:center}
 .card{width:520px;background:#fff;border:1px solid var(--bd);border-radius:12px;padding:22px}
 h1{font-size:20px;margin:0 0 14px}
 .drop{border:2px dashed #9aa6b0;border-radius:10px;padding:26px;text-align:center;color:#556;cursor:pointer;transition:.15s}
 .drop.over{border-color:var(--ac);background:#eaf7ee;color:var(--ac)}
 .drop b{display:block;font-size:15px}
 .fname{margin-top:10px;font-size:13px;color:#345}
 label{display:block;margin:16px 0 6px;font-size:13px;color:#556}
 input[type=text]{width:100%;padding:9px 10px;border:1px solid var(--bd);border-radius:8px;font-size:14px}
 .row{margin-top:16px;display:flex;align-items:center;gap:12px}
 button{padding:10px 22px;border:0;border-radius:8px;background:var(--ac);color:#fff;font-size:15px;cursor:pointer}
 button:disabled{background:#aab6bf;cursor:not-allowed}
 .bar{height:12px;background:#e3e8ed;border-radius:6px;overflow:hidden;margin-top:16px}
 .bar i{display:block;height:100%;width:0;background:var(--ac);transition:width .25s}
 .pct{font-size:12px;color:#556;margin-top:5px}
 .log{margin-top:14px;background:#11151a;color:#9fe8b0;border-radius:8px;padding:10px;font:12px/1.5 Consolas,monospace;height:150px;overflow:auto;white-space:pre-wrap}
 .hint{font-size:12px;color:#889;margin-top:14px}
</style></head><body><div class="card">
<h1>STM32N647 OTA · 多槽位固件</h1>
<div class="drop" id="drop"><b>把 appli 固件拖到此处 (支持 .bin / .hex)</b><span>或点这里选择文件</span></div>
<input type="file" id="file" accept=".bin,.hex,application/octet-stream" hidden>
<div class="fname" id="fname"></div>
<label>描述（切换界面会显示）</label>
<input type="text" id="desc" placeholder="例如：Camera demo v2" maxlength="200">
<div class="row"><button id="send" disabled>发送</button><span class="pct" id="pct"></span></div>
<div class="bar"><i id="bar"></i></div>
<div class="log" id="log"></div>
<div class="hint">后端监听 8080 等板子(TCP 客户端 192.168.2.11)连接；板子 LCD 会显示接收进度，收完自动存入空闲槽位。</div>
</div>
<script>
let fileBuf=null, fileName="";
const $=id=>document.getElementById(id);
const drop=$('drop');
['dragenter','dragover'].forEach(ev=>drop.addEventListener(ev,e=>{e.preventDefault();drop.classList.add('over');}));
['dragleave','drop'].forEach(ev=>drop.addEventListener(ev,e=>{e.preventDefault();drop.classList.remove('over');}));
drop.addEventListener('drop',e=>{const f=e.dataTransfer.files[0];if(f)load(f);});
drop.addEventListener('click',()=>$('file').click());
$('file').addEventListener('change',e=>{if(e.target.files[0])load(e.target.files[0]);});
function load(f){
  fileName=f.name;
  const r=new FileReader();
  r.onload=()=>{fileBuf=r.result;$('fname').textContent=`${fileName}  (${fileBuf.byteLength.toLocaleString()} B)`;$('send').disabled=false;};
  r.readAsArrayBuffer(f);
}
function toB64(blob){return new Promise((res,rej)=>{const r=new FileReader();r.onload=()=>res(r.result.split(',')[1]);r.onerror=rej;r.readAsDataURL(blob);});}
$('send').onclick=async()=>{
  if(!fileBuf)return;
  const desc=$('desc').value.trim();
  if(!desc){alert('请填写描述');return;}
  $('send').disabled=true;
  const b64=await toB64(new Blob([fileBuf]));
  await fetch('/api/send',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({file:fileName,desc:desc,data:b64})});
};
let seen=0;
setInterval(async()=>{
  let s;
  try{s=await (await fetch('/api/state')).json();}catch(e){return;}
  const p=s.total?s.sent/s.total:0;
  $('bar').style.width=(p*100).toFixed(1)+'%';
  const ph={idle:'就绪',listen:'等待板子连接…',sending:'发送中',done:'已完成'}[(s.phase)]||s.phase;
  $('pct').textContent=`${ph}  ${s.sent? (s.sent/1024).toFixed(0)+'/':''}${s.total? (s.total/1024).toFixed(0)+' KB':''}`;
  if(s.phase==='done')$('send').disabled=false;
  if(s.log.length>seen){$('log').textContent=s.log.join('\n');$('log').scrollTop=$('log').scrollHeight;seen=s.log.length;}
},300);
</script></body></html>"""


def main():
    srv = ThreadingHTTPServer(("0.0.0.0", WEB_PORT), Handler)
    print(f"OTA GUI 运行中 -> 浏览器打开 http://127.0.0.1:{WEB_PORT}/")
    srv.serve_forever()


if __name__ == "__main__":
    main()
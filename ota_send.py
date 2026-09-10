#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
OTA appli 发送工具 (PC 端)
板子(TCP 客户端)主动连 PC 192.168.2.10:8080; 本脚本监听后把新的 appli.bin 裸流发给板子, 发完关闭连接。

用法:
  python ota_send.py --file <appli.bin>           # 默认发 Appli/Release/LED_2_Appli.bin
  python ota_send.py --port 8080 --file x.bin
  python ota_send.py --yes                       # 无需确认直接发(测试用)

板子侧串口应在收到的字节数后打印 "OTA: session end, received N bytes (appli)."
"""
import argparse
import socket
import sys

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bind", default="0.0.0.0", help="监听地址")
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--file", default=r"F:\stm32CubeIde\Project\LED_2_noai\Appli\Release\LED_2_Appli.bin")
    ap.add_argument("--yes", action="store_true", help="不确认直接发")
    args = ap.parse_args()

    try:
        with open(args.file, "rb") as f:
            data = f.read()
    except OSError as e:
        print("! 打不开文件:", e)
        sys.exit(1)
    print(f"appli: {len(data)} bytes from {args.file}")
    if len(data) == 0:
        print("! 文件为空, 退出")
        sys.exit(1)

    if not args.yes:
        r = input("确认发送给板子(y/N)? ").strip().lower()
        if r not in ("y", "yes"):
            print("取消")
            sys.exit(0)

    ln = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    ln.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    ln.bind((args.bind, args.port))
    ln.listen(1)
    print(f"监听 {args.bind}:{args.port}, 等板子连接...")
    conn, addr = ln.accept()
    print(f"板子已连接: {addr}, 开始发送 {len(data)} 字节...")

    sent = 0
    view = memoryview(data)
    try:
        conn.sendall(data)               # 一次性发整包(Windows TCP 会自动分片)
        sent = len(data)
    except OSError as e:
        print("! 发送中断:", e)
    finally:
        conn.close(); ln.close()

    print(f"发送完成: {sent}/{len(data)} 字节。板子串口应显示收到 {sent} bytes。")

if __name__ == "__main__":
    main()
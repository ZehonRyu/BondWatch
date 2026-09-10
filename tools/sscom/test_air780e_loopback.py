"""合宙 Air780E AT 回环测试（CH340 串口）。

回环含义：
1) 发 AT，应回 OK
2) 开回显 ATE1，再发 AT，应先回显 AT 再回 OK
3) 读版本 ATI / AT+GMR
"""
from __future__ import annotations

import sys
import time

import serial
import serial.tools.list_ports

DEFAULT_PORT = "COM4"
BAUDS = [115200, 9600, 57600, 921600]
CMDS = [
    ("AT", "basic"),
    ("ATE1", "echo on"),
    ("AT", "echo loop"),
    ("ATI", "identify"),
    ("AT+GMR", "firmware"),
    ("AT+CPIN?", "sim"),
    ("AT+CSQ", "signal"),
]


def list_ports() -> None:
    print("=== COM ports ===")
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("  (none)")
        return
    for p in ports:
        print(f"  {p.device}: {p.description} [{p.hwid}]")


def drain(ser: serial.Serial, wait_s: float = 0.2) -> bytes:
    time.sleep(wait_s)
    chunks = []
    deadline = time.time() + 0.4
    while time.time() < deadline:
        n = ser.in_waiting
        if n:
            chunks.append(ser.read(n))
            deadline = time.time() + 0.15
        else:
            time.sleep(0.02)
    return b"".join(chunks)


def at_exchange(ser: serial.Serial, cmd: str, wait_s: float = 1.0) -> str:
    ser.reset_input_buffer()
    payload = (cmd + "\r\n").encode("ascii")
    ser.write(payload)
    ser.flush()
    time.sleep(wait_s)
    raw = ser.read(ser.in_waiting or 512)
    # keep reading a bit more for slow modules
    extra = drain(ser, 0.15)
    return (raw + extra).decode("utf-8", errors="replace")


def test_baud(port: str, baud: int) -> dict:
    result = {
        "port": port,
        "baud": baud,
        "ok": False,
        "echo_ok": False,
        "steps": [],
        "error": "",
    }
    try:
        ser = serial.Serial(
            port,
            baud,
            timeout=1.2,
            write_timeout=2,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
        )
        ser.dtr = True
        ser.rts = True
        boot = drain(ser, 0.35)
        if boot:
            result["steps"].append(("boot", boot.decode("utf-8", errors="replace")[:160]))

        for cmd, label in CMDS:
            resp = at_exchange(ser, cmd, wait_s=1.0 if cmd.startswith("AT+") else 0.8)
            compact = resp.replace("\r", "\\r").replace("\n", "\\n")
            has_ok = "OK" in resp
            echoed = cmd in resp.replace("\r", "").replace("\n", "")
            result["steps"].append((f"{label}:{cmd}", compact[:200], has_ok, echoed))
            if cmd == "AT" and label == "basic" and has_ok:
                result["ok"] = True
            if cmd == "AT" and label == "echo loop" and has_ok and echoed:
                result["echo_ok"] = True

        ser.close()
    except Exception as exc:  # noqa: BLE001
        result["error"] = str(exc)
    return result


def main() -> int:
    port = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_PORT
    list_ports()
    print()
    print(f"=== 合宙 Air780E AT 回环 @ {port} ===")

    ports = {p.device for p in serial.tools.list_ports.comports()}
    if port not in ports:
        print(f"FAIL: {port} 不存在。请插上合宙 USB（CH340）后再试。")
        return 2

    any_ok = False
    any_echo = False
    for baud in BAUDS:
        r = test_baud(port, baud)
        if r["error"]:
            print(f"[ERROR] {port} @ {baud}: {r['error']}")
            continue
        status = "PASS" if r["ok"] else "FAIL"
        echo = "ECHO-OK" if r["echo_ok"] else "echo-miss"
        print(f"[{status}/{echo}] {port} @ {baud}")
        for step in r["steps"]:
            if step[0] == "boot":
                print(f"  boot: {step[1]!r}")
            else:
                label, text, has_ok, echoed = step
                mark = "OK" if has_ok else "--"
                emark = "echo" if echoed else "    "
                print(f"  [{mark}|{emark}] {label} -> {text}")
        if r["ok"]:
            any_ok = True
        if r["echo_ok"]:
            any_echo = True
            # 找到可用波特率后不必再扫
            break

    print()
    if any_echo:
        print("OVERALL: PASS — AT 通，回显回环正常")
        return 0
    if any_ok:
        print("OVERALL: PARTIAL — AT 有 OK，但回显回环未确认（可再开 ATE1）")
        return 0
    print("OVERALL: FAIL - no AT response. Check first: PK->GND, 5V, TXD<->RX / RXD<->TX, GND, close apps holding the COM port")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())

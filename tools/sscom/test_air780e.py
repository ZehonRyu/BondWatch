"""Air780E / CH340 AT test — same commands as SSCOM tutorial."""
import time
import serial
import serial.tools.list_ports

TARGET = "COM6"
BAUDS = [115200, 9600, 57600, 921600]

print("=== COM ports ===")
for p in serial.tools.list_ports.comports():
    print(f"  {p.device}: {p.description} [{p.hwid}]")

if not any(p.device == TARGET for p in serial.tools.list_ports.comports()):
    print(f"WARNING: {TARGET} not found")

any_ok = False
for baud in BAUDS:
    try:
        ser = serial.Serial(TARGET, baud, timeout=2, write_timeout=2)
        ser.dtr = True
        ser.rts = True
        time.sleep(0.3)
        boot = ser.read(ser.in_waiting or 0)
        ser.reset_input_buffer()
        ser.write(b"AT\r\n")
        time.sleep(0.8)
        resp = ser.read(ser.in_waiting or 256)
        ser.close()
        text = resp.decode("utf-8", errors="replace").strip()
        boot_text = boot.decode("utf-8", errors="replace").strip()
        ok = "OK" in text
        if ok:
            any_ok = True
        status = "PASS" if ok else "FAIL"
        print(f"[{status}] {TARGET} @ {baud}: boot={len(boot)}B AT->{text!r}")
        if boot_text:
            print(f"       boot: {boot_text[:120]}")
    except Exception as e:
        print(f"[ERROR] {TARGET} @ {baud}: {e}")

print()
print("OVERALL:", "PASS - module responded" if any_ok else "FAIL - no AT response")

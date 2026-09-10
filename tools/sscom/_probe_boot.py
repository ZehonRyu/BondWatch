import time
import serial

port = "COM3"
print("open", port)
ser = serial.Serial(port, 115200, timeout=0.5, write_timeout=2)
ser.dtr = False
ser.rts = False
print("wait boot 3s...")
time.sleep(3.0)
boot = ser.read(4096)
print("boot bytes", len(boot), boot[:300])
for end in (b"\r\n", b"\r"):
    for cmd in (b"AT", b"ATI", b"AT+GMR"):
        ser.reset_input_buffer()
        ser.write(cmd + end)
        ser.flush()
        time.sleep(1.2)
        resp = ser.read(1024)
        print(repr(cmd + end), "->", resp)
ser.close()
print("done")

import socket
import json
import time
import math
import random
import sys

TARGET_IP = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("-") else "127.0.0.1"
TARGET_PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 5000

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
print(f"[UDP] Streaming to {TARGET_IP}:{TARGET_PORT} (Usage: python udp_test_sender.py [TARGET_IP] [PORT])")

t = 0
try:
    while True:
        rpm = int(5000 + 3000 * math.sin(t * 0.5) + random.randint(-200, 200))
        rpm = max(0, min(rpm, 10000))
        speed = max(0, min(int(40 + 60 * abs(math.sin(t * 0.3)) + random.randint(-5, 5)), 150))
        temp = int(55 + 20 * abs(math.sin(t * 0.1)) + random.randint(-2, 2))
        volt = round(355.0 + 10 * math.sin(t * 0.2) + random.uniform(-0.5, 0.5), 1)
        throttle = min(round(abs(math.sin(t * 0.4)) * 0.9 + random.uniform(0, 0.1), 2), 1.0)
        
        state = "Drive" if rpm > 7000 else ("Ready" if rpm > 3000 else "Idle")
        fault = 1 if (int(t * 10) % 100 == 0) else 0
        
        payload = json.dumps({
            "rpm": rpm,
            "speed": speed,
            "temp": temp,
            "volt": volt,
            "throttle": throttle,
            "state": state,
            "fault": fault
        })
        sock.sendto(payload.encode(), (TARGET_IP, TARGET_PORT))
        
        print(f"\r[>] RPM:{rpm:5d} | SPD:{speed:3d}km/h | TEMP:{temp}°C | BAT:{volt}V | THR:{throttle:.0%} | {state}", end="")
        t += 0.1
        time.sleep(0.1)

except KeyboardInterrupt:
    sock.close()
    print("\n[UDP] Sender stopped.")

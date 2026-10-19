import json
import subprocess

p = r"\\wsl.localhost\Ubuntu\home\bseceleanu\ha_config\.storage\http"
try:
    with open(p, "r", encoding="utf-8") as f:
        d = json.load(f)
except Exception:
    # fallback via wsl cat
    out = subprocess.check_output(["wsl", "-d", "Ubuntu", "-u", "root", "--", "cat", "/home/bseceleanu/ha_config/.storage/http"]).decode("utf-8")
    d = json.loads(out)

d["data"]["stable"]["use_x_forwarded_for"] = True
d["data"]["stable"]["trusted_proxies"] = ["127.0.0.1", "::1", "172.16.0.0/12", "192.168.1.0/24"]

new_json = json.dumps(d, indent=2)

try:
    with open(p, "w", encoding="utf-8") as f:
        f.write(new_json)
    print("SUCCESS_LOCAL_WRITE")
except Exception:
    import os
    tmp_path = os.path.abspath("scratch/temp_http.json")
    os.makedirs("scratch", exist_ok=True)
    with open(tmp_path, "w", encoding="utf-8") as f:
        f.write(new_json)
    wsl_tmp = subprocess.check_output(["wsl", "-d", "Ubuntu", "--", "wslpath", "-a", "-u", tmp_path.replace('\\', '/')]).decode().strip()
    subprocess.check_call(["wsl", "-d", "Ubuntu", "-u", "root", "--", "cp", wsl_tmp, "/home/bseceleanu/ha_config/.storage/http"])
    print("SUCCESS_WSL_COPY")



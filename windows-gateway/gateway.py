#!/usr/bin/env python3
"""CYD Satellite Tracker Windows gateway: local telemetry -> Cloudflare."""
from __future__ import annotations
import base64, ctypes, ctypes.wintypes, json, logging, os, sqlite3, time, urllib.error, urllib.request
from logging.handlers import RotatingFileHandler
from pathlib import Path

APP_DIR = Path(os.environ.get("LOCALAPPDATA", str(Path.home()))) / "CYDSatelliteTracker"
CONFIG_PATH, DB_PATH, LOG_PATH = APP_DIR / "config.json", APP_DIR / "telemetry.sqlite3", APP_DIR / "gateway.log"
DEFAULT_CONFIG = {"cyd_url":"http://cyd-tracker.local","cloud_url":"https://cyd-satellite-tracker.pages.dev","device_name":"CYD Satellite Tracker","pairing_code":""}
POLL_SECONDS, UPLOAD_SECONDS, HTTP_TIMEOUT = 5, 30, 8

class DATA_BLOB(ctypes.Structure):
    _fields_ = [("cbData", ctypes.wintypes.DWORD), ("pbData", ctypes.POINTER(ctypes.c_byte))]

def _blob(data):
    buf=ctypes.create_string_buffer(data,len(data))
    return DATA_BLOB(len(data),ctypes.cast(buf,ctypes.POINTER(ctypes.c_byte))),buf

def protect_secret(value):
    if os.name!="nt": raise RuntimeError("Windows DPAPI required")
    src,keep=_blob(value.encode("utf-8")); dst=DATA_BLOB()
    if not ctypes.windll.crypt32.CryptProtectData(ctypes.byref(src),"CYD Satellite Tracker",None,None,None,0,ctypes.byref(dst)): raise ctypes.WinError()
    try: return base64.b64encode(ctypes.string_at(dst.pbData,dst.cbData)).decode("ascii")
    finally: ctypes.windll.kernel32.LocalFree(dst.pbData)

def unprotect_secret(value):
    if os.name!="nt": raise RuntimeError("Windows DPAPI required")
    src,keep=_blob(base64.b64decode(value)); dst=DATA_BLOB()
    if not ctypes.windll.crypt32.CryptUnprotectData(ctypes.byref(src),None,None,None,None,0,ctypes.byref(dst)): raise ctypes.WinError()
    try: return ctypes.string_at(dst.pbData,dst.cbData).decode("utf-8")
    finally: ctypes.windll.kernel32.LocalFree(dst.pbData)

def setup():
    APP_DIR.mkdir(parents=True,exist_ok=True)
    h=RotatingFileHandler(LOG_PATH,maxBytes=1_000_000,backupCount=3,encoding="utf-8")
    logging.basicConfig(level=logging.INFO,handlers=[h],format="%(asctime)s %(levelname)s %(message)s")
    if not CONFIG_PATH.exists(): CONFIG_PATH.write_text(json.dumps(DEFAULT_CONFIG,ensure_ascii=False,indent=2),encoding="utf-8")
    cfg=DEFAULT_CONFIG.copy(); cfg.update(json.loads(CONFIG_PATH.read_text(encoding="utf-8")))
    return cfg

def db_open():
    db=sqlite3.connect(DB_PATH)
    db.execute("CREATE TABLE IF NOT EXISTS pending(id INTEGER PRIMARY KEY AUTOINCREMENT,created_at INTEGER NOT NULL,payload TEXT NOT NULL)")
    db.execute("CREATE TABLE IF NOT EXISTS state(key TEXT PRIMARY KEY,value TEXT NOT NULL)")
    db.commit(); return db

def state_get(db,key):
    row=db.execute("SELECT value FROM state WHERE key=?",(key,)).fetchone()
    return row[0] if row else ""

def state_set(db,key,value):
    db.execute("INSERT INTO state(key,value) VALUES(?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value",(key,value)); db.commit()

def request_json(url,method="GET",payload=None,token=""):
    data=None if payload is None else json.dumps(payload,separators=(",",":")).encode()
    headers={"Accept":"application/json"}
    if data is not None: headers["Content-Type"]="application/json"
    if token: headers["Authorization"]="Bearer "+token
    req=urllib.request.Request(url,data=data,headers=headers,method=method)
    with urllib.request.urlopen(req,timeout=HTTP_TIMEOUT) as res:
        raw=res.read(256_000).decode("utf-8",errors="replace")
        return res.status,json.loads(raw) if raw else {}

def pair(cfg,db):
    saved=state_get(db,"device_token")
    if saved:
        try: return unprotect_secret(saved)
        except Exception: logging.exception("Cannot decrypt saved token; pairing again")
    code="".join(c for c in str(cfg.get("pairing_code","")) if c.isdigit())
    if len(code)!=8: return ""
    _,result=request_json(cfg["cloud_url"].rstrip("/")+"/api/devices/claim","POST",{"code":code,"device_name":cfg["device_name"]})
    token=result.get("device_token","")
    if not token: raise RuntimeError("Pairing failed: "+str(result.get("error",result)))
    state_set(db,"device_token",protect_secret(token)); state_set(db,"device_id",str(result.get("device_id","")))
    cfg["pairing_code"]=""; CONFIG_PATH.write_text(json.dumps(cfg,ensure_ascii=False,indent=2),encoding="utf-8")
    logging.info("Device paired; token protected by Windows DPAPI")
    return token

def read_local(cfg):
    _,data=request_json(cfg["cyd_url"].rstrip("/")+"/api/v1/status")
    return data

def cloud_payload(s):
    h=s.get("hardware") or {}
    return {"wifi_rssi":s.get("wifi_rssi"),"ip":str(s.get("ip","")),"gps_valid":s.get("gps_valid") is True,
      "latitude":s.get("latitude"),"longitude":s.get("longitude"),"altitude":s.get("altitude_m"),
      "gps_satellites":s.get("gps_satellites"),"firmware":str(s.get("firmware","")),
      "hardware_id":str(s.get("hardware_id","")),"gps_connected":h.get("gps_connected") is True,
      "angle_sensor_connected":h.get("angle_sensor_connected") is True,"compass_connected":h.get("compass_connected") is True,
      "motor_drivers_connected":h.get("motor_drivers_connected") is True,"limit_switches_connected":h.get("limit_switches_connected") is True}

def queue(db,payload):
    db.execute("INSERT INTO pending(created_at,payload) VALUES(?,?)",(int(time.time()),json.dumps(payload,separators=(",",":"))))
    db.execute("DELETE FROM pending WHERE id NOT IN (SELECT id FROM pending ORDER BY id DESC LIMIT 500)"); db.commit()

def upload_one(cfg,db,token):
    row=db.execute("SELECT id,payload FROM pending ORDER BY id LIMIT 1").fetchone()
    if not row: return True
    try:
        _,result=request_json(cfg["cloud_url"].rstrip("/")+"/api/devices/heartbeat","POST",json.loads(row[1]),token)
        if result.get("ok") is True:
            db.execute("DELETE FROM pending WHERE id=?",(row[0],)); db.commit()
            logging.info("Cloud sync successful; queue=%d",db.execute("SELECT COUNT(*) FROM pending").fetchone()[0])
            return True
        logging.warning("Cloud did not accept telemetry: %s",result)
        return False
    except urllib.error.HTTPError as e:
        if e.code==401:
            db.execute("DELETE FROM state WHERE key='device_token'"); db.commit()
            logging.error("Cloud token rejected (401); set a new pairing code")
            return False
        logging.warning("Cloud HTTP %s; telemetry queued",e.code)
        return False
    except Exception as e:
        logging.warning("Cloud unavailable; telemetry queued: %s",e)
        return False

def main():
    if os.name!="nt": raise SystemExit("Windows 10/11 required")
    cfg=setup(); db=db_open(); token=""; next_capture=0; next_upload=0
    logging.info("Gateway started; CYD endpoint=%s",cfg["cyd_url"])
    while True:
        try:
            if not token: token=pair(cfg,db)
            status=read_local(cfg); payload=cloud_payload(status)
            logging.info("CYD local API OK; IP=%s GPS fix=%s",status.get("ip"),payload["gps_valid"])
            now=time.monotonic()
            if token and now>=next_capture:
                queue(db,payload); next_capture=now+UPLOAD_SECONDS
            if token and now>=next_upload:
                if not upload_one(cfg,db,token): token=""
                next_upload=now+15
        except urllib.error.URLError as e: logging.warning("Local CYD or network unavailable: %s",e)
        except Exception: logging.exception("Gateway cycle failed")
        time.sleep(POLL_SECONDS)

if __name__=="__main__": main()

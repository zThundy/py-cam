from flask import Flask, request, send_file, jsonify, render_template
from flask_sock import Sock

from pathlib import Path
from datetime import datetime
import threading
import json
import re
import tempfile
import subprocess
import socket
import time
import logging

app = Flask(__name__)
sock = Sock(app)
app.logger.setLevel(logging.ERROR)

BASE_DIR = Path(__file__).parent
IMAGE_DIR = BASE_DIR / "images"
IMAGE_DIR.mkdir(exist_ok=True)
MAX_IMAGES = 600

clients = []
clients_lock = threading.Lock()

MULTICAST_GROUP = '239.1.2.3'
MULTICAST_PORT = 3344
MESSAGGIO = b"SERVER_ALIVE"

def avvia_multicast_beacon():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 1)
    print("[MULTICAST] Beacon avviato. Invio segnali di scoperta...")
    
    while True:
        try:
          sock.sendto(MESSAGGIO, (MULTICAST_GROUP, MULTICAST_PORT))
          time.sleep(3)
        except Exception as e:
          print(f"[MULTICAST] Errore: {e}")
          time.sleep(5)

cameras = {}

def device_from_request():
  header = request.headers.get("X-Device-ID", "unknown")
  device_id = re.sub(r"[^A-Za-z0-9._-]", "_", header)
  return device_id

def device_dir(device):
  p = IMAGE_DIR / device
  p.mkdir(exist_ok=True)
  return p

@sock.route("/stream")
def stream(ws):
  with clients_lock:
    clients.append(ws)
  try:
    while True:
      time.sleep(30)
  except:
    pass
  finally:
    with clients_lock:
      if ws in clients:
        clients.remove(ws)

def send_to_clients(message):
  dead=[]
  with clients_lock:
    for ws in clients:
      try:
        ws.send(json.dumps(message))
      except:
        dead.append(ws)
    for ws in dead:
      clients.remove(ws)

def cleanup(device):
  folder = device_dir(device)
  files = sorted(folder.glob("*.jpg"), key=lambda x:x.stat().st_mtime)

  while len(files)>MAX_IMAGES:
    try:
      files[0].unlink()
    except:
      pass

    files.pop(0)


@app.post("/upload")
def upload():
  data = request.data
  if not data:
    return "empty", 400

  device=device_from_request()
  folder=device_dir(device)
  timestamp=datetime.now().isoformat()

  count=cameras.get(device, {}).get("counter", 0)

  count+=1
  filename=f"{count}.jpg"
  path=folder / filename

  with open(path,"wb") as f:
    f.write(data)

  temp=request.headers.get("X-Device-TEMP", "")

  info={
    "device_id":device,
    "filename":filename,
    "timestamp":timestamp,
    "temp":temp,
  }

  cameras[device]={
    **info,
    "counter":count
  }

  send_to_clients(info)

  if count % 50 ==0:
    threading.Thread(target=cleanup, args=(device,), daemon=True).start()

  return jsonify({ "status":"ok"})

@app.get("/photo/<device>")
def photo(device):
  info=cameras.get(device)

  if not info:
    return "none",404

  path=device_dir(device)/info["filename"]
  if not path.exists():
    return "none",404

  return send_file(
      path,
      mimetype="image/jpeg"
  )


@app.get("/")
def index():
  return render_template("index.html")

@app.get("/download-video/<device>")
def video(device):
  folder = device_dir(device)

  images = sorted(
    folder.glob("*.jpg"),
    key=lambda x:x.stat().st_mtime
  )

  if not images:
    return "none", 404

  txt=tempfile.NamedTemporaryFile(
    mode="w",
    delete=False
  )

  for img in images:
    txt.write(
      f"file '{img}'\n"
    )

    txt.write(
      "duration 0.1\n"
    )


  txt.close()

  out=tempfile.NamedTemporaryFile(
    suffix=".mp4",
    delete=False
  )

  out.close()
  subprocess.run(
    [
      "ffmpeg",
      "-y",
      "-f",
      "concat",
      "-safe",
      "0",
      "-i",
      txt.name,
      "-pix_fmt",
      "yuv420p",
      out.name
    ]
  )

  return send_file(
    out.name,
    as_attachment=True,
    download_name=device+".mp4"
  )

if __name__=="__main__":
  thread_beacon = threading.Thread(target=avvia_multicast_beacon, daemon=True)
  thread_beacon.start()

  app.run(host="0.0.0.0", port=4512, threaded=True, debug=True)
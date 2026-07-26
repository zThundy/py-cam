from flask import Flask, request, send_file, jsonify, render_template
from flask.logging import default_handler
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
import os

app = Flask(__name__)
sock = Sock(app)

BASE_DIR = Path(__file__).parent
IMAGE_DIR = BASE_DIR / (os.getenv("IMAGESPATH") or "images")
IMAGE_DIR.mkdir(exist_ok=True)
MAX_IMAGES = os.getenv("MAXIMAGES") or 600

clients = []
clients_lock = threading.Lock()

MULTICAST_GROUP = os.getenv("MULTICASTGROUP") or '239.1.2.3'
MULTICAST_PORT = os.getenv("MULTICASTPORT") or 3344
MESSAGE = b"SERVER_ALIVE"

def defineLogsLevel():
  logLevel = os.getenv("LOGLEVEL") or "DEBUG"
  match logLevel:
    case "DEBUG":
      return logging.DEBUG
    case "WARNING":
      return logging.WARN
    case "INFO":
      return logging.INFO
    case "ERROR":
      return logging.ERROR
    case _:
      return logging.INFO

logBaseDir = os.getenv("LOGPATH") or "logs"

app.logger.removeHandler(default_handler)
logger = logging.getLogger('app')
logger.setLevel(defineLogsLevel())
app.logger.setLevel(defineLogsLevel())

formatter = logging.Formatter('[%(levelname)s] - (%(asctime)s) - %(message)s')

file_handler = logging.FileHandler(BASE_DIR / logBaseDir / 'main.log')
file_handler.setLevel(defineLogsLevel())
file_handler.setFormatter(formatter)

console_handler = logging.StreamHandler()
console_handler.setLevel(defineLogsLevel())
console_handler.setFormatter(formatter)

flask_file_handler = logging.FileHandler(BASE_DIR / logBaseDir / 'flask.log')
flask_file_handler.setLevel(defineLogsLevel())
flask_file_handler.setFormatter(formatter)

flask_console_handler = logging.StreamHandler()
flask_console_handler.setLevel(defineLogsLevel())
flask_console_handler.setFormatter(formatter)

logger.addHandler(file_handler)
logger.addHandler(console_handler)
app.logger.addHandler(flask_file_handler)
app.logger.addHandler(flask_console_handler)

def avvia_multicast_beacon():
  sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
  sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 1)
  logger.debug(f"Multicast broadcast running on {MULTICAST_GROUP}:{MULTICAST_PORT}")
  
  while True:
    try:
      sock.sendto(MESSAGE, (MULTICAST_GROUP, int(MULTICAST_PORT)))
      time.sleep(3)
    except Exception as e:
      logger.error(f"Multicast error: {e}")
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
  logger.debug("Got /stream request. Adding ws client to clients list")
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
        logger.debug("Client is dead. Removing from clients list")
        clients.remove(ws)

def send_to_clients(message):
  logger.debug(f"About to send message to clients: {message}")
  dead = []
  with clients_lock:
    for ws in clients:
      try:
        logger.debug("Message sent to clients via websocket")
        ws.send(json.dumps(message))
      except:
        logger.error("Found dead client while sending message.")
        dead.append(ws)
    for ws in dead:
      logger.debug("Client died while sending message. Removing client from list of clients")
      clients.remove(ws)

def cleanup(device):
  logger.debug("Trying to cleanup older files")
  folder = device_dir(device)
  files = sorted(folder.glob("*.jpg"), key=lambda x:x.stat().st_mtime)

  logger.debug(f"About to remove {MAX_IMAGES - len(files)}")

  while len(files) > MAX_IMAGES:
    try:
      files[0].unlink()
    except Exception as e:
      logger.error(f"Error while removing pictures: {e}")
      pass

    files.pop(0)


@app.post("/upload")
def upload():
  data = request.data
  if not data:
    return "empty", 400

  device = device_from_request()
  folder = device_dir(device)
  timestamp=datetime.now().isoformat()
  logger.debug(f"Got upload request from client {device}")

  count = cameras.get(device, {}).get("counter", 0)

  count += 1
  filename = f"{count}.jpg"
  path = folder / filename

  with open(path,"wb") as f:
    f.write(data)

  temp = request.headers.get("X-Device-TEMP", "")
  logger.debug(f"Device {device} temperature is {temp}")

  info={
    "device_id": device,
    "filename": filename,
    "timestamp": timestamp,
    "temp": temp,
  }

  cameras[device] = { **info, "counter":count }

  send_to_clients(info)

  if count % 50 == 0:
    threading.Thread(target=cleanup, args=(device,), daemon=True).start()

  return jsonify({ "status":"ok"})

@app.get("/photo/<device>")
def photo(device):
  logger.debug(f"Got photo request for device {device}")
  info = cameras.get(device)

  if not info:
    return "none", 404

  path = device_dir(device) / info["filename"]
  if not path.exists():
    return "none", 404

  logger.debug(f"Sending image from device {device}")
  return send_file(path, mimetype="image/jpeg")


@app.get("/")
def index():
  logger.debug("Serving index to client.")
  return render_template("index.html")

@app.get("/download-video/<device>")
def video(device):
  logger.debug(f"Got video download request from client for device {device}")
  folder = device_dir(device)

  images = sorted(folder.glob("*.jpg"), key=lambda x:x.stat().st_mtime)
  logger.debug(f"About to generate video using {len(images)} from device {device}")

  if not images:
    return "none", 404

  txt = tempfile.NamedTemporaryFile(mode="w", delete=False)

  for img in images:
    txt.write(f"file '{img}'\n")
    txt.write("duration 0.1\n")

  txt.close()

  out = tempfile.NamedTemporaryFile(suffix=".mp4", delete=False)

  out.close()
  subprocess.run([
    "ffmpeg",
    "-y", 
    "-f", "concat",
    "-safe", "0",
    "-i", txt.name,
    "-pix_fmt", "yuv420p",
    out.name
  ])

  return send_file(
    out.name,
    as_attachment=True,
    download_name=device+".mp4"
  )

if __name__=="__main__":
  thread_beacon = threading.Thread(target=avvia_multicast_beacon, daemon=True)
  thread_beacon.start()

  port = os.getenv("SERVICE_PORT") or 4512
  
  hostname = socket.gethostname()
  IPAddr = socket.gethostbyname(hostname)

  logger.info(f"Application is running on http://{IPAddr}:{port}")
  app.run(host="0.0.0.0", port=port, threaded=True, debug=False)
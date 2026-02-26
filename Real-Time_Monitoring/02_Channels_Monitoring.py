import socket
import struct
import time
import subprocess
import select
import os
import sys
import signal
import logging
import csv
import datetime
import threading
import re
from logging.handlers import RotatingFileHandler

# ====================== CONFIG ======================
MONITOR_INTERVAL = 1.0
BIND_RETRIES = 5
BIND_RETRY_DELAY = 1.5
CSV_FILE = '02_channels_monitoring.csv'
LOG_FILE = '02_channels_monitoring.log'
NTP_SERVER = 'time.apple.com'
NTP_OFFSET_THRESHOLD_SEC = 0.050
FFMPEG_RESTART_DELAY = 5.0

channels = {
    "Rotana Masriya": "239.239.8.90:1234",
    "Rotana Moussica": "239.239.8.91:1234",
    "Rotana Clip": "239.239.8.92:1234",
    "Rotana Khalijah": "239.239.8.93:1234",
    "Rotana Cinema": "239.239.8.95:1234",
    "Rotana Classic": "239.239.8.96:1234"
}

channel_order = list(channels.keys())

# ====================== LOGGING ======================
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        RotatingFileHandler(LOG_FILE, maxBytes=10*1024*1024, backupCount=5),
        logging.StreamHandler(sys.stdout)
    ]
)

# ====================== FFPROBE CHECK ======================
try:
    subprocess.check_output(["ffprobe", "-version"], timeout=5)
    logging.info("✓ ffprobe detected.")
except Exception as e:
    logging.error(f"ffprobe not found: {e}")
    sys.exit(1)

# ====================== SOCKET SETUP ======================
port = 1234
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
if sys.platform == 'darwin':
    try: sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    except: pass

bound = False
for attempt in range(1, BIND_RETRIES + 1):
    try:
        sock.bind(('0.0.0.0', port))
        bound = True
        break
    except OSError as e:
        logging.warning(f"Bind attempt {attempt} failed: {e}")
        if attempt < BIND_RETRIES: time.sleep(BIND_RETRY_DELAY)
if not bound:
    logging.error("Failed to bind UDP socket.")
    sys.exit(1)

if sys.platform == 'darwin':
    IP_RECVDSTADDR = 7
    sock.setsockopt(socket.IPPROTO_IP, IP_RECVDSTADDR, 1)
    DSTADDR_CMSG_TYPE = IP_RECVDSTADDR

for addr in channels.values():
    host = addr.split(':')[0]
    mreq = struct.pack("4sl", socket.inet_aton(host), socket.INADDR_ANY)
    try:
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    except: pass

# ====================== STATUS ======================
lock = threading.Lock()
channel_status = {}
system_sync_status = "Checking..."
monitoring_processes = []

col_widths = [25, 25, 22, 18, 22, 16, 18, 22]
ansi_escape = re.compile(r'\x1B\[[0-?]*[ -/]*[@-~]')

def pad_colored(text: str, width: int) -> str:
    """Always returns string with EXACT visible width (colors preserved)"""
    clean = ansi_escape.sub('', text)
    color = "\033[91m" if '\033[91m' in text else "\033[93m" if '\033[93m' in text else ""
    end = "\033[0m" if color else ""

    if len(clean) > width:
        clean = clean[:width-3] + "..."   # safe truncate

    pad = width - len(clean)
    left = pad // 2
    right = pad - left
    return ' ' * left + color + clean + end + ' ' * right

# ====================== TIME SYNC ======================
def update_system_sync():
    global system_sync_status
    while True:
        try:
            out = subprocess.check_output(['sntp', NTP_SERVER], timeout=8, stderr=subprocess.STDOUT).decode('utf-8', errors='ignore').strip()
            m = re.match(r'^([+-][0-9.]+)', out)
            if m:
                offset_sec = abs(float(m.group(1)))
                system_sync_status = "Good Sync" if offset_sec <= NTP_OFFSET_THRESHOLD_SEC else f"\033[93mOffset {offset_sec:.3f}s\033[0m"
            else:
                system_sync_status = "\033[91mSync Failed\033[0m"
        except:
            system_sync_status = "\033[91mNTP Error\033[0m"
        time.sleep(60)

# ====================== FFmpeg MONITORING ======================
def start_channel_monitor(name: str, url: str):
    def monitor_output(proc):
        total_frames = 0
        black_ongoing = False
        black_start_time = None
        no_frame_timer = time.time()
        last_v_pts = last_a_pts = None

        while True:
            if proc.poll() is not None:
                with lock:
                    channel_status[name]['video_status'] = "\033[91mError\033[0m"
                logging.error(f"FFmpeg for {name} exited.")
                break

            rlist, _, _ = select.select([proc.stdout.fileno()], [], [], 0.1)
            if not rlist:
                if time.time() - no_frame_timer > 15.0:
                    with lock:
                        channel_status[name].update({
                            'video_status': "\033[91mError\033[0m",
                            'video_frames': "\033[91mNo Frames\033[0m",
                            'audio_status': "\033[91mNo Audio\033[0m",
                            'av_sync': "\033[91mN/A\033[0m"
                        })
                continue

            line = proc.stdout.readline().strip()
            if not line: continue

            if "black_start" in line:
                black_ongoing = True
                black_start_time = time.time()
                with lock: channel_status[name]['video_frames'] = "\033[91mBlack Video\033[0m"
                logging.info(f"BLACK START → {name}")

            elif "black_end" in line:
                black_ongoing = False
                black_start_time = None
                with lock: channel_status[name]['video_frames'] = f"{total_frames} frames"
                logging.info(f"BLACK END → {name}")

            if "freeze_start" in line:
                with lock: channel_status[name]['video_status'] = "\033[91mFrozen\033[0m"
            elif "freeze_end" in line:
                with lock: channel_status[name]['video_status'] = "Good Video"

            if "silence_start" in line:
                with lock: channel_status[name]['audio_status'] = "\033[91mNo Audio\033[0m"
            elif "silence_end" in line:
                with lock: channel_status[name]['audio_status'] = "Good Audio"

            if "pts_time:" in line:
                m = re.search(r'pts_time:([0-9.]+)', line)
                if m:
                    pts = float(m.group(1))
                    no_frame_timer = time.time()
                    if "ashowinfo" not in line:  # VIDEO
                        total_frames += 1
                        last_v_pts = pts
                        with lock:
                            if black_ongoing and time.time() - black_start_time > 25:
                                black_ongoing = False
                                channel_status[name]['video_frames'] = f"{total_frames} frames"
                                logging.warning(f"BLACK FALSE POSITIVE RECOVERED → {name}")
                            else:
                                channel_status[name]['video_frames'] = "\033[91mBlack Video\033[0m" if black_ongoing else f"{total_frames} frames"

                            if channel_status[name]['video_status'].startswith('\033[91m'):
                                channel_status[name]['video_status'] = "Good Video"
                                channel_status[name]['audio_status'] = "Good Audio"
                                channel_status[name]['av_sync'] = "In Sync"
                        last_a_pts = None
                    else:  # AUDIO
                        last_a_pts = pts
                        if last_v_pts is not None:
                            diff = abs(last_v_pts - last_a_pts)
                            new_sync = "\033[91mOut Sync\033[0m" if diff > 0.5 else "In Sync"
                            with lock:
                                if new_sync != channel_status[name]['av_sync']:
                                    channel_status[name]['av_sync'] = new_sync

    while True:
        cmd = ["ffmpeg", "-hide_banner", "-loglevel", "info", "-i", url,
               "-vf", "freezedetect=d=5.0,blackdetect=d=10.0:pix_th=0.12:pic_th=0.97,showinfo",
               "-af", "silencedetect=n=-30dB:d=15.0,ashowinfo", "-f", "null", "-"]
        try:
            proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    text=True, bufsize=1, universal_newlines=True)
            monitoring_processes.append(proc)
        except Exception as e:
            logging.error(f"Failed to start FFmpeg for {name}: {e}")
            time.sleep(FFMPEG_RESTART_DELAY)
            continue

        with lock:
            channel_status[name] = {'video_status': 'Good Video', 'video_frames': '0 frames',
                                    'audio_status': 'Good Audio', 'av_sync': 'In Sync'}

        threading.Thread(target=monitor_output, args=(proc,), daemon=True).start()
        proc.wait()
        time.sleep(FFMPEG_RESTART_DELAY)

# ====================== SIGNAL HANDLER ======================
def signal_handler(sig, frame):
    logging.info("Shutdown signal received.")
    for p in monitoring_processes:
        try: p.terminate(); p.wait(3)
        except: p.kill()
    sock.close()
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)
signal.signal(signal.SIGTERM, signal_handler)

# ====================== START ======================
logging.info("Starting perfectly aligned monitoring...")
for name in channel_order:
    url = f"udp://{channels[name]}?reuse=1&fifo_size=50000000"
    threading.Thread(target=start_channel_monitor, args=(name, url), daemon=True).start()
    logging.info(f"  ✓ {name}")

threading.Thread(target=update_system_sync, daemon=True).start()

# ====================== MAIN LOOP ======================
try:
    while True:
        period_start = time.time()
        period_bytes = {h.split(':')[0]: 0 for h in channels.values()}
        period_ts_packets = {h.split(':')[0]: 0 for h in channels.values()}

        while time.time() < period_start + MONITOR_INTERVAL:
            r, _, _ = select.select([sock], [], [], max(0, period_start + MONITOR_INTERVAL - time.time()))
            if r:
                data, ancdata, _, _ = sock.recvmsg(8192, 1024)
                dst_ip = None
                for level, typ, dat in ancdata:
                    if level == socket.IPPROTO_IP and typ == DSTADDR_CMSG_TYPE:
                        dst_ip = socket.inet_ntoa(dat) if sys.platform == 'darwin' else socket.inet_ntoa(dat[8:12])
                        break
                if dst_ip in period_bytes:
                    period_bytes[dst_ip] += len(data)
                    if len(data) % 188 == 0:
                        period_ts_packets[dst_ip] += len(data) // 188

        duration = time.time() - period_start
        if duration <= 0: continue

        os.system('clear')
        print(f"|{'Channel Name':<25}|{'Multicast':^25}|{'Video Status':^22}|"
              f"{'Video Frames':^18}|{'Audio Status':^22}|{'AV Sync':^16}|"
              f"{'Bitrate (Mbps)':^18}|{'System Sync':^22}|")
        print('|' + '|'.join('-' * w for w in col_widths) + '|')

        timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        with open(CSV_FILE, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(['Timestamp','Channel','Multicast','VideoStatus','VideoFrames',
                             'AudioStatus','AVSync','Bitrate_mbps','PPS','SystemSync','Status'])

            for name in channel_order:
                grp = channels[name].split(':')[0]
                b = period_bytes.get(grp, 0)
                p = period_ts_packets.get(grp, 0)
                bitrate_mbps = (b * 8.0 / duration) / 1_000_000 if duration > 0 else 0.0
                bitrate_str = f"{bitrate_mbps:.2f} Mbps"

                with lock:
                    vs = channel_status.get(name, {}).get('video_status', 'N/A')
                    vf = channel_status.get(name, {}).get('video_frames', 'N/A')
                    ast = channel_status.get(name, {}).get('audio_status', 'N/A')
                    avs = channel_status.get(name, {}).get('av_sync', 'N/A')

                if bitrate_mbps < 0.01:
                    bitrate_str = f"\033[91m{bitrate_str}\033[0m"
                    logging.warning(f"ZERO BITRATE: {name}")
                    with lock:
                        channel_status[name].update({
                            'video_status': "\033[91mNo Signal\033[0m",
                            'video_frames': "\033[91mNo Frames\033[0m",
                            'audio_status': "\033[91mNo Audio\033[0m",
                            'av_sync': "\033[91mN/A\033[0m"
                        })
                    vs = channel_status[name]['video_status']
                    vf = channel_status[name]['video_frames']
                    ast = channel_status[name]['audio_status']
                    avs = channel_status[name]['av_sync']

                # Safety override for false black with good signal
                if "Black Video" in vf and bitrate_mbps > 0.8:
                    vf = f"{channel_status[name].get('video_frames', '0 frames')}"
                    with lock: channel_status[name]['video_frames'] = vf

                print(f"|{name:<25}|{channels[name]:^25}|{pad_colored(vs, 22)}|"
                      f"{pad_colored(vf, 18)}|{pad_colored(ast, 22)}|"
                      f"{pad_colored(avs, 16)}|{pad_colored(bitrate_str, 18)}|"
                      f"{pad_colored(system_sync_status, 22)}|")

                writer.writerow([
                    timestamp, name, channels[name],
                    ansi_escape.sub('', vs), ansi_escape.sub('', vf),
                    ansi_escape.sub('', ast), ansi_escape.sub('', avs),
                    round(bitrate_mbps, 4), round(p / duration, 2) if duration > 0 else 0,
                    ansi_escape.sub('', system_sync_status),
                    "ZERO_BITRATE" if bitrate_mbps < 0.01 else "OK"
                ])

        time.sleep(0.05)

except Exception as e:
    logging.error(f"Main loop error: {e}", exc_info=True)
finally:
    for p in monitoring_processes:
        try: p.terminate(); p.wait(3)
        except: p.kill()
    sock.close()
    logging.info("Monitoring stopped.")
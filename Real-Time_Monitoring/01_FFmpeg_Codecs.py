import json
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

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('01_codecs.log'),
        logging.StreamHandler()
    ]
)

MONITOR_INTERVAL = 1.0
PROBE_TIMEOUT = 12
PROBE_RETRIES = 3
BIND_RETRIES = 5
BIND_RETRY_DELAY = 1.5
CSV_FILE = '01_channels_codecs.csv'

channels = {
    "Rotana Masriya": "239.239.8.90:1234",
    "Rotana Moussica": "239.239.8.91:1234",
    "Rotana Clip": "239.239.8.92:1234",
    "Rotana Khalijah": "239.239.8.93:1234",
    "Rotana Cinema": "239.239.8.95:1234",
    "Rotana Classic": "239.239.8.96:1234"
}

channel_order = list(channels.keys())

try:
    subprocess.check_output(["ffprobe", "-version"], timeout=5)
    logging.info("ffprobe detected.")
except Exception:
    logging.error("ffprobe not found. Install FFmpeg and ensure it's in PATH.")
    sys.exit(1)

static_info = {}
logging.info("Probing source stream status for all the Channels:")
for name in channel_order:
    addr = channels[name]
    host, port_str = addr.split(':')
    input_url = f"udp://{host}:{port_str}?reuse=1"
    cmd = ["ffprobe", "-v", "quiet", "-print_format", "json",
           "-show_format", "-show_streams", "-timeout", "10000000", input_url]

    success = False
    for attempt in range(PROBE_RETRIES):
        try:
            out = subprocess.check_output(cmd, timeout=PROBE_TIMEOUT)
            data = json.loads(out.decode('utf-8', errors='ignore'))
            video = next((s for s in data.get('streams', []) if s.get('codec_type') == 'video'), None)
            audio = next((s for s in data.get('streams', []) if s.get('codec_type') == 'audio'), None)

            if video:
                field_order = video.get('field_order', 'unknown')
                scan_type = 'Progressive' if field_order == 'progressive' else \
                            'Interlaced' if field_order in ['tt', 'bb', 'tb', 'bt'] else 'N/A'

                static_info[name] = {
                    'video_format': video.get('codec_name', 'N/A').upper(),
                    'resolution': f"{video.get('width', 'N/A')}x{video.get('height', 'N/A')}",
                    'scan_type': scan_type,
                    'audio_format': audio.get('codec_name', 'N/A').upper() if audio else 'N/A',
                    'fps': f"{eval(video.get('r_frame_rate', '0/1')):.2f}" if video.get('r_frame_rate') else 'N/A',
                    'source': addr
                }
                logging.info(f"  ✓ {name:.<35} OK")
                success = True
                break
        except Exception as e:
            logging.warning(f"  ✗ {name:.<35} failed (attempt {attempt+1}): {str(e)}")

    if not success:
        static_info[name] = {
            'video_format': 'N/A', 'resolution': 'N/A', 'scan_type': 'N/A',
            'audio_format': 'N/A', 'fps': 'N/A', 'source': addr
        }

logging.info("Static Probing Finished.")

# ────────────────────────────────────────────────
# Socket setup
# ────────────────────────────────────────────────
port = 1234
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
try:
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    logging.info("SO_REUSEPORT enabled")
except AttributeError:
    logging.warning("SO_REUSEPORT not available")

bound = False
for attempt in range(1, BIND_RETRIES + 1):
    try:
        sock.bind(('0.0.0.0', port))
        logging.info(f"Successfully bound to UDP port {port}")
        bound = True
        break
    except OSError as e:
        logging.warning(f"Bind attempt {attempt} failed: {e}")
        if attempt < BIND_RETRIES:
            time.sleep(BIND_RETRY_DELAY)

if not bound:
    sys.exit(1)

if sys.platform == 'darwin':
    IP_RECVDSTADDR = 7
    sock.setsockopt(socket.IPPROTO_IP, IP_RECVDSTADDR, 1)
    DSTADDR_CMSG_TYPE = IP_RECVDSTADDR
else:
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_PKTINFO, 1)
    DSTADDR_CMSG_TYPE = socket.IP_PKTINFO

for addr in channels.values():
    host, _ = addr.split(':')
    mreq = struct.pack("4sl", socket.inet_aton(host), socket.INADDR_ANY)
    try:
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)
    except Exception as e:
        logging.error(f"Failed to join {host}: {e}")

logging.info("Starting Real-Time Monitoring. Press Ctrl+C to stop.")
logging.info(f"CSV file will be OVERWRITTEN every second with latest values → {CSV_FILE}")

# ────────────────────────────────────────────────
# Column order and widths for table output
# ────────────────────────────────────────────────
col_order = ['channel', 'source', 'video_format', 'audio_format', 'resolution', 'scan_type', 'fps', 'current_bitrate']

col_widths = {
    'channel': 25,
    'source': 25,
    'video_format': 14,
    'audio_format': 14,
    'resolution': 14,
    'scan_type': 14,
    'fps': 7,
    'current_bitrate': 18
}

format_string = (
    "|{channel:<" + str(col_widths['channel']) + "}"
    "|{source:^" + str(col_widths['source']) + "}"
    "|{video_format:^" + str(col_widths['video_format']) + "}"
    "|{audio_format:^" + str(col_widths['audio_format']) + "}"
    "|{resolution:^" + str(col_widths['resolution']) + "}"
    "|{scan_type:^" + str(col_widths['scan_type']) + "}"
    "|{fps:^" + str(col_widths['fps']) + "}"
    "|{current_bitrate:^" + str(col_widths['current_bitrate']) + "}|"
)

def signal_handler(sig, frame):
    logging.info("Shutdown signal received.")
    sock.close()
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)

try:
    while True:
        period_start = time.time()
        period_bytes = {host.split(':')[0]: 0 for host in channels.values()}
        period_ts_packets = {host.split(':')[0]: 0 for host in channels.values()}

        end_time = period_start + MONITOR_INTERVAL
        while time.time() < end_time:
            timeout = max(0, end_time - time.time())
            r, _, _ = select.select([sock], [], [], timeout)
            if r:
                try:
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
                except Exception:
                    pass

        duration = time.time() - period_start
        if duration > 0:
            os.system('clear')
            print(format_string.format(
                channel='Channel Name',
                source='Multicast Source',
                video_format='Video Format',
                audio_format='Audio Format',
                resolution='Resolution',
                scan_type='Scan Type',
                fps='FPS',
                current_bitrate='Bitrate (Mbps)'
            ))
            print('|' + '|'.join('-' * col_widths[c] for c in col_order) + '|')

            timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

            # OVERWRITE CSV
            with open(CSV_FILE, 'w', newline='') as f:
                writer = csv.writer(f)
                writer.writerow([
                    'Timestamp', 'Channel Name', 'Multicast Source', 'Video_Format',
                    'Audio Format', 'Resolution', 'Scan Type', 'FPS', 'Bitrate_mbps', 'PPS', 'Status'
                ])

                for name in channel_order:
                    grp = channels[name].split(':')[0]
                    info = static_info.get(name, {
                        'video_format':'N/A', 'resolution':'N/A', 'scan_type':'N/A',
                        'audio_format':'N/A', 'fps':'N/A', 'source':'N/A'
                    })
                    b = period_bytes.get(grp, 0)
                    p = period_ts_packets.get(grp, 0)

                    bitrate_mbps = (b * 8.0 / duration) / 1_000_000 if b > 0 and duration > 0 else 0.0
                    bitrate_str = f"{bitrate_mbps:.2f} Mbps"

                    pps_val = round(p / duration, 2) if duration > 0 else 0.0
                    status = "ZERO_BITRATE" if b == 0 else "OK"

                    if b == 0:
                        bitrate_str = f"\033[91m{bitrate_str}\033[0m"
                        logging.warning(f"Zero bitrate detected: {name}")

                    print(format_string.format(
                        channel=name,
                        source=info['source'],
                        video_format=info['video_format'],
                        audio_format=info['audio_format'],
                        resolution=info['resolution'],
                        scan_type=info['scan_type'],
                        fps=info['fps'],
                        current_bitrate=bitrate_str
                    ))

                    writer.writerow([
                        timestamp,
                        name,
                        info['source'],
                        info['video_format'],
                        info['audio_format'],
                        info['resolution'],
                        info['scan_type'],
                        info['fps'],
                        round(bitrate_mbps, 4),
                        pps_val,
                        status
                    ])

        time.sleep(0.05)

except Exception as e:
    logging.error(f"Monitoring loop error: {e}")
finally:
    sock.close()
    logging.info("Monitoring stopped.")
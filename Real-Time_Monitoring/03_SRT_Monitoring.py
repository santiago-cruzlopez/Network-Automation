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
import curses  # For stable terminal UI
from logging.handlers import RotatingFileHandler

# ====================== CONFIG ======================
MONITOR_INTERVAL = 1.0  # Seconds
CSV_FILE = '03_srt_monitoring.csv'
LOG_FILE = '03_srt_monitoring.log'
NTP_SERVER = 'pool.ntp.org'  # Changed to a public pool for reliability
NTP_OFFSET_THRESHOLD_SEC = 0.010  # 10ms
PTP_INTERFACE = 'enp2s0'  # Adjusted to user's wired interface
PTP_OFFSET_THRESHOLD_US = 1.0  # Microseconds
FFMPEG_RESTART_DELAY = 5.0
ALERT_THRESHOLD_BITRATE_LOW = 0.01  # Mbps
ALERT_THRESHOLD_PCR_DISCONT = 1  # Count of discontinuities triggering alert

SRT_FEED_NAME = "SRT Feed"
SRT_URI = "srt://207.35.238.30:39530?latency=3000&mode=caller&streamid=Nxt4464&pkt_size=1316&transtype=live"

# ====================== LOGGING ======================
logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        RotatingFileHandler(LOG_FILE, maxBytes=10*1024*1024, backupCount=5),
        logging.StreamHandler(sys.stdout)
    ]
)

# ====================== TOOL CHECKS ======================
def check_tool(cmd, version_arg="-version"):
    try:
        subprocess.check_output([cmd, version_arg], timeout=5)
        logging.info(f"✓ {cmd} detected.")
        return True
    except Exception as e:
        logging.error(f"{cmd} not found: {e}")
        return False

if not (check_tool("ffmpeg", "-version") and check_tool("sntp", "--version")):
    sys.exit(1)

# Check if FFmpeg supports SRT
try:
    protocols = subprocess.check_output(["ffmpeg", "-protocols"]).decode()
    if "srt" not in protocols:
        logging.error("FFmpeg does not support SRT protocol.")
        sys.exit(1)
    logging.info("✓ FFmpeg supports SRT.")
except Exception as e:
    logging.error(f"Failed to check FFmpeg protocols: {e}")

has_ptp = check_tool("ptp4l", "--version")

# ====================== STATUS ======================
lock = threading.Lock()
status = {'content': {}, 'transport': {}}
system_sync_status = "Checking..."
monitoring_processes = []

ansi_escape = re.compile(r'\x1B\[[0-?]*[ -/]*[@-~]')

# ====================== TIME SYNC ======================
def update_system_sync():
    global system_sync_status
    while True:
        if has_ptp:
            try:
                out = subprocess.check_output(['pmc', '-u', '-i', PTP_INTERFACE, 'GET TIME_STATUS_NP'], timeout=8).decode()
                m = re.search(r'master_offset\s+([-+]?[0-9]+)', out)  # Nanoseconds
                if m:
                    offset_us = abs(int(m.group(1)) / 1000.0)
                    system_sync_status = "PTP Good" if offset_us <= PTP_OFFSET_THRESHOLD_US else f"\033[93mPTP Offset {offset_us:.3f}us\033[0m"
                else:
                    system_sync_status = "\033[91mPTP Sync Failed\033[0m"
            except Exception as e:
                logging.warning(f"PTP check failed: {e}")
                system_sync_status = "\033[91mPTP Error\033[0m"
        else:
            try:
                out = subprocess.check_output(['sntp', '-K', '/dev/null', NTP_SERVER], timeout=8).decode().strip()
                m = re.search(r'([+-][0-9.]+)\s*\+\/-', out)
                if m:
                    offset_sec = abs(float(m.group(1)))
                    system_sync_status = "NTP Good" if offset_sec <= NTP_OFFSET_THRESHOLD_SEC else f"\033[93mNTP Offset {offset_sec:.3f}s\033[0m"
                else:
                    system_sync_status = "\033[91mNTP Sync Failed\033[0m"
            except Exception as e:
                logging.warning(f"NTP check failed: {e}")
                system_sync_status = "\033[91mNTP Error\033[0m"
        time.sleep(60)

# ====================== FFmpeg MONITOR (Content + Transport) ======================
def start_ffmpeg_monitor():
    def monitor_output(proc):
        total_frames = 0
        black_ongoing = False
        black_start_time = None
        no_frame_timer = time.time()
        last_v_pts = last_a_pts = None
        last_bitrate = 0.0
        continuity_errors = 0
        pcr_discont = 0
        psi_summary = "N/A"  # Updated periodically

        def update_psi_summary():
            nonlocal psi_summary
            while True:
                try:
                    # Periodic ffprobe for PSI/SI (every 30s)
                    probe_cmd = ["ffprobe", "-loglevel", "debug", "-err_detect", "ignore_err", "-f", "mpegts", "-analyzeduration", "30000000", "-probesize", "10000000", "-i", SRT_URI, "-show_programs", "-print_format", "compact"]
                    out = subprocess.check_output(probe_cmd, timeout=60, stderr=subprocess.STDOUT).decode()
                    # Extract basic PSI (e.g., programs, PIDs)
                    programs = re.findall(r'program:(\d+).*?pmt_id:(\d+)', out)
                    psi_summary = f"{len(programs)} programs" if programs else "No PSI"
                except Exception as e:
                    psi_summary = "PSI Error"
                    logging.error(f"ffprobe failed: {e}")
                time.sleep(30)

        threading.Thread(target=update_psi_summary, daemon=True).start()

        while True:
            if proc.poll() is not None:
                with lock:
                    status['content']['video_status'] = "\033[91mFFmpeg Error\033[0m"
                    status['transport']['status'] = "\033[91mFFmpeg Error\033[0m"
                logging.error("FFmpeg exited in monitor loop.")
                break

            rlist, _, _ = select.select([proc.stdout.fileno()], [], [], 0.1)
            if not rlist:
                if time.time() - no_frame_timer > 15.0:
                    with lock:
                        status['content'].update({
                            'video_status': "\033[91mNo Frames\033[0m",
                            'audio_status': "\033[91mNo Audio\033[0m",
                            'av_sync': "\033[91mN/A\033[0m"
                        })
                        status['transport']['status'] = "\033[91mNo Signal\033[0m"
                continue

            line = proc.stdout.readline().strip()
            if not line: continue
            logging.debug(line)  # Log all FFmpeg output for debugging

            # Content Parsing (as before)
            if "black_start" in line:
                black_ongoing = True
                black_start_time = time.time()
                with lock: status['content']['video_status'] = "\033[91mBlack Video\033[0m"
            elif "black_end" in line:
                black_ongoing = False
                with lock: status['content']['video_status'] = "Good Video"

            if "freeze_start" in line:
                with lock: status['content']['video_status'] = "\033[91mFrozen\033[0m"
            elif "freeze_end" in line:
                with lock: status['content']['video_status'] = "Good Video"

            if "silence_start" in line:
                with lock: status['content']['audio_status'] = "\033[91mSilent\033[0m"
            elif "silence_end" in line:
                with lock: status['content']['audio_status'] = "Good Audio"

            if "pts_time:" in line:
                m = re.search(r'pts_time:([0-9.]+)', line)
                if m:
                    pts = float(m.group(1))
                    no_frame_timer = time.time()
                    if "ashowinfo" not in line:  # Video
                        total_frames += 1
                        last_v_pts = pts
                        with lock:
                            status['content']['frame_count'] = total_frames
                    else:  # Audio
                        last_a_pts = pts
                    if last_v_pts is not None and last_a_pts is not None:
                        diff = abs(last_v_pts - last_a_pts)
                        with lock:
                            status['content']['av_sync'] = "In Sync" if diff <= 0.5 else "\033[91mOut Sync\033[0m"

            # Transport Parsing (from -debug ts)
            if "TS duplicate" in line or "TS discontinuity" in line:
                continuity_errors += 1
            if "PCR discontinuity" in line:
                pcr_discont += 1

            # Bitrate Parsing (from FFmpeg stats)
            if "bitrate=" in line:
                m = re.search(r'bitrate=([0-9.]+)kbits/s', line)
                if m:
                    last_bitrate = float(m.group(1)) / 1000.0  # Convert to Mbps

            with lock:
                status['transport'] = {
                    'bitrate_mbps': last_bitrate,
                    'pcr_discont': pcr_discont,
                    'continuity_errors': continuity_errors,
                    'psi_summary': psi_summary,
                    'status': "\033[91mAlert\033[0m" if (last_bitrate < ALERT_THRESHOLD_BITRATE_LOW or pcr_discont > ALERT_THRESHOLD_PCR_DISCONT or continuity_errors > 0) else "Good TS"
                }

    while True:
        logging.info("Starting FFmpeg process...")
        # Enhanced FFmpeg: Add -debug ts for transport debug, -stats for bitrate
        cmd = ["ffmpeg", "-hide_banner", "-loglevel", "debug", "-stats", "-err_detect", "ignore_err", "-f", "mpegts", "-analyzeduration", "30000000", "-probesize", "10000000", "-i", SRT_URI, "-vf", "freezedetect=d=5.0,blackdetect=d=10.0:pix_th=0.12:pic_th=0.97,showinfo", "-af", "silencedetect=n=-30dB:d=15.0,ashowinfo", "-f", "null", "-"]
        try:
            proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
            monitoring_processes.append(proc)
            logging.info("FFmpeg process started.")
        except Exception as e:
            logging.error(f"Failed to start FFmpeg: {e}")
            time.sleep(FFMPEG_RESTART_DELAY)
            continue

        with lock:
            status['content'] = {
                'video_status': 'Good Video', 'audio_status': 'Good Audio',
                'av_sync': 'In Sync', 'frame_count': 0
            }
            status['transport'] = {
                'bitrate_mbps': 0.0, 'pcr_discont': 0,
                'continuity_errors': 0, 'psi_summary': 'N/A',
                'status': 'Initializing'
            }

        threading.Thread(target=monitor_output, args=(proc,), daemon=True).start()
        proc.wait()
        remaining_output = proc.stdout.read()
        logging.error(f"FFmpeg exited with return code {proc.returncode}. Remaining output: {remaining_output}")
        time.sleep(FFMPEG_RESTART_DELAY)

# ====================== SIGNAL HANDLER ======================
def signal_handler(sig, frame):
    logging.info("Shutdown signal received.")
    for p in monitoring_processes:
        try: p.terminate(); p.wait(3)
        except: p.kill()
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)
signal.signal(signal.SIGTERM, signal_handler)

# ====================== START ======================
logging.info("Starting SRT monitoring...")
threading.Thread(target=start_ffmpeg_monitor, daemon=True).start()
logging.info(f"  ✓ {SRT_FEED_NAME}")

threading.Thread(target=update_system_sync, daemon=True).start()

# ====================== CURSES UI SETUP ======================
def draw_ui(stdscr):
    curses.curs_set(0)
    stdscr.nodelay(True)
    headers = ["Feed", "URI", "Video", "Frames", "Audio", "AV Sync", "TS Status", "Bitrate (Mbps)", "PCR Discont", "Cont. Errors", "PSI Summary", "Sync"]
    
    try:
        while True:
            stdscr.clear()
            h, w = stdscr.getmaxyx()
            col_width = w // len(headers)
            
            # Draw headers
            for i, hdr in enumerate(headers):
                stdscr.addstr(0, i * col_width, hdr[:col_width-1], curses.A_BOLD)
            
            timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            row = 1
            csv_rows = [['Timestamp'] + headers]

            with lock:
                content = status.get('content', {})
                transport = status.get('transport', {})
                
                data = [
                    SRT_FEED_NAME,
                    SRT_URI[:col_width-1],
                    ansi_escape.sub('', content.get('video_status', 'N/A')),
                    str(content.get('frame_count', 0)),
                    ansi_escape.sub('', content.get('audio_status', 'N/A')),
                    ansi_escape.sub('', content.get('av_sync', 'N/A')),
                    ansi_escape.sub('', transport.get('status', 'N/A')),
                    f"{transport.get('bitrate_mbps', 0.0):.2f}",
                    str(transport.get('pcr_discont', 0)),
                    str(transport.get('continuity_errors', 0)),
                    transport.get('psi_summary', 'N/A')[:col_width-1],
                    ansi_escape.sub('', system_sync_status)
                ]
                
                for i, d in enumerate(data):
                    color = curses.color_pair(1) if "Error" in str(d) or "Alert" in str(d) else 0
                    try:
                        curses.init_pair(1, curses.COLOR_RED, curses.COLOR_BLACK)
                    except:
                        pass
                    stdscr.addstr(row, i * col_width, str(d)[:col_width-1], color)
                
                csv_rows.append([timestamp] + data)
            
            stdscr.refresh()
            
            # Append to CSV
            with open(CSV_FILE, 'a', newline='') as f:
                writer = csv.writer(f)
                writer.writerows(csv_rows)
            
            time.sleep(MONITOR_INTERVAL)
    
    except KeyboardInterrupt:
        pass

# ====================== MAIN ======================
if __name__ == "__main__":
    try:
        curses.wrapper(draw_ui)
    except Exception as e:
        logging.error(f"Main error: {e}", exc_info=True)
    finally:
        for p in monitoring_processes:
            try: p.terminate(); p.wait(3)
            except: p.kill()
        logging.info("Monitoring stopped.")
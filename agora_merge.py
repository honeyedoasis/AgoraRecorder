import av
import pandas as pd
from fractions import Fraction
import re
import subprocess
import os

# Configuration
VIDEO_BIN = 'video.h264'
AUDIO_BIN = 'audio.pcm'
VIDEO_CSV = 'fullVideoInfo.csv'
AUDIO_CSV = 'fullAudioInfo.csv'
OUTPUT_FILE_NAME = 'final'
OUTPUT_FILE = f'{OUTPUT_FILE_NAME}.mp4'
TEMP_OUTPUT_FILE = f'{OUTPUT_FILE_NAME}_temp.mkv'

def get_video_frames_with_types(file_path):
    print("Extracting video frames and identifying types...")
    with open(file_path, 'rb') as f:
        data = f.read()
    starts = [m.start() for m in re.finditer(b'\x00\x00\x01', data)]
    nals = []
    for i in range(len(starts)):
        s = starts[i]
        if s > 0 and data[s-1] == 0: s -= 1
        e = starts[i+1] if i+1 < len(starts) else len(data)
        nals.append(data[s:e])

    frames = []
    current_unit = b''
    is_keyframe = False
    
    for nal in nals:
        header_idx = 4 if nal.startswith(b'\x00\x00\x00\x01') else 3
        nal_type = nal[header_idx] & 0x1F
        current_unit += nal
        
        if nal_type == 5: is_keyframe = True # IDR (Keyframe)
        
        if 1 <= nal_type <= 5: # VCL (Picture Data)
            frames.append({'payload': current_unit, 'key': is_keyframe})
            current_unit = b''
            is_keyframe = False
    return frames

def process_anchored():
    v_headers = ["monoMs", "localMs", "captureTimeMs", "presentationMs", "decodeTimeMs", "codec", "width", "height", "fps", "rotation", "trackId", "frameType", "streamType", "length"]
    a_headers = ["monoMs", "localMs", "renderTimeMs", "presentationMs", "samplesPerSec", "channels", "samplesPerChannel", "bytesPerSample", "rtpTimestamp"]
    
    v_df = pd.read_csv(VIDEO_CSV, names=v_headers).sort_values('monoMs')
    a_df = pd.read_csv(AUDIO_CSV, names=a_headers).sort_values('monoMs')

    sample_rate = int(a_df.iloc[0]['samplesPerSec'])
    channels = int(a_df.iloc[0]['channels'])
    bytes_per_sample = int(a_df.iloc[0]['bytesPerSample'])

    if bytes_per_sample == 2:
        pcm_codec = 'pcm_s16le' # 16-bit
    elif bytes_per_sample == 4:
        pcm_codec = 'pcm_s32le' # 32-bit
    else:
        raise ValueError(f"Unsupported bit depth: {bytes_per_sample} bytes")

    video_frames = get_video_frames_with_types(VIDEO_BIN)
    
    # 1. FIND THE ANCHOR
    # The anchor is the monoMs of the first video frame that is actually a Keyframe.
    anchor_ms = None
    first_v_index = 0
    
    for i in range(min(len(v_df), len(video_frames))):
        if video_frames[i]['key']: # Found our first usable frame
            anchor_ms = v_df.iloc[i]['monoMs']
            first_v_index = i
            print(f"Sync Anchor set at {anchor_ms}ms (Video Frame {i})")
            break
            
    if anchor_ms is None:
        print("Error: No keyframes found in video!")
        return

    # 2. CALCULATE GLOBAL BASE
    # We find the earliest time across BOTH files to ensure the file starts at 0.
    global_base = min(v_df.iloc[first_v_index]['monoMs'], a_df['monoMs'].min())

    output = av.open(TEMP_OUTPUT_FILE, 'w')
    
    v_stream = output.add_stream('h264', rate=30)
    v_stream.width = int(v_df.iloc[0]['width'])
    v_stream.height = int(v_df.iloc[0]['height'])
    v_stream.time_base = Fraction(1, 1000)
    v_stream.codec_context.options = {'flags': '+global_header'}

    a_stream = output.add_stream(pcm_codec, rate=sample_rate)
    a_stream.layout = 'stereo' if channels == 2 else 'mono'
    a_stream.time_base = Fraction(1, 1000)

    # 3. COLLECT ALL VALID PACKETS
    packets = []

    # Add Video (starting from first keyframe)
    for i in range(first_v_index, min(len(v_df), len(video_frames))):
        pts = int(v_df.iloc[i]['monoMs'] - global_base)
        packets.append({'pts': pts, 'payload': video_frames[i]['payload'], 'type': 'v'})

    # Add Audio
    with open(AUDIO_BIN, 'rb') as f_aud:
        for _, row in a_df.iterrows():
            chunk_size = int(row['samplesPerChannel'] * channels * int(row['bytesPerSample']))
            payload = f_aud.read(chunk_size)
            if not payload: break
            
            pts = int(row['monoMs'] - global_base)
            # Only add audio that occurs after the global base
            if pts >= 0:
                packets.append({'pts': pts, 'payload': payload, 'type': 'a'})

    # 4. INTERLEAVE AND MUX
    packets.sort(key=lambda x: (x['pts'], x['type'] != 'v'))

    last_pts = {'v': -1, 'a': -1}
    print(f"Muxing {len(packets)} packets...")
    
    for p in packets:
        t = p['type']
        pts = p['pts']
        
        # Ensure strict monotonicity for the muxer
        if pts <= last_pts[t]:
            pts = last_pts[t] + 1
            
        packet = av.Packet(p['payload'])
        packet.pts = pts
        packet.dts = pts
        packet.stream = v_stream if t == 'v' else a_stream
        output.mux(packet)
        last_pts[t] = pts

    output.close()

    convert_to_mp4(TEMP_OUTPUT_FILE, OUTPUT_FILE)
    print(f"Done! Final sync applied. {OUTPUT_FILE}")

    os.remove(TEMP_OUTPUT_FILE)
    print(f"Removed temp file. {TEMP_OUTPUT_FILE}")


def convert_to_mp4(input_mkv, output_mp4):
    print(f"Finalizing: Converting {input_mkv} to {output_mp4}...")

    a_headers = ["monoMs", "localMs", "renderTimeMs", "presentationMs", "samplesPerSec", "channels", "samplesPerChannel", "bytesPerSample", "rtpTimestamp"]
    a_df = pd.read_csv(AUDIO_CSV, names=a_headers)
    
    # Grab the first row values to define the stream
    detected_sample_rate = int(a_df.iloc[0]['samplesPerSec'])
    detected_channels = int(a_df.iloc[0]['channels'])

    cmd = [
        'ffmpeg', '-y',
        '-i', input_mkv,
        '-c:v', 'copy',      # Keep the video exactly as is
        '-af', 'aresample=async=1:min_hard_comp=0.1:first_pts=0',
        '-c:a', 'aac',       # Convert PCM to AAC
        '-b:a', '192k',      # Good quality bitrate
        '-ac', str(detected_channels),          # Force stereo for better Resolve compatibility
        '-ar', str(detected_sample_rate),      # Force 48kHz (Resolve's favorite sample rate)
        '-movflags', '+faststart', # Optimizes for web streaming
        output_mp4
    ]

    subprocess.run(cmd)
    print("Optimization Complete!")

if __name__ == "__main__":
    # convert_to_mp4(TEMP_OUTPUT_FILE, 'test2.mp4')
    process_anchored()
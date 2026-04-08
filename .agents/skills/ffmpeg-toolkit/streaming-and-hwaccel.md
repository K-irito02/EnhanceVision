# Streaming and Hardware Acceleration

## HLS (HTTP Live Streaming)

```bash
# Basic HLS
ffmpeg -i input.mp4 -c:v libx264 -c:a aac \
       -hls_time 10 -hls_playlist_type vod \
       -hls_segment_filename "segment_%03d.ts" \
       playlist.m3u8

# Multi-bitrate adaptive HLS
ffmpeg -i input.mp4 \
       -filter_complex "[0:v]split=3[v1][v2][v3]; \
       [v1]scale=1920:1080[v1out]; \
       [v2]scale=1280:720[v2out]; \
       [v3]scale=854:480[v3out]" \
       -map "[v1out]" -map 0:a -c:v libx264 -b:v 5M -c:a aac -b:a 192k \
       -hls_time 10 -hls_playlist_type vod 1080p.m3u8 \
       -map "[v2out]" -map 0:a -c:v libx264 -b:v 2M -c:a aac -b:a 128k \
       -hls_time 10 -hls_playlist_type vod 720p.m3u8 \
       -map "[v3out]" -map 0:a -c:v libx264 -b:v 1M -c:a aac -b:a 96k \
       -hls_time 10 -hls_playlist_type vod 480p.m3u8
```

## DASH (Dynamic Adaptive Streaming)

```bash
ffmpeg -i input.mp4 -c:v libx264 -c:a aac \
       -f dash -seg_duration 10 \
       -use_template 1 -use_timeline 1 \
       manifest.mpd
```

## Hardware Acceleration

### NVIDIA (NVENC/NVDEC)

```bash
# NVENC encoding only
ffmpeg -i input.mp4 -c:v h264_nvenc -preset fast output.mp4

# NVDEC decoding + NVENC encoding (full GPU pipeline)
ffmpeg -hwaccel cuda -i input.mp4 -c:v h264_nvenc output.mp4
```

### macOS VideoToolbox

```bash
ffmpeg -i input.mp4 -c:v h264_videotoolbox -b:v 5M output.mp4
```

### Intel QuickSync

```bash
ffmpeg -i input.mp4 -c:v h264_qsv output.mp4
```

### Detect Available Hardware

```bash
# List available encoders (look for _nvenc, _videotoolbox, _qsv suffixes)
ffmpeg -encoders 2>/dev/null | grep -E "nvenc|videotoolbox|qsv"

# List available hardware acceleration methods
ffmpeg -hwaccels
```

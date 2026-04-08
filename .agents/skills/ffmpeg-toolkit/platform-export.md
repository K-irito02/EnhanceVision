# Platform Export Optimization

Optimize video output for specific distribution platforms. These recipes work with any source video, not just Remotion renders.

## Workflow

```
Master render              FFmpeg optimization      Platform upload
     |                            |                       |
  video.mp4  ---------->  video-youtube.mp4  -------->  YouTube
             ---------->  video-twitter.mp4  -------->  Twitter/X
             ---------->  video-linkedin.mp4 -------->  LinkedIn
             ---------->  video-web.mp4      -------->  Website embed
```

## Platform Requirements

| Platform | Max Resolution | Max Size | Max Duration | Audio |
|----------|---------------|----------|--------------|-------|
| YouTube | 8K | 256GB | 12 hours | AAC 48kHz |
| Twitter/X | 1920x1200 | 512MB | 140s | AAC 44.1kHz |
| LinkedIn | 4096x2304 | 5GB | 10 min | AAC 48kHz |
| Instagram Feed | 1080x1350 | 4GB | 60s | AAC 48kHz |
| Instagram Reels | 1080x1920 | 4GB | 90s | AAC 48kHz |
| TikTok | 1080x1920 | 287MB | 10 min | AAC |

## YouTube

```bash
# YouTube optimized (1080p, high quality - YouTube re-encodes everything)
ffmpeg -i input.mp4 \
  -c:v libx264 -preset slow -crf 18 \
  -profile:v high -level 4.0 \
  -bf 2 -g 30 \
  -c:a aac -b:a 192k -ar 48000 \
  -movflags +faststart \
  video-youtube.mp4

# YouTube Shorts (vertical 1080x1920)
ffmpeg -i input.mp4 \
  -vf "scale=1080:1920:force_original_aspect_ratio=decrease,pad=1080:1920:(ow-iw)/2:(oh-ih)/2" \
  -c:v libx264 -crf 18 -c:a aac -b:a 192k \
  video-shorts.mp4
```

## Twitter/X

Twitter has strict limits: max 140s, 512MB, 1920x1200.

```bash
# Twitter optimized (under 15MB target for fast upload)
ffmpeg -i input.mp4 \
  -c:v libx264 -preset medium -crf 24 \
  -profile:v main -level 3.1 \
  -vf "scale='min(1280,iw)':'min(720,ih)':force_original_aspect_ratio=decrease" \
  -c:a aac -b:a 128k -ar 44100 \
  -movflags +faststart \
  -fs 15M \
  video-twitter.mp4

# Check file size and duration
ffprobe -v error -show_entries format=duration,size -of csv=p=0 video-twitter.mp4
```

## LinkedIn

LinkedIn prefers MP4 with AAC audio, max 10 minutes.

```bash
ffmpeg -i input.mp4 \
  -c:v libx264 -preset medium -crf 22 \
  -profile:v main \
  -vf "scale='min(1920,iw)':'min(1080,ih)':force_original_aspect_ratio=decrease" \
  -c:a aac -b:a 192k -ar 48000 \
  -movflags +faststart \
  video-linkedin.mp4
```

## Website / Embed

```bash
# Web-optimized MP4 (small file, progressive loading)
ffmpeg -i input.mp4 \
  -c:v libx264 -preset medium -crf 26 \
  -profile:v baseline -level 3.0 \
  -vf "scale=1280:720" \
  -c:a aac -b:a 128k \
  -movflags +faststart \
  video-web.mp4

# WebM alternative (better compression)
ffmpeg -i input.mp4 \
  -c:v libvpx-vp9 -crf 30 -b:v 0 \
  -vf "scale=1280:720" \
  -c:a libopus -b:a 128k \
  -deadline good \
  video-web.webm
```

## GIF Previews

```bash
# High-quality GIF (first 5 seconds, palette-optimized)
ffmpeg -i input.mp4 -t 5 \
  -vf "fps=15,scale=480:-1:flags=lanczos,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse" \
  preview.gif

# Smaller GIF (first 3 seconds)
ffmpeg -i input.mp4 -t 3 \
  -vf "fps=10,scale=320:-1:flags=lanczos,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse" \
  preview-small.gif
```

## Batch Export Script

```bash
#!/bin/bash
INPUT="${1:-input.mp4}"

# YouTube (high quality)
ffmpeg -i "$INPUT" -c:v libx264 -preset slow -crf 18 \
  -c:a aac -b:a 192k -movflags +faststart \
  "${INPUT%.mp4}-youtube.mp4"

# Twitter (compressed)
ffmpeg -i "$INPUT" -c:v libx264 -crf 24 \
  -vf "scale='min(1280,iw)':'-2'" \
  -c:a aac -b:a 128k -movflags +faststart \
  "${INPUT%.mp4}-twitter.mp4"

# LinkedIn
ffmpeg -i "$INPUT" -c:v libx264 -crf 22 \
  -c:a aac -b:a 192k -movflags +faststart \
  "${INPUT%.mp4}-linkedin.mp4"

# Web embed (small)
ffmpeg -i "$INPUT" -c:v libx264 -crf 26 \
  -vf "scale=1280:720" \
  -c:a aac -b:a 128k -movflags +faststart \
  "${INPUT%.mp4}-web.mp4"

echo "Exported:"
ls -lh "${INPUT%.mp4}"-*.mp4
```

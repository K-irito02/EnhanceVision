# Remotion Integration

FFmpeg patterns for preparing and processing video assets used in Remotion projects.

## Asset Preparation

### Standard Remotion-Compatible Flags

```bash
# Ensure web-playable MP4 with even dimensions
ffmpeg -i input.gif -movflags faststart -pix_fmt yuv420p \
  -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" output.mp4
```

**Why these flags:**
- `-movflags faststart` - Moves metadata to start for web streaming
- `-pix_fmt yuv420p` - Ensures compatibility with most players
- `scale=trunc(...)` - Forces even dimensions (required by most codecs)

### Prepare Demo Recording

```bash
# Standard 1080p, 30fps, Remotion-ready
ffmpeg -i raw-recording.mp4 \
  -vf "scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2,fps=30" \
  -c:v libx264 -crf 18 -preset slow \
  -c:a aac -b:a 192k \
  -movflags faststart \
  public/demos/demo.mp4
```

### Screen/Mobile Recording Conversion

```bash
# iPhone/iPad recording (usually 60fps, variable resolution)
ffmpeg -i iphone-recording.mov \
  -vf "scale=1920:-2,fps=30" \
  -c:v libx264 -crf 20 \
  -an \
  public/demos/mobile-demo.mp4
```

### GIF to MP4 with Remotion Flags

```bash
ffmpeg -i input.gif -movflags faststart -pix_fmt yuv420p \
  -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" output.mp4
```

### Batch GIF Conversion

```bash
for f in assets/*.gif; do
  ffmpeg -i "$f" -movflags faststart -pix_fmt yuv420p \
    -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" \
    "public/demos/$(basename "$f" .gif).mp4"
done
```

## Speed: FFmpeg vs Remotion playbackRate

| Scenario | Use FFmpeg | Use Remotion |
|----------|------------|--------------|
| Constant speed (1.5x, 2x) | Either works | Simpler |
| Extreme speeds (>4x or <0.25x) | More reliable | May have issues |
| Variable speed (accelerate over time) | Pre-process | Complex workaround needed |
| Need perfect audio sync | Guaranteed | Usually fine |
| Demo needs to fit voiceover timing | Pre-calculate | Runtime adjustment |

**Remotion limitation:** `playbackRate` must be constant. Dynamic interpolation like `playbackRate={interpolate(frame, [0, 100], [1, 5])}` won't work correctly because Remotion evaluates frames independently.

```bash
# Speed up demo to fit a scene (e.g., 60s demo into 20s = 3x speed)
ffmpeg -i demo-raw.mp4 \
  -filter_complex "[0:v]setpts=0.333*PTS[v];[0:a]atempo=3.0[a]" \
  -map "[v]" -map "[a]" \
  public/demos/demo-fast.mp4

# Timelapse effect (10x speed, drop audio)
ffmpeg -i long-demo.mp4 -filter:v "setpts=0.1*PTS" -an public/demos/timelapse.mp4
```

## OffthreadVideo Integration Notes

- Remotion uses `<OffthreadVideo>` which handles most formats
- Prefer H.264 (libx264) in MP4 container
- Always use `-movflags faststart` for web playback
- Match fps to composition (usually 30fps)
- Resolution should match composition (1920x1080 typical)

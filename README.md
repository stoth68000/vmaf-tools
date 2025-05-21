# vmaf-tools
Tools related to VMAF video analysis.
BSD licensed.

* pic2x2 - Read four PNGS and create a single grid combined PNG output.
* picvmaf - Read vmaf stats files and produce a PNG output, showing core and a red highlight line
* picdiff - Read two PNGS, compute a grey normalized diff map, output the diffmap to PNG.
* yuvmse - Read a pair of YUV files and compute the luma MSE per frame, also sharpness, DCT hashes and PSNR details.

## Assumptions
* You already have two YUV 420p files, which are frame aligned (by hand). IE, the first frame of each YUV file is from the same point in time, but from a different workflow. They are YUV420 8bit and 1920x1080
* reference.yuv
* distorted.yuv
* You've already run VMAF across the reference and distored YUV files, the vlaf output is called vmaf.json.

Example:
```
$ docker run --rm -v $PWD/yuv:/files vmaf yuv420p 1920 1080 \
  /files/reference.yuv /files/distorted.yuv \
  --out-fmt json >vmaf.json
```

## 0. Make the dev container, run it and compile the tools
```
$ make build-devenv
$ make devenv
# cd /src
# make
```

## 1. Create the reference and distorted YUV PNGs
```
$ cd /files
$ ffmpeg -y -f rawvideo -s 1920x1080 -pixel_format yuv420p -i /files/reference.yuv -f image2 -start_number 0 REF%06d.png
$ ffmpeg -y -f rawvideo -s 1920x1080 -pixel_format yuv420p -i /files/distorted.yuv -f image2 -start_number 0 DIST%06d.png
```

## 2. Convert the VMAF json to a specific csv format needed by picvmaf, for example 0-1499 frames
```
AGGREGATE=`cat vmaf.json | jq -r '.aggregate.VMAF_score'`
$ cat vmaf.json | jq -r '.frames[] | "\(.frameNum),\(.VMAF_score)"' | sed "s!\$!,$AGGREGATE!g" >vmaf.csv
```

## 3. Create the VMAF PNGs from an existing json file, for frames 0 through 1499
```
$ picvmaf -i vmaf.csv -o VMAF000000.png -c 0
$ picvmaf -i vmaf.csv -o VMAF000001.png -c 1
$ picvmaf -i vmaf.csv -o VMAF000002.png -c 2 .... etc
```

## 4. For each REF and DIST PNG frame pair, create a difference PNG
```
$ picdiff -n -t0 -1 REF000000.png -2 DIST000000.png -o DIFF000000.png
$ picdiff -n -t0 -1 REF000001.png -2 DIST000001.png -o DIFF000001.png .... etc
```

## 5. Combined the REF/DIST/DIFF/VMAF pngs intoa  single 2x2 grid.
```
$ pic2x2 -t0 -1 REF000000.png -2 DIST000000.png -3 VMAF000000.png -4 DIFF000000.png -o COMPOSITE000000.png
$ pic2x2 -t0 -1 REF000001.png -2 DIST000001.png -3 VMAF000001.png -4 DIFF000001.png -o COMPOSITE000001.png .... etc
```

## 6. Bring all of the composite 2x2 pngs together into a final viewable video.
```
$ ffmpeg -y -r 29.97 -pattern_type glob -i 'COMPOSITE*.png' \
	-c:v libx264 -threads 8 -preset veryfast -b:v 40M \
	-pix_fmt yuv420p side-by-side-comparison.mp4
```

## Problem - What happens if you can't figure out how to align the YUV files?

Use yuvmse tool to hash each YUV file, determine where the frames sequences match and provide instructions for trimming the input YUV files to being them into alignment.

Example:
```
root@docker-desktop:/src# ./yuvmse -1 /files/AA60-ac-aligned.yuv -2 /files/bb-ab-nonaligned.yuv -s 5 -D
# Detected possible   720x480p, with exactly   8400 frames in /files/AA60-ac-aligned.yuv
# Detected possible   720x576p, with exactly   7000 frames in /files/AA60-ac-aligned.yuv
# Detected possible  1280x720p, with exactly   3150 frames in /files/AA60-ac-aligned.yuv
# Detected possible 1920x1080p, with exactly   1400 frames in /files/AA60-ac-aligned.yuv
# Detected possible 3840x2160p, with exactly    350 frames in /files/AA60-ac-aligned.yuv
# Operator needs to provide width (-W) and height (-H) args
# Detected possible   720x480p, with exactly   8400 frames in /files/bb-ab-nonaligned.yuv
# Detected possible   720x576p, with exactly   7000 frames in /files/bb-ab-nonaligned.yuv
# Detected possible  1280x720p, with exactly   3150 frames in /files/bb-ab-nonaligned.yuv
# Detected possible 1920x1080p, with exactly   1400 frames in /files/bb-ab-nonaligned.yuv
# Detected possible 3840x2160p, with exactly    350 frames in /files/bb-ab-nonaligned.yuv
# Operator needs to provide width (-W) and height (-H) args
# dimensions: 1920 x 1080 (defaults)
# file0: /files/AA60-ac-aligned.yu
# file1: /files/bb-ab-nonaligned.yuv
# windowsize: 30
# skipframes: 5
# bestmatch: 0
# verbose: 0
# dcthashmatch: 1
# hash sequence matches: 31
# Frame sequence, file 1 begins frame 00000005, file 2 begins frame 00000005
# Trimming instructions:
#   dd if=/files/AA60-ac-aligned.yuv of=/files/AA60-ac-aligned.yuv.trimmed bs=3110400 skip=5
# Trimming instructions:
#   dd if=/files/bb-ab-nonaligned.yuv of=/files/bb-ab-nonaligned.yuv.trimmed bs=3110400 skip=5
```

## Alignment - OK, now the files are aligned, what else can the tools do?

Use yuvmse tool to compute MSE, PSNR, image sharpness and a DCT hash for each file.
This is useful because a basic PSNR assessment helps you understand the level of distortion
between a reference.yuv file and distorted.yuv file.

Example:
```
root@docker-desktop:/src# ./yuvmse -1 /files/AA60-ac-aligned.yuv -2 /files/bb-ab-aligned.yuv        
# Detected possible   720x480p, with exactly   8400 frames in /files/AA60-ac-aligned.yuv
# Detected possible   720x576p, with exactly   7000 frames in /files/AA60-ac-aligned.yuv
# Detected possible  1280x720p, with exactly   3150 frames in /files/AA60-ac-aligned.yuv
# Detected possible 1920x1080p, with exactly   1400 frames in /files/AA60-ac-aligned.yuv
# Detected possible 3840x2160p, with exactly    350 frames in /files/AA60-ac-aligned.yuv
# Operator needs to provide width (-W) and height (-H) args
# Detected possible   720x480p, with exactly   8400 frames in /files/bb-ab-aligned.yuv
# Detected possible   720x576p, with exactly   7000 frames in /files/bb-ab-aligned.yuv
# Detected possible  1280x720p, with exactly   3150 frames in /files/bb-ab-aligned.yuv
# Detected possible 1920x1080p, with exactly   1400 frames in /files/bb-ab-aligned.yuv
# Detected possible 3840x2160p, with exactly    350 frames in /files/bb-ab-aligned.yuv
# Operator needs to provide width (-W) and height (-H) args
# dimensions: 1920 x 1080 (defaults)
# file0: /files/AA60-ac-aligned.yuv
# file1: /files/bb-ab-aligned.yuv
# windowsize: 30
# skipframes: 0
# bestmatch: 0
# verbose: 0
# dcthashmatch: 0
#  Frame       MSE                          PSNR                         Sharp                    DCT Hash                    Hamming                  Hash
#     Nr         Y         U         V         Y         U         V        f1        f2                f1                f2     Dist            Assessment
#------> <---------------------------> <---------------------------> <-----------------> <---------------------------------------------------------------->
00000000,    12.94,     2.20,     3.96,    37.01,    44.70,    42.16,   189.04,   207.89, df5fad7ce2618100, df5fad7ce2618100,       0,          Exact Match
00000001,   105.70,     3.81,    10.22,    27.89,    42.32,    38.03,   180.75,   208.92, df7fad7ce0618100, df7fad7ce0618100,       0,          Exact Match
00000002,    16.29,     2.21,     4.13,    36.01,    44.69,    41.97,   195.97,   231.50, de7fac7ce061a300, de7fac7ce061a300,       0,          Exact Match
00000003,    11.85,     2.12,     3.53,    37.40,    44.87,    42.65,   209.74,   228.62, de5eac7de061b300, de5eac7de061b300,       0,          Exact Match
00000004,    89.72,     3.11,     7.82,    28.60,    43.20,    39.20,   197.70,   220.25, de5eac7ce061f300, de5eac7ce261b300,       2,       Near Identical
00000005,    15.94,     2.11,     4.14,    36.10,    44.89,    41.96,   202.66,   233.69, de5ea47ce161f300, de5ea47ce261f300,       2,       Near Identical
00000006,    11.82,     2.20,     4.11,    37.41,    44.70,    41.99,   207.42,   223.49, de5ea47ce360f202, de5ea47ce360f202,       0,          Exact Match
```

Typical PSNR values for 8-bit images:
* greater than 40 dB: Very high quality (imperceptible differences)
* 30–40 dB: Good quality
* 20–30 dB: Noticeable degradation
* less than 20 dB: Poor quality

| Metric | Measures         | Scale            | Interpretation |
| ------ | ---------------- | ---------------- | -------------- |
| VMAF   | Human Perception | Linear           | High = better  |
| MSE    | Absolute diff    | Linear           | Low  = better  |
| PSNR   | Relative diff    | Logarithmic (dB) | High = better  |
| Sharp  | Absolute         | Linear           | High = better  |
| DCT    | Absolute         | Linear           | Low = better   |

| VMAF Score | Quality Perception                                     |
| ---------- | ------------------------------------------------------ |
| **90–100** | Excellent (virtually indistinguishable from reference) |
| **80–90**  | Very good (minor differences, often imperceptible)     |
| **70–80**  | Good (visible differences, but acceptable)             |
| **50–70**  | Fair (noticeable degradation)                          |
| **<50**    | Poor (clearly degraded, possibly distracting)          |

| Sharpness    | Quality Perception              |
| ------------ | ------------------------------- |
| higher value | Better                          |
| lower value  | Softer, blurred or out of focus |

| PSNR (dB)    | Quality Perception (8 bit image)              |
| ------------ | --------------------------------------------- |
| 40 or higher | Very high quality (imperceptible differences) |
| 30–40        | Good quality                                  |
| 20–30        | Noticeable degradation                        |
| 20 or less   | Poor quality                                  |

| MSE          | Quality Perception (8 bit image)     |
| ------------ | ------------------------------------ |
| higher value | More distortion the higher the value |
| lower value  | Better quality, typically < 100      |


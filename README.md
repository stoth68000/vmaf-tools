# vmaf-tools
Tools related to VMAF video analysis.
BSD licensed.

* pic2x2 - Read four PNGS and create a single grid combined PNG output.
* picvmaf- Read vmaf stats files and produce a PNG output, showing core and a red highlight line
* picdiff- Read two PNGS, compute a grey normalized diff map, output the diffmap to PNG.

## Assumptions
* You already have two YUV 420p files, which are frame aligned (by hand). IE, the first frame of each YUV file is from the same point in time, but from a different workflow.
* reference.yuv
* distorted.yuv
  
## 1. Create the reference and distorted YUV PNGs
```
$ ffmpeg -y -f rawvideo -s 1920x1080 -pixel_format yuv420p -i ../../reference.yuv -f image2 -start_number 0 REF%06d.png
$ ffmpeg -y -f rawvideo -s 1920x1080 -pixel_format yuv420p -i ../../distorted.yuv -f image2 -start_number 0 DIST%06d.png
```

## 2. Convert the VMAF json to a specific csv format needed by picvmaf, for example 0-1499 frames
```
AGGREGATE=`cat vmaf.json | jq -r '.aggregate.VMAF_score'`
cat vmaf.json | jq -r '.frames[] | "\(.frameNum),\(.VMAF_score)"' | sed "s!\$!,$AGGREGATE!g" >vmaf.csv
```

## 3. Create the VMAF PNGs from an existing json file, for frames 0 through 1499
```
picvmaf -i vmaf.csv -o VMAF000000.png -c 0
picvmaf -i vmaf.csv -o VMAF000001.png -c 1
picvmaf -i vmaf.csv -o VMAF000002.png -c 2 .... etc
```

## 4. For each REF and DIST PNG frame pair, create a difference PNG
```
picdiff -n -t0 -1 REF000000.png -2 DIST000000.png -o DIFF000000.png
picdiff -n -t0 -1 REF000001.png -2 DIST000001.png -o DIFF000001.png .... etc
```

## 5. Combined the REF/DIST/DIFF/VMAD pngs intoa  single 2x2 grid.
```
pic2x2 -t0 -1 REF000000.png -2 DIST000000.png -3 VMAF000000.png -4 DIFF000000.png -o COMPOSITE000000.png
pic2x2 -t0 -1 REF000001.png -2 DIST000001.png -3 VMAF000001.png -4 DIFF000001.png -o COMPOSITE000001.png .... etc
```

## 6. Bring all of the composite 2x2 pngs together into a final viewable video.
```
ffmpeg -y -r 29.97 -pattern_type glob -i 'COMPOSITE*.png' \
	-c:v libx264 -threads 8 -preset veryfast -b:v 40M \
	-pix_fmt yuv420p side-by-side-comparison.mp4
```

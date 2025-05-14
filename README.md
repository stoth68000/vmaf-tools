# vmaf-tools
Tools related to VMAF video analysis

* pic2x2 - Read four PNGS and create a single grid combined PNG output.
* picvmaf- Read vmaf stats files and produce a PNG output, showing core and a red highlight line
* picdiff- Read two PNGS, compute a grey normalized diff map, output the diffmap to PNG.

# 1. Create the reference and distorted YUV PNGs
* ffmpeg -y -f rawvideo -s 1920x1080 -pixel_format yuv420p -i ../../reference.yuv -f image2 -start_number 0 REF%06d.png
* ffmpeg -y -f rawvideo -s 1920x1080 -pixel_format yuv420p -i ../../reference.yuv -f image2 -start_number 0 DIST%06d.png

# 2. Convert the VMAF json to a specific csv format needed by picvmaf, for example 0-1499 frames
* AGGREGATE=`cat vmaf.json | jq -r '.aggregate.VMAF_score'`
* cat vmaf.json | jq -r '.frames[] | "\(.frameNum),\(.VMAF_score)"' | sed "s!\$!,$AGGREGATE!g" >vmaf.csv

# 3. Create the VMAF PNGs from an existing json file, for frames 0 through 1499
* picvmaf -i vmaf.csv -o VMAF000000.png -c 0
* picvmaf -i vmaf.csv -o VMAF000001.png -c 1
* picvmaf -i vmaf.csv -o VMAF000002.png -c 2 .... etc


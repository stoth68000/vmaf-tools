/* Copyright Kernel Labs Inc 2025, All Rights Reserved. */

#include <stdio.h>
#include <getopt.h>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>

using namespace cv;

#define RENDER_TITLE_DEFAULT 1

struct tool_context_s {
#define MAX_INPUTS 2
	char *fn[MAX_INPUTS];
	Mat mat[MAX_INPUTS];
	int max_cols;
	int max_rows;
	int normalize;

	char *outfn;
	int verbose;
	int render_title;
};

int matLoad(struct tool_context_s *ctx, int nr)
{
	ctx->mat[nr] = imread(ctx->fn[nr], IMREAD_COLOR);
	if (ctx->mat[nr].rows == 0) {
		printf("Error reading file %s, aborting.\n", ctx->fn[nr]);
		return -1;
	}

	/* Don't render title into a source difference image */

	if (ctx->verbose) {
		printf("%s resolution is %dx%d\n", ctx->fn[nr], ctx->mat[nr].cols, ctx->mat[nr].rows);
	}

	if (ctx->mat[nr].rows > ctx->max_rows) {
		ctx->max_rows = ctx->mat[nr].rows;
	}
	if (ctx->mat[nr].cols > ctx->max_cols) {
		ctx->max_cols = ctx->mat[nr].cols;
	}

	return 0; /* Success */
}

void usage()
{
        printf("A tool to create compare absolute differences between two images, creating an output difference image.\n");
        printf("Usage:\n");
        printf("  -1 image1.png\n");
        printf("  -2 image2.png\n");
        printf("  -n normalize output diff to gray (default black)\n");
        printf("  -v raise verbosity\n");
	printf("  -t render filenames into images [def: %d]\n", RENDER_TITLE_DEFAULT);
        printf("  -o output.png\n");
}

int main(int argc, char *argv[])
{
	struct tool_context_s tool_ctx, *ctx = &tool_ctx;
	memset(ctx, 0, sizeof(*ctx));
	ctx->render_title = RENDER_TITLE_DEFAULT;

	int ch, idx;

	while ((ch = getopt(argc, argv, "?h1:2:3:4:no:t:v")) != -1) {
		switch (ch) {
		case '1':
		case '2':
			idx = ch - '0' - 1;
			ctx->fn[idx] = strdup(optarg);
			if (matLoad(ctx, idx) < 0) {
				fprintf(stderr, "Failed to load image\n");
				return -1;
			}
			break;
		case 'n':
			ctx->normalize = 1;
			break;
		case 'o':
			ctx->outfn = strdup(optarg);
			break;
		case 't':
			ctx->render_title = atoi(optarg);
			break;
		case 'v':
			ctx->verbose++;
			break;
		default:
		case '?':
		case 'h':
			exit(1);
		}
	}

	if (argc < 2) {
		usage();
		exit(1);
	}

	/* Create a utput mat, we'll composite into this */
	Mat mOutput = Mat(ctx->max_rows, ctx->max_cols, CV_8UC3);

	if (ctx->verbose) {
		printf("Output resolution is %dx%d\n", mOutput.cols, mOutput.rows);
	}

	absdiff(ctx->mat[0], ctx->mat[1], mOutput);

	Mat diff_normalized = Mat(ctx->max_rows, ctx->max_cols, CV_8UC3);
	normalize(mOutput, diff_normalized, 150, 255, cv::NORM_MINMAX);

	if (ctx->render_title) {
		if (ctx->normalize) {
			putText(diff_normalized, ctx->outfn, Point(10, 40), FONT_HERSHEY_DUPLEX, 1.0, CV_RGB(255, 255, 255), 2);
		} else {
			putText(mOutput, ctx->outfn, Point(10, 40), FONT_HERSHEY_DUPLEX, 1.0, CV_RGB(255, 255, 255), 2);
		}
	}

	/* Save */
	if (ctx->normalize) {
		cv::imwrite(ctx->outfn, diff_normalized, { cv::ImwriteFlags::IMWRITE_PNG_COMPRESSION, 0 });
	} else {
		cv::imwrite(ctx->outfn, mOutput, { cv::ImwriteFlags::IMWRITE_PNG_COMPRESSION, 0 });
	}

	return 0;
}


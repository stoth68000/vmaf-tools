/* Copyright Kernel Labs Inc 2025, All Rights Reserved. */

#include <stdio.h>
#include <getopt.h>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>

using namespace cv;

#define RENDER_TITLE_DEFAULT 1

struct tool_context_s {
#define MAX_INPUTS 4
	char *fn[MAX_INPUTS];
	Mat mat[MAX_INPUTS];
	int max_cols;
	int max_rows;

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

	if (ctx->render_title) {
		putText(ctx->mat[nr], ctx->fn[nr], Point(10, 40), FONT_HERSHEY_DUPLEX, 1.0, CV_RGB(255, 255, 255), 2);
	}

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
	printf("A tool to create a 2x2 multiview grid from four seperate pictures.\n");
	printf("If a specific image is not required, skip it, black will be composited.\n");
	printf("Usage:\n");
	printf("  -1 topleft.png\n");
	printf("  -2 topright.png\n");
	printf("  -3 bottomleft.png\n");
	printf("  -4 bottomright.png\n");
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

	while ((ch = getopt(argc, argv, "?h1:2:3:4:o:t:v")) != -1) {
		switch (ch) {
		case '1':
		case '2':
		case '3':
		case '4':
			idx = ch - '0' - 1;
			ctx->fn[idx] = strdup(optarg);
			if (matLoad(ctx, idx) < 0) {
				fprintf(stderr, "Failed to load image\n");
				return -1;
			}
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

	/* Create a 2x2 output mat, we'll composite into this */
	Mat mOutput = Mat(ctx->max_rows * 2, ctx->max_cols * 2, CV_8UC3);

	if (ctx->verbose) {
		printf("Output resolution is %dx%d\n", mOutput.cols, mOutput.rows);
	}

	for (int i = 0; i < MAX_INPUTS; i++) {
		if (ctx->fn[i] == NULL) {
			continue;
		}

		Rect src = Rect(0, 0, ctx->mat[i].cols, ctx->mat[i].rows);
		Rect dst = Rect((i % 2) * ctx->max_cols, (i / 2) * ctx->max_rows, ctx->max_cols, ctx->max_rows);

		ctx->mat[i](src).copyTo(mOutput(dst));
	}

	/* Save */
	cv::imwrite(ctx->outfn, mOutput, { cv::ImwriteFlags::IMWRITE_PNG_COMPRESSION, 0 });

	return 0;
}


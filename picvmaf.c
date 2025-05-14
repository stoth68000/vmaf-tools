/* Copyright Kernel Labs Inc 2025, All Rights Reserved. */

#include <stdio.h>
#include <getopt.h>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>

using namespace cv;

#define RENDER_TITLE_DEFAULT 1

struct vmaf_measurement_s {
	float fVMAF_score;
	float fVMAF_score_agg;
};

struct tool_context_s {
	char *ifn;
	char *ofn;
	int verbose;
	int render_title;
	int cursor_column;

	float min_score;
	int framecount;
	struct vmaf_measurement_s *measurements;
};

void usage()
{
        printf("A tool to create a vmaf chart with a cursor position on a specific measurement.\n");
        printf("The vmaf file has been processed and converted to a csv using\n"
			"\tAGGREGATE=`cat vmaf.json | jq -r '.aggregate.VMAF_score'`\n"
			"\tcat vmaf.json | jq -r '.frames[] | \"\(.frameNum),\(.VMAF_score)\"' | sed \"s!\\$!,$AGGREGATE!g\"\n");
        printf("Usage:\n");
        printf("  -i vmaf.csv\n");
        printf("  -c framenumber to draw cursor at (0..max vmaf frame number)\n");
        printf("  -o output.png\n");
        printf("  -v raise verbosity\n");
	printf("  -t render filenames into images [def: %d]\n", RENDER_TITLE_DEFAULT);
}

int main(int argc, char *argv[])
{
	struct tool_context_s tool_ctx, *ctx = &tool_ctx;
	memset(ctx, 0, sizeof(*ctx));
	ctx->render_title = RENDER_TITLE_DEFAULT;
	ctx->min_score = 110;

	int ch, idx;

	while ((ch = getopt(argc, argv, "?hc:i:o:t:v")) != -1) {
		switch (ch) {
		case 'c':
			ctx->cursor_column = atoi(optarg);
			break;
		case 'i':
			ctx->ifn = strdup(optarg);
			break;
		case 'o':
			ctx->ofn = strdup(optarg);
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


	/* Read entire vmaf file, we need a to know the total number of frames */
	char line[128];
	FILE *fh = fopen(ctx->ifn, "rb");
	while (!feof(fh)) {
		line[0] = 0;
		fgets(&line[0], sizeof(line), fh);
		if (feof(fh))
			break;

		if (line[0] == ' ' || line[0] == ';' || line[0] == '#') {
			continue;
		}

		line[ strlen(line) - 1 ] = 0;

		ctx->framecount++;
	}

	if (ctx->verbose ) {
		printf("Found %d frames.\n", ctx->framecount);
	}

	/* Alloc some storage and read the measurements */
	ctx->measurements = (struct vmaf_measurement_s *)calloc(ctx->framecount, sizeof(struct vmaf_measurement_s));
	rewind(fh);

	int c = 0;
	while (!feof(fh)) {
		line[0] = 0;
		fgets(&line[0], sizeof(line), fh);
		if (feof(fh))
			break;

		if (line[0] == ' ' || line[0] == ';' || line[0] == '#') {
			continue;
		}

		line[ strlen(line) - 1 ] = 0;

		if (ctx->verbose ) {
			printf("[%s]\n", line);
		}

		int f;
		float x, y;
		if (sscanf(&line[0], "%d,%f,%f", &f, &x, &y) != 3) {
			break;
		}

		//printf("element write %d\n", c);
		ctx->measurements[c].fVMAF_score = x;
		ctx->measurements[c].fVMAF_score_agg = y;

		if (x <= ctx->min_score) {
			ctx->min_score = x;
		}
		c++;
	}

	if (ctx->verbose) {
		printf("min_score %f\n", ctx->min_score);
	}

	/* Create a mat where vertical represents vmaf score, right is the number of frame measurements */
	Mat mOutput = Mat(100, ctx->framecount, CV_8UC3);
	for (int i = 0; i < ctx->framecount; i++) {
		//printf("element: i, %f, %f\n", i, ctx->measurements[i].fVMAF_score, ctx->measurements[i].fVMAF_score_agg);
		cv::line(mOutput, Point(i, mOutput.rows), Point(i, 100 - ctx->measurements[i].fVMAF_score), Scalar(0, 128, 0), 1, LINE_8);
	}

	/* Draw the cursor column */
	cv::line(mOutput, Point(ctx->cursor_column, mOutput.rows), Point(ctx->cursor_column, 0), Scalar(0, 0, 250), 2, LINE_8);

	Mat mOutputResized;
	cv::resize(mOutput, mOutputResized, Size(1920, 1080), INTER_LINEAR);

	if (ctx->render_title) {
		putText(mOutputResized, ctx->ofn, Point(40, 800), FONT_HERSHEY_DUPLEX, 1.0, CV_RGB(255, 255, 255), 2);

		char score[64];
		sprintf(score, "VMAF_score: %5.2f%%", ctx->measurements[ctx->cursor_column].fVMAF_score);
		putText(mOutputResized, score, Point(40, 840), FONT_HERSHEY_DUPLEX, 1.0, CV_RGB(250, 250, 150), 2);

		char agg[64];
		sprintf(agg, "VMAF_average: %5.2f%%", ctx->measurements[ctx->cursor_column].fVMAF_score_agg);
		putText(mOutputResized, agg, Point(40, 880), FONT_HERSHEY_DUPLEX, 1.0, CV_RGB(250, 250, 150), 2);
	}

	/* Save */
	cv::imwrite(ctx->ofn, mOutputResized, { cv::ImwriteFlags::IMWRITE_PNG_COMPRESSION, 0 });

	if (ctx->verbose) {
		printf("Created %s\n", ctx->ofn);
	}

	free(ctx->measurements);
	return 0;
}


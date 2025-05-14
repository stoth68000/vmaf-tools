/* Copyright Kernel Labs Inc 2025, All Rights Reserved. */

#include <stdio.h>
#include <getopt.h>
#include <sys/stat.h>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>

using namespace cv;

#define RENDER_TITLE_DEFAULT 1

struct tool_context_s {
#define MAX_INPUTS 2
	char *fn[MAX_INPUTS];
	int max_cols;
	int max_rows;

	int verbose;
	int width;
	int height;
};

static struct {
	int width;
	int height;
	int frame_size;
} tbl[] = {
	{ 1280, 720, (1280 * 720 * 3) / 2, },
	{ 1920, 1080, (1920 * 1080 * 3) / 2, },
};

double compute_psnr(double mse, double max_pixel_value)
{
    if (mse == 0) {
        return INFINITY;  // Perfect match
    }
    return 10.0 * log10((max_pixel_value * max_pixel_value) / mse);
}

double compute_luma_mse(const cv::Mat &frame1, const cv::Mat &frame2)
{
    if (frame1.size() != frame2.size() || frame1.type() != frame2.type()) {
        fprintf(stderr, "Error: Frame dimensions or types do not match.\n");
        return -1.0;
    }

    cv::Mat diff;
    cv::absdiff(frame1, frame2, diff);  // Compute absolute difference
    diff.convertTo(diff, CV_64F);      // Convert to double for accurate computation
    diff = diff.mul(diff);            // Square the differences

    double mse = cv::sum(diff)[0] / (frame1.rows * frame1.cols); // Normalize by the number of pixels
    return mse;
}

void usage()
{
        printf("A tool to generate mse for a pair of YUV files, containing many frames.\n");
        printf("Usage:\n");
        printf("  -1 file1.yuv\n");
        printf("  -2 file2.yuv\n");
        printf("  -W width (pixels def: 1920)\n");
        printf("  -H height (pixels def: 1080)\n");
        printf("  -v raise verbosity\n");
}

int main(int argc, char *argv[])
{
	struct tool_context_s tool_ctx, *ctx = &tool_ctx;
	memset(ctx, 0, sizeof(*ctx));
	ctx->width = 1920;
	ctx->height = 1080;

	int ch, idx;

	while ((ch = getopt(argc, argv, "?h1:2:3:4:vW:H:")) != -1) {
		switch (ch) {
		case '1':
		case '2':
			idx = ch - '0' - 1;
			ctx->fn[idx] = strdup(optarg);
			break;
		case 'v':
			ctx->verbose++;
			break;
		case 'H':
			ctx->height = atoi(optarg);
			break;
		case 'W':
			ctx->width = atoi(optarg);
			break;
		default:
		case '?':
		case 'h':
			usage();
			exit(1);
		}
	}

	if (argc < 2) {
		usage();
		exit(1);
	}

	FILE *fh1 = fopen(ctx->fn[0], "rb");
	if (!fh1) {
		fprintf(stderr, "input file 1 not found, aborting\n");
		exit(1);
	}

	FILE *fh2 = fopen(ctx->fn[1], "rb");
	if (!fh2) {
		fprintf(stderr, "input file 2 not found, aborting\n");
		exit(1);
	}

	int frame_size = (ctx->width * ctx->height * 3) / 2; /* YUV420 */

	/* Check the files are a perfect multiple of frame_size */
	struct stat s1, s2;
	stat(ctx->fn[0], &s1);
	stat(ctx->fn[1], &s2);
	if (s1.st_size != s2.st_size) {
		fprintf(stderr, "file input 1 isn't the same size as input 2, aborting\n");
		exit(1);
	}
	if (s1.st_size % frame_size) {
		fprintf(stderr, "file input 1 isn't a perfect multiple of frame_size %d\n", frame_size);
		exit(1);
	}

	unsigned char *b1 = (unsigned char *)malloc(frame_size);
	unsigned char *b2 = (unsigned char *)malloc(frame_size);
	if (b1 == NULL || b2 == NULL) {
		fprintf(stderr, "unable to allocate memory for frame, aborting\n");
		exit(1);
	}

	int nr = 0;

	while(1) {
		size_t l1 = fread(b1, 1, frame_size, fh1);
		size_t l2 = fread(b2, 1, frame_size, fh2);

		if (l1 != frame_size || l2 != frame_size) {
			break;
		}

		Mat y1 = Mat(ctx->height, ctx->width, CV_8UC1, b1);
		Mat y2 = Mat(ctx->height, ctx->width, CV_8UC1, b2);
		double y_mse = compute_luma_mse(y1, y2);

		int chroma_width = ctx->width / 2;
		int chroma_height = ctx->height / 2;

		unsigned char *u1 = b1 + (ctx->width * ctx->height);
		unsigned char *u2 = b2 + (ctx->width * ctx->height);
		Mat u1_mat = Mat(chroma_height, chroma_width, CV_8UC1, u1);
		Mat u2_mat = Mat(chroma_height, chroma_width, CV_8UC1, u2);
		double u_mse = compute_luma_mse(u1_mat, u2_mat);

		unsigned char *v1 = u1 + (chroma_width * chroma_height);
		unsigned char *v2 = u2 + (chroma_width * chroma_height);
		Mat v1_mat = Mat(chroma_height, chroma_width, CV_8UC1, v1);
		Mat v2_mat = Mat(chroma_height, chroma_width, CV_8UC1, v2);
		double v_mse = compute_luma_mse(v1_mat, v2_mat);

		const double max_pixel_value = 255.0;
		double y_psnr = compute_psnr(y_mse, max_pixel_value);
		double u_psnr = compute_psnr(u_mse, max_pixel_value);
		double v_psnr = compute_psnr(v_mse, max_pixel_value);

		if (y_mse >= 0.0) {
			printf("frame %08d, mse Y %8.2f, U %8.2f, V %8.2f, psnr(dB) Y %8.2f, U %8.2f, V %8.2f\n", nr,
				y_mse, u_mse, v_mse,
				y_psnr, u_psnr, v_psnr);
		}
		nr++;
	}

	free(b1);
	free(b2);
	fclose(fh1);
	fclose(fh2);
	return 0;
}


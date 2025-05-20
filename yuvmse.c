/* Copyright Kernel Labs Inc 2025, All Rights Reserved. */

#include <stdio.h>
#include <getopt.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>

using namespace cv;

#define RENDER_TITLE_DEFAULT 1

struct tool_context_s {
#define MAX_INPUTS 2
	char *fn[MAX_INPUTS];
	uint64_t *hashes[MAX_INPUTS];
	int hash_count[MAX_INPUTS];

	int verbose;
	int width;
	int height;
	int skipframes;
	int windowsize;
	int bestmatch;
	int dimension_defaults; /* 1, defaults, 0 = user supplied, 2 = detected */
	int dcthashmatch;
};

static struct {
	int width;
	int height;
	int frame_size;
	const char *label;
} tbl[] = {
	{  720,  480, ( 720 *  480 * 3) / 2,   "720x480p" },
	{  720,  576, ( 720 *  576 * 3) / 2,   "720x576p" },
	{ 1280,  720, (1280 *  720 * 3) / 2,  "1280x720p" },
	{ 1920, 1080, (1920 * 1080 * 3) / 2, "1920x1080p" },
	{ 3840, 2160, (3840 * 2160 * 3) / 2, "3840x2160p" },
};

int detect_frame_size(struct tool_context_s *ctx, int inputnr)
{
	int detections = 0;
	int detected = -1;

	if (ctx->fn[inputnr] == 0) {
		return -1;
	}

	struct stat s;
	if (stat(ctx->fn[inputnr], &s) < 0) {
		fprintf(stderr, "file input %d not found, aborting\n", inputnr);
		exit(1);
	}

	for (int i = 0; i < (sizeof(tbl) / sizeof(tbl[0])); i++) {
		//printf("i %d, fs %d, size %ld\n", i, tbl[i].frame_size, s.st_size);
		if (s.st_size % tbl[i].frame_size == 0) {
			printf("# Detected possible %10s, with exactly %6ld frames in %s\n", tbl[i].label, s.st_size / tbl[i].frame_size, ctx->fn[inputnr]);
			detections++;
			detected = i;
		}
	}

	if (detections == 1) {
		return detected;
	}

	printf("# Operator needs to provide width (-W) and height (-H) args\n");
	return -1; /* Error */
}

int hamming_distance(uint64_t a, uint64_t b)
{
	return __builtin_popcountll(a ^ b);
}

uint64_t computeDCTHash(struct tool_context_s *ctx, const Mat& image)
{
	Mat resized, floatImage, dctImage;

	/* Get the image down to a meaningful 32x32 sample */
	resize(image, resized, Size(32, 32), 0, 0, INTER_AREA);
	//cv::imwrite("out2.png", resized, { cv::ImwriteFlags::IMWRITE_PNG_COMPRESSION, 0 });

	/* DCT func needs floats, convert to */
	resized.convertTo(floatImage, CV_32F); // Convert to float for DCT
	dct(floatImage, dctImage);

	/* Get the dct block from the dct'd image */
	Mat dctBlock = dctImage(Rect(0, 0, 8, 8));

	int idx = 0;
	float values[64];

	if (ctx->verbose) {
		printf("DCT 8x8 Block:\n");
	}

	for (int i = 0; i < 8; ++i) {
		for (int j = 0; j < 8; ++j) {
			if (ctx->verbose) {
				printf("%7.2f ", dctBlock.at<float>(i, j));
			}
			values[i * 8 + j] = dctBlock.at<float>(i, j);
		}
		if (ctx->verbose) {
			printf("\n");
		}
	}

	/* Get the high and low medians, be careful not to disrupt
	 * the values array, leave them unsorted. */
	float temp[64];
	std::copy(values, values + 64, temp);
	std::nth_element(temp, temp + 31, temp + 64);
	float low = temp[31];
	std::nth_element(temp, temp + 32, temp + 64);
	float high = temp[32];

	float median = (low + high) / 2.0f;

	if (ctx->verbose) {
		printf("median %f h %f l %f\n", median, high, low);
	}

	/* Compute the hash */
	uint64_t hash = 0;
	for (int i = 0; i < 64; ++i) {
		if (values[i] > median) {
			hash |= (1ULL << (63 - i));
		}
	}

	if (ctx->verbose) {
		printf("DCT Hash: %" PRIx64 "\n", hash);
	}

	return hash;
}

double compute_sharpness(const cv::Mat &image)
{
	Mat gray;
	if (image.channels() == 3) {
		cvtColor(image, gray, COLOR_BGR2GRAY);
	} else {
		gray = image.clone();
	}

	Mat laplacian;
	Laplacian(gray, laplacian, CV_64F);

	// Calculate the variance of the Laplacian
	Scalar mean, stddev;
	meanStdDev(laplacian, mean, stddev);

	// Variance is the square of the standard deviation
	return stddev[0] * stddev[0];
}

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
        printf("A tool to generate mse/psnr/sharpness/dct-hashes for a pair of YUV files, containing many frames.\n");
        printf("The bestmatch mode tries to match YUV frames within a window of -w frames, and you can\n");
        printf("elect to skip -s #frames on file1 to try and find a best match for misaligned YUV files.\n");
        printf("The DCT hash match mode tries to match YUV frames within a window of -w frames\n");
        printf("showing trimming instructions if avail.\n");
        printf("Usage:\n");
        printf("  -1 file1.yuv\n");
        printf("  -2 file2.yuv\n");
        printf("  -W width (pixels def: 1920)\n");
        printf("  -H height (pixels def: 1080)\n");
        printf("  -v raise verbosity\n");
        printf("  -b run best match and try to find frame offsets for best mse match\n");
        printf("    -w number of frames to process [def: 30] (bestmatch)\n");
        printf("    -s number of frames from input 1 to skip (bestmatch)\n");
        printf("  -D run DCT hashes and try to find frame offsets for best aligned match\n");
}

struct frame_stats_s
{
	double y_mse, u_mse, v_mse;
	double y_psnr, u_psnr, v_psnr;
	double sharpness[2];
	uint64_t hash[2];
};

int compute_frame_stats(struct tool_context_s *ctx, unsigned char *b1, unsigned char *b2, struct frame_stats_s *stats)
{
	// MMM
	memset(stats, 0, sizeof(*stats));

	Mat y1 = Mat(ctx->height, ctx->width, CV_8UC1, b1);

	Mat y2;
	if (b2) {
		y2 = Mat(ctx->height, ctx->width, CV_8UC1, b2);
		stats->y_mse = compute_luma_mse(y1, y2);
	}

	int chroma_width = ctx->width / 2;
	int chroma_height = ctx->height / 2;

	unsigned char *u1 = b1 + (ctx->width * ctx->height);
	unsigned char *u2 = b2 + (ctx->width * ctx->height);

	Mat u1_mat = Mat(chroma_height, chroma_width, CV_8UC1, u1);
	if (b2) {
		Mat u2_mat = Mat(chroma_height, chroma_width, CV_8UC1, u2);
		stats->u_mse = compute_luma_mse(u1_mat, u2_mat);
	}

	unsigned char *v1 = u1 + (chroma_width * chroma_height);
	unsigned char *v2 = u2 + (chroma_width * chroma_height);
	Mat v1_mat = Mat(chroma_height, chroma_width, CV_8UC1, v1);
	if (b2) {
		Mat v2_mat = Mat(chroma_height, chroma_width, CV_8UC1, v2);
		stats->v_mse = compute_luma_mse(v1_mat, v2_mat);
	}

	if (b2) {
		const double max_pixel_value = 255.0;
		stats->y_psnr = compute_psnr(stats->y_mse, max_pixel_value);
		stats->u_psnr = compute_psnr(stats->u_mse, max_pixel_value);
		stats->v_psnr = compute_psnr(stats->v_mse, max_pixel_value);
	}

	stats->sharpness[0] = compute_sharpness(y1);
	if (b2) {
		stats->sharpness[1] = compute_sharpness(y2);
	}

	stats->hash[0] = computeDCTHash(ctx, y1);
	if (b2) {
		stats->hash[1] = computeDCTHash(ctx, y2);
	}

	return 0; /* Success */
}

int compute_sequence_bestmatch(struct tool_context_s *ctx)
{
	double low_y_mse = 60000.0;
	int low_frame = 0;

	int frame_size = (ctx->width * ctx->height * 3) / 2; /* YUV420 */

	/* Check the files are a perfect multiple of frame_size */
	struct stat s1, s2;
	if (stat(ctx->fn[0], &s1) < 0) {
		fprintf(stderr, "file input 1 not found, aborting\n");
		exit(1);
	}
	if (stat(ctx->fn[1], &s2) < 0) {
		fprintf(stderr, "file input 2 not found, aborting\n");
		exit(1);
	}
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

	FILE *fh1 = fopen(ctx->fn[0], "rb");
	if (!fh1) {
		fprintf(stderr, "input file 1 not found, aborting\n");
		exit(1);
	}

	int nr1 = 0;
	int skip_frames = ctx->skipframes;
	while (!feof(fh1)) {
		FILE *fh2 = fopen(ctx->fn[1], "rb");
		if (!fh2) {
			fprintf(stderr, "input file 2 not found, aborting\n");
			exit(1);
		}
		size_t l1 = fread(b1, 1, frame_size, fh1);
		if (skip_frames-- > 0) {
			nr1++;
			continue;
		}

		int nr2 = 0;

		low_y_mse = 60000.0;
		while (!feof(fh2)) {
			size_t l2 = fread(b2, 1, frame_size, fh2);

			if (l1 != frame_size || l2 != frame_size) {
				break;
			}

			struct frame_stats_s stats;
			compute_frame_stats(ctx, b1, b2, &stats);

			if (ctx->verbose && stats.y_mse >= 0.0) {
				printf("frame %08d.%08d, mse Y %8.2f, U %8.2f, V %8.2f, psnr(dB) Y %8.2f, U %8.2f, V %8.2f\n",
					nr1, nr2,
					stats.y_mse, stats.u_mse, stats.v_mse,
					stats.y_psnr, stats.u_psnr, stats.v_psnr);
			}
			if (stats.y_mse < low_y_mse) {
				low_y_mse = stats.y_mse;
				low_frame = nr2;
			}

			if (nr2++ >= ctx->windowsize) {
				printf("best match for file1.frame %08d, y mse was %8.2f file2.frame %08d\n", nr1, low_y_mse, low_frame);
				break;
			}
		}
		fclose(fh2);
		if (nr1++ >= ctx->windowsize) {
			break;
		}
	}

	free(b1);
	free(b2);
	fclose(fh1);

	return 0;
}

int compute_sequence_dct_hashes_input(struct tool_context_s *ctx, int inputnr, uint64_t **hashes, int *hash_count)
{
	FILE *fh1 = fopen(ctx->fn[inputnr], "rb");
	if (!fh1) {
		fprintf(stderr, "input file %d not found, aborting\n", inputnr);
		exit(1);
	}

	int frame_size = (ctx->width * ctx->height * 3) / 2; /* YUV420 */

	struct stat s;
	stat(ctx->fn[inputnr], &s);
	if (s.st_size % frame_size) {
		fprintf(stderr, "file input %d isn't a perfect multiple of frame_size %d\n", inputnr, frame_size);
		exit(1);
	}

	int frame_count = (s.st_size / frame_size) + 1;

	uint64_t *hlist = (uint64_t *)malloc(sizeof(uint64_t) * frame_count);
	if (hlist == NULL) {
		fprintf(stderr, "unable to allocate memory for hashlist, aborting\n");
		exit(1);
	}

	unsigned char *b1 = (unsigned char *)malloc(frame_size);
	if (b1 == NULL) {
		fprintf(stderr, "unable to allocate memory for frame, aborting\n");
		exit(1);
	}

	int nr = 0;

	int line = 0;
	int skip_frames = ctx->skipframes;
	while(1) {
		size_t l1 = fread(b1, 1, frame_size, fh1);
		if (l1 != frame_size) {
			break;
		}
		if (skip_frames-- > 0) {
			nr++;
			continue;
		}
		if (nr > ctx->windowsize) {
			break;
		}

		struct frame_stats_s stats;
		compute_frame_stats(ctx, b1, NULL, &stats);

		hlist[nr] = stats.hash[0];

		if (ctx->verbose) {
			printf("frame %08d, hash %" PRIx64 ", %s\n", nr, stats.hash[0], ctx->fn[inputnr]);
		}

		nr++;
	}

	free(b1);
	fclose(fh1);

	*hash_count = nr;
	*hashes = hlist;

	return 0; /* Success */
}

/* Function to find the longest matching sequence in two arrays
 * Input arrays must be the same length.
 * return number of matches, else -1 on error.
 * return positions of matching sequence in posA and B
 */
static int findLongestMatch(uint64_t *a, uint64_t *b, int len, int *posA, int *posB, int verbose)
{
	int maxLen = 0;
	int startA = 0, startB = 0;

	for (int offset = -len + 1; offset < len; ++offset) {
		int currentLen = 0;
		for (int i = 0; i < len; ++i) {
			int j = i + offset;
			if (j >= 0 && j < len) {
				int hd = hamming_distance(a[i], b[j]);
				if (hd <= 2) {
					currentLen++;
					if (currentLen > maxLen) {
						maxLen = currentLen;
						startA = i - currentLen + 1;
						startB = j - currentLen + 1;
					}
				} else {
					currentLen = 0;
				}
			}
		}
	}

	if (maxLen > 0) {
		*posA = startA;
		*posB = startB;

		if (verbose) {
			printf("Matching sequence: ");
			for (int i = 0; i < maxLen; ++i) {
				printf("%" PRIx64 " ", a[startA + i]);
			}
			printf("\n");
		}
	} else {
		return -1; /* Error - No matching sequence */
	}

	return maxLen; /* Success */
}

/* Product DCT hashes for each YUV file.
 * Put those hashes into lists.
 * search the lists to find the longest sequence of matches
 * between YUV frames from different files.
 * Use this feature to help align random YUV files for the same basic content.
 */
int compute_sequence_dct_hashes(struct tool_context_s *ctx)
{
	int inputs = 0;
	for (int i = 0; i < MAX_INPUTS; i++) {
		if (ctx->fn[i]) {
			inputs++;
			compute_sequence_dct_hashes_input(ctx, i, &ctx->hashes[i], &ctx->hash_count[i]);
		}
	}

	if (ctx->verbose) {
		for (int i = 0; i < MAX_INPUTS; i++) {
			if (ctx->fn[i] && ctx->hashes[i]) {
				for (int j = 0; j < ctx->hash_count[i]; j++) {
					printf("frame %08d, hash %" PRIx64 ", %s\n", j, ctx->hashes[i][j], ctx->fn[i]);
				}
			}
		}
	}

	/* Search hashes for input 2 and align with input 1 */
	int matches = 0;
	int match_seq_count = 0;
	int posA, posB;
	if (inputs > 1) {
		matches = findLongestMatch(ctx->hashes[0], ctx->hashes[1], ctx->hash_count[0], &posA, &posB, ctx->verbose);
	}
	printf("# hash sequence matches: %d\n", matches);
	if (matches) {
		printf("# Frame sequence, file 1 begins frame %08d, file 2 begins frame %08d\n", posA, posB);
		if (posA > 0) {
			printf("# Trimming instructions:\n");
			printf("#   dd if=%s of=%s.trimmed bs=%d skip=%d\n",
				ctx->fn[0], ctx->fn[0],
				(ctx->width * ctx->height * 3) / 2, posA);
		}
		if (posB > 0) {
			printf("# Trimming instructions:\n");
			printf("#   dd if=%s of=%s.trimmed bs=%d skip=%d\n",
				ctx->fn[1], ctx->fn[1],
				(ctx->width * ctx->height * 3) / 2, posB);
		}
		if (posA == 0 && posB == 0) {
			printf("# No trimming instructions necessary, YUV is already aligned.\n");
		}
	}

	for (int i = 0; i < MAX_INPUTS; i++) {
		if (ctx->fn[i] && ctx->hashes[i]) {
			free(ctx->hashes[i]);
			ctx->hashes[i] = NULL;
		}
	}

	return 0;
}

int compute_sequence_mse(struct tool_context_s *ctx)
{
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
	if (stat(ctx->fn[0], &s1) < 0) {
		fprintf(stderr, "file input 1 not found, aborting\n");
		exit(1);
	}
	if (stat(ctx->fn[1], &s2) < 0) {
		fprintf(stderr, "file input 2 not found, aborting\n");
		exit(1);
	}
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

	int line = 0;
	while(1) {
		size_t l1 = fread(b1, 1, frame_size, fh1);
		size_t l2 = fread(b2, 1, frame_size, fh2);

		if (l1 != frame_size || l2 != frame_size) {
			break;
		}

		struct frame_stats_s stats;
		compute_frame_stats(ctx, b1, b2, &stats);

		if (line == 0) {
			printf("%8s %9s %9s %9s %9s %9s %9s %9s %27s %17s %8s %21s", "#  Frame", "MSE", "", "", "PSNR", "", "", "Sharp", "DCT Hash", "", "Hamming", "Hash");
			printf("\n");
			printf("%8s %9s %9s %9s %9s %9s %9s %9s %9s", "#     Nr", "Y", "U", "V", "Y", "U", "V", "f1", "f2");
			printf("%18s %17s %8s %21s", "f1", "f2", "Dist", "Assessment");
			printf("\n");
			printf("#------> <---------------------------> <---------------------------> <-----------------> <---------------------------------------------------------------->\n");
		}

		if (line++ > 24) {
			line = 0;
		}

		printf("%08d, %8.2f, %8.2f, %8.2f, %8.2f, %8.2f, %8.2f", nr,
			stats.y_mse, stats.u_mse, stats.v_mse,
			stats.y_psnr, stats.u_psnr, stats.v_psnr);

		int hd = hamming_distance(stats.hash[0], stats.hash[1]);

		printf(", %8.2f, %8.2f, %" PRIx64 ", %" PRIx64 ", %7d, %20s",
			stats.sharpness[0], stats.sharpness[1], stats.hash[0], stats.hash[1],
			hd,
			hd == 0 ? "Exact Match" :
			hd <= 10 ? "Near Identical" : "Different");

		printf("\n");

		nr++;
	}

	free(b1);
	free(b2);
	fclose(fh1);
	fclose(fh2);

	return 0;
}

void args_to_console(struct tool_context_s *ctx)
{
	printf("# dimensions: %d x %d (%s)\n", ctx->width, ctx->height,
		ctx->dimension_defaults == 0 ? "user supplied" : 
		ctx->dimension_defaults == 1 ? "defaults" : "autodetected");
	for (int i = 0; i < MAX_INPUTS; i++) {
		if (ctx->fn[i]) {
			printf("# file%d: %s\n", i, ctx->fn[i]);
		}
	}
	printf("# windowsize: %d\n", ctx->windowsize);
	printf("# skipframes: %d\n", ctx->skipframes);
	printf("# bestmatch: %d\n", ctx->bestmatch);
	printf("# verbose: %d\n", ctx->verbose);
	printf("# dcthashmatch: %d\n", ctx->dcthashmatch);
}

int main(int argc, char *argv[])
{
	struct tool_context_s tool_ctx, *ctx = &tool_ctx;
	memset(ctx, 0, sizeof(*ctx));
	ctx->dimension_defaults = 1;
	ctx->width = 1920;
	ctx->height = 1080;
	ctx->windowsize = 30;

	int ch, idx, ret;

	while ((ch = getopt(argc, argv, "?h1:2:3:4:bs:vw:DW:H:")) != -1) {
		switch (ch) {
		case '1':
		case '2':
			idx = ch - '0' - 1;
			ctx->fn[idx] = strdup(optarg);
			ret = detect_frame_size(ctx, idx);
			if (ret == 0) {
				ctx->width = tbl[ret].width;
				ctx->height = tbl[ret].height;
				ctx->dimension_defaults = 2;
			}
			break;
		case 'b':
			ctx->dcthashmatch = 0;
			ctx->bestmatch = 1;
			break;
		case 'v':
			ctx->verbose++;
			break;
		case 's':
			ctx->skipframes = atoi(optarg);
			break;
		case 'w':
			ctx->windowsize = atoi(optarg);
			break;
		case 'D':
			ctx->dcthashmatch = 1;
			ctx->bestmatch = 0;
			break;
		case 'H':
			ctx->height = atoi(optarg);
			ctx->dimension_defaults = 0;
			break;
		case 'W':
			ctx->width = atoi(optarg);
			ctx->dimension_defaults = 0;
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
	args_to_console(ctx);

	ctx->windowsize += ctx->skipframes;

	if (ctx->bestmatch) {
		int ret = compute_sequence_bestmatch(ctx);
	} else if (ctx->dcthashmatch) {
		int ret = compute_sequence_dct_hashes(ctx);
	} else {
		int ret = compute_sequence_mse(ctx);
	}

	for (int i = 0; i < MAX_INPUTS; i++) {
		if (ctx->fn[i]) {
			free(ctx->fn[i]);
		}
	}
	return 0;
}


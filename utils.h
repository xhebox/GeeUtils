#ifndef UTILS_INCLUDE_H
#define UTILS_INCLUDE_H

#include <stdbool.h>
#include <stdio.h>
#include <errno.h>

#define MAX_MOUSE_DATA_COUNT 512

// from clib/logfmt
bool quiet = false;
#define clean_errno() (errno == 0 ? "None" : strerror(errno))
#define log_debug(M, ...) if(!quiet) fprintf(stdout, "\33[34mDEBUG\33[90m (%s:%d)[%s]\33[39m " M "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__);
#define log_error(M, ...) if(!quiet) fprintf(stdout, "\33[31mERR\33[90m (%s:%d)[%s], errno: %s\33[39m " M "\n", __FILE__, __LINE__, __func__, clean_errno(), ##__VA_ARGS__);
#define log_info(M, ...) if(!quiet) fprintf(stdout, "\33[32mINFO\33[90m (%s:%d)[%s]\33[39m " M "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__);

typedef struct login {
	char hash[64];
	char key[512];
	char gt[64];
	char challenge[64];
	char path[256]; // jspath
	char id[64];
	char slice[64];
	char fullbg[64];
	char bg[64];
	char ypos[12];
} login;

// para: input key, output[52](caller prepared, not malloc]
void imgoff(const char*, int*);
// para: width, height, channels, input uint8_t[][channels]
// return: pointer to uint8_t[][channels], need to free
void* recover_img(int, int, int, const void*);

// para: workdir path, ypos from server
// return: possible xpos
int find_xpos(const char*, int);
// para: xpos, challenge token
// return: userresponse string, need to free
char* enc_xpos(int, const char*);

// para: input[][x,y,time], array count, output[][delta x, delta y, delta time](need to free)
// return: output count
int diff_mouse(int (*)[3], int, int (**)[3]);
// para: input[][delta x,delta y, delta time], array count, output[][x, y, time](need to free)
// return: output count
int undiff_mouse(int (*)[3], int, int (**)[3]);

// para: input[][delta x, delta y, delta time], count
// return: a arg, need to free
char* serialize_mouse(int (*)[3], int);
// para: a arg, output[][delta x, delta y, delta time](need to free, when NULL, count only)
// return: count, need to free
int unserialize_mouse(const char*, int (**)[3]);

// para: input, output arr[caller prepared, not malloc]
// return: encoded len
int encuricomp(const char*, char*);

// para: input, input len, output arr[caller prepared, not malloc]
// return: decoded len
int decuricomp(const char*, int, char*);

// para: input, input len, output arr[caller prepared, not malloc]
// return: decoded len
int decuri(const char*, int, char *);

// return: login struct
login* init();
// para: login struct
void cleanup(login*);

// return encdata or encdata length or xor key
char* get_encdata();
int get_encdata_len();
char* get_encdata_key();

// para: input data, input len, xor key, output arr[need to free, when NULL, count only]
// return: output count
int dec_encdata(const char *, int, const char *, char**); 

// para: login struct, workdir path(writeable), which url to get gt/challenge
void prepare(login*, const char*, const char*);
// para: login struct, workdir path(writeable)
void refresh(login*, const char*);
// para: login struct, userresponse, a arg, imgload time, passtime
void verify(login*, const char*, const char*, unsigned, unsigned);
#endif

#ifdef UTILS_IMPL_H

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <curl/curl.h>
#include <ctype.h>
#include <regex.h>
#include "jsmn.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#ifdef DEBUG
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif

char uricomp[256] = {0};
bool encode_i = false;

uint8_t off_n[52] = {0};
bool off_s = false;

char data[40960] = {0};
int dc = 0;
bool data_s = false;
bool data_i = false;

char data_key[12] = {0};
bool dkey_s = false;

int barr[][2] = {{1, 0}, {2, 0}, {1, -1}, {1, 1}, {0, 1}, {0, -1}, {3, 0}, {2, -1}, {2, 1}};
bool barr_s = false;

char estr[75] = "stuvwxyz~";
char dstr[75] = "()*,-./0123456789:?@ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqr";	

static inline double rand1() {
	return ((double)rand()/(double)RAND_MAX);
}

typedef struct HSV
{
	double h;
	double s;
	double v;
} HSV;

static inline HSV rgb_hsv(uint8_t r, uint8_t g, uint8_t b) {
	HSV hsv = {.h=0, .s=0, .v=0};
	double delta, min;

	min = r <= g ? r : g;
	min = min <= b ? min : b;
	hsv.v = r >= g ? r : g;
	hsv.v = hsv.v >= b ? hsv.v : b;
	delta = hsv.v - min;

	if (hsv.v == 0) hsv.s = 0;
	else hsv.s = delta/hsv.v;

	if (hsv.s != 0) {
		if (r == hsv.v) hsv.h = (g-b)/delta;
		else if (g == hsv.v) hsv.h = 2 + (b-r)/delta;
		else if (b == hsv.v) hsv.h = 4 + (r-g)/delta;
		hsv.h *= 60;
		if (hsv.h < 0) hsv.h = hsv.h + 360;
	}

	hsv.v /= 255;
	return hsv;
}

void imgoff(const char *input, int *ret) {
	int a[13] = {0};
	const char *token = input;
	for (int b=0; b < 13; b++, token = strchr(token, '_')+1) {
		a[b] = atoi(token);
	}
	for (int h=9, l=0; (h*(h+1)%2+7) && l < 52; l++) {
		int c = 2 * a[l%26/2] + l%2;
		if (!( (l/2) % 2)) {
			c += l%2 ? -1 : 1;
		}
		c += l < 26 ? 26 : 0;
		if (ret) ret[l] = c;
		off_n[l] = c;
		h = h > 68872 ? h/2 : h*2;
	}
	log_info("refreshed the default image offset data");
#ifdef DEBUG
	char buf[52*3] = {0};
	int bc=0;
	for (int a=0; a < sizeof(off_n); a++) {
		bc += snprintf(&buf[bc], sizeof(buf)-bc, "%d ", off_n[a]);
	}
	log_debug("image offset: [ %s]", buf);
#endif
}

void* recover_img(int w, int h, int n, const void *in) {
	int off[52] = {39,38,48,49,41,40,46,47,35,34,50,51,33,32,28,29,27,26,36,37,31,30,44,45,43,42,12,13,23,22,14,15,21,20,8,9,25,24,6,7,3,2,0,1,11,10,4,5,19,18,16,17};
	if (off_s) {
		log_error("fall back to default image offset data, use prepare() to set offset if that's your case");
	} else memcpy(&off[0], &off_n[0], sizeof(off_n));

	int rw = w-sizeof(off_n);
	uint8_t *out = calloc(1, rw*h*n);
	if (!out) {
		log_error("failed to calloc(1, %d)", rw*h*n);
		return NULL;
	}

	int slice_w = w/26;
	int slice_h = h/2;
	int rsw = slice_w-2;
	for (int a=0; a < sizeof(off_n); a++) {
		int smod = off[a] % 26;
		int snum = off[a] > 25 ? 1 : 0;
		const uint8_t *s = in + snum*slice_h*w*n + smod*slice_w*n + n;

		int dmod = a % 26;
		int dnum = a > 25 ? 1 : 0;
		uint8_t *d = out + dnum*slice_h*rw*n + dmod*rsw*n;
		for (int h=0; h < slice_h; h++, d+=rw*n, s+=w*n) memcpy(d, s, rsw*n);
	}
	return out;
}

int find_xpos(const char *workdir, int ypos) {
	log_info("workdir: %s, ypos: %d", workdir, ypos);
	char bg_path[PATH_MAX];
	char fbg_path[PATH_MAX];
	char slice_path[PATH_MAX];
	char dec_bg_path[PATH_MAX];
	char dec_fbg_path[PATH_MAX];
	char dec_slice_path[PATH_MAX];
	assert(strlen(workdir) < PATH_MAX-20);
	snprintf(bg_path, sizeof(bg_path), "%s/bg.png", workdir);
	snprintf(fbg_path, sizeof(fbg_path), "%s/fullbg.png", workdir);
	snprintf(slice_path, sizeof(slice_path), "%s/slice.png", workdir);
	snprintf(dec_bg_path, sizeof(bg_path), "%s/dec_bg.png", workdir);
	snprintf(dec_fbg_path, sizeof(fbg_path), "%s/dec_fbg.png", workdir);
	snprintf(dec_slice_path, sizeof(slice_path), "%s/dec_slice.png", workdir);


	int bg_w=0, bg_h=0, bg_n=0;
	uint8_t *r = stbi_load(bg_path, &bg_w, &bg_h, &bg_n, 3), (*bg)[bg_w][bg_n] = (typeof(bg))r;
	if (!r) {
		log_error("can not load image %s", bg_path);
		return -1;
	}

	uint8_t (*dbg)[bg_w-sizeof(off_n)][bg_n] = recover_img(bg_w, bg_h, bg_n, bg);
	if (!dbg) {
		log_error("can not recover image %s", bg_path);
		return -1;
	}
#ifdef DEBUG
	stbi_write_png(dec_bg_path, bg_w-sizeof(off_n), bg_h, bg_n, &dbg[0][0][0], (bg_w-sizeof(off_n))*bg_n);
#endif

	int fbg_w=0, fbg_h=0, fbg_n=0;
	r = stbi_load(fbg_path, &fbg_w, &fbg_h, &fbg_n, 3);
	uint8_t	(*fbg)[fbg_w][fbg_n] = (typeof(fbg))r;
	if (!r) {
		log_error("can not load image %s", fbg_path);
		return -1;
	}
	uint8_t (*dfbg)[fbg_w-sizeof(off_n)][fbg_n] = recover_img(fbg_w, fbg_h, fbg_n, fbg);
	if (!dfbg) {
		log_error("can not recover image %s", fbg_path);
		return -1;
	}
#ifdef DEBUG
	stbi_write_png(dec_fbg_path, fbg_w-sizeof(off_n), fbg_h, fbg_n, &dfbg[0][0][0], (fbg_w-sizeof(off_n))*fbg_n);
#endif

	int slice_w=0, slice_h=0, slice_n=0;
	r = stbi_load(slice_path, &slice_w, &slice_h, &slice_n, 4);
	uint8_t (*slice)[slice_w][slice_n] = (typeof(slice))r;
	if (!slice) {
		log_error("can not load image %s", slice_path);
		return -1;
	}
	for (int a=0; a < slice_h; a++) {
		for (int b=0; b < slice_w; b++)
			if (slice[a][b][3] < 255) slice[a][b][3] = 0;
	}
#ifdef DEBUG
	stbi_write_png(dec_slice_path, slice_w, slice_h, slice_n, slice, slice_w*slice_n);
#endif

	unsigned xpos=0;
	double dark=0;
	for (int a=0, d=bg_w-sizeof(off_n)-slice_w; a < d; a++) {
		double cur=0;
		for (int b=ypos, e=ypos+slice_h; b < e; b++) {
			for (int c=a, f=a+slice_w; c < f; c++) {
					HSV dbg_hsv = rgb_hsv(dbg[b][c][0], dbg[b][c][1], dbg[b][c][2]);
					HSV dfbg_hsv = rgb_hsv(dfbg[b][c][0], dfbg[b][c][1], dfbg[b][c][2]);
					cur+=fabs(dfbg_hsv.v-dbg_hsv.v);
			}
		}
#ifdef DEBUG
		log_debug("xpos %u, most dark weight %f", a, cur);
#endif
		if (cur > dark) {
			dark = cur;
			xpos = a;
		}
	}
	log_info("first loop: initial xpos %u, most dark weight %f", xpos, dark);

	dark=0;
	for (int a=xpos-5, b=xpos+5; a < b; a++) {
		double cur=0;
		for (int y=0, e=slice_h, yd=ypos; y < e; y++, yd++) {
			for (int x=0, f=slice_w, xd=a; x < f; x++, xd++) {
				if (slice[y][x][3]) {
					HSV dbg_hsv = rgb_hsv(dbg[yd][xd][0], dbg[yd][xd][1], dbg[yd][xd][2]);
					HSV dfbg_hsv = rgb_hsv(dfbg[yd][xd][0], dfbg[yd][xd][1], dfbg[yd][xd][2]);
					cur+=fabs(dfbg_hsv.v-dbg_hsv.v);
				}
			}
		}
#ifdef DEBUG
		log_debug("xpos %u, most dark weight %f", a, cur);
#endif
		if (cur > dark) {
			dark = cur;
			xpos = a;
		}
	}
	log_info("second loop: final xpos %u, weight %f", xpos, dark);

	free(dbg);
	free(dfbg);
	stbi_image_free(bg);
	stbi_image_free(fbg);
	stbi_image_free(slice);
	return xpos;
}

char* enc_xpos(int xpos, const char *challenge) {
	log_info("xpos %u, challenge %s", xpos, challenge);
	int clen = strlen(challenge);
	char d[32] = {0};
	for (int a=0, b=clen-32; a < b; a++) {
		assert(a < sizeof(d));
		d[a] = challenge[a+32] > 57 ? challenge[a+32] - 87 : challenge[a+32] - 48;
#ifdef DEBUG
		log_debug("d[%d] = %d\n", a, d[a]);
#endif
	}

	int g = xpos + 36 * d[0] + d[1];
#ifdef DEBUG
	log_debug("var g = %d\n", g);
#endif

	bool j[128] = {0};
	char i[5][7] = {0};
	int ic[5] = {0};
	for (int a=0, k=0; a < 32; a++) {
		char h = challenge[a];
		if (!j[(int)h]) {
			j[(int)h] = true;
#ifdef DEBUG
			log_debug("j[%c] = true\n", h);
#endif

			i[k][ic[k]++] = h;
#ifdef DEBUG
			log_debug("i[%d].push(%c)\n", k, h);
#endif
			if (5 == ++k) k = 0;
		}
	}

	char p[64] = {0};
	int pc = 0;
	int q[5] = { 1, 2, 5, 10, 50 };
	for (int o=4, n=g; n > 0;) {
		if ( n - q[o] >= 0 ) {
			int m = rand1() * ic[o];
			p[pc++] = i[o][m];
#ifdef DEBUG
			log_debug("p += i[%d][%d] -> %s\n", o, m, p);
#endif
			n -= q[o];
		} else o--;
	}

	log_info("userresponse(%u): %s\n", pc, p);
	char *ret = calloc(1, pc+1);
	if (!ret) {
		log_error("failed to calloc(1, %d)\n", pc);
		return NULL;
	} else {
		snprintf(ret, pc+1, "%*s", pc, p);
		return ret;
	}
}

int diff_mouse(int (*m)[3], int mc, int (**n)[3]) {
	int g[3] = { 0, 0, 0 };
	int (*e)[3] = calloc(1, (mc-1)*3*sizeof(int));
	if (!e) {
		log_error("failed to calloc(1, %d*3)", mc-1);
		return -1;
	}

	log_info("input count %d", mc);
	int f = 0;
	int la = 0;
	for (int a=0, b=mc-1; a < b; a++) {
		g[0] = m[a+1][0] - m[a][0];
		g[1] = m[a+1][1] - m[a][1];
		g[2] = m[a+1][2] - m[a][2];
		if (!(g[0] == 0 && g[1] == 0 && g[2] == 0)) {
			if (g[0] == 0 && g[1] == 0) {
				f += g[2];
			} else {
				assert(la < mc);
				e[la][0] = g[0];
				e[la][1] = g[1];
				e[la][2] = g[2] + f;
#ifdef DEBUG
				log_debug("e.push([%d, %d, %d])", e[la][0], e[la][1], e[la][2]);
#endif
				la++;
				f=0;
			}
		}
	}

	if (f != 0) {
		assert(la < mc);
		e[la][0] = g[0];
		e[la][1] = g[1];
		e[la][2] = f;
#ifdef DEBUG
		log_debug("e.lastpush([%d, %d, %d])", e[la][0], e[la][1], e[la][2]);
#endif
		la++;
	}

	*n = e;
	log_info("e.length == %d", la);
	return la;
}

int undiff_mouse(int (*m)[3], int mc, int (**n)[3]) {
	int (*e)[3] = calloc(1, (mc+1)*3*sizeof(int));
	if (!e) {
		log_error("failed to calloc(1, %d*3)", mc+1);
		return -1;
	}

	log_info("input count %d", mc);
	e[0][0] = -m[0][0];
	e[0][1] = -m[0][1];
	e[1][0]=e[1][1]=e[1][2]=0;
#ifdef DEBUG
	log_debug("e.push([%d, %d, %d])", e[0][0], e[0][1], e[0][2]);
	log_debug("e.push([%d, %d, %d])", e[1][0], e[1][1], e[1][2]);
#endif
	for (int a=2, b=mc+1; a < b; a++) {
		e[a][0] = e[a-1][0] + m[a][0];
		e[a][1] = e[a-1][1] + m[a][1];
		e[a][2] = e[a-1][2] + m[a][2];
#ifdef DEBUG
		log_debug("e.push([%d, %d, %d])", e[a][0], e[a][1], e[a][2]);
#endif
	}

	*n = e;
	log_info("e.length == %d", mc+1);
	return mc+1;
}

static char e(int m[]) {
	for (int d = 0, e = sizeof(estr); d < e; d++)
		if (m[0] == barr[d][0] && m[1] == barr[d][1])
			return estr[d];

	return 0;
}

static void d(int a, char *g, int *gc) {
	bool flag = false;
	int c = strlen(dstr);
	unsigned e = abs(a);
	int f = e / c;

	if (f >= c) f = c - 1;
	if (f) flag = true;
	e %= c;

	if (a < 0)
	{
		assert(*gc < MAX_MOUSE_DATA_COUNT);
		*gc += snprintf(&g[*gc], MAX_MOUSE_DATA_COUNT, "!");
	}
	if (flag)
	{
		assert(*gc < MAX_MOUSE_DATA_COUNT);
		*gc += snprintf(&g[*gc], MAX_MOUSE_DATA_COUNT, "$%c", dstr[f]);
	}
	assert(*gc < MAX_MOUSE_DATA_COUNT);
	*gc += snprintf(&g[*gc], MAX_MOUSE_DATA_COUNT, "%c", dstr[e]);
	g[*gc] = 0;
}

char* serialize_mouse(int (*f)[3], int count) {
	log_info("input count %d", count);
	char g[MAX_MOUSE_DATA_COUNT] = {0};
	char h[MAX_MOUSE_DATA_COUNT] = {0};
	char i[MAX_MOUSE_DATA_COUNT] = {0};
	int gc=0, hc=0, ic=0;

	for (int j=0; j < count; j++) {
		char b = e(&f[j][0]);
#ifdef DEBUG
		const char *start = g;
#endif
		if (b) {
			assert(hc < sizeof(h));
			h[hc++] = b;
#ifdef DEBUG
			log_debug("h.push('%c')", b);
#endif
		} else {
			d(f[j][0], g, &gc);
#ifdef DEBUG
			log_debug("g.push(d(f[%d][0]) -> %s)", j, start);
			start = h;
#endif
			d(f[j][1], h, &hc);
#ifdef DEBUG
			log_debug("h.push(d(f[%d][1]) -> %s)", j, start);
			start = i;
#endif
		}

		d(f[j][2], i, &ic);
#ifdef DEBUG
		log_debug("i.push(d(f[%d][2]) -> %s)", j, start);
#endif
	}
	char *r = calloc(1, gc+hc+ic+5);
	if (!r) {
		log_error("failed to calloc(1, %d+%d+%d+5)", gc, hc, ic);
		return 0;
	} else {
		log_info("serialized: %s!!%s!!%s", g, h ,i);
		snprintf(r, gc+hc+ic+5, "%s!!%s!!%s", g, h ,i);
	}
	return r;
}

static int ue(char c) {
	for (int d = 0, e = sizeof(estr); d < e; d++) {
		if (c == estr[d]) return d;
	}
	return -1;
}

static int ud(const char *c, int *bytes) {
	bool negf = false;
	int quo = 0;
	int mod = 0;
	int a = 0;
	if (c[a] == '!') {
		negf = true;
		a++;
	}
	if (c[a] == '$') {
		a++;
		for (int d=0, e=sizeof(dstr); d < e; d++) {
			if (c[a] == dstr[d]) {
				quo = d;
				a++;
				break;
			}
		}
	}
	for (int d=0, e=sizeof(dstr); d < e; d++) {
		if (c[a] == dstr[d]) {
			mod = d;
			a++;
			break;
		}
	}
	int num = quo*sizeof(dstr) + mod;
	num = negf ? -num : num;
	*bytes = a;
	return num;
}

int unserialize_mouse(const char* in, int (**out)[3]) {
	const char *g = in;
	assert(g);
	const char *h = strstr(g, "!!");
	assert(h);
	h += 2;
	const char *i = strstr(h, "!!");
	assert(i);
	i += 2;
#ifdef DEBUG
	log_debug("\ng: %.*s\nh: %.*s\ni: %s", (int)(h-g-2), g, (int)(i-h-2), h, i);
#endif
	int count = 0;

	for (int a=0, f=i-h-2; a < f;) {
		if (ue(h[a]) == -1) {
			int skip = 0;
			ud(&h[a], &skip);
			a+=skip;
			count++;
		} else {
			a++;
			count++;
		}
	}

	if (out != NULL) {
		int (*p)[3] = calloc(1, sizeof(int)*3*count);
		if (!p) {
			log_error("cant calloc(1, sizeof(int)*3*%d)", count);
			exit(1);
		}

		int hc=0, gc=0, ic=0;
		for (int a=0, f=count; a < f; a++) {
			int bs = ue(h[hc]);

			if (bs == -1) {
				int skip=0;
				p[a][0] = ud(&g[gc], &skip);
				gc+=skip;

				skip=0;
				p[a][1] = ud(&h[hc], &skip);
				hc+=skip;

				skip=0;
				p[a][2] = ud(&i[ic], &skip);
				ic+=skip;
			} else {
				int skip=0;
				p[a][0] = barr[bs][0];
				p[a][1] = barr[bs][1];
				p[a][2] = ud(&i[ic], &skip);
				hc++;
				ic+=skip;
			}
#ifdef DEBUG
			log_debug("p.push([ %d, %d, %d ]);", p[a][0], p[a][1], p[a][2]);
#endif
		}

		*out = p;
	}
	log_info("unserialized, count: %d", count);
	return count;
}

int encuricomp(const char *s, char *enc)
{
	if (!encode_i) {
		for (int i = 0; i < 256; i++)
			uricomp[i] = isalnum(i)||i == '-'||i == '_'||i == '.'||i == '!'||i == '~'||i == '*'||i == '\''||i == '('||i== ')' ? i : 0;
		encode_i = true;
	}

	const char *start = enc;
	for (; *s; s++) {
		if (uricomp[(int)*s]) sprintf(enc, "%c", uricomp[(int)*s]);
		else sprintf(enc, "%%%02X", *s);
		while (*++enc);
	}
#ifdef DEBUG
	log_debug("encode uri comp(%lu): %s", enc-start, s);
#endif
	return enc-start;
}

static bool ishex(char x)
{
	return (x >= '0' && x <= '9')	||
		(x >= 'a' && x <= 'f')	||
		(x >= 'A' && x <= 'F');
}

int decuricomp(const char *s, int l, char *dec) {
	return decuri(s, l, dec);
}

int decuri(const char *s, int l, char *dec)
{
	char *o;
	const char *end = s + l;
	int c;

	for (o = dec; s <= end; o++) {
		c = *s++;
		if (c == '%' && ishex(*s++)	&& ishex(*s++))
			sscanf(s - 2, "%2x", &c);
		if (dec) *o = c;
	}
#ifdef DEBUG
	log_debug("decode uri/comp(%lu): %.10s...", o-dec, dec);
#endif
	return o - dec;
}

#define setv(str) \
	do { \
		if (strlen(#str) == t[i].end-t[i].start && strncmp(s+t[i].start, #str, t[i].end-t[i].start) == 0) { \
			snprintf(p->str, sizeof(p->str), "%.*s", t[i+1].end-t[i+1].start, s+t[i+1].start); \
			log_info("p->%s: \"%s\"", #str, p->str); \
		} \
	} while (0);

static size_t set(char *buffer, size_t size, size_t nmemb, login *p) {
	jsmn_parser o;
	jsmntok_t t[128];
	jsmn_init(&o);
	char *s = memchr(buffer, '{', size*nmemb);
	char *e = NULL;
	for (e=buffer+size*nmemb; e >= buffer && *(e-1) != '}'; e--);
	assert(s && e && e != buffer);
	int r = jsmn_parse(&o, s, e-s, t, sizeof(t)/sizeof(t[0]));
	if (r < 0) {
		log_error("Failed to parse JSON: %d", r);
		return 0;
	}
	if (r < 1 || t[0].type != JSMN_OBJECT) {
		log_error("Object excepted %d", r);
		return 0;
	}
	for (int i = 1; i < r; i++) {
		if (t[i].type == JSMN_STRING) {
			setv(gt);
			setv(challenge);
			setv(hash);
			setv(key);
			setv(path);
			setv(id);
			setv(slice);
			setv(bg);
			setv(fullbg);
			setv(ypos);
		}
	}
	return size*nmemb;
}

static size_t set_js(char *buffer, size_t size, size_t nmemb, FILE *fp) {
	if (!data_s) {
		const char *s = NULL;
		const char *e = NULL;
		if (data_i == false) {
			s = memchr(buffer, '"', size*nmemb);
			if (s && strncmp(s-10, "decodeURI(", 10) == 0) {
				s += 1;
				e = memchr(s, '\"', buffer+size*nmemb-s);
			}
		} else e = e ? e : memchr(buffer, '\"', size*nmemb);

		if (!s && !e && data_i == true) {
			assert(sizeof(data) > nmemb*size);
			memcpy(&data[dc], buffer, nmemb*size);
			dc += nmemb*size;
		} else if (s && !e) {
			assert(sizeof(data) > buffer+size*nmemb-s);
			memcpy(&data[dc], s, buffer+size*nmemb-s);
			dc += buffer+size*nmemb-s;
			data_i = true;
#ifdef DEBUG
			log_debug("possible encrypted strings in geetest.js, start to copy")
#endif
		} else if (!s && e) {
			assert(sizeof(data) > dc+e-buffer);
			memcpy(&data[dc], buffer, e-buffer);
			dc += e-buffer;
			data_s = true;
			data_i = false;
#ifdef DEBUG
			log_debug("encrypted strings copied")
#endif
		} else if (s && e) {
			assert(sizeof(data) > e-s);
			memcpy(&data[dc], s, e-s);
			dc += e-s;
			data_s = true;
			data_i = false;
#ifdef DEBUG
			log_debug("possible encrypted strings in geetest.js, copied")
#endif
		}
	}
	if (!dkey_s) {
		char *dtk = memchr(buffer, '}', nmemb*size);
		while (dtk && ((dtk+3-buffer) < nmemb*size)) {
			if (!memcmp(dtk, "}('", 3)) {
				dtk += 3;
				char *detk = memchr(dtk, '\'', 11);
				assert(detk);
				memcpy(data_key, dtk, detk-dtk);
				dkey_s = true;
#ifdef DEBUG
				log_debug("found xor decryption key in geetest.js: %s", data_key);
#endif
				break;
			} else dtk = memchr(dtk+1, '}', nmemb*size);
		}
	}
	return fwrite(buffer, size, nmemb, fp);
}

login* init() {
	login *p = calloc(1, sizeof(login));
	if (!p) {
		log_error("cant allocate struct login");
		exit(1);
	}
	return p;
}

void cleanup(login* p) {
	free(p);
}

int dec_encdata(const char *input, int len, const char *key, char **ostr) { 
	char *out = calloc(1, len+1);
	int rtlen = decuri(input, len, out);
	char *str = calloc(1, rtlen+1);
	int sc = 0;
	for (int f=0,klen=strlen(key); sc < rtlen; f++, sc++) {
		if (f == klen) f=0;
		str[sc] = out[sc] ^ key[f];
	}
	str[sc] = 0;
	free(out);
	*ostr = str;
#ifdef DEBUG
	log_debug("decrypted strings(%d, %d): %.10s...", len, rtlen, str)
#endif
	return rtlen;
}

char* get_encdata() {
	return data;
}

int get_encdata_len() {
	return sizeof(data);
}

char* get_encdata_key() {
	return data_key;
}

static void downres(login *p, const char *workdir, const char *funcname) {
	CURLcode res;
	CURL* curl = curl_easy_init();
	struct curl_slist *slist=NULL;
	slist = curl_slist_append(slist, "Accept: image/apng");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "summer-draw");
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	char url[1024];
	char file[512];

	snprintf(file, sizeof(file), "%s/bg.png", workdir);
	snprintf(url, sizeof(url), "https://static.geetest.com/%s", p->bg);
#ifdef DEBUG
	log_debug("(%s): %s", funcname, url);
#endif
	FILE *fp = fopen(file, "w");
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		log_error("curl_easy_perform() failed: %s", curl_easy_strerror(res));
		exit(1);
	}
	fclose(fp);

	snprintf(file, sizeof(file), "%s/fullbg.png", workdir);
	snprintf(url, sizeof(url), "https://static.geetest.com/%s", p->fullbg);
#ifdef DEBUG
	log_debug("(%s): %s", funcname, url);
#endif
	fp = fopen(file, "w");
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		log_error("curl_easy_perform() failed: %s", curl_easy_strerror(res));
		exit(1);
	}
	fclose(fp);

	snprintf(file, sizeof(file), "%s/slice.png", workdir);
	snprintf(url, sizeof(url), "https://static.geetest.com/%s", p->slice);
#ifdef DEBUG
	log_debug("(%s): %s", funcname, url);
#endif
	fp = fopen(file, "w");
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		log_error("curl_easy_perform() failed: %s", curl_easy_strerror(res));
		exit(1);
	}
	fclose(fp);

	snprintf(file, sizeof(file), "%s/geetest.js", workdir);
	snprintf(url, sizeof(url), "https://static.geetest.com/%s", p->path);
#ifdef DEBUG
	log_debug("(%s): %s", funcname, url);
#endif
	fp = fopen(file, "w");
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, set_js);
	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		log_error("curl_easy_perform() failed: %s", curl_easy_strerror(res));
		exit(1);
	}
	fclose(fp);

	if (dkey_s && data_s) {
		char *str = NULL;
		int len = dec_encdata(data, dc, data_key, &str);
		assert(str);

		if (!off_s) {
			regex_t regex;
			char regex_str[] = "^([0-9]+_)+";
			int reti = regcomp(&regex, regex_str, REG_EXTENDED);
			if (reti) {
				log_error("could not compile regex: %s", regex_str);
			}

			char *src = memchr(str, '}', len);
			assert(src);
			for (src++;regexec(&regex, src, 0, NULL, 0) != 0 && (src=memchr(src, '}', str+len-src));src++);

			if (src && src != str+len) {
				char *end = memchr(src, '}', str+len-src);
				*end = 0;
				log_info("found the key to extract image offset data from geetest.js: %.*s", (int)(end-src), src);
				imgoff(src, NULL);
				off_s = true;
				*end = '}';
			}
			regfree(&regex);
		}

		free(str);
	}

	curl_slist_free_all(slist);
	curl_easy_cleanup(curl);
}

void prepare(login* p, const char* workdir, const char* token_url)
{
	CURLcode res;
	CURL* curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "summer-draw");
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	char url[1024];

	snprintf(url, sizeof(url), token_url, time(NULL));
#ifdef DEBUG
	log_debug("%s", url);
#endif
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, p);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, set);
	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		log_error("curl_easy_perform() failed: %s", curl_easy_strerror(res));
		exit(1);
	}

	snprintf(url, sizeof(url), "https://api.geetest.com/gettype.php?gt=%s&callback=geetest_%lu", p->gt, time(NULL));
#ifdef DEBUG
	log_debug("%s", url);
#endif
	curl_easy_setopt(curl, CURLOPT_URL, url);
	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		log_error("curl_easy_perform() failed: %s", curl_easy_strerror(res));
		exit(1);
	}

	snprintf(url, sizeof(url), "https://api.geetest.com/get.php?gt=%s&challenge=%s&offline=false&product=float&width=100&protocol=https://&path=%s&type=slide&callback=geetest_%lu", p->gt, p->challenge, p->path, time(NULL));
#ifdef DEBUG
	log_debug("%s", url);
#endif
	curl_easy_setopt(curl, CURLOPT_URL, url);
	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		log_error("curl_easy_perform() failed: %s", curl_easy_strerror(res));
		exit(1);
	}

	downres(p, workdir, "prepare");
	curl_easy_cleanup(curl);
}

void refresh(login* p, const char* workdir)
{
	CURLcode res;
	CURL* curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "summer-draw");
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	char url[1024];

	snprintf(url, sizeof(url), "https://api.geetest.com/refresh.php?challenge=%s&gt=%s&callback=geetest_%lu", p->challenge, p->gt, time(NULL));
#ifdef DEBUG
	log_debug("%s", url);
#endif
	curl_easy_setopt(curl, CURLOPT_URL, url);
	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		log_error("curl_easy_perform() failed: %s", curl_easy_strerror(res));
		exit(1);
	}

	downres(p, workdir, "refresh");
	curl_easy_cleanup(curl);
}

void verify(login* p, const char* userresponse, const char* aa, unsigned imgload, unsigned passtime) {
	CURLcode res;
	CURL* curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "summer-draw");
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	char url[512];

	snprintf(url, sizeof(url), "https://api.geetest.com/ajax.php?gt=%s&challenge=%s&userresponse=%s&passtime=%u&imgload=%u&aa=%s&callback=geetest_%lu", p->gt, p->challenge, userresponse, passtime, imgload, aa, time(NULL));
#ifdef DEBUG
	log_debug("%s", url);
#endif
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, stdout);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		log_error("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		exit(1);
	}

	curl_easy_cleanup(curl);
}
#endif

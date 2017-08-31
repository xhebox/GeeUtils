#ifndef UTILS_INCLUDE_H
#define UTILS_INCLUDE_H

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define MAX_MOUSE_DATA_COUNT 512

// from clib/log
#define log_debug(M, ...) fprintf(stderr, "\33[34mDEBUG\33[39m " M " \33[90mat %s() (%s:%d) \33[39m\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__);
#define log_error(M, ...) fprintf(stderr,  "\33[31mERR\33[39m " M " \33[90mat %s() (%s:%d) \33[94merrno: %s\33[39m\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__, errno == 0 ? "None" : strerror(errno));
#define log_info(M, ...) fprintf(stdout, "\33[32mINFO\33[39m " M " \33[90mat %s() (%s:%d) \33[39m\n", ##__VA_ARGS__, __func__, __FILE__, __LINE__);

typedef struct login {
	char hash[64];
	char key[512];
	char gt[64];
	char challenge[64];
	char path[256]; // js path
	char id[64];
	char slice[64];
	char fullbg[64];
	char bg[64];
	char ypos[12];
} login;

void* recover_img(int, int, int, void*);
int find_xpos(const char*, int);
char* enc_xpos(int, const char*);
int diff_mouse(int (*)[3], int, int (**)[3]);
char* proc_mouse(int (*)[3], int);
login* init();
void prepare(login*, const char*);
void verify(login*, const char*, const char*, long, long);
void cleanup(login*);
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
#include "jsmn.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#ifdef DEBUG
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif

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

	if(hsv.s != 0) {
		if (r == hsv.v) hsv.h = (g-b)/delta;
		else if (g == hsv.v) hsv.h = 2 + (b-r)/delta;
		else if (b == hsv.v) hsv.h = 4 + (r-g)/delta;
		hsv.h *= 60;
		if (hsv.h < 0) hsv.h = hsv.h + 360;
	}

	hsv.v /= 255;
	return hsv;
}

uint8_t off[52] = { 39, 38, 48, 49, 41, 40, 46, 47, 35, 34, 50, 51, 33, 32, 28, 29, 27, 26, 36, 37, 31, 30, 44, 45, 43, 42, 12, 13, 23, 22, 14, 15, 21, 20, 8, 9, 25, 24, 6, 7, 3, 2, 0, 1, 11, 10, 4, 5, 19, 18, 16, 17 };

// para: width, height, channels, input array
// return: uint8_t[][3] array, need to free
void* recover_img(int w, int h, int n, void *in) {
	int rw = w-sizeof(off);
	uint8_t *out = calloc(1, rw*h*n);
	if(!out) {
		log_error("failed to calloc(1, %d)", (w-sizeof(off))*h*n);
		return NULL;
	}

	int slice_w = w/26;
	int slice_h = h/2;
	int rsw = slice_w-2;
	for(int a=0; a < sizeof(off); a++) {
		int smod = off[a] % 26;
		int snum = off[a] > 25 ? 1 : 0;
		uint8_t *s = in + snum*slice_h*w*n + smod*slice_w*n + n;

		int dmod = a % 26;
		int dnum = a > 25 ? 1 : 0;
		uint8_t *d = out + dnum*slice_h*rw*n + dmod*rsw*n;
		for(int h=0; h < slice_h; h++, d+=rw*n, s+=w*n) memcpy(d, s, rsw*n);
	}
	return out;
}

// para: workdir path, ypos from server
// return: possible xpos
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
	if(!r) {
		log_error("can not load image %s", bg_path);
		return -1;
	}

	uint8_t (*dbg)[bg_w-sizeof(off)][bg_n] = recover_img(bg_w, bg_h, bg_n, bg);
	if(!dbg) {
		log_error("can not recover image %s", bg_path);
		return -1;
	}
#ifdef DEBUG
	stbi_write_png(dec_bg_path, bg_w-sizeof(off), bg_h, bg_n, &dbg[0][0][0], (bg_w-sizeof(off))*bg_n);
#endif

	int fbg_w=0, fbg_h=0, fbg_n=0;
	r = stbi_load(fbg_path, &fbg_w, &fbg_h, &fbg_n, 3);
	uint8_t	(*fbg)[fbg_w][fbg_n] = (typeof(fbg))r;
	if(!r) {
		log_error("can not load image %s", fbg_path);
		return -1;
	}
	uint8_t (*dfbg)[fbg_w-sizeof(off)][fbg_n] = recover_img(fbg_w, fbg_h, fbg_n, fbg);
	if(!dfbg) {
		log_error("can not recover image %s", fbg_path);
		return -1;
	}
#ifdef DEBUG
	stbi_write_png(dec_fbg_path, fbg_w-sizeof(off), fbg_h, fbg_n, &dfbg[0][0][0], (fbg_w-sizeof(off))*fbg_n);
#endif

	int slice_w=0, slice_h=0, slice_n=0;
	r = stbi_load(slice_path, &slice_w, &slice_h, &slice_n, 4);
	uint8_t (*slice)[slice_w][slice_n] = (typeof(slice))r;
	if(!slice) {
		log_error("can not load image %s", slice_path);
		return -1;
	}
	for(int a=0; a < slice_h; a++) {
		for(int b=0; b < slice_w; b++)
			if(slice[a][b][3] < 255) slice[a][b][3] = 0;
	}
#ifdef DEBUG
	stbi_write_png(dec_slice_path, slice_w, slice_h, slice_n, slice, slice_w*slice_n);
#endif

	unsigned xpos=0;
	double dark=0;
	for(int a=0, d=bg_w-sizeof(off)-slice_w; a < d; a++) {
		double cur=0;
		for(int b=ypos, e=ypos+slice_h; b < e; b++) {
			for(int c=a, f=a+slice_w; c < f; c++) {
					HSV dbg_hsv = rgb_hsv(dbg[b][c][0], dbg[b][c][1], dbg[b][c][2]);
					HSV dfbg_hsv = rgb_hsv(dfbg[b][c][0], dfbg[b][c][1], dfbg[b][c][2]);
					cur+=fabs(dfbg_hsv.v-dbg_hsv.v);
			}
		}
#ifdef DEBUG
		log_debug("xpos %u, most dark weight %f", a, cur);
#endif
		if(cur > dark) {
			dark = cur;
			xpos = a;
		}
	}
	log_info("first loop: initial xpos %u, most dark weight %f", xpos, dark);

	dark=0;
	for(int a=xpos-5, b=xpos+5; a < b; a++) {
		double cur=0;
		for(int y=0, e=slice_h, yd=ypos; y < e; y++, yd++) {
			for(int x=0, f=slice_w, xd=a; x < f; x++, xd++) {
				if(slice[y][x][3]) {
					HSV dbg_hsv = rgb_hsv(dbg[yd][xd][0], dbg[yd][xd][1], dbg[yd][xd][2]);
					HSV dfbg_hsv = rgb_hsv(dfbg[yd][xd][0], dfbg[yd][xd][1], dfbg[yd][xd][2]);
					cur+=fabs(dfbg_hsv.v-dbg_hsv.v);
				}
			}
		}
#ifdef DEBUG
		log_debug("xpos %u, most dark weight %f", a, cur);
#endif
		if(cur > dark) {
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

// para: xpos, challenge token
// return: userresponse string, need to free
char* enc_xpos(int xpos, const char *challenge) {
	log_info("xpos %u, challenge %s", xpos, challenge);
	char d[32] = {0};
	for(int a=0; a < strlen(challenge) - 32; a++) {
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

	char j[128] = {0};
	char i[5][7] = {0};
	int ic[5] = {0};
	for(int a=0, k=0; a < 32; a++) {
		char h = challenge[a];
		if(!j[h]) {
			j[h] = 1;
#ifdef DEBUG
			log_debug("j[%c] = 1\n", h);
#endif

			i[k][ic[k]++] = h;
#ifdef DEBUG
			log_debug("i[%d].push(%c)\n", k, h);
#endif
			if(5 == ++k) k = 0;
		}
	}

	char p[64] = {0};
	int pc = 0;
	int q[5] = { 1, 2, 5, 10, 50 };
	for(int o=4, n=g; n > 0;) {
		if( n - q[o] >= 0 ) {
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
	if(!ret) {
		log_error("failed to calloc(1, %d)\n", pc);
		return NULL;
	} else {
		snprintf(ret, pc+1, "%*s", pc, p);
		return ret;
	}
}

// para: input[x,y,time], array count, output[need to free]
// return: output count
int diff_mouse(int (*m)[3], int mc, int (**n)[3]) {
	int g[3] = { 0, 0, 0 };
	int (*e)[3] = calloc(1, (mc-1)*3*sizeof(int));
	if(!e) {
		log_error("failed to calloc(1, %d*3)", mc-1);
		return -1;
	}

	log_info("input count %d", mc);
	int f = 0;
	int la = 0;
	for(int a=0, b=mc-1; a < b; a++) {
		g[0] = m[a+1][0] - m[a][0];
		g[1] = m[a+1][1] - m[a][1];
		g[2] = m[a+1][2] - m[a][2];
		if(!(g[0] == 0 && g[1] == 0 && g[2] == 0)) {
			if(g[0] == 0 && g[1] == 0) {
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

	if(f != 0) {
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

#define set_ret(n, o) \
	if(ret.n)	{ \
		assert(o##c < sizeof(o)); \
		if(escape(ret.n)) { \
			o[o##c++] = '%'; \
			o##c += snprintf(&o[o##c], sizeof(o[o##c]), "%X", ret.n); \
		} else o[o##c++] = ret.n; \
	}

#define set_dret(o) \
	set_ret(g1, o) \
	set_ret(g2, o) \
	set_ret(d, o) \
	set_ret(n, o)

typedef struct d_ret {
	char d;
	char g1;
	char g2;
	char n;
} d_ret;

static inline bool escape(char a) {
	if(isalnum(a)) return 0;
	if(a == '/'||a == '@'|| a == ','|| a == ':' || a == '$') return 1;
	return 0;
}

static char e(int m[]) {
	char c[] = "stuvwxyz~";
	int b[][2] = {{1, 0}, {2, 0}, {1, -1}, {1, 1}, {0, 1}, {0, -1}, {3, 0}, {2, -1}, {2, 1}};
	for (int d = 0, e = 9; d < e; d++)
		if (m[0] == b[d][0] && m[1] == b[d][1])
			return c[d];

	return 0;
}

static struct d_ret d(int a) {
	d_ret r = { .d=0, .g1=0, .g2=0, .n=0 };
	char b[] = "()*,-./0123456789:?@ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqr";	
	int c = strlen(b);
	unsigned e = abs(a);
	int f = e / c;

	if(f >= c) f = c - 1;
	if(f) r.d = b[f];
	e %= c;

	if(a < 0) r.g1 = '!';
	if(r.d) r.g2 = '$';
	r.n = b[e];
	return r;
}

// para: diff_arr data, diff_arr[count-1] == the last one
// return: string
// need to free
char* proc_mouse(int (*f)[3], int count) {
	log_info("input count %d", count);
	char g[MAX_MOUSE_DATA_COUNT] = {0};
	char h[MAX_MOUSE_DATA_COUNT] = {0};
	char i[MAX_MOUSE_DATA_COUNT] = {0};
	int gc=0, hc=0, ic=0;

	for(int j=0; j < count; j++) {
		char b = e(&f[j][0]);
		if(b) {
			assert(hc < sizeof(h));
			h[hc++] = b;
#ifdef DEBUG
			log_debug("h.push(%c)", b);
#endif
		} else {
			{
				d_ret ret = d(f[j][0]);
				set_dret(g);
#ifdef DEBUG
				log_debug("g.push(d(f[%d][0] == %d) -> %c%c%c%c)", j, f[j][0], ret.g1, ret.g2, ret.d, ret.n);
#endif
			}

			{
				d_ret ret = d(f[j][1]);
				set_dret(h);
#ifdef DEBUG
				fprintf(stdout, "\t[proc_mouse]: h.push(d(f[%d][0] == %d) -> %c%c%c%c)\n", j, f[j][1], ret.g1, ret.g2, ret.d, ret.n);
#endif
			}
		}

		{
			d_ret ret = d(f[j][2]);
			set_dret(i);
#ifdef DEBUG
			fprintf(stdout, "\t[proc_mouse]: i.push(d(f[%d][0] == %d) -> %c%c%c%c)\n", j, f[j][2], ret.g1, ret.g2, ret.d, ret.n);
#endif
		}
	}
	char *r = malloc(gc+hc+ic+5);
	if(!r) {
		log_error("failed to malloc(%d+%d+%d+1)", gc, hc, ic);
		return 0;
	} else {
		log_info("%s!!%s!!%s", g, h ,i);
		snprintf(r, gc+hc+ic+5, "%s!!%s!!%s", g, h ,i);
	}
	return r;
}

#define setv(str) \
	do { \
		jsmn_parser o; \
		jsmntok_t t[128]; \
		jsmn_init(&o); \
		char *s = strstr(buffer, "{\""); \
		assert(s); \
		char *e = strstr(s, "},true)"); \
		e = e ? e + 1 : buffer+nmemb*size; \
		int r = jsmn_parse(&o, s, e-s, t, sizeof(t)/sizeof(t[0])); \
		if (r < 0) { \
			log_error("[setv]: Failed to parse JSON: %d", r); \
			return 1; \
		} \
		if (r < 1 || t[0].type != JSMN_OBJECT) { \
			log_error("[setv]: Object excepted %d", r); \
			return 1; \
		} \
		for (int i = 1; i < r; i++) { \
			if (jsoneq(s, &t[i], #str) == 0) { \
				snprintf(p->str, sizeof(p->str), "%.*s", t[i+1].end-t[i+1].start, s + t[i+1].start); \
				log_info("[setv]: p->%s = \"%s\"", #str, p->str); \
				break; \
			} \
		} \
	} while(0);

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
			strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
		return 0;
	}
	return -1;
}

static size_t set(void *buffer, size_t size, size_t nmemb, login *p) {
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
	return size*nmemb;
}

login* init() {
	login *p = malloc(sizeof(login));
	if(!p) {
		log_error("[init]: cant allocate struct login");
		exit(1);
	}
	return p;
}

void cleanup(login* p) {
	free(p);
}

void prepare(login* p, const char* workdir)
{
	CURLcode res;
	CURL* curl = curl_easy_init();
	struct curl_slist *slist=NULL;
	slist = curl_slist_append(slist, "Accept: image/apng");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "summer-draw");
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	char url[512];
	char file[512];

	snprintf(url, sizeof(url), "https://passport.bilibili.com/captcha/gc?cType=2&ts=%u", time(NULL));
#ifdef DEBUG
	log_debug("[prepare]: %s", url);
#endif
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, p);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, set);
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		log_error("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		exit(1);
	}

	snprintf(url, sizeof(url), "https://api.geetest.com/gettype.php?gt=%s&callback=geetest_%u", p->gt, time(NULL));
#ifdef DEBUG
	log_debug("[prepare]: %s", url);
#endif
	curl_easy_setopt(curl, CURLOPT_URL, url);
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		log_error("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		exit(1);
	}

	snprintf(url, sizeof(url), "https://api.geetest.com/get.php?gt=%s&challenge=%s&offline=false&product=float&width=100%&protocol=&path=%s&type=slide&callback=geetest_%u", p->gt, p->challenge, p->path, time(NULL));
#ifdef DEBUG
	log_debug("[prepare]: %s", url);
#endif
	curl_easy_setopt(curl, CURLOPT_URL, url);
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		log_error("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		exit(1);
	}

	snprintf(file, sizeof(file), "%s/bg.png", workdir);
	snprintf(url, sizeof(url), "https://static.geetest.com/%s", p->bg);
#ifdef DEBUG
	log_debug("[prepare]: %s", url);
#endif
	FILE *fp = fopen(file, "w");
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		log_error("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		exit(1);
	}
	fclose(fp);

	snprintf(file, sizeof(file), "%s/fullbg.png", workdir);
	snprintf(url, sizeof(url), "https://static.geetest.com/%s", p->fullbg);
#ifdef DEBUG
	log_debug("[prepare]: %s", url);
#endif
	fp = fopen(file, "w");
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		log_error("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		exit(1);
	}
	fclose(fp);

	snprintf(file, sizeof(file), "%s/slice.png", workdir);
	snprintf(url, sizeof(url), "https://static.geetest.com/%s", p->slice);
#ifdef DEBUG
	log_debug("[prepare]: %s", url);
#endif
	fp = fopen(file, "w");
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		log_error("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		exit(1);
	}
	fclose(fp);

	curl_slist_free_all(slist);
	curl_easy_cleanup(curl);
}

void verify(login* p, const char* userresponse, const char* a, long imgload, long passtime) {
	CURLcode res;
	CURL* curl = curl_easy_init();
	struct curl_slist *slist=NULL;
	slist = curl_slist_append(slist, "Accept: image/apng");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "summer-draw");
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	char url[512];

	snprintf(url, sizeof(url), "https://api.geetest.com/ajax.php?gt=%s&challenge=%s&userresponse=%s&passtime=%u&imgload=%u&a=%s&callback=geetest_%u", p->gt, p->challenge, userresponse, passtime, imgload, a, time(NULL));
#ifdef DEBUG
	log_debug("[prepare]: %s", url);
#endif
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, stdout);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
	res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		log_error("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		exit(1);
	}

	curl_slist_free_all(slist);
	curl_easy_cleanup(curl);
}
#endif

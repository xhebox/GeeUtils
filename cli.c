#define UTILS_IMPL_H
#include "utils.h"
#include <stdbool.h>
#define streq(A, B) (!strcmp(A, B))

FILE *out;
FILE *in;

void writeimage(void *output, void *data, int size) {
	fwrite(data, size, 1, output); 
}

void set_file(int out, char* fn, FILE** dest) {
	if(streq(fn, "-")) {
		if(out) {
			*dest = stdout;
		} else {
			char b[4096];
			size_t n=0;
			FILE* tmpf = tmpfile();
			if(!tmpf)
				log_error("failed to tmpfile()");

			while((n=fread(b, sizeof(*b), sizeof(b), stdin)) > 0)
				fwrite(b, sizeof(*b), n, tmpf);

			fseek(tmpf, 0, SEEK_SET);
			*dest = tmpf;
		}
	} else {
		*dest = fopen(fn, out ? "w" : "r");
	}
	if(!*dest) {
		log_error("failed to fopen(%s)", fn);
		exit(1);
	}
}

char gt[64] = {0};
char challenge[64] = {0};
int xpos = 0;
int ypos = 0;
char s[14]={0};
int c[9]={0};

static void help() {
		fprintf(stderr, "usage: -i in -o out -gt .. -challenge .. -xpos .. -ypos .. -c .. -s .. -[q/qq] -[diff/serialize/undiff/unserialize/enc_xpos/enc_mouse/dec_mouse/fake_mouse/recover_img/find_xpos]\n");
}

int main(int argc, char *arg[]) {
	int action = -1;
	out = stdout;
	in = stdin;
	debug = false;
	info = false;
	for(int a=1; a < argc; a++) {
		if(arg[a][0] == '-') {
			if(streq(&arg[a][1], "i")) {
				a++;
				set_file(0, &arg[a][0], &in);
			} else if(streq(&arg[a][1], "o")) {
				a++;
				set_file(1, &arg[a][0], &out);
			} else if(streq(&arg[a][1], "gt")) {
				a++;
				assert(strlen(&arg[a][0]) < sizeof(gt));
				memcpy(gt, &arg[a][0], strlen(&arg[a][0]));
			} else if(streq(&arg[a][1], "challenge")) {
				a++;
				assert(strlen(&arg[a][0]) < sizeof(challenge));
				memcpy(challenge, &arg[a][0], strlen(&arg[a][0]));
			} else if(streq(&arg[a][1], "ypos")) {
				a++;
				ypos = atoi(&arg[a][0]);
			} else if(streq(&arg[a][1], "xpos")) {
				a++;
				xpos = atoi(&arg[a][0]);
			} else if(streq(&arg[a][1], "c")) {
				a++;
				jsmn_parser o;
				jsmntok_t t[128];
				jsmn_init(&o);
				char *s = &arg[a][0];
				char *e = s+strlen(&arg[a][0]);
				assert(s && e);
				int r = jsmn_parse(&o, s, e-s, t, sizeof(t)/sizeof(t[0]));
				if (r < 0) {
					log_error("Failed to parse JSON: %d", r);
					return 0;
				}
				if (r < 1 || t[0].type != JSMN_ARRAY) {
					log_error("ARRAY excepted %d", r);
					return 0;
				}
				for (int i=1,d=0; i < r; i++,d++) {
					if (t[i].type == JSMN_PRIMITIVE) {
						assert(d < (sizeof(c)/sizeof(int)));
						c[d] = atoi(s+t[i].start);
					}
				}
			} else if(streq(&arg[a][1], "s")) {
				a++;
				assert(strlen(&arg[a][0]) < sizeof(s));
				memcpy(s, &arg[a][0], strlen(&arg[a][0]));
			} else if(streq(&arg[a][1], "v")) {
				info = true;
			} else if(streq(&arg[a][1], "vv")) {
				info = true;
				debug = true;
			} else if(streq(&arg[a][1], "q")) {
				debug = false;
			} else if(streq(&arg[a][1], "qq")) {
				debug = false;
				info = false;
			} else if(streq(&arg[a][1], "diff")) {
				action = 0;
			} else if(streq(&arg[a][1], "serialize")) {
				action = 1;
			} else if(streq(&arg[a][1], "undiff")) {
				action = 2;
			} else if(streq(&arg[a][1], "unserialize")) {
				action = 3;
			} else if(streq(&arg[a][1], "enc_xpos")) {
				action = 4;
			} else if(streq(&arg[a][1], "enc_mouse")) {
				action = 5;
			} else if(streq(&arg[a][1], "dec_mouse")) {
				action = 6;
			} else if(streq(&arg[a][1], "recover_img")) {
				action = 7;
			} else if(streq(&arg[a][1], "fake_mouse")) {
				action = 8;
			} else if(streq(&arg[a][1], "find_xpos")) {
				action = 9;
			} else {
				action = -1;
			}
		}
	}

	int e[MAX_MOUSE_DATA_COUNT][3];
	switch(action) {
	case 0:
		{
			int a = 0;
			for(;fscanf(in, "%d,%d,%d", &e[a][0], &e[a][1], &e[a][2]) == 3;a++);
			int (*o)[3];
			int len = diff_mouse(&e[0], a, &o);
			for(int c=0; c<len; c++) fprintf(out, "%d,%d,%d\n", o[c][0], o[c][1], o[c][2]);
			free(o);
			break;
		}
	case 1:
		{
			int a = 0;
			for(;fscanf(in, "%d,%d,%d", &e[a][0], &e[a][1], &e[a][2]) == 3;a++);
			char *str = serialize_mouse(&e[0], a);
			fprintf(out, "%s\n", str);
			free(str);
			break;
		}
	case 2:
		{
			int a = 0;
			for(;fscanf(in, "%d,%d,%d", &e[a][0], &e[a][1], &e[a][2]) == 3;a++);
			int (*o)[3];
			int len = undiff_mouse(&e[0], a, &o);
			for(int c=0; c<len; c++) fprintf(out, "%d,%d,%d\n", o[c][0], o[c][1], o[c][2]);
			free(o);
			break;
		}
	case 3:
		{
			char buf[2048] = {0};
			fscanf(in, "%s", buf);
			int (*o)[3];
			int len = unserialize_mouse(buf, &o);
			for(int c=0; c<len; c++) fprintf(out, "%d,%d,%d\n", o[c][0], o[c][1], o[c][2]);
			free(o);
			break;
		}
	case 4:
		{
			assert(xpos && challenge[0]);
			char *str = enc_xpos(xpos, challenge);
			fprintf(out, "%s\n", str);
			free(str);
			break;
		}
	case 5:
		{
			assert(s[0] && c[0]);
			char buf[2048] = {0};
			fscanf(in, "%s", buf);
			char *str = enc_mouse(buf, s, c);
			fprintf(out, "%s\n", str);
			free(str);
			break;
		}
	case 6:
		{
			assert(s[0] && c[0]);
			char buf[2048] = {0};
			fscanf(in, "%s", buf);
			char *str = dec_mouse(buf, s, c);
			fprintf(out, "%s\n", str);
			free(str);
			break;
		}
	case 7:
		{
			assert(in != stdin && out != stdout);
			int bg_n=0,bg_w=0,bg_h=0;
			uint8_t *bg = stbi_load_from_file(in, &bg_w, &bg_h, &bg_n, 4);

			uint8_t (*dbg)[bg_w-sizeof(off_n)][bg_n] = recover_img(bg_w, bg_h, bg_n, bg);
			if (!dbg) {
				log_error("can not recover image");
				return -1;
			}
			stbi_write_png_to_func(writeimage, out, bg_w-sizeof(off_n), bg_h, bg_n, &dbg[0][0][0], (bg_w-sizeof(off_n))*bg_n);
			break;
		}
	case 8:
		{
			int (*o)[3];
			int len = fake_mouse(xpos, &o);
			for(int c=0; c<len; c++) fprintf(out, "%d,%d,%d\n", o[c][0], o[c][1], o[c][2]);
			free(o);
			break;
		}
	case 9:
		{
			assert(ypos);
			int xx = find_xpos(ypos, "bg.jpg", "fullbg.jpg", "slice.png", ".");
			fprintf(out, "xpos: %d\n", xx);
			break;
		}
	default:
		{
			help();
			break;
		}
	}

	if(out) fclose(out);
	if(in) fclose(in);
}

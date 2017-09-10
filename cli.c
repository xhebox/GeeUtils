#define UTILS_IMPL_H
#include "utils.h"
#include <stdbool.h>
#define streq(A, B) (!strcmp(A, B))

FILE *out = NULL;
FILE *in = NULL;

void set_file(int out, char* fn, FILE** dest) {
	if(streq(fn, "-")) {
		log_error("dont support read from stdin or write to stdout");
		exit(1);
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

int main(int argc, char *arg[]) {
	if (argc == 1) {
		printf( "usage: %s -i in -o out -gt .. -challenge .. -xpos .. -q -[diff/serialize/undiff/unserialize/enc_xpos]\n", arg[0] );
	}
	bool flag[5] = {false};
	int action = -1;
	for(int a=1; a < argc; a++) {
		if (flag[0]) {
			set_file(0, &arg[a][0], &in);
			flag[0] = false;
		} else if (flag[1]) {
			set_file(1, &arg[a][0], &out);
			flag[1] = false;
		} else if (flag[2]) {
			assert(strlen(&arg[a][0]) < sizeof(gt));
			memcpy(gt, &arg[a][0], strlen(&arg[a][0]));
			flag[2] = false;
		} else if (flag[3]) {
			assert(strlen(&arg[a][0]) < sizeof(challenge));
			memcpy(challenge, &arg[a][0], strlen(&arg[a][0]));
			flag[3] = false;
		} else if (flag[4]) {
			xpos = atoi(&arg[a][0]);
			flag[4] = false;
		} else if(arg[a][0] == '-') {
			if(streq(&arg[a][1], "i")) {
				flag[0] = true;
			} else if(streq(&arg[a][1], "o")) {
				flag[1] = true;
			} else if(streq(&arg[a][1], "gt")) {
				flag[2] = true;
			} else if(streq(&arg[a][1], "challenge")) {
				flag[3] = true;
			} else if(streq(&arg[a][1], "xpos")) {
				flag[4] = true;
			} else if(streq(&arg[a][1], "q")) {
				quiet = true;
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
			}
		}
	}

	int e[MAX_MOUSE_DATA_COUNT][3];
	int a = 0;
	switch(action) {
	case 0:
		{
			for(;fscanf(in, "%d %d %d", &e[a][0], &e[a][1], &e[a][2]) == 3;a++);
			int (*o)[3];
			int len = diff_mouse(&e[0], a, &o);
			for(int c=0; c<len; c++) fprintf(out, "%d %d %d\n", o[c][0], o[c][1], o[c][2]);
			free(o);
			break;
		}
	case 1:
		{
			for(;fscanf(in, "%d %d %d", &e[a][0], &e[a][1], &e[a][2]) == 3;a++);
			char *str = serialize_mouse(&e[0], a);
			fprintf(out, "%s\n", str);
			free(str);
			break;
		}
	case 2:
		{
			for(;fscanf(in, "%d %d %d", &e[a][0], &e[a][1], &e[a][2]) == 3;a++);
			int (*o)[3];
			int len = undiff_mouse(&e[0], a, &o);
			for(int c=0; c<len; c++) fprintf(out, "%d %d %d\n", o[c][0], o[c][1], o[c][2]);
			free(o);
			break;
		}
	case 3:
		{
			char buf[2048] = {0};
			fscanf(in, "%s", buf);
			int (*o)[3];
			int len = unserialize_mouse(buf, &o);
			for(int c=0; c<len; c++) fprintf(out, "%d %d %d\n", o[c][0], o[c][1], o[c][2]);
			free(o);
			break;
		}
	case 4:
		{
			assert(xpos && gt[0]);
			char *str = enc_xpos(xpos, gt);
			fprintf(out, "%s\n", str);
			free(str);
			break;
		}
	}

	if(out) fclose(out);
	if(in) fclose(in);
}

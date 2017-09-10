#define UTILS_IMPL_H
#include "utils.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int main() {
	srand(time(NULL));

	char workdir[] = "./workdir";
	login* l = init("https://passport.bilibili.com/ajax/miniLogin/minilogin");

	struct stat st = {0};
	if (stat(workdir, &st) == -1) {
		mkdir(workdir, 0700);
	}

	token(l, "https://passport.bilibili.com/captcha/gc?cType=2&ts=%lu");
	prepare(l, workdir);

	//refresh(l, workdir);
	int xpos = find_xpos(get_ypos(l), get_bg(l), get_fullbg(l), get_slice(l), workdir);
	int (*fake)[3];
	int (*diff)[3];
	int fc = fake_mouse(xpos, &fake);
	int dc = diff_mouse(fake, fc, &diff);
	char *ser = serialize_mouse(diff, dc);
	char *rt = enc_mouse(ser, get_s(l), get_c(l));

	char *us = enc_xpos(xpos, get_challenge(l));

	verify(l, us, rt, lround(nearbyint(fake[fc-1][2]/10)), fake[fc-1][2]);
	printf("\n%d\n", xpos);

	free(fake);
	free(diff);
	free(ser);
	free(us);
	free(rt);
	cleanup(l);
}

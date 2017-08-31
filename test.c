#define UTILS_IMPL_H
#include "utils.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#define die(a) fprintf(stderr, "%s\n", a);
int main() {
	srand(time(NULL));

	char workdir[] = "./workdir";
	login* l = init();

	struct stat st = {0};
	if (stat(workdir, &st) == -1) {
		mkdir(workdir, 0700);
	}

	prepare(l, workdir);

	int xpos = find_xpos(workdir, atoi(l->ypos));

	cleanup(l);
}

#define NOB_IMPLEMENTATION
#include "nob.h"
#include <assert.h>

// _DEFAULT_SOURCE definition is not *reallyy* needed. it really only gives me reallocarray() which basically is just realloc... but i like the function and I think its akeen to using calloc over malloc //
#define append_dev(bin, src) nob_cmd_append(&dev, "cc", "--std=c23", "-g", "-D _DEFAULT_SOURCE", "-o", bin, src)
#define append_build(bin, src) nob_cmd_append(&build, "cc", "--std=c23", "-Wall", "-Wextra", "-D _DEFAULT_SOURCE", "-D NDEBUG","-O2", "-fstack-protector-all","-o", bin, src)

int main(int argc, char** argv) {
	NOB_GO_REBUILD_URSELF(argc, argv);

	Nob_Cmd dev = {0};
	append_dev("dev/lsf", "src/ls.c");

	assert(nob_cmd_run_sync(dev));

	Nob_Cmd build = {0};
	append_build("bin/lsf", "src/ls.c");
	
	assert(nob_cmd_run_sync(build));
	return 0;
}
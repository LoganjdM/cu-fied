#define NOB_IMPLEMENTATION
#include "nob.h"

char* cc = "clang";
bool zigbuild(const char* root, const char* name, const bool debug) {
	Nob_Cmd cmd = {0};
	nob_cmd_append(&cmd, "zig", "build-exe", "-I", "src/moving", "-lc", "--cache-dir", ".zig-cache");
	if(!debug) {
		nob_cmd_append(&cmd, "-OReleaseSmall");
		nob_cmd_append(&cmd, "-cflags", "-std=c23", "-fstack-protector-all", "--");
	} else {
		nob_cmd_append(&cmd, "-ODebug");
		nob_cmd_append(&cmd, "-cflags", "-std=c23", "-g", "--");
	}
	bool fail = true;
	nob_cmd_append(&cmd, root, "--name", name);
	if(!(fail=nob_cmd_run_sync_and_reset(&cmd))) return fail;

	// what the fuck zig? //
	nob_cmd_append(&cmd, "mv", name, debug ? "dev/" : "bin/");
	if(!(fail=nob_cmd_run_sync_and_reset(&cmd))) return fail;

	char bin_blob[strlen(name)+2] = {};
	sprintf(bin_blob, "%s.o", name);
	nob_cmd_append(&cmd, "rm", bin_blob);
	return nob_cmd_run_sync(cmd);
}

bool cbuild(const char* src, const char* name, const bool debug) {
	Nob_Cmd cmd = {0};
	nob_cmd_append(&cmd, cc, "--std=c23", "-lm", "-D_DEFAULT_SOURCE");
	char bin[strlen(name) + 4] = {};
	if(!debug) {
		nob_cmd_append(&cmd, "-Wall", "-Wextra", "-O2", "-fstack-protector-all", "-DNDEBUG");
	} else {
		nob_cmd_append(&cmd, "-g"); // https://i.imgflip.com/768lyp.jpg //
	}
	sprintf(bin, debug ? "dev/%s" : "bin/%s", name);
	nob_cmd_append(&cmd, src, "-o", bin);
	return nob_cmd_run_sync(cmd);
}

int main(int argc, char** argv) {
	NOB_GO_REBUILD_URSELF(argc, argv);

	nob_mkdir_if_not_exists("bin");
	nob_mkdir_if_not_exists("dev");
	
	if(argc>1) cc = argv[1];
	for(int8_t debug = true; debug >= false; --debug) {
		assert(cbuild("src/ls.c", "lsf", debug));
		// assert(zigc("-Mroot=src/moving/mv.zig", "mvf", debug));
	}
	return 0;
}
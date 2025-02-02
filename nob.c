#define NOB_IMPLEMENTATION
#include "nob.h"

void build_zig_bin(const char* root, const char* name, bool debug) {
	Nob_Cmd cmd = {0};
	nob_cmd_append(&cmd, "zig", "build-exe", "-I", "src/moving", "-lc", "--cache-dir", ".zig-cache", "-cflags", "-std=c23", "-fstack-protector-all", "--");
	nob_cmd_append(&cmd, debug ? "-ODebug" : "-OReleaseSafe");
	nob_cmd_append(&cmd, root, "--name", name);
	NOB_ASSERT(nob_cmd_run_sync(cmd));
	
	Nob_Cmd mv = {0};
	nob_cmd_append(&mv, "mv", name, debug ? "dev/" : "bin/");
	NOB_ASSERT(nob_cmd_run_sync(mv));

	char blob[strlen(name) + 2] = {};
	sprintf(blob, "%s.o", name);
	
	Nob_Cmd rm_blob = {0};
	nob_cmd_append(&rm_blob, "rm", blob);
	NOB_ASSERT(nob_cmd_run_sync(rm_blob));
}

void build_bin(const char* src, const char* name, bool debug) {
	Nob_Cmd cmd = {0};
	nob_cmd_append(&cmd, "clang", "--std=c23", "-lm", "-D_DEFAULT_SOURCE");
	char bin[strlen(name) + 4] = {}; 
	if(!debug) {
		nob_cmd_append(&cmd, "-Wall", "-Wextra", "-DNDEBUG", "-O2", "-fstack-protector-all");
	} else {
		nob_cmd_append(&cmd, "-g"); // https://i.imgflip.com/768lyp.jpg //
	}
	sprintf(bin, debug ? "dev/%s" : "bin/%s", name);
	nob_cmd_append(&cmd, src, "-o", bin);
	NOB_ASSERT(nob_cmd_run_sync(cmd));
}

int main(int argc, char** argv) {
	NOB_GO_REBUILD_URSELF(argc, argv);

	nob_mkdir_if_not_exists("bin");
	nob_mkdir_if_not_exists("dev");
	
	for(int is_debug = true; is_debug >= false; --is_debug) {
		build_bin("src/ls.c", "lsf", is_debug);
		build_zig_bin("-Mroot=src/moving/mv.zig", "mvf", is_debug);
	}
	return 0;
}
#include <table.h>

table_t* init_filetype_dict(void) {
	table_t* result = ht_create(100);
	// full file names //
		// programming shessie //
	result->put(result, ".gitignore", " ");
	result->put(result, ".github", " ");
	result->put(result, ".git", " ");
	result->put(result, ".config", " ");
	result->put(result, "Config", " ");
	result->put(result, "makefile", " ");
	result->put(result, ".bashrc", " ");
	result->put(result, ".bash_logout", " ");
	result->put(result, ".bash_history", " ");
	result->put(result, ".zshrc", " ");
	result->put(result, ".zshrc_history", " ");
		// Home //
	result->put(result, "Downloads", "󰉍 ");
	result->put(result, "Pictures", "󰉏 ");
	result->put(result, "Videos", "󰉏 ");
	result->put(result, "Music", "󱍙 ");
	result->put(result, "Media", "󰉏 ");
		// config //
	result->put(result, "sxhkdrc", " ");
	result->put(result, ".xinit", " ");
	result->put(result, ".Xauthority", " ");
	result->put(result, "dunstrc", "  ");
	result->put(result, "config", "  ");
	
	// extensions //
		// programming languages //
	result->put(result, "c", " ");
	result->put(result, "cpp", " ");
	result->put(result, "h", " ");
	result->put(result, "hpp", " ");
	result->put(result, "zig", " ");
	result->put(result, "zon", " ");
	result->put(result, "lua", " ");
	result->put(result, "tl", " ");
	result->put(result, "py", " ");
	result->put(result, "js", " ");
	result->put(result, "ts", " ");
	result->put(result, "html", " ");
	result->put(result, "css", " ");
	result->put(result, "cs", " ");
	result->put(result, "csproj", " ");
	result->put(result, "rs", " ");
	result->put(result, "cargo", " ");
	result->put(result, "f", " ");
	result->put(result, "for", " ");
	result->put(result, "f90", " ");
	result->put(result, "f03", " ");
	result->put(result, "f15", " ");
	result->put(result, "adb", " ");
		// shell languages //
	result->put(result, "sh", " ");
	result->put(result, "bash", " ");
	result->put(result, "zsh", " ");
	result->put(result, "fish", " ");
	result->put(result, "ps1", " ");
	result->put(result, "bat", " ");
		// configs //
	result->put(result, "rasi", " ");
	result->put(result, "conf", " ");
	result->put(result, "cfg", " ");
	result->put(result, "ini", " ");
	result->put(result, "toml", " ");
	result->put(result, "yaml", " ");
	result->put(result, "yml", " ");
	result->put(result, "json", " ");
	result->put(result, "jsonc", " ");
	result->put(result, "xml", "󰗀 ");
	result->put(result, "micro", "  ");
		// media //
			// image formats //
	result->put(result, "png", "󰈟 ");
	result->put(result, "avif", "󰈟 ");
	result->put(result, "webp", "󰈟 ");
	result->put(result, "jpeg", "󰈟 ");
	result->put(result, "jpg", "󰈟 ");
	result->put(result, "jpegxl", "󰈟 ");
	result->put(result, "gif", "󰵸 ");
	result->put(result, "apng", "󰈟 ");
	result->put(result, "svg", "󰜡 ");
	result->put(result, "kra", " ");
			// video formats //
	result->put(result, "mp4", "󰈫 ");
	result->put(result, "mkv", "󰈫 ");
	result->put(result, "mov", "󰈫 ");
	result->put(result, "webm", "󰈫 ");
		// binaries //
	result->put(result, "a", " ");
	result->put(result, "dll", " ");
	result->put(result, "exe", "󰨡 ");
	result->put(result, "o", " ");
	result->put(result, "lib", " ");
		// misc //
	result->put(result, "md", " ");
	result->put(result, "markdown", " ");
	result->put(result, "txt", " ");
	result->put(result, "kdenlive", "󰗀 ");
	result->put(result, "log", " ");
	return result;
}

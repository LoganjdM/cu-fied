#include "../ctypes/table.h"

table_t* file_t_map = NULL;
void init_filetype_dict(void) {
	file_t_map = ht_create(100);
	// full file names //
		// programming shessie //
	file_t_map->put(file_t_map, ".gitignore", " ");
	file_t_map->put(file_t_map, ".github", " ");
	file_t_map->put(file_t_map, ".git", " ");
	file_t_map->put(file_t_map, ".config", " ");
	file_t_map->put(file_t_map, "Config", " ");
	file_t_map->put(file_t_map, "makefile", " ");
	file_t_map->put(file_t_map, ".bashrc", " ");
	file_t_map->put(file_t_map, ".bash_logout", " ");
	file_t_map->put(file_t_map, ".bash_history", " ");
	file_t_map->put(file_t_map, ".zshrc", " ");
	file_t_map->put(file_t_map, ".zshrc_history", " ");
		// Home //
	file_t_map->put(file_t_map, "Downloads", "󰉍 ");
	file_t_map->put(file_t_map, "Pictures", "󰉏 ");
	file_t_map->put(file_t_map, "Videos", "󰉏 ");
	file_t_map->put(file_t_map, "Music", "󱍙 ");
	file_t_map->put(file_t_map, "Media", "󰉏 ");
		// config //
	file_t_map->put(file_t_map, "sxhkdrc", " ");
	file_t_map->put(file_t_map, ".xinit", " ");
	file_t_map->put(file_t_map, ".Xauthority", " ");
	file_t_map->put(file_t_map, "dunstrc", "  ");
	file_t_map->put(file_t_map, "config", "  ");
	
	// extensions //
		// programming languages //
	file_t_map->put(file_t_map, "c", " ");
	file_t_map->put(file_t_map, "cpp", " ");
	file_t_map->put(file_t_map, "h", " ");
	file_t_map->put(file_t_map, "hpp", " ");
	file_t_map->put(file_t_map, "zig", " ");
	file_t_map->put(file_t_map, "zon", " ");
	file_t_map->put(file_t_map, "lua", " ");
	file_t_map->put(file_t_map, "tl", " ");
	file_t_map->put(file_t_map, "py", " ");
	file_t_map->put(file_t_map, "js", " ");
	file_t_map->put(file_t_map, "ts", " ");
	file_t_map->put(file_t_map, "html", " ");
	file_t_map->put(file_t_map, "css", " ");
	file_t_map->put(file_t_map, "cs", " ");
	file_t_map->put(file_t_map, "csproj", " ");
	file_t_map->put(file_t_map, "rs", " ");
	file_t_map->put(file_t_map, "cargo", " ");
	file_t_map->put(file_t_map, "f", " ");
	file_t_map->put(file_t_map, "for", " ");
	file_t_map->put(file_t_map, "f90", " ");
	file_t_map->put(file_t_map, "f03", " ");
	file_t_map->put(file_t_map, "f15", " ");
	file_t_map->put(file_t_map, "adb", " ");
		// shell languages //
	file_t_map->put(file_t_map, "sh", " ");
	file_t_map->put(file_t_map, "bash", " ");
	file_t_map->put(file_t_map, "zsh", " ");
	file_t_map->put(file_t_map, "fish", " ");
	file_t_map->put(file_t_map, "ps1", " ");
	file_t_map->put(file_t_map, "bat", " ");
		// configs //
	file_t_map->put(file_t_map, "rasi", " ");
	file_t_map->put(file_t_map, "conf", " ");
	file_t_map->put(file_t_map, "cfg", " ");
	file_t_map->put(file_t_map, "ini", " ");
	file_t_map->put(file_t_map, "toml", " ");
	file_t_map->put(file_t_map, "yaml", " ");
	file_t_map->put(file_t_map, "yml", " ");
	file_t_map->put(file_t_map, "json", " ");
	file_t_map->put(file_t_map, "jsonc", " ");
	file_t_map->put(file_t_map, "xml", "󰗀 ");
	file_t_map->put(file_t_map, "micro", "  ");
		// media //
			// image formats //
	file_t_map->put(file_t_map, "png", "󰈟 ");
	file_t_map->put(file_t_map, "avif", "󰈟 ");
	file_t_map->put(file_t_map, "webp", "󰈟 ");
	file_t_map->put(file_t_map, "jpeg", "󰈟 ");
	file_t_map->put(file_t_map, "jpg", "󰈟 ");
	file_t_map->put(file_t_map, "jpegxl", "󰈟 ");
	file_t_map->put(file_t_map, "gif", "󰵸 ");
	file_t_map->put(file_t_map, "apng", "󰈟 ");
	file_t_map->put(file_t_map, "svg", "󰜡 ");
	file_t_map->put(file_t_map, "kra", " ");
			// video formats //
	file_t_map->put(file_t_map, "mp4", "󰈫 ");
	file_t_map->put(file_t_map, "mkv", "󰈫 ");
	file_t_map->put(file_t_map, "mov", "󰈫 ");
	file_t_map->put(file_t_map, "webm", "󰈫 ");
		// binaries //
	file_t_map->put(file_t_map, "a", " ");
	file_t_map->put(file_t_map, "dll", " ");
	file_t_map->put(file_t_map, "exe", "󰨡 ");
	file_t_map->put(file_t_map, "o", " ");
	file_t_map->put(file_t_map, "lib", " ");
		// misc //
	file_t_map->put(file_t_map, "md", " ");
	file_t_map->put(file_t_map, "markdown", " ");
	file_t_map->put(file_t_map, "txt", " ");
	file_t_map->put(file_t_map, "kdenlive", "󰗀 ");
	file_t_map->put(file_t_map, "log", " ");
}

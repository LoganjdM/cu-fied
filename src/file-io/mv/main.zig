const std = @import("std");
const mem = std.mem;
const Allocator = mem.Allocator;
const clap = @import("clap"); // this is fine for now But i want to eventually get rid of this //
const file_io = @import("file_io");

const params = clap.parseParamsComptime(
    \\-h, --help             Display this help and exit.
    \\<FILE>...
    \\<FILE>
);

const parsers = .{
    .FILE = clap.parsers.string,
};

fn zigStrToC(str: []const u8) [*c]u8 {
    return @ptrCast(@constCast(str));
}
fn getLongestOperand(files: []const []const u8) usize {
	var result: usize = 0;
	for (files) |fname| {
		if(fname.len > result) result = fname.len;
	} return result;
}

fn move(allocator: Allocator, sources: []const []const u8, destination: []const u8) error{OutOfMemory, OperationError}!void {
    var arena = std.heap.ArenaAllocator.init(allocator);
    defer arena.deinit();
    const aAllocator = arena.allocator();

    // Create null-terminated string.
    const dest = try aAllocator.dupeZ(u8, destination);

	const verbose_longest_operand = getLongestOperand(sources);
	var dot_count: u8 = 0;
	const verbose_zig_padding_char = try aAllocator.alloc(u8, verbose_longest_operand);
	@memset(verbose_zig_padding_char, '-');
    const verbose_padding_char: [*c]u8 = @ptrCast(verbose_zig_padding_char);
    
    for (sources) |source| {
        // Create more null-terminated strings.
        const src = try aAllocator.dupeZ(u8, source);
        // TODO: this should follow the coreutils way of doing things and only log this on -v //
		if (dot_count < 3) dot_count += 1 else dot_count -= 2;
		const padding = verbose_longest_operand - src.len;
		// TODO: this probably should be abstracted into file-io.zig to reduce code duplication //
		// printf may as well be its own programming language kek //
        _ = std.c.printf("\"%s\" %.*s--[moving]--> \"%s\"%.*s\n", zigStrToC(src), padding, verbose_padding_char, zigStrToC(dest), dot_count, "...");
        file_io.copy(source, dest, .{ .force = false, .recursive = false, .link = false}) catch return error.OperationError;
        // TODO: remove source //
    }
}

pub fn main() !u8 {
    var gpa: std.heap.DebugAllocator(.{}) = .init;
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var diag = clap.Diagnostic{};
    var res = clap.parse(clap.Help, &params, parsers, .{
        .diagnostic = &diag,
        .allocator = allocator,
    }) catch |err| {
        try diag.report(std.io.getStdErr().writer(), err);
        return 1;
    };
    defer res.deinit();

    if (res.args.help != 0) {
        try clap.help(std.io.getStdErr().writer(), clap.Help, &params, .{});

        return 0;
    }

    const sources = res.positionals[0];
    const destination = res.positionals[1] orelse return error.NoDestination;

    try move(allocator, sources, destination);

    return 0;
}

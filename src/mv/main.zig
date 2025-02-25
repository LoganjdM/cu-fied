const std = @import("std");
const mem = std.mem;
const Allocator = std.mem.Allocator;
const clap = @import("clap");

const params = clap.parseParamsComptime(
    \\-h, --help             Display this help and exit.
    \\<FILE>...
    \\<FILE>
);

const parsers = .{
    .FILE = clap.parsers.string,
};
fn move(allocator: Allocator, sources: []const []const u8, destination: []const u8) error{OutOfMemory}!void {
    var arena = std.heap.ArenaAllocator.init(allocator);
    defer arena.deinit();
    const aAllocator = arena.allocator();

    // Create null-terminated string.
    const dest = try aAllocator.dupeZ(u8, destination);

    for (sources) |source| {
        // Create more null-terminated strings.
        const src = try aAllocator.dupeZ(u8, source);
        _ = std.log.info("mv {s} {s}", .{ src.ptr, dest.ptr });
    }
}

pub fn main() !u8 {
    var gpa = std.heap.DebugAllocator(.{}).init;
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

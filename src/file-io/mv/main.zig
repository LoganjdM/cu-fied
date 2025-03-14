const std = @import("std");
const mem = std.mem;
const Allocator = mem.Allocator;
const file_io = @import("file_io");
const assert = std.debug.assert;
const ArgIterator = std.process.ArgIterator;

const Params = struct {
    help: bool = false,
    version: bool = false,
    verbose: bool = false,
    force: bool = false,
    sources: ?[]const [:0]const u8 = null,
    destination: ?[:0]const u8 = null,
};

fn zigStrToC(str: []const u8) [*c]u8 {
    return @ptrCast(@constCast(str));
}
fn getLongestOperand(files: []const []const u8) usize {
    var result: usize = 0;
    for (files) |fname| {
        if (fname.len > result) result = fname.len;
    }
    return result;
}

fn parseArgs(args: *ArgIterator, allocator: Allocator) error{ OutOfMemory, BadArgs }!Params {
    var positionals = std.ArrayListUnmanaged([:0]const u8){};
    // or iterate over positionals.items[i] != NULL //
    var result: Params = .{};

    while (args.next()) |arg| {
        if (std.mem.eql(u8, arg, "--help")) {
            return Params{ .help = true };
        } else if (std.mem.eql(u8, arg, "-h")) {
            return Params{ .help = true };
        } else if (std.mem.eql(u8, arg, "--version")) {
            return Params{ .version = true };
        } else if (std.mem.eql(u8, arg, "-f")) {
            result.force = true;
        } else if (std.mem.eql(u8, arg, "--force")) {
            result.force = true;
        } else if (std.mem.eql(u8, arg, "-v")) {
            result.verbose = true;
        } else if (std.mem.eql(u8, arg, "--verbose")) {
            result.verbose = true;
        } else if (arg[0] == '-') {
            std.log.warn("Unknown option: {?s}", .{arg});
        } else {
            // FIXME: you handle this incorrectly and leak memory //
            // i'd fix it but I spent god knows how long fucking with coerccing zig types and was going insane //
            // maybe do arena allocator then memcpy on sources and dest? //
            const pos = try positionals.addOne(allocator);
            pos.* = arg;
        }
    }

    if (positionals.items.len < 2) {
        return error.BadArgs;
    }

    const sources = positionals.items[0 .. positionals.items.len - 1];
    const destination = positionals.items[positionals.items.len - 1];

    result.sources = sources;
    result.destination = destination;
    return result;
}

fn move(allocator: Allocator, args: Params) error{ OutOfMemory, OperationError }!void {
    var arena = std.heap.ArenaAllocator.init(allocator);
    defer arena.deinit();
    const aAllocator = arena.allocator();

    // Create null-terminated string.
    const dest = try aAllocator.dupeZ(u8, args.destination.?);

    // couldn't get struct to destruct... struct to destruct... that has a ring to it //
    const verbose_args = try file_io.getPaddingVars(args.sources.?, aAllocator);

    for (args.sources.?) |source| {
        // Create more null-terminated strings.
        const src = try aAllocator.dupeZ(u8, source);

        if (args.verbose) file_io.printf_operation(src, dest, verbose_args.len, verbose_args.str, "moving");
        file_io.copy(source, dest, .{
            .force = args.force,
            .recursive = false,
            .link = false,
        }) catch |err| {
            if (err == error.FileFound) continue; // dont delete the file !! //
            return error.OperationError;
        };
        std.posix.unlink(source) catch return error.OperationError;
    }
}

pub fn main() !u8 {
    var gpa: std.heap.DebugAllocator(.{}) = .init;
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    var args_iter = try std.process.ArgIterator.initWithAllocator(allocator);
    defer args_iter.deinit();

    _ = args_iter.next();
    const args = try parseArgs(&args_iter, allocator);

    if (args.help) {
        try std.io.getStdOut().writer().print(@embedFile("help.txt"), .{});

        return 0;
    }

    if (args.version) {
        try std.io.getStdOut().writer().print(
            \\mvf version {s}
            \\
        , .{"0.0.0"});

        return 0;
    }

    assert(args.sources != null);
    assert(args.destination != null);

    try move(allocator, args);

    return 0;
}

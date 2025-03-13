const std = @import("std");
const mem = std.mem;
const Allocator = mem.Allocator;
const file_io = @import("file_io");
const assert = std.debug.assert;
const ArgIterator = std.process.ArgIterator;

const Params = struct {
    help: bool = false,
    version: bool = false,
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

    while (args.next()) |arg| {
        if (std.mem.eql(u8, arg, "--help")) {
            return Params{ .help = true };
        } else if (std.mem.eql(u8, arg, "-h")) {
            return Params{ .help = true };
        } else if (std.mem.eql(u8, arg, "--version")) {
            return Params{ .version = true };
        } else if (arg[0] == '-') {
            std.log.warn("Unknown option: {?s}", .{arg});
        } else {
            const pos = try positionals.addOne(allocator);
            pos.* = arg;
        }
    }

    if (positionals.items.len < 2) {
        return error.BadArgs;
    }

    const sources = positionals.items[0 .. positionals.items.len - 1];
    const destination = positionals.items[positionals.items.len - 1];

    return Params{
        .sources = sources,
        .destination = destination,
    };
}

fn move(allocator: Allocator, sources: []const []const u8, destination: []const u8) error{ OutOfMemory, OperationError }!void {
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
        file_io.copy(source, dest, .{ .force = false, .recursive = false, .link = false }) catch return error.OperationError;
        // TODO: remove source //
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

    try move(allocator, args.sources.?, args.destination.?);

    return 0;
}

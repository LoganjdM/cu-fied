const std = @import("std");
const assert = std.debug.assert;
const fs = std.fs;
const mem = std.mem;
const Allocator = mem.Allocator;
const process = std.process;
const ArgIterator = process.ArgIterator;
const builtin = @import("builtin");
const native_os = builtin.os.tag;
const options = @import("options");
const file_io = @import("file_io");

const Params = struct {
    help: bool = false,
    version: bool = false,
    verbose: bool = false,
    force: bool = false,
    sources: ?[]const [:0]const u8 = null,
    destination: ?[:0]const u8 = null,

    arena: std.heap.ArenaAllocator,
};

fn getLongestOperand(files: []const []const u8) u64 {
    var result: u64 = 0;
    for (files) |fname| {
        if (fname.len > result) result = fname.len;
    }
    return result;
}

fn parseArgs(args: *ArgIterator, allocator: Allocator) error{ OutOfMemory, BadArgs }!Params {
    var arena = std.heap.ArenaAllocator.init(allocator);
    const aAllocator = arena.allocator();

    var positionals: std.ArrayList([:0]const u8) = .{};
    var result: Params = .{ .arena = arena };

    while (args.next()) |arg| {
        if (std.mem.eql(u8, arg, "--help")) {
            result.help = true;

            return result;
        } else if (std.mem.eql(u8, arg, "-h")) {
            result.help = true;

            return result;
        } else if (std.mem.eql(u8, arg, "--version")) {
            result.version = true;

            return result;
        } else if (std.mem.eql(u8, arg, "-f")) {
            result.force = true;
        } else if (std.mem.eql(u8, arg, "--force")) {
            result.force = true;
        } else if (std.mem.eql(u8, arg, "-v")) {
            result.verbose = true;
        } else if (std.mem.eql(u8, arg, "--verbose")) {
            result.verbose = true;
        } else if (arg[0] == '-') {
            std.log.warn("Unknown option: {s}", .{arg});
        } else {
            try positionals.append(aAllocator, arg);
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

fn move(allocator: Allocator, args: Params) (error{ OutOfMemory, OperationError } || fs.File.OpenError)!void {
    var arena = std.heap.ArenaAllocator.init(allocator);
    defer arena.deinit();
    const aAllocator = arena.allocator();

    // Create null-terminated string.
    const dest = try aAllocator.dupeZ(u8, args.destination.?);

    // couldn't get struct to destruct... struct to destruct... that has a ring to it //
    const padding_vars = try file_io.getPaddingVars(args.sources.?, aAllocator);

    var dot_count: u8 = 0;
    for (args.sources.?) |source| {
        // Create more null-terminated strings.
        const src = try aAllocator.dupeZ(u8, source);

        if (args.verbose) file_io.printfOperation(&dot_count, src, dest, padding_vars, "moving");

        const cwd = fs.cwd();

        file_io.copy(&cwd, source, dest, .{
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
    var debug_allocator: std.heap.DebugAllocator(.{}) = comptime .init;

    const gpa, const is_debug = gpa: {
        if (native_os == .wasi) break :gpa .{ std.heap.wasm_allocator, false };
        break :gpa switch (builtin.mode) {
            .Debug, .ReleaseSafe => .{ debug_allocator.allocator(), true },
            .ReleaseFast, .ReleaseSmall => .{ std.heap.smp_allocator, false },
        };
    };
    defer if (is_debug) assert(debug_allocator.deinit() == .ok);

    var args_iter = try std.process.argsWithAllocator(gpa);
    defer args_iter.deinit();

    _ = args_iter.next();
    const args = try parseArgs(&args_iter, gpa);
    defer args.arena.deinit();

    var stdout_buffer: [1024]u8 = undefined;
    var stdout_writer = std.fs.File.stdout().writer(&stdout_buffer);
    const stdout = &stdout_writer.interface;

    if (args.help) {
        try stdout.print(@embedFile("help.txt"), .{});

        return 0;
    }

    if (args.version) {
        try stdout.print(
            \\mvf version {s}
            \\
        , .{options.version});

        return 0;
    }

    assert(args.sources != null);
    assert(args.destination != null);

    try move(gpa, args);

    return 0;
}

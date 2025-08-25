const std = @import("std");
const assert = std.debug.assert;
const fs = std.fs;
const File = std.fs.File;
const Io = std.Io;
const log = std.log;
const mem = std.mem;
const ArrayList = std.ArrayList;
const Allocator = std.mem.Allocator;
const heap = std.heap;
const ArenaAllocator = std.heap.ArenaAllocator;
const process = std.process;
const ArgIterator = std.process.ArgIterator;
const builtin = @import("builtin");
const native_os = builtin.os.tag;

const file_io = @import("file_io");
const options = @import("options");

const Params = struct {
    help: bool = false,
    version: bool = false,
    verbose: bool = false,
    force: bool = false,
    sources: ?[]const [:0]const u8 = null,
    destination: ?[:0]const u8 = null,

    arena: ArenaAllocator,
};

fn getLongestOperand(files: []const []const u8) u64 {
    var result: u64 = 0;
    for (files) |fname| {
        if (fname.len > result) result = fname.len;
    }
    return result;
}

fn parseArgs(allocator: Allocator, args: *ArgIterator) error{ OutOfMemory, BadArgs }!Params {
    var arena: ArenaAllocator = .init(allocator);
    const aAllocator = arena.allocator();

    var positionals: ArrayList([:0]const u8) = .empty;
    var result: Params = .{ .arena = arena };

    _ = args.next(); // Drop argv[0]

    while (args.next()) |arg| {
        if (mem.eql(u8, arg, "--help")) {
            result.help = true;

            return result;
        } else if (mem.eql(u8, arg, "-h")) {
            result.help = true;

            return result;
        } else if (mem.eql(u8, arg, "--version")) {
            result.version = true;

            return result;
        } else if (mem.eql(u8, arg, "-f")) {
            result.force = true;
        } else if (mem.eql(u8, arg, "--force")) {
            result.force = true;
        } else if (mem.eql(u8, arg, "-v")) {
            result.verbose = true;
        } else if (mem.eql(u8, arg, "--verbose")) {
            result.verbose = true;
        } else if (arg[0] == '-') {
            log.warn("Unknown option: {s}", .{arg});
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

fn move(allocator: Allocator, stderr: *Io.Writer, args: Params) (file_io.OperationError || fs.File.OpenError)!void {
    const dest = args.destination.?;

    const padding = file_io.getPadding(args.sources.?, allocator) catch @panic("OOM!");
    defer allocator.free(padding);

    var dot_count: u8 = 0;
    for (args.sources.?) |source| {
        if (args.verbose) file_io.printfOperation(stderr, &dot_count, source, dest, padding, "moving");

        const cwd = fs.cwd();

        file_io.move(cwd, source, dest, .{
            .force = args.force,
            .recursive = false,
            .link = false,
        }) catch |err| {
            if (err == error.FileFound) continue; // dont delete the file !!

            return err;
        };
    }
}

pub fn main() !u8 {
    var debug_allocator: heap.DebugAllocator(.{}) = comptime .init;

    const allocator, const is_debug = allocator: {
        if (native_os == .wasi) break :allocator .{ heap.wasm_allocator, false };
        break :allocator switch (builtin.mode) {
            .Debug, .ReleaseSafe => .{ debug_allocator.allocator(), true },
            .ReleaseFast, .ReleaseSmall => .{ heap.smp_allocator, false },
        };
    };
    defer if (is_debug) assert(debug_allocator.deinit() == .ok);

    var args_iter = try process.argsWithAllocator(allocator);
    defer args_iter.deinit();

    const args = try parseArgs(allocator, &args_iter);
    defer args.arena.deinit();

    var stdout_buffer: [1024]u8 = undefined;
    var stdout_writer = File.stdout().writer(&stdout_buffer);
    const stdout = &stdout_writer.interface;

    var stderr_buffer: [1024]u8 = undefined;
    var stderr_writer = File.stderr().writer(&stderr_buffer);
    const stderr = &stderr_writer.interface;

    if (args.help) {
        try stdout.print(@embedFile("help.txt"), .{});

        try stdout.flush();

        return 0;
    }

    if (args.version) {
        try stdout.print(
            \\mvf version {s}
            \\
        , .{options.version});

        try stdout.flush();

        return 0;
    }

    assert(args.sources != null);
    assert(args.destination != null);

    try move(allocator, stderr, args);

    return 0;
}

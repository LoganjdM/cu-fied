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
const process = std.process;
const ArgIterator = std.process.ArgIterator;
const builtin = @import("builtin");
const native_os = builtin.os.tag;

const file_io = @import("file_io");
const options = @import("options");

const Params = union(enum) {
    help,
    version,

    operation: OperationParams,

    pub fn deinit(this: @This()) void {
        switch (this) {
            .help => {},
            .version => {},
            .operation => |opargs| {
                opargs.allocator.free(opargs.sources);
            },
        }
    }
};

const Arguments = packed struct {
    recursive: bool = false,
    force: bool = false,
    link: bool = false,
    interactive: bool = false,
    verbose: bool = false,
};

const OperationParams = struct {
    arguments: Arguments,

    sources: [][:0]const u8,
    destination: [:0]const u8,

    allocator: Allocator,
};

fn getLongestOperand(files: []const []const u8) u64 {
    var result: u64 = 0;
    for (files) |fname| {
        if (fname.len > result) result = fname.len;
    }
    return result;
}

fn parseArgs(allocator: Allocator, args: *ArgIterator) error{ OutOfMemory, BadArgs }!Params {
    var positionals: ArrayList([:0]const u8) = .empty;

    var result: Arguments = .{};

    _ = args.next(); // Drop argv[0]

    while (args.next()) |arg| {
        if (mem.eql(u8, arg, "--help")) {
            return .help;
        } else if (mem.eql(u8, arg, "-h")) {
            return .help;
        } else if (mem.eql(u8, arg, "--version")) {
            return .version;
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
            try positionals.append(allocator, arg);
        }
    }

    if (positionals.items.len < 2) {
        return error.BadArgs;
    }

    const dest = positionals.pop().?;

    return .{ .operation = .{
        .arguments = result,
        .sources = try positionals.toOwnedSlice(allocator),
        .destination = dest,

        .allocator = allocator,
    } };
}

fn move(allocator: Allocator, stderr: *Io.Writer, args: OperationParams) (file_io.OperationError || fs.File.OpenError)!void {
    const padding = file_io.getPadding(args.sources, allocator) catch @panic("OOM!");
    defer allocator.free(padding);

    var dot_count: u8 = 0;
    for (args.sources) |source| {
        if (dot_count < 3) dot_count += 1 else dot_count -= 2;

        if (args.arguments.verbose) file_io.printfOperation(stderr, dot_count, source, args.destination, padding, "moving");

        const cwd = fs.cwd();

        file_io.move(cwd, source, args.destination, .{
            .force = args.arguments.force,
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

    var stdout_buffer: [1024]u8 = undefined;
    var stdout_writer = File.stdout().writer(&stdout_buffer);
    const stdout = &stdout_writer.interface;

    var stderr_buffer: [1024]u8 = undefined;
    var stderr_writer = File.stderr().writer(&stderr_buffer);
    const stderr = &stderr_writer.interface;

    var args_iter = try process.argsWithAllocator(allocator);
    defer args_iter.deinit();

    const args = parseArgs(allocator, &args_iter) catch |err| switch (err) {
        error.BadArgs => {
            stderr.print("Bad Arguments!\n", .{}) catch {};
            stderr.flush() catch {};

            process.exit(1);
        },

        error.OutOfMemory => {
            stderr.print("OOM!\n", .{}) catch {};
            stderr.flush() catch {};

            process.exit(1);
        },
    };
    defer args.deinit();

    switch (args) {
        .help => {
            try stdout.writeAll(@embedFile("help.txt"));
            try stdout.flush();

            return 0;
        },
        .version => {
            try stdout.print(
                \\mvf version {s}
                \\
                \\
            , .{options.version});

            try stdout.flush();

            return 0;
        },

        .operation => |opargs| {
            try move(allocator, stderr, opargs);

            return 0;
        },
    }
}

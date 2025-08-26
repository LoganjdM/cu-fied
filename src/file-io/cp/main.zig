const std = @import("std");
const assert = std.debug.assert;
const ArrayList = std.ArrayList;
const log = std.log;
const Io = std.Io;
const mem = std.mem;
const heap = std.heap;
const ArenaAllocator = std.heap.ArenaAllocator;
const process = std.process;
const ArgIterator = std.process.ArgIterator;
const fs = std.fs;
const File = std.fs.File;
const Allocator = mem.Allocator;
const builtin = @import("builtin");
const native_os = builtin.os.tag;

const color = @import("colors");
const AnsiCode = color.AnsiCode;
const file_io = @import("file_io");
const options = @import("options");

const Arguments = packed struct {
    recursive: bool = false,
    force: bool = false,
    link: bool = false,
    interactive: bool = false,
    verbose: bool = false,
};

const Params = struct {
    arguments: Arguments,
    positionals: [][*:0]u8,

    pub fn deinit(this: @This(), allocator: Allocator) void {
        allocator.free(this.positionals);
    }
};

fn isArg(arg: [*:0]const u8, comptime short: []const u8, comptime long: []const u8) bool {
    return mem.eql(u8, arg[0..short.len], short) or mem.eql(u8, arg[0..long.len], long);
}

fn parseArgs(
    allocator: Allocator,
    args: *ArgIterator,
    stdout: *Io.Writer,
) (error{OutOfMemory} || Io.Writer.Error)!Params {
    var arguments: Arguments = .{};
    var positionals: ArrayList([*:0]u8) = .empty;

    _ = args.next(); // Drop argv[0]

    while (args.next()) |arg| {
        if (arg[0] != '-') {
            try positionals.append(allocator, @constCast(arg));
            continue;
        }

        if (isArg(arg, "-r", "--recursive")) {
            arguments.recursive = true;
        } else if (isArg(arg, "-f", "--force")) {
            arguments.force = true;
        } else if (isArg(arg, "-l", "--link")) {
            arguments.link = true;
        } else if (isArg(arg, "-i", "--interactive")) {
            arguments.interactive = true;
        } else if (isArg(arg, "-v", "--verbose")) {
            arguments.verbose = true;
        } else if (isArg(arg, "-h", "--help")) {
            try stdout.writeAll(@embedFile("help.txt"));
            try stdout.flush();

            process.exit(0);
        } else if (isArg(arg, "--version", "--version")) {
            try stdout.print(
                \\cpf version {s}
                \\
            , .{options.version});
            try stdout.flush();

            process.exit(0);
        }
    }

    return Params{
        .arguments = arguments,
        .positionals = try positionals.toOwnedSlice(allocator),
    };
}

fn getLongestOperand(files: [][*:0]u8) u64 {
    var result: u64 = 0;
    for (files) |fname| {
        const flen = mem.len(fname);
        if (flen > result) result = flen;
    }
    return result;
}

pub fn main() u8 {
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
    var stderr_file = File.stderr();
    var stderr_writer = stderr_file.writer(&stderr_buffer);
    const stderr = &stderr_writer.interface;

    var args_iter = process.argsWithAllocator(allocator) catch @panic("OOM!");
    defer args_iter.deinit();

    var args = parseArgs(
        allocator,
        &args_iter,
        stdout,
    ) catch |err| switch (err) {
        error.OutOfMemory => {
            color.print(&stderr_file, AnsiCode.red, "Failed to reallocate memory for extra file arguments!\n", .{});
            return 1;
        },
        else => {
            return 1;
        },
    };
    defer args.deinit(allocator);

    if (args.positionals.len < 2) {
        const msg = if (args.positionals.len == 0) "Missing file arguments!" else "Missing destination file argument!";
        color.print(&stderr_file, AnsiCode.red, "{s}\n", .{msg});

        return 2;
    }

    log.debug("operands:", .{});
    for (args.positionals) |file| {
        log.debug("\t{s}", .{file});
    }

    var positionals = ArrayList([*:0]u8).empty;
    positionals.appendSlice(allocator, args.positionals) catch @panic("OOM!");
    defer positionals.deinit(allocator);

    const dest: []u8 = mem.span(positionals.pop().?);

    const padding = file_io.getPadding(&.{}, allocator) catch @panic("OOM!");
    defer allocator.free(padding);

    var dot_count: u8 = 0;
    for (positionals.items) |file_pointer| {
        if (dot_count < 3) dot_count += 1 else dot_count -= 2;

        const file: []u8 = mem.span(file_pointer);

        if (args.arguments.verbose) file_io.printfOperation(stderr, dot_count, file, dest, padding, "copying");

        const cwd = fs.cwd();

        // GNU source looking function call //
        file_io.copy(cwd, file, dest, .{
            .recursive = args.arguments.recursive,
            .force = args.arguments.force,
            .link = args.arguments.link,
        }) catch |err| {
            color.print(&stderr_file, AnsiCode.red, "Failed to copy file", .{});
            color.print(&stderr_file, AnsiCode.blue, " {s} ", .{file});
            const reason = switch (err) {
                error.AccessDenied => "Do you have permission?",
                error.FileNotFound => "Does it exist?",
                error.BadPathName => "Is it a valid path?",
                error.Unexpected => "Yikes!",
                else => "Oops",
            };

            color.print(&stderr_file, AnsiCode.red, "({s})\n", .{reason});
            continue;
        };
    }

    return 0;
}

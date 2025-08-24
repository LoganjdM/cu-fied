const std = @import("std");
const assert = std.debug.assert;
const ArrayList = std.ArrayList;
const log = std.log;
const mem = std.mem;
const Io = std.Io;
const Allocator = mem.Allocator;
const ArenaAllocator = std.heap.ArenaAllocator;
const process = std.process;
const ArgIterator = process.ArgIterator;
const fs = std.fs;
const File = fs.File;
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
    positionals: ArrayList([*:0]u8),

    arena: ArenaAllocator,
};

fn isArg(arg: [*:0]const u8, comptime short: []const u8, comptime long: []const u8) bool {
    return std.mem.eql(u8, arg[0..short.len], short) or std.mem.eql(u8, arg[0..long.len], long);
}

fn parseArgs(
    allocator: Allocator,
    args: *ArgIterator,
    stdout: *Io.Writer,
) (error{OutOfMemory} || Io.Writer.Error)!Params {
    var arena: std.heap.ArenaAllocator = .init(allocator);
    const aAllocator = arena.allocator();

    var arguments: Arguments = .{};
    var positionals: ArrayList([*:0]u8) = .empty;

    _ = args.next(); // Drop argv[0]

    while (args.next()) |arg| {
        if (arg[0] != '-') {
            try positionals.append(aAllocator, @constCast(arg));
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
            const help_message = @embedFile("help.txt");
            try stdout.print(help_message, .{});
            try stdout.flush();
            std.process.exit(0);
        } else if (isArg(arg, "--version", "--version")) {
            try stdout.print(
                \\cpf version {s}
                \\
            , .{options.version});
            try stdout.flush();

            std.process.exit(0);
        }
    }

    return Params{
        .arguments = arguments,
        .positionals = positionals,

        .arena = arena,
    };
}

fn getLongestOperand(files: [][*:0]u8) u64 {
    var result: u64 = 0;
    for (files) |fname| {
        const flen = std.mem.len(fname);
        if (flen > result) result = flen;
    }
    return result;
}

pub fn main() u8 {
    var debug_allocator: std.heap.DebugAllocator(.{}) = comptime .init;

    const allocator, const is_debug = allocator: {
        if (native_os == .wasi) break :allocator .{ std.heap.wasm_allocator, false };
        break :allocator switch (builtin.mode) {
            .Debug, .ReleaseSafe => .{ debug_allocator.allocator(), true },
            .ReleaseFast, .ReleaseSmall => .{ std.heap.smp_allocator, false },
        };
    };
    defer if (is_debug) assert(debug_allocator.deinit() == .ok);

    var stdout_buffer: [1024]u8 = undefined;
    var stdout_writer = std.fs.File.stdout().writer(&stdout_buffer);
    const stdout = &stdout_writer.interface;

    var stderr_buffer: [1024]u8 = undefined;
    var stderr_file = std.fs.File.stderr();
    var stderr_writer = stderr_file.writer(&stderr_buffer);
    const stderr = &stderr_writer.interface;

    var args_iter = try std.process.argsWithAllocator(allocator);
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
    defer args.arena.deinit();

    if (args.positionals.items.len < 2) {
        const msg = if (args.positionals.items.len == 0) "Missing file arguments!" else "Missing destination file argument!";
        color.print(&stderr_file, color.AnsiCode.red, "{s}\n", .{msg});

        return 2;
    }

    log.debug("operands:", .{});
    for (args.positionals.items) |file| {
        log.debug("\t{s}", .{file});
    }

    const dest: []u8 = allocator.dupe(u8, std.mem.span(args.positionals.pop().?)) catch {
        color.print(&stderr_file, AnsiCode.red, "Failed to allocate memory for destination file argument!\n", .{});
        return 1;
    };
    defer allocator.free(dest);

    const padding_vars = file_io.getPaddingVars(&.{}, allocator);

    var dot_count: u8 = 0;
    for (args.positionals.items) |file_pointer| {
        const file: []u8 = std.mem.span(file_pointer);

        if (args.arguments.verbose) file_io.printfOperation(stderr, &dot_count, file, dest, padding_vars, "moving");

        const cwd = fs.cwd();

        // GNU source looking function call //
        file_io.copy(&cwd, file, dest, .{
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

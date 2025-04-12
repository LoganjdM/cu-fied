const std = @import("std");
const assert = std.debug.assert;
const ArrayListUnmanaged = std.ArrayListUnmanaged;
const log = std.log;
const mem = std.mem;
const Allocator = mem.Allocator;
const process = std.process;
const ArgIterator = process.ArgIterator;
const fs = std.fs;
const File = fs.File;
const builtin = @import("builtin");
const native_os = builtin.os.tag;
const color = @import("colors");
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
    arguments: *Arguments,
    positionals: *ArrayListUnmanaged([*:0]u8),
};

fn isArg(arg: [*:0]const u8, comptime short: []const u8, comptime long: []const u8) bool {
    return std.mem.eql(u8, arg[0..short.len], short) or std.mem.eql(u8, arg[0..long.len], long);
}

fn parseArgs(
    allocator: Allocator,
    args: *ArgIterator,
    stdout: *const File.Writer,
) (error{OutOfMemory} || File.WriteError)!Params {
    var arguments: Arguments = .{};
    var positionals: ArrayListUnmanaged([*:0]u8) = .{};

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
            const help_message = @embedFile("help.txt");
            try stdout.print(help_message, .{});
            std.process.exit(0);
        } else if (isArg(arg, "--version", "--version")) {
            try stdout.print(
                \\cpf version {s}
                \\
            , .{options.version});

            std.process.exit(0);
        }
    }

    return Params{
        .arguments = &arguments,
        .positionals = &positionals,
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

    const gpa, const is_debug = gpa: {
        if (native_os == .wasi) break :gpa .{ std.heap.wasm_allocator, false };
        break :gpa switch (builtin.mode) {
            .Debug, .ReleaseSafe => .{ debug_allocator.allocator(), true },
            .ReleaseFast, .ReleaseSmall => .{ std.heap.smp_allocator, false },
        };
    };
    defer if (is_debug) assert(debug_allocator.deinit() == .ok);

    const stderr = std.io.getStdErr();
    const stdout = std.io.getStdOut();
    const stdout_writer = stdout.writer();

    var args_iter = try std.process.argsWithAllocator(gpa);
    defer args_iter.deinit();

    std.log.debug("args: {s}\n", .{std.os.argv});

    _ = args_iter.next();

    const args = parseArgs(
        gpa,
        &args_iter,
        &stdout_writer,
    ) catch |err| switch (err) {
        error.OutOfMemory => {
            color.print(stderr, color.red, "Failed to reallocate memory for extra file arguments!\n", .{});
            return 1;
        },
        else => {
            return 1;
        },
    };
    defer args.positionals.deinit(gpa);

    if (args.positionals.items.len < 2) {
        const msg = if (args.positionals.items.len == 0) "Missing file arguments!" else "Missing destination file argument!";
        color.print(stderr, color.red, "{s}\n", .{msg});

        return 2;
    }

    log.debug("operands:", .{});
    for (args.positionals.items) |file| {
        log.debug("\t{s}", .{file});
    }

    const dest: []u8 = gpa.dupe(u8, std.mem.span(args.positionals.pop().?)) catch {
        color.print(stderr, color.red, "Failed to allocate memory for destination file argument!\n", .{});
        return 1;
    };
    defer gpa.free(dest);

    // verbose padding //
    // kinda ugly vars //
    var verbose_longest_operand: u64 = undefined;
    var verbose_zig_padding_char: []u8 = undefined;
    var verbose_padding_char: [*c]u8 = undefined;
    if (args.arguments.verbose) {
        verbose_longest_operand = getLongestOperand(args.positionals.items);
        verbose_zig_padding_char = gpa.alloc(u8, verbose_longest_operand) catch {
            color.print(stderr, color.red, "Failed to allocate memory for verbose padding char!\n", .{});
            return 1;
        };
        @memset(verbose_zig_padding_char, '-');
        verbose_padding_char = @ptrCast(verbose_zig_padding_char);
    }
    defer if (args.arguments.verbose) gpa.free(verbose_zig_padding_char);

    var dot_count: u8 = 0;
    for (args.positionals.items) |file_slice| {
        // these *:0 are really annoying so do it the c way of looking for \0 //
        const file: []u8 = gpa.alloc(u8, std.mem.len(file_slice)) catch {
            color.print(stderr, color.red, "Failed to allocate memory for source file argument!\n", .{});
            continue;
        };
        @memcpy(file, file_slice);
        defer gpa.free(file);

        if (args.arguments.verbose) {
            if (dot_count < 3) dot_count += 1 else dot_count -= 2;
            const padding = (verbose_longest_operand - file.len);
            // printf may as well be its own programming language kek //
            _ = std.c.printf("\"%s\" %.*s--[copying]--> \"%s\"%.*s\n", file_io.zigStrToCStr(file), padding, verbose_padding_char, file_io.zigStrToCStr(dest), dot_count, "...");
        }

        // GNU source looking function call //
        file_io.copy(file, dest, .{
            .recursive = args.arguments.recursive,
            .force = args.arguments.force,
            .link = args.arguments.link,
        }) catch |err| {
            color.print(stderr, color.red, "Failed to copy file ", .{});
            color.print(stderr, color.blue, " {s}", .{file});
            const reason = switch (err) {
                error.AccessDenied => "Do you have permission?",
                error.FileNotFound => "does it exist?",
                error.BadPathName => "is it a valid path?",
                else => "Oops",
            };

            color.print(stderr, color.red, "({s})\n", .{reason});
            continue;
        };
    }

    return 0;
}

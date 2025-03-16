const std = @import("std");
const log = std.log;
const mem = std.mem;
const Allocator = mem.Allocator;
const fs = std.fs;
const File = fs.File;
const color = @import("colors");
const file_io = @import("file_io");
const options = @import("options");

const Params = packed struct {
    recursive: bool = false,
    force: bool = false,
    link: bool = false,
    interactive: bool = false,
    verbose: bool = false,
};

fn isArg(arg: [*:0]const u8, comptime short: []const u8, comptime long: []const u8) bool {
    return std.mem.eql(u8, arg[0..short.len], short) or std.mem.eql(u8, arg[0..long.len], long);
}

fn parseArgs(argv: [][*:0]u8, allocator: Allocator, stdout: *const File.Writer, files: *std.ArrayListUnmanaged([*:0]u8)) (error{ OutOfMemory, InvalidArgument } || File.WriteError)!Params {
    var args: Params = .{};

    for (argv, 0..) |arg, i| {
        if (i == 0) continue; // skip exec

        if (arg[0] != '-') {
            try files.append(allocator, arg);
            continue;
        }

        if (isArg(arg, "-r", "--recursive")) {
            args.recursive = true;
        } else if (isArg(arg, "-f", "--force")) {
            args.force = true;
        } else if (isArg(arg, "-l", "--link")) {
            args.link = true;
        } else if (isArg(arg, "-i", "--interactive")) {
            args.interactive = true;
        } else if (isArg(arg, "-v", "--verbose")) {
            args.verbose = true;
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
    if (files.items.len < 2) return error.InvalidArgument;

    return args;
}

fn getLongestOperand(files: [][*:0]u8) u64 {
    var result: u64 = 0;
    for (files) |fname| {
        const flen = std.mem.len(fname);
        if (flen > result) result = flen;
    }
    return result;
}

fn zigStrToC(str: []u8) [*c]u8 {
    return @ptrCast(str);
}

pub fn main() u8 {
    var gpa: std.heap.DebugAllocator(.{}) = .init;
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const stderr = std.io.getStdErr();
    const stdout = std.io.getStdOut();
    const stdout_writer = stdout.writer();

    var files = std.ArrayListUnmanaged([*:0]u8){};
    defer files.deinit(allocator);

    const args = parseArgs(std.os.argv, allocator, &stdout_writer, &files) catch |err| {
        if (err == error.InvalidArgument) {
            const msg = if (files.items.len == 0) "Missing file arguments!" else "Missing destination file argument!";
            color.print(stderr, color.red, "{s}\n", .{msg});
            return 2;
        } else {
            color.print(stderr, color.red, "Failed to reallocate memory for extra file arguments!\n", .{});
            return 1;
        }
    };

    log.debug("operands:\n", .{});
    for (files.items) |file| {
        log.debug("\t{s}\n", .{file});
    }

    const dest: []u8 = allocator.dupe(u8, std.mem.span(files.pop().?)) catch {
        color.print(stderr, color.red, "Failed to allocate memory for destination file argument!\n", .{});
        return 1;
    };
    defer allocator.free(dest);

    // verbose padding //
    // kinda ugly vars //
    var verbose_longest_operand: u64 = undefined;
    var verbose_zig_padding_char: []u8 = undefined;
    var verbose_padding_char: [*c]u8 = undefined;
    if (args.verbose) {
        verbose_longest_operand = getLongestOperand(files.items);
        verbose_zig_padding_char = allocator.alloc(u8, verbose_longest_operand) catch {
            color.print(stderr, color.red, "Failed to allocate memory for verbose padding char!\n", .{});
            return 1;
        };
        @memset(verbose_zig_padding_char, '-');
        verbose_padding_char = @ptrCast(verbose_zig_padding_char);
    }
    defer if (args.verbose) allocator.free(verbose_zig_padding_char);

    var dot_count: u8 = 0;
    for (files.items) |file_slice| {
        // these *:0 are really annoying so do it the c way of looking for \0 //
        const file: []u8 = allocator.alloc(u8, std.mem.len(file_slice)) catch {
            color.print(stderr, color.red, "Failed to allocate memory for source file argument!\n", .{});
            continue;
        };
        @memcpy(file, file_slice);
        defer allocator.free(file);

        if (args.verbose) {
            if (dot_count < 3) dot_count += 1 else dot_count -= 2;
            const padding = (verbose_longest_operand - file.len);
            // printf may as well be its own programming language kek //
            _ = std.c.printf("\"%s\" %.*s--[copying]--> \"%s\"%.*s\n", zigStrToC(file), padding, verbose_padding_char, zigStrToC(dest), dot_count, "...");
        }

        // GNU source looking function call //
        file_io.copy(file, dest, .{
            .recursive = args.recursive,
            .force = args.force,
            .link = args.link,
        }) catch |err| {
            color.print(stderr, color.red, "Failed to copy file ", .{});
            color.print(stderr, color.blue, " {s}", .{file});
            const reason = blk: {
                if (err == error.PermissionDenied) break :blk "Do you have permission?";
                if (err == error.FileNotFound) break :blk "does it exist?";
                if (err == error.BadPathName) break :blk "is it a valid path?";
                break :blk err;
            };
            color.print(stderr, color.red, "({any})\n", .{reason});
            continue;
        };
    }
    return 0;
}

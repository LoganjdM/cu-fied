const std = @import("std");
const log = std.log;
const stdout = std.io.getStdOut();
const stderr = std.io.getStdErr();
const color = @import("colors");
const file_io = @import("file_io");
const fs = std.fs;
// const zon = @import("../../../build.zig.zon"); // how get .version tag on `build.zig.zon`?

const Params = packed struct {
    recursive: bool,
    force: bool,
    link: bool,
    interactive: bool,
    verbose: bool,

    pub const init: Params = .{
        .recursive = false,
        .force = false,
        .link = false,
        .interactive = false,
        .verbose = false,
    };
};

fn isArg(arg: [*:0]const u8, comptime short: []const u8, comptime long: []const u8) bool {
    return std.mem.eql(u8, arg[0..short.len], short) or std.mem.eql(u8, arg[0..long.len], long);
}

fn parseArgs(argv: [][*:0]u8, files: *std.ArrayListAligned([*:0]u8, null)) error{ OutOfMemory, InvalidArgument }!Params {
    var args: Params = .init;

    for (argv, 0..) |arg, i| {
        if (i == 0) continue; // skip exec

        if (arg[0] != '-') {
            try files.append(arg);
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
            nosuspend stdout.writer().print(help_message, .{}) catch continue; // eh dont give a shi, we are closing anyways //
            std.process.exit(0);
        } else if (isArg(arg, "-v", "--version")) {
            // stdout.print("{i}", .{.version}); // how get .version tag on `build.zig.zon`?
            std.process.exit(0);
        }
    }
    if (files.items.len < 2) return error.InvalidArgument;

    return args;
}

fn getLongestOperand(files: [][*:0]u8) usize {
    var result: usize = 0;
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

    var files = std.ArrayList([*:0]u8).init(allocator);
    defer files.deinit();

    const args = parseArgs(std.os.argv, &files) catch |err| {
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

    const dest: []u8 = allocator.alloc(u8, files.items.len + 1) catch {
        color.print(stderr, color.red, "Failed to allocate memory for destination file argument!\n", .{});
        return 1;
    };
    @memcpy(dest, files.pop().?);
    defer allocator.free(dest);

    // verbose padding //
    // kinda ugly vars //
    var verbose_longest_operand: usize = undefined;
    var verbose_zig_padding_char: []u8 = undefined;
    var verbose_padding_char: [*c]u8 = undefined;
    if (args.verbose) {
        verbose_longest_operand = getLongestOperand(files.items);
        verbose_zig_padding_char  = allocator.alloc(u8, verbose_longest_operand) catch {
            color.print(stderr, color.red, "Failed to allocate memory for verbose padding char!\n", .{});
            return 1;
        };
        @memset(verbose_zig_padding_char, '-');
        verbose_padding_char = @ptrCast(verbose_zig_padding_char);
    } defer {
        if (args.verbose) allocator.free(verbose_zig_padding_char);
    }

    var dot_count: u8 = 0;
    for (files.items) |file_slice| {
        // these *:0 are really fucking annoying so do it the c way of looking for \0 //
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

        // does this file exist? //
        fs.cwd().access(file, .{ .mode = .read_only }) catch |err| {
            if (!args.verbose) continue;

            color.print(stderr, color.yellow, "Failed to open", .{});
            color.print(stderr, color.blue, " {s}", .{file});
            const reason = blk: {
                if (err == error.PermissionDenied) break :blk " (Do you have permission?)";
                if (err == error.FileNotFound) break :blk " (does it exist?)";
                if (err == error.BadPathName) break :blk " (is it a valid path?)";
                break :blk "!";
            };
            color.print(stderr, color.yellow, "{s}\n", .{reason});
            continue;
        };

        file_io.move(file, dest, .{
            .force = args.force,
            .recursive = args.recursive,
            .link = args.link,
        }) catch |err| color.print(stderr, color.red, "Unexpected error! ({})\n", .{err});
    }
    return 0;
}

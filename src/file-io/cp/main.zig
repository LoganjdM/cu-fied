const std = @import("std");
const log = std.log;
const stdout = std.io.getStdOut();
const stderr = std.io.getStdErr();
const color = @import("colors");
const IsDebug = (@import("builtin").mode == std.builtin.OptimizeMode.Debug);
const debug = std.debug;
// const zon = @import("../../../build.zig.zon"); // how get .version tag on `build.zig.zon`?

const params = enum(u16) {
    recursive = 0b1,
    force = 0b10,
    link = 0b100,
    interactive = 0b1000,
    verbose = 0b10000,
};

fn isArg(arg: [*:0]const u8, comptime short: []const u8, comptime long: []const u8) bool {
    return std.mem.eql(u8, arg[0..short.len], short) or std.mem.eql(u8, arg[0..long.len], long);
}

fn parseArgs(argv: [][*:0]u8, files: *std.ArrayListAligned([*:0]u8, null)) error{ OutOfMemory, Einval }!u16 {
    var args: u16 = 0b0;

    for (argv, 0..) |arg, i| {
        if (i == 0) continue; // skip exec

        if (arg[0] != '-') {
            try files.append(arg);
            continue;
        }

        if (isArg(arg, "-r", "--recursive")) {
            args |= @intFromEnum(params.recursive);
        } else if (isArg(arg, "-f", "--force")) {
            args |= @intFromEnum(params.force);
        } else if (isArg(arg, "-l", "--link")) {
            args |= @intFromEnum(params.link);
        } else if (isArg(arg, "-i", "--interactive")) {
            args |= @intFromEnum(params.interactive);
        } else if (isArg(arg, "-v", "--verbose")) {
            args |= @intFromEnum(params.verbose);
        } else if (isArg(arg, "-h", "--help")) {
            const help_message = @embedFile("help.txt");
            nosuspend stdout.writer().print(help_message, .{}) catch continue; // eh dont give a shi, we are closing anyways //
            std.process.exit(0);
        } else if (isArg(arg, "-v", "--version")) {
            // stdout.print("{i}", .{.version}); // how get .version tag on `build.zig.zon`?
            std.process.exit(0);
        }
    }
    if (files.items.len < 2) return error.Einval;

    return args;
}

pub fn main() u8 {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    const gpa_alloc = gpa.allocator();
    defer _ = gpa.deinit();

    var files = std.ArrayList([*:0]u8).init(gpa_alloc);
    defer files.deinit();

    const args = parseArgs(std.os.argv, &files) catch |err| {
        if (err == error.Einval) {
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
    color.print(stdout, color.green, "{}", .{args});
    return 0;
}

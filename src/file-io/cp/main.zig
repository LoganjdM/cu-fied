const std = @import("std");
const stdout = std.io.getStdOut().writer();
const color = @import("colors");
const IsDebug = (@import("builtin").mode == std.builtin.OptimizeMode.Debug);
const debug = std.debug;
// const zon = @import("../../../build.zig.zon"); // how get .version tag on `build.zig.zon`?

var args: u16 = 0b0;
const params = enum(u16) {
    recursive = 0b1,
    force = 0b10,
    link = 0b100,
    interactive = 0b1000,
    verbose = 0b10000,
};

fn is_arg(arg: [*:0]const u8, comptime short: []const u8, comptime long: []const u8) bool {
    return std.mem.eql(u8, arg[0..short.len], short) or std.mem.eql(u8, arg[0..long.len], long);
}

fn parse_args(argv: [][*:0]u8, files: *std.ArrayListAligned([*:0]u8, null)) error{ OutOfMemory, Einval }!void {
    for (argv, 0..) |arg, i| {
        if (i == 0) continue; // skip exec

        if (arg[0] != '-') {
            try files.append(arg);
            continue;
        }

        if (is_arg(arg, "-r", "--recursive")) {
            args |= @intFromEnum(params.recursive);
        } else if (is_arg(arg, "-f", "--force")) {
            args |= @intFromEnum(params.force);
        } else if (is_arg(arg, "-l", "--link")) {
            args |= @intFromEnum(params.link);
        } else if (is_arg(arg, "-i", "--interactive")) {
            args |= @intFromEnum(params.interactive);
        } else if (is_arg(arg, "-v", "--verbose")) {
            args |= @intFromEnum(params.verbose);
        } else if (is_arg(arg, "-h", "--help")) {
            const help_message = @embedFile("help.txt");
            nosuspend stdout.print(help_message, .{}) catch continue; // eh dont give a shi, we are closing anyways //
            std.process.exit(0);
        } else if (is_arg(arg, "-v", "--version")) {
            // stdout.print("{i}", .{.version}); // how get .version tag on `build.zig.zon`?
            std.process.exit(0);
        }
    }
    if (files.items.len < 2) return error.Einval;
}

pub fn main() u8 {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    const gpa_alloc = gpa.allocator();
    defer {
        _ = gpa.deinit();
    }

    var files = std.ArrayList([*:0]u8).init(gpa_alloc);
    defer files.deinit();

    parse_args(std.os.argv, &files) catch |err| {
        if (err == error.Einval) {
            const msg = if (files.items.len == 0) "Missing file arguments!" else "Missing destination file argument!";
            color.print(.stderr, color.red, "{s}\n", .{msg});
            return 2;
        } else {
            color.print(.stderr, color.red, "Failed to reallocate memory for extra file aguments!\n", .{});
            return 1;
        }
    };
    // I miss C conditional compiling but this works IG :/ //
    if (IsDebug) {
        debug.print("operands:\n", .{});
        for (files.items) |file| {
            debug.print("\t{s}\n", .{file});
        }
        debug.print("args: {b}\n", .{args});
    }
    return 0;
}

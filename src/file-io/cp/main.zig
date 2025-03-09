const std = @import("std");
const io = std.io;
const color = @import("colors");
const builtin = @import("builtin");
// const zon = @import("../../../build.zig.zon");

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

fn parse_args(argv: [][*:0]u8) void {
    for (argv) |arg| {
        if (arg[0] != '-') continue;

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
            nosuspend io.getStdOut().writer().print(help_message, .{}) catch continue; // eh dont give a shi //
            std.process.exit(0);
        } else if (is_arg(arg, "-v", "--version")) {
            // io.getStdOut().writer().print("{i}", .{.version}); // how get .version tag on `build.zig.zon`?
            std.process.exit(0);
        }
    }
}

pub fn main() !void {
    parse_args(std.os.argv);
    // std.debug.print("{b}\n", .{args});
    color.print_escape_code(.stdout, color.green, "Hello, {s}!\n", .{"World"});
}

const std = @import("std");
const io = std.io;
const debug = std.debug;
const c = @cImport({
    @cInclude("stdbool.h");
    @cInclude("colors.h");
});

pub const red = c.RED;
pub const blue = c.BLUE;
pub const green = c.GREEN;
pub const yellow = c.YELLOW;
pub const magenta = c.MAGENTA;

pub const bold = c.BOLD;
pub const italic = c.ITALIC;

pub const reset = c.RESET;

// based off std.debug.print() source code //
// https://ziglang.org/documentation/0.14.0/std/#src/std/debug.zig //
pub fn print_escape_code(comptime stream: enum { stdout, stderr }, ansi: *const [5:0]u8, comptime fmt: []const u8, va_args: anytype) void {
    if (stream == .stdout) {
        const zig_stdout = io.getStdOut().writer();

        nosuspend zig_stdout.print("{s}", .{c.escape_code(c.stdout, ansi)}) catch return;
        nosuspend zig_stdout.print(fmt, va_args) catch return;
    } else {
        _ = c.print_escape_code(c.stderr, ansi);
        debug.print(fmt, va_args);
    }
}

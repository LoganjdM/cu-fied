const std = @import("std");
const c = @import("colors");

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
pub fn print(stream: std.fs.File, ansi: *const [5:0]u8, comptime fmt: []const u8, va_args: anytype) void {
    nosuspend stream.writer().print("{s}", .{c.get_escape_code(stream.handle, ansi)}) catch return;
    nosuspend stream.writer().print(fmt, va_args) catch return;
    nosuspend stream.writer().print("{s}", .{c.get_escape_code(stream.handle, reset)}) catch return;
}

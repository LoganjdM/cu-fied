const std = @import("std");
const Io = std.Io;
const File = std.fs.File;
const tty = std.Io.tty;

// based off std.debug.print() source code //
// https://ziglang.org/documentation/0.14.0/std/#src/std/debug.zig //
pub fn print(writer: *Io.Writer, ansiConfig: tty.Config, ansi: tty.Color, comptime fmt: []const u8, va_args: anytype) void {
    nosuspend ansiConfig.setColor(writer, ansi) catch return;
    nosuspend writer.print(fmt, va_args) catch return;
    nosuspend ansiConfig.setColor(writer, tty.Color.reset) catch return;

    // This is the lazy way...
    nosuspend writer.flush() catch return;
}

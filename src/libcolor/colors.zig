const std = @import("std");

pub const AnsiCode = enum {
    red,
    blue,
    cyan,
    green,
    yellow,
    magenta,

    bold,
    italic,

    reset,
};

fn ansiCodeToString(ansi: AnsiCode) []const u8 {
    return switch (ansi) {
        .red => "\x1b[31m",
        .blue => "\x1b[34m",
        .cyan => "\x1b[36m",
        .green => "\x1b[32m",
        .yellow => "\x1b[33m",
        .magenta => "\x1b[35m",

        .bold => "\x1b[1m",
        .italic => "\x1b[3m",

        .reset => "\x1b[0m",
    };
}

pub fn getEscapeCode(ansi: AnsiCode, stream: *std.fs.File) ?[]const u8 {
    if (stream.getOrEnableAnsiEscapeSupport()) {
        return ansiCodeToString(ansi);
    }

    return null;
}

// based off std.debug.print() source code //
// https://ziglang.org/documentation/0.14.0/std/#src/std/debug.zig //
pub fn print(stream: *std.fs.File, ansi: AnsiCode, comptime fmt: []const u8, va_args: anytype) void {
    var buffer: [1024]u8 = undefined;
    var writer = stream.writer(&buffer).interface;

    nosuspend writer.print("{s}", .{getEscapeCode(ansi, stream) orelse ""}) catch return;
    nosuspend writer.print(fmt, va_args) catch return;
    nosuspend writer.print("{s}", .{getEscapeCode(AnsiCode.reset, stream) orelse ""}) catch return;

    // This is the lazy way...
    nosuspend writer.flush() catch return;
}

const std = @import("std");
const Io = std.Io;
const File = std.fs.File;

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

    pub fn format(this: @This(), writer: *Io.Writer) Io.Writer.Error!void {
        try writer.writeAll(switch (this) {
            .red => "\x1b[31m",
            .blue => "\x1b[34m",
            .cyan => "\x1b[36m",
            .green => "\x1b[32m",
            .yellow => "\x1b[33m",
            .magenta => "\x1b[35m",

            .bold => "\x1b[1m",
            .italic => "\x1b[3m",

            .reset => "\x1b[0m",
        });
    }
};

pub fn writeEscapeCode(ansi: AnsiCode, stream: File, writer: *Io.Writer) void {
    if (stream.getOrEnableAnsiEscapeSupport()) {
        ansi.format(writer) catch return;
    }
}

// based off std.debug.print() source code //
// https://ziglang.org/documentation/0.14.0/std/#src/std/debug.zig //
pub fn print(stream: File, ansi: AnsiCode, comptime fmt: []const u8, va_args: anytype) void {
    var buffer: [1024]u8 = undefined;
    var stream_writer = stream.writer(&buffer);
    var writer = &stream_writer.interface;

    nosuspend writeEscapeCode(ansi, stream, writer);
    nosuspend writer.print(fmt, va_args) catch return;
    nosuspend writeEscapeCode(AnsiCode.reset, stream, writer);

    // This is the lazy way...
    nosuspend writer.flush() catch return;
}

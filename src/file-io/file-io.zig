const std = @import("std");
const Io = std.Io;
const fs = std.fs;
const posix = std.posix;

pub const OperationSettings = packed struct {
    force: bool,
    recursive: bool,
    link: bool,
};

pub const OperationError = error{
    ReadOnlyFileSystem,
    SystemResources,
    AccessDenied,
    FileNotFound,
    Unexpected,
    BadPathName,
    FileFound,
    FileBusy,
    IsDir,
};

pub fn copy(dir: *const fs.Dir, src: []const u8, dest: []const u8, flags: OperationSettings) OperationError!void {
    if (!flags.force) force: {
        dir.access(dest, .{}) catch |err| switch (err) {
            error.FileNotFound => break :force, // File doesn't exist, proceed with copy.
            else => {
                return error.FileFound;
            },
        };
    }

    const source_buffer: []u8 = undefined;
    const source_file = dir.openFile(src, .{}) catch return error.Unexpected;
    var source_file_reader = source_file.reader(source_buffer);

    const dest_buffer: []u8 = undefined;
    const dest_file = dir.openFile(dest, .{}) catch return error.Unexpected;
    const dest_file_writer = dest_file.writer(dest_buffer);
    var dest_writer = dest_file_writer.interface;

    _ = fs.File.Writer.sendFile(&dest_writer, &source_file_reader, std.Io.Limit.unlimited) catch return error.Unexpected;
}

const PaddingVars = struct {
    str: [*c]const u8,
    len: u64,
};

pub fn getPaddingVars(source_files: []const []const u8, allocator: std.mem.Allocator) error{OutOfMemory}!PaddingVars {
    var longest_operand: u64 = 0;
    for (source_files) |file| {
        if (file.len > longest_operand) longest_operand = file.len;
    }

    const zig_padding_str = try allocator.alloc(u8, longest_operand);
    @memset(zig_padding_str, '-');
    const C_padding_str: [*c]const u8 = @ptrCast(zig_padding_str);
    return .{
        .str = C_padding_str,
        .len = longest_operand,
    };
}

pub fn zigStrToCStr(str: []const u8) [*c]u8 {
    return @ptrCast(@constCast(str));
}

pub fn printfOperation(dot_count: *u8, src: []const u8, dest: []const u8, padding_vars: PaddingVars, comptime operation: []const u8) void {
    if (dot_count.* < 3) dot_count.* += 1 else dot_count.* -= 2;

    // printf may as well be its own programming language kek //
    _ = std.c.printf(
        \\ "%s" %.*s--[%s]--> "%s"%.*s
        \\
    , zigStrToCStr(src), padding_vars.len - src.len, padding_vars.str, zigStrToCStr(operation), zigStrToCStr(dest), dot_count.*, "...");
}

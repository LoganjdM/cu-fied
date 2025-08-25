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
    defer source_file.close();
    var source_file_reader = source_file.reader(source_buffer);

    const dest_buffer: []u8 = undefined;
    const dest_file = dir.createFile(dest, .{}) catch return error.Unexpected;
    defer dest_file.close();
    var dest_file_writer = dest_file.writer(dest_buffer);
    var dest_writer = &dest_file_writer.interface;

    _ = dest_writer.sendFileAll(&source_file_reader, Io.Limit.unlimited) catch return error.Unexpected;
    _ = dest_writer.flush() catch return error.Unexpected;
}

pub fn move(dir: *const fs.Dir, src: []const u8, dest: []const u8, flags: OperationSettings) OperationError!void {
    if (!flags.force) force: {
        dir.access(dest, .{}) catch |err| switch (err) {
            error.FileNotFound => break :force, // File doesn't exist, proceed with move.
            else => {
                return error.FileFound;
            },
        };
    }

    dir.rename(src, dest) catch return error.Unexpected;
}

const PaddingVars = struct {
    str: [*c]const u8,
    len: u64,
};

pub fn getPaddingVars(source_files: []const []const u8, allocator: std.mem.Allocator) PaddingVars {
    var longest_operand: u64 = 0;
    for (source_files) |file| {
        if (file.len > longest_operand) longest_operand = file.len;
    }

    const zig_padding_str = allocator.alloc(u8, longest_operand) catch @panic("OOM!");
    @memset(zig_padding_str, '-');
    const C_padding_str: [*c]const u8 = @ptrCast(zig_padding_str);
    return .{
        .str = C_padding_str,
        .len = longest_operand,
    };
}

pub fn printfOperation(stderr: *Io.Writer, dot_count: *u8, src: []const u8, dest: []const u8, padding_vars: PaddingVars, comptime operation: []const u8) void {
    if (dot_count.* < 3) dot_count.* += 1 else dot_count.* -= 2;

    // Build padding slice from the C pointer + known length.
    const pad_len = padding_vars.len - src.len;
    const padding_slice = padding_vars.str[0..pad_len];

    // Build dots slice from the static "..." string.
    const dots_len = dot_count.*;
    const dots = "..."[0..dots_len];

    // Format to stderr using std.fmt
    _ = nosuspend stderr.print("\"{s}\" {s}--[{s}]--> \"{s}\"{s}\n", .{ src, padding_slice, operation, dest, dots }) catch return;
    _ = nosuspend stderr.flush() catch return;
}

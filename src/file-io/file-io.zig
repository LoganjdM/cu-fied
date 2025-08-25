const std = @import("std");
const Allocator = std.mem.Allocator;
const fs = std.fs;
const Io = std.Io;
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

pub fn copy(dir: fs.Dir, src: []const u8, dest: []const u8, flags: OperationSettings) OperationError!void {
    if (!flags.force) force: {
        dir.access(dest, .{}) catch |err| switch (err) {
            error.FileNotFound => break :force, // File doesn't exist, proceed with copy.
            else => {
                return error.FileFound;
            },
        };
    }

    fs.Dir.copyFile(dir, src, dir, dest, .{}) catch return error.Unexpected;
}

pub fn move(dir: fs.Dir, src: []const u8, dest: []const u8, flags: OperationSettings) OperationError!void {
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

pub fn getPadding(source_files: []const []const u8, allocator: Allocator) ![]u8 {
    var longest_operand: usize = 0;
    for (source_files) |file| {
        if (file.len > longest_operand) longest_operand = file.len;
    }

    const padding_str = try allocator.alloc(u8, longest_operand);
    // Fill the allocated buffer with '-' using @memset.
    @memset(padding_str, '-');

    return padding_str;
}

pub fn printfOperation(stderr: *Io.Writer, dot_count: u8, src: []const u8, dest: []const u8, padding: []const u8, comptime operation: []const u8) void {

    // Compute padding length safely: avoid unsigned underflow by clamping to 0
    // when src is longer than the recorded longest operand.
    const pad_len: usize = if (padding.len > src.len) padding.len - src.len else 0;
    const padding_slice = padding[0..pad_len];

    // Build dots slice from the static "..." string.
    const dots = "..."[0..dot_count];

    // Format to stderr using std.fmt
    _ = nosuspend stderr.print("\"{s}\" {s}--[{s}]--> \"{s}\"{s}\n", .{ src, padding_slice, operation, dest, dots }) catch return;
    _ = nosuspend stderr.flush() catch return;
}

const std = @import("std");
const target = @import("builtin").target;
const posix = std.posix;
// const os = std.os;
const fs = std.fs;

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
    FileBusy,
    IsDir,
};

pub fn copy(src: []const u8, dest: []const u8, flags: OperationSettings) OperationError!void {
    _ = flags; // TODO

    const src_fd = std.posix.open(src, .{}, 0) catch |err| return switch (err) {
        error.AccessDenied => error.AccessDenied,
        error.BadPathName => error.BadPathName,
        error.SystemResources => error.SystemResources,
        error.DeviceBusy => error.SystemResources,
        error.FileNotFound => error.FileNotFound,
        // `OperationError` doesn’t have every possible error.
        else => return error.Unexpected,
    };

    // we already did error checking, and all of this should happen so fast that these next things  shouldnt fail //
    // https://ziglang.org/documentation/master/std/#std.c.Stat //
    const src_mode = (posix.fstat(src_fd) catch return error.Unexpected).mode;

    const dest_fd = posix.open(dest, .{ .ACCMODE = .RDWR, .CREAT = true }, src_mode) catch |err| return switch (err) {
        error.AccessDenied => error.AccessDenied,
        error.BadPathName => error.BadPathName,
        error.SystemResources => error.SystemResources,
        error.DeviceBusy => error.SystemResources,
        error.FileNotFound => error.FileNotFound,
        error.FileBusy => error.FileBusy,
        else => return error.Unexpected,
    };

    // sendfile works differently on macos and freebsd! //
    // zig follows the BSD approach and if in_len is 0, it copies the most data it can //
    // linux: https://www.man7.org/linux/man-pages/man2/sendfile.2.html
    // mac: https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/sendfile.2.html
    _ = posix.sendfile(dest_fd, src_fd, 0, 0, &[_]posix.iovec_const{}, &[_]posix.iovec_const{}, 0) catch |err| return switch (err) {
        error.AccessDenied => error.AccessDenied,
        error.SystemResources => error.SystemResources,
        error.DeviceBusy => error.SystemResources,
        else => return error.Unexpected,
    };
}

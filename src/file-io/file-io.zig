const std = @import("std");
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

// FIXME: we use linux syscall... however macos has an equivelant and zig has an abstracted generic, but zig was doing some weird stuff with its abstracted std.posix version when I checked `strace` //
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

    // prepare to see alot of error.Unexpected. //
    // we already did error checking, and all of this should happen so fast that these next things  shouldnt faill //
    // but I dont want to risk `catch unreachable` because something *could* happen within these few microseconds //
    posix.lseek_END(src_fd, 0) catch return error.Unexpected;
    const src_len = posix.lseek_CUR_get(src_fd) catch return error.Unexpected;
    posix.lseek_SET(src_fd, 0) catch return error.Unexpected;

    // posix.stat... auctualy the types in std.posix in zig in general are a bit of a "what?" //
    // it boils down to this though if you don't wanna go down the rabbit hole of "if (os), if (arch)" //
    // https://ziglang.org/documentation/master/std/#std.c.Stat //
    const src_mode = (posix.fstat(src_fd) catch return error.Unexpected).mode;

    const dest_fd = posix.open(dest, .{ .CREAT = true }, src_mode) catch |err| return switch (err) {
        error.AccessDenied => error.AccessDenied,
        error.BadPathName => error.BadPathName,
        error.SystemResources => error.SystemResources,
        error.DeviceBusy => error.SystemResources,
        error.FileNotFound => error.FileNotFound,
        error.FileBusy => error.FileBusy,
        else => return error.Unexpected,
    };

    // sendfile works differently on macos and freebsd! //
    // header and trailer args are not needed on linux, and flags arg differs per OS, but 0 is fine as linux doesn't have this argument, and macos returns EINVAL if not given 0 //
    // mac: https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/sendfile.2.html
    // linux: https://www.man7.org/linux/man-pages/man2/sendfile.2.html
    // FIXME: HERE //
    const bytes_written = std.os.linux.sendfile(dest_fd, src_fd, null, src_len);
    // FIXME: the standard linux way checking for error here is seeing if we returned -1... //
    // but zig returns usize..? //
    _ = bytes_written;
}

pub fn remove(path: []const u8, flags: OperationSettings) OperationError!void {
    _ = flags; // TODO
    fs.cwd().deleteFile(path, .{}) catch |err| return switch (err) {
        error.AccessDenied => error.AccessDenied,
        error.BadPathName => error.BadPathName,
        error.SystemResources => error.SystemResources,
        error.FileNotFound => error.FileNotFound,
        error.FileBusy => error.FileBusy,
        error.IsDir => error.IsDir,

        else => return error.Unexpected,
    };
}

pub fn move(src: []const u8, dest: []const u8, flags: OperationSettings) OperationError!void {
    try copy(src, dest, flags);
    try remove(src, flags);
}

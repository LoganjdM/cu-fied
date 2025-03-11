const std = @import("std");
const posix = std.posix;
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

// these functions arent done, im just throwing things at the wall, we fail at rename() //
pub fn move(src: []const u8, dest: []const u8, flags: OperationSettings) OperationError!void {
    _ = flags; // TODO

    var src_fp: fs.Dir = fs.cwd().openDir(src, .{}) catch |err| return switch (err) {
        error.AccessDenied => error.AccessDenied,
        error.BadPathName => error.BadPathName,
        error.SystemResources => error.SystemResources,
        error.FileNotFound => error.FileNotFound,

        // `OperationError` doesn’t have every possible error.
        else => return error.Unexpected,
    };
    defer src_fp.close();

    src_fp.rename(src, dest) catch return error.Unexpected;
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

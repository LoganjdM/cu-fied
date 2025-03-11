const std = @import("std");
const posix = std.posix;
const fs = std.fs;

pub const OperationSettings = struct {
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
    // long ass `if (err == error.x)` is because OperationError doesn't have every possible error. //
    var src_fp: fs.Dir = fs.cwd().openDir(src, .{}) catch |err| {
        if (err == error.AccessDenied) return error.AccessDenied;
        if (err == error.BadPathName) return error.BadPathName;
        if (err == error.SystemResources) return error.SystemResources;
        if (err == error.FileNotFound) return error.FileNotFound;
        if (err == error.FileBusy) return error.FileBusy;
        return error.Unexpected;
    };
    defer src_fp.close();
    src_fp.rename(src, dest) catch return error.Unexpected;
}
pub fn remove(path: []const u8, flags: OperationSettings) OperationError!void {
    _ = flags; // TODO
    fs.cwd().deleteFile(path, .{}) catch |err| {
        if (err == error.AccessDenied) return error.AccessDenied;
        if (err == error.BadPathName) return error.BadPathName;
        if (err == error.SystemResources) return error.SystemResources;
        if (err == error.FileBusy) return error.FileBusy;
        if (err == error.FileNotFound) return error.FileNotFound;
        if (err == error.IsDir) return error.IsDir;
        return error.Unexpected;
    };
}

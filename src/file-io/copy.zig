const std = @import("std");
// TODO: copy is a bad name, this will prob be used for mv.zig as well and probably will also have a remove() //
// I just was loosely following the source code of coreutils, specifically copy.c and copy.h //

pub const OperationSettings = struct {
    force: bool,
    recursive: bool,
    link: bool,
};

pub const OperationError = error{
    ReadOnlyFileSystem,
    PermissionDenied,
    SystemResources,
    FileNotFound,
    FileBusy,
};

pub fn copy(src: []const u8, dest: []const u8, flags: OperationSettings) OperationError!void {}

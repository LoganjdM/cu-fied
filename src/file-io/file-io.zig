const std = @import("std");
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

pub fn copy(src: []const u8, dest: []const u8, flags: OperationSettings) OperationError!void {
    if (!flags.force) {
        // I miss goto, when used lightly it can make stuff like this nice, but I understand how its easy to enshittify code with it //
        var skip: bool = true;
        std.posix.access(dest, 0) catch |err| {
            if (err == error.FileNotFound) {
                skip = false;
            } // should switch(err) but I don't feel like it and we error check later anyways
        };
        if (skip) return error.FileFound;
    }

    const src_fd = std.posix.open(src, .{}, 0) catch |err| return switch (err) {
        error.AccessDenied => error.AccessDenied,
        error.BadPathName => error.BadPathName,
        error.SystemResources => error.SystemResources,
        error.DeviceBusy => error.SystemResources,
        error.FileNotFound => error.FileNotFound,
        // `OperationError` doesn’t have every possible error.
        else => return error.Unexpected,
    };

    // we already did error checking, and all of this should happen so fast that these next things shouldnt fail //
    // https://ziglang.org/documentation/master/std/#std.c.Stat //
    const src_st = posix.fstat(src_fd) catch return error.Unexpected;

    const dest_fd = posix.open(dest, .{ .ACCMODE = .RDWR, .CREAT = true }, src_st.mode) catch |err| return switch (err) {
        error.AccessDenied => error.AccessDenied,
        error.BadPathName => error.BadPathName,
        error.SystemResources => error.SystemResources,
        error.DeviceBusy => error.SystemResources,
        error.FileNotFound => error.FileNotFound,
        error.FileBusy => error.FileBusy,
        else => return error.Unexpected,
    };

    // sendfile works differently on macos and freebsd! //
    // linux: https://www.man7.org/linux/man-pages/man2/sendfile.2.html
    // mac: https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/sendfile.2.html
    _ = posix.sendfile(dest_fd, src_fd, 0, @intCast(src_st.size), &[_]posix.iovec_const{}, &[_]posix.iovec_const{}, 0) catch |err| return switch (err) {
        error.AccessDenied => error.AccessDenied,
        error.SystemResources => error.SystemResources,
        error.DeviceBusy => error.SystemResources,
        else => return error.Unexpected,
    };
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

fn zigStrToCStr(str: []const u8) [*c]u8 {
    return @ptrCast(@constCast(str));
}

pub fn printfOperation(dot_count: *u8, src: []const u8, dest: []const u8, padding_vars: PaddingVars, comptime operation: []const u8) void {
    if (dot_count.* < 3) dot_count.* += 1 else dot_count.* -= 2;

    // printf may as well be its own programming language kek //
    _ = std.c.printf("\"%s\" %.*s--[%s]--> \"%s\"%.*s\n", zigStrToCStr(src), padding_vars.len - src.len, padding_vars.str, zigStrToCStr(operation), zigStrToCStr(dest), dot_count.*, "...");
}

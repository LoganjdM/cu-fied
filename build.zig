const std = @import("std");
const builtin = @import("builtin");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{ .default_target = .{
        .abi = if (builtin.target.os.tag == .linux) .musl else null,
    } });
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseFast });

    // LSF executable
    const lsf = b.addExecutable(.{
        .name = "lsf",
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });

    // Add C source with appropriate flags
    const c_flags = if (b.release_mode == .off)
        &[_][]const u8{ "-std=c23", "-g", "-D_DEFAULT_SOURCE" }
    else
        &[_][]const u8{ "-std=c23", "-fstack-protector-all", "-D_DEFAULT_SOURCE" };

    lsf.addCSourceFile(.{
        .file = b.path("src/ls.c"),
        .flags = c_flags,
    });

    b.installArtifact(lsf);
}

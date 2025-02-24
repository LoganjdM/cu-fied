const std = @import("std");
const builtin = @import("builtin");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{ .default_target = .{
        .abi = if (builtin.target.os.tag == .linux) .musl else null,
    } });
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseFast });
    const c_flags = if (b.release_mode == .off)
        &[_][]const u8{ "-std=c23", "-g", "-D_DEFAULT_SOURCE" }
    else
        &[_][]const u8{ "-std=c23", "-fstack-protector-all", "-D_DEFAULT_SOURCE" };

    // LSF
    const lsf_main = b.path("src/ls/main.c");
    const lsf_exe = b.addExecutable(.{
        .name = "lsf",
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });

    lsf_exe.addCSourceFile(.{
        .file = lsf_main,
        .flags = c_flags,
    });
    b.installArtifact(lsf_exe);

    const lsf_exe_check = b.addExecutable(.{
        .name = "lsf",
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    lsf_exe_check.addCSourceFile(.{
        .file = lsf_main,
        .flags = c_flags,
    });

    const lsf_check = b.step("check-lsf", "Check if LSF compiles");
    lsf_check.dependOn(&lsf_exe_check.step);

    const run_lsf_exe = b.addRunArtifact(lsf_exe);
    const run_lsf = b.step("run-lsf", "Run LSF");
    run_lsf.dependOn(&run_lsf_exe.step);

    // Global
    const check = b.step("check", "Check if all apps compile");
    check.dependOn(lsf_check);

    const fmt_step = b.step("fmt", "Format all zig code");
    const check_fmt_step = b.step("check-fmt", "Check formatting of all zig code");

    const fmt_paths = .{ "src", "build.zig", "build.zig.zon" };
    const fmt = b.addFmt(.{ .paths = &fmt_paths });
    fmt_step.dependOn(&fmt.step);

    const check_fmt = b.addFmt(.{ .paths = &fmt_paths, .check = true });
    check_fmt_step.dependOn(&check_fmt.step);
}

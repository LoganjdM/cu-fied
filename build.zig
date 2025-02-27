const std = @import("std");
const builtin = @import("builtin");
const Build = std.Build;

pub fn build(b: *Build) void {
    // General options
    const target = b.standardTargetOptions(.{ .default_target = .{
        .abi = if (builtin.target.os.tag == .linux) .gnu else null,
    } });
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseFast });
    const c_flags = if (b.release_mode == .off)
        &[_][]const u8{ "-std=c23", "-g", "-D_DEFAULT_SOURCE" }
    else
        &[_][]const u8{ "-std=c23", "-fstack-protector-all", "-D_DEFAULT_SOURCE" };

    // Dependencies
    const clap = b.dependency("clap", .{});

    // LSF
    const lsf_main = b.path("src/ls/main.c");
    const lsf_exe = b.addExecutable(.{
        .name = "lsf",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });

    lsf_exe.root_module.addCSourceFile(.{
        .file = lsf_main,
        .flags = c_flags,
    });
    b.installArtifact(lsf_exe);

    const lsf_exe_check = b.addExecutable(.{
        .name = "lsf",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    lsf_exe_check.root_module.addCSourceFile(.{
        .file = lsf_main,
        .flags = c_flags,
    });

    const lsf_check = b.step("check-lsf", "Check if LSF compiles");
    lsf_check.dependOn(&lsf_exe_check.step);

    const run_lsf_exe = b.addRunArtifact(lsf_exe);
    if (b.args) |args| run_lsf_exe.addArgs(args);
    const run_lsf = b.step("run-lsf", "Run LSF");
    run_lsf.dependOn(&run_lsf_exe.step);

    // MVF
    const mvf_main = b.path("src/mv/main.zig");

    const mvf_exe = b.addExecutable(.{
        .name = "mvf",
        .root_module = b.createModule(.{
            .root_source_file = mvf_main,
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    mvf_exe.root_module.addImport("clap", clap.module("clap"));
    b.installArtifact(mvf_exe);

    const mvf_exe_check = b.addExecutable(.{
        .name = "mvf",
        .root_module = b.createModule(.{
            .root_source_file = mvf_main,
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    mvf_exe_check.root_module.addImport("clap", clap.module("clap"));

    const mvf_check = b.step("check-mvf", "Check if MVF compiles");
    mvf_check.dependOn(&mvf_exe_check.step);

    const run_mvf_exe = b.addRunArtifact(mvf_exe);
    if (b.args) |args| run_mvf_exe.addArgs(args);
    const run_mvf = b.step("run-mvf", "Run MVF");
    run_mvf.dependOn(&run_mvf_exe.step);

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

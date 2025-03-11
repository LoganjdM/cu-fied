const std = @import("std");
const builtin = @import("builtin");
const Build = std.Build;

fn addBuildSteps(b: *Build, name: []const u8, check_exe: *Build.Step, run_exe: *Build.Step, global_check: *Build.Step) void {
    // Generate `check-*` step.
    const check_str = b.fmt("check-{s}", .{name});

    const check_desc_str = b.fmt("See if {s} compiles", .{name});

    const check = b.step(check_str, check_desc_str);
    check.dependOn(check_exe);
    global_check.dependOn(check_exe);

    // Generate `run-*` step.
    const run_str = b.fmt("run-{s}", .{name});

    const run_desc_str = b.fmt("Run {s}", .{name});

    const run = b.step(run_str, run_desc_str);
    run.dependOn(run_exe);
}

fn buildC(b: *Build, srcs: anytype, target: Build.ResolvedTarget, optimize: std.builtin.OptimizeMode, name: []const u8, global_check: *Build.Step, cflags: [5][]const u8) void {
    const module = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    module.addCSourceFiles(.{
        .files = srcs,
        .flags = &cflags,
    });

    const exe = b.addExecutable(.{
        .name = name,
        .root_module = module,
    });
    b.installArtifact(exe);

    const exe_check = b.addExecutable(.{
        .name = name,
        .root_module = module,
    });

    const run_exe = b.addRunArtifact(exe);
    if (b.args) |args| run_exe.addArgs(args);
    addBuildSteps(b, name, &exe_check.step, &run_exe.step, global_check);
}

fn buildZig(b: *Build, main: Build.LazyPath, target: Build.ResolvedTarget, optimize: std.builtin.OptimizeMode, name: []const u8, global_check: *Build.Step, imports: []const Build.Module.Import) void {
    const module = b.createModule(.{
        .root_source_file = main,
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .imports = imports,
    });
    const exe = b.addExecutable(.{
        .name = name,
        .root_module = module,
    });

    b.installArtifact(exe);

    const exe_check = b.addExecutable(.{
        .name = name,
        .root_module = module,
    });

    const run_exe = b.addRunArtifact(exe);
    if (b.args) |args| run_exe.addArgs(args);
    addBuildSteps(b, name, &exe_check.step, &run_exe.step, global_check);
}

fn runHelp2man(step: *Build.Step, _: Build.Step.MakeOptions) !void {
    const b = step.owner;

    const clis = [_][]const u8{ "lsf", "mvf", "touchf" };

    for (clis) |cli| {
        const tool_run = b.addSystemCommand(&.{"help2man"});
        tool_run.addArgs(&.{
            b.fmt("zig-out/bin/{s}", .{cli}),
            "-o",
            b.fmt("docs/{s}.1", .{cli}),
        });
        tool_run.addFileArg(b.path(""));

        _ = tool_run.captureStdOut();
    }
}

pub fn build(b: *Build) !void {
    const cflags = [_][]const u8{
        "-std=c23",
        if (b.release_mode == .off) "-g" else "",
        if (b.release_mode == .safe) "-fstack-protector-all" else "",
        "-D_DEFAULT_SOURCE",
        "-DC23",
    };

    // General options
    const target = b.standardTargetOptions(.{ .default_target = .{
        .abi = if (builtin.target.os.tag == .linux) .gnu else null,
    } });
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseFast });

    // Global
    const global_check = b.step("check", "Check if all apps compile");

    const fmt_step = b.step("fmt", "Format all zig code");
    const check_fmt_step = b.step("check-fmt", "Check formatting of all zig code");

    const fmt_paths = .{ "src", "build.zig", "build.zig.zon" };
    const fmt = b.addFmt(.{ .paths = &fmt_paths });
    fmt_step.dependOn(&fmt.step);

    const check_fmt = b.addFmt(.{ .paths = &fmt_paths, .check = true });
    check_fmt_step.dependOn(&check_fmt.step);

    // Dependencies
    const clap = b.dependency("clap", .{});

    // Utilities
    const colors_h = b.addTranslateC(.{
        .root_source_file = b.path("src/colors.h"),
        .target = target,
        .optimize = optimize,
    });
    const colors_h_module = colors_h.createModule();
    const colors_module = b.addModule("colors", .{
        .root_source_file = b.path("src/colors.zig"),
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .imports = &.{
            .{ .name = "colors_h", .module = colors_h_module },
        },
    });

    const copy_module = b.addModule("copy", .{
        .root_source_file = b.path("src/file-io/copy.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{
            .{ .name = "colors_h", .module = colors_h_module },
        },
    });

    const imports: []const Build.Module.Import = &.{
        .{ .name = "colors", .module = colors_module },
        .{ .name = "copy", .module = copy_module },
    };

    // First update versioning's on the C side.. //
    var fp: std.fs.File = try std.fs.cwd().openFile("src/app_info.h", .{ .mode = .write_only });
    // TODO: get this to grab `.version` in build.zig.zon //
    try fp.writer().print("// This is a [Semantic Version](https://semver.org/).\nconst char vers[] = \"{s}\";", .{"0.0.0"}); // catch {
    fp.close();

    // LSF
    const lsf_src_files = [_][]const u8{ "src/ls/main.c", "src/ctypes/strbuild.c", "src/ctypes/table.c" };
    buildC(b, &lsf_src_files, target, optimize, "lsf", global_check, cflags);

    // TOUCHF
    const touchf_src_files = [_][]const u8{ "src/touch/main.c", "src/ctypes/table.c" };
    buildC(b, &touchf_src_files, target, optimize, "touchf", global_check, cflags);

    // MVF
    const mvf_main = b.path("src/file-io/mv/main.zig");

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

    // CPF
    const cpf = b.path("src/file-io/cp/main.zig");
    buildZig(b, cpf, target, optimize, "cpf", global_check, imports);

    // generate man pages //
    const help2man = b.step("help2man", "Use GNU `help2man` to generate man pages.");
    help2man.makeFn = runHelp2man;
}

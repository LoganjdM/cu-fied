const std = @import("std");
const builtin = @import("builtin");
const Build = std.Build;
const Compile = Build.Step.Compile;

fn addBuildSteps(b: *Build, name: []const u8, exe: *Compile) void {
    b.installArtifact(exe);

    const run_exe = b.addRunArtifact(exe);
    if (b.args) |args| run_exe.addArgs(args);

    // Generate `run-*` step.
    const run = b.step(
        b.fmt("run-{s}", .{name}),
        b.fmt("Run {s}", .{name}),
    );
    run.dependOn(&run_exe.step);
}
fn buildC(b: *Build, srcs: []const []const u8, target: Build.ResolvedTarget, optimize: std.builtin.OptimizeMode, name: []const u8, cflags: []const []const u8, no_bin: bool) *Compile {
    const module = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    module.addCSourceFiles(.{
        .files = srcs,
        .flags = cflags,
    });

    const exe = b.addExecutable(.{
        .name = name,
        .root_module = module,
    });
    if (!no_bin) addBuildSteps(b, name, exe);

    return exe;
}

fn buildZig(b: *Build, main: Build.LazyPath, target: Build.ResolvedTarget, optimize: std.builtin.OptimizeMode, name: []const u8, imports: []const Build.Module.Import, no_bin: bool) *Compile {
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
    if (!no_bin) addBuildSteps(b, name, exe);

    return exe;
}

pub fn build(b: *Build) !void {
    const cflags = [_][]const u8{
        "-std=c23",
        if (b.release_mode == .off) "-g" else "",
        if (b.release_mode == .safe) "-fstack-protector-all" else "",
        if (b.release_mode == .safe) "-O" else "",
        "-D_GNU_SOURCE",
        "-Isrc/ctypes",
        "-DC23",
    };

    // General options
    const target = b.standardTargetOptions(.{ .default_target = .{
        .abi = if (builtin.target.os.tag == .linux) .gnu else null,
    } });
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseFast });

    // Arguments
    const no_bin = b.option(bool, "no-bin", "Don't emit binaries") orelse false;

    // Global
    const fmt_step = b.step("fmt", "Format all zig code");
    const check_fmt_step = b.step("check-fmt", "Check formatting of all zig code");

    const fmt_paths = .{ "src", "build.zig", "build.zig.zon" };
    const fmt = b.addFmt(.{ .paths = &fmt_paths });
    fmt_step.dependOn(&fmt.step);

    const check_fmt = b.addFmt(.{ .paths = &fmt_paths, .check = true });
    check_fmt_step.dependOn(&check_fmt.step);

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

    const file_io_module = b.addModule("file_io", .{
        .root_source_file = b.path("src/file-io/file-io.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{
            .{ .name = "colors_h", .module = colors_h_module },
        },
    });

    const imports: []const Build.Module.Import = &.{
        .{ .name = "colors", .module = colors_module },
        .{ .name = "file_io", .module = file_io_module },
    };

    // First update versioning's on the C side.. //
    var fp: std.fs.File = std.fs.cwd().openFile("src/app_info.h", .{ .mode = .write_only }) catch {
        std.debug.print("You aren't compiling in base dir\n", .{});
        return error.ShitDidntOpen;
    };
    // todo(if it can be done 0.15.0): get this to grab `.version` in build.zig.zon //
    try fp.writer().print("// This is a [Semantic Version](https://semver.org/)\n" ++
        "#define __CU_FIED_VERSION__ \"{s}\"\n", .{"0.0.0"});
    fp.close();

    // LSF
    const lsf_src_files = [_][]const u8{ "src/ls/main.c", "src/stat/do_stat.c", "src/ctypes/strbuild.c", "src/ctypes/table.c" };
    const lsf = buildC(b, &lsf_src_files, target, optimize, "lsf", @constCast(&cflags), no_bin);

    // STATF
    const statf_src_files = [_][]const u8{ "src/stat/main.c", "src/stat/do_stat.c", "src/ctypes/strbuild.c" };
    const statf = buildC(b, &statf_src_files, target, optimize, "statf", @constCast(&cflags), no_bin);

    // TOUCHF
    const touchf_src_files = [_][]const u8{ "src/touch/main.c", "src/ctypes/table.c" };
    const touchf = buildC(b, &touchf_src_files, target, optimize, "touchf", @constCast(&cflags), no_bin);

    // MVF
    const mvf_main = b.path("src/file-io/mv/main.zig");
    const mvf = buildZig(b, mvf_main, target, optimize, "mvf", imports, no_bin);

    // CPF
    const cpf_main = b.path("src/file-io/cp/main.zig");
    _ = buildZig(b, cpf_main, target, optimize, "cpf", imports, no_bin);

    // generate man pages //
    const help2man = b.step("help2man", "Use GNU `help2man` to generate man pages.");

    const clis = [_]*Compile{ lsf, mvf, touchf, statf };

    for (clis) |cli| {
        const tool_run = b.addSystemCommand(&.{"help2man"});
        tool_run.addArtifactArg(cli);

        const manpage = b.fmt("{s}.1", .{cli.name});
        const output = tool_run.addPrefixedOutputFileArg("-o", manpage);

        const installMan = b.addInstallFile(output, b.fmt("man/{s}", .{manpage}));
        help2man.dependOn(&installMan.step);
    }
}

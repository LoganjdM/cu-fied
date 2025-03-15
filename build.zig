const std = @import("std");
const builtin = @import("builtin");
const Build = std.Build;
const Step = Build.Step;

fn addBuildSteps(b: *Build, name: []const u8, exe: *Step.Compile) void {
    b.installArtifact(exe);

    if (exe.rootModuleTarget().os.tag != b.graph.host.result.os.tag) {
        return;
    }

    const run_exe = b.addRunArtifact(exe);
    if (b.args) |args| run_exe.addArgs(args);

    // Generate `run-*` step.
    const run = b.step(
        b.fmt("run-{s}", .{name}),
        b.fmt("Run {s}", .{name}),
    );
    run.dependOn(&run_exe.step);
}

fn buildManPage(b: *Build, exe: *Step.Compile) void {
    const manpage = b.fmt("{s}.1", .{exe.name});
    const section = 1;

    const tool_run = b.addSystemCommand(&.{"help2man"});
    tool_run.addArtifactArg(exe);

    const output = tool_run.captureStdOut();

    const install = b.addInstallFile(output, b.fmt("share/man/man{any}/{s}", .{ section, manpage }));
    b.getInstallStep().dependOn(&install.step);
}

const SharedBuildOptions = struct {
    target: Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    no_bin: bool,
    emit_man: bool,
};

fn buildC(b: *Build, name: []const u8, options: SharedBuildOptions, srcs: []const []const u8, cflags: []const []const u8) void {
    const module = b.createModule(.{
        .target = options.target,
        .optimize = options.optimize,
        .link_libc = true,
    });
    module.addCSourceFiles(.{
        .files = srcs,
        .flags = cflags,
    });
    module.addIncludePath(b.path("src/ctypes"));

    const exe = b.addExecutable(.{
        .name = name,
        .root_module = module,
    });
    if (!options.no_bin) addBuildSteps(b, name, exe);
    if (options.emit_man) buildManPage(b, exe);
}

fn buildZig(b: *Build, name: []const u8, options: SharedBuildOptions, main: Build.LazyPath, imports: []const Build.Module.Import) void {
    const module = b.createModule(.{
        .root_source_file = main,
        .target = options.target,
        .optimize = options.optimize,
        .link_libc = true,
        .imports = imports,
    });
    const exe = b.addExecutable(.{
        .name = name,
        .root_module = module,
    });
    if (!options.no_bin) addBuildSteps(b, name, exe);
    if (options.emit_man) buildManPage(b, exe);
}

pub fn build(b: *Build) !void {
    const cflags = [_][]const u8{
        "-std=c23",
        if (b.release_mode == .off) "-g" else "",
        if (b.release_mode == .safe) "-fstack-protector-all" else "",
        if (b.release_mode == .safe) "-O" else "",
        "-D_GNU_SOURCE",
        "-DC23",
    };

    // General options
    const target = b.standardTargetOptions(.{ .default_target = .{
        .abi = if (builtin.target.os.tag == .linux) .gnu else null,
    } });
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseFast });

    const no_bin = b.option(bool, "no-bin", "Don't emit binaries") orelse false;
    const emit_man = b.option(bool, "emit-man-pages", "Generate man pages using GNU `help2man`") orelse false;

    const target_os = target.result.os.tag;
    const current_os = builtin.os.tag;
    if (emit_man and target_os != current_os) {
        return error.Unsupported;
    }

    const options: SharedBuildOptions = .{
        .target = target,
        .optimize = optimize,
        .no_bin = no_bin,
        .emit_man = emit_man,
    };

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

    // Update versioning on the C side first. //
    const info_dir = b.addWriteFiles();
    const info_file = info_dir.add("app_info.h", b.fmt(
        \\// This is a [Semantic Version](https://semver.org/)
        \\#define __CU_FIED_VERSION__ "{s}"
        \\
    , .{
        // TODO: grab `.version` in build.zig.zon (if it can be done 0.15.0, ziglang/zig#22775).
        "0.0.0",
    }));

    const write_info = b.addUpdateSourceFiles();
    write_info.addCopyFileToSource(info_file, "src/app_info.h");

    b.step("write-info", "Generate app_info.h").dependOn(&write_info.step);

    // LSF
    const lsf_src_files = [_][]const u8{ "src/ls/main.c", "src/stat/do_stat.c", "src/ctypes/strbuild.c", "src/ctypes/table.c" };
    buildC(b, "lsf", options, &lsf_src_files, &cflags);

    // STATF
    const statf_src_files = [_][]const u8{ "src/stat/main.c", "src/stat/do_stat.c", "src/ctypes/strbuild.c" };
    buildC(b, "statf", options, &statf_src_files, &cflags);

    // TOUCHF
    const touchf_src_files = [_][]const u8{ "src/touch/main.c", "src/ctypes/table.c" };
    buildC(b, "touchf", options, &touchf_src_files, &cflags);

    // MVF
    const mvf_main = b.path("src/file-io/mv/main.zig");
    buildZig(b, "mvf", options, mvf_main, imports);

    // CPF
    const cpf_main = b.path("src/file-io/cp/main.zig");
    buildZig(b, "cpf", options, cpf_main, imports);
}

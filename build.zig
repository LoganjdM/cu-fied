const std = @import("std");
const Build = std.Build;
const LazyPath = Build.LazyPath;
const Module = Build.Module;
const Step = Build.Step;
const builtin = @import("builtin");

fn addBuildSteps(b: *Build, name: []const u8, exe: *Step.Compile) void {
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

fn buildCli(b: *Build, name: []const u8, root_module: *Module, options: *const SharedBuildOptions) void {
    const exe = b.addExecutable(.{
        .name = name,
        .root_module = root_module,
    });

    if (options.no_bin) {
        b.getInstallStep().dependOn(&exe.step);

        return;
    }

    addBuildSteps(b, name, exe);

    if (options.emit_man) buildManPage(b, exe);
}

fn buildCModule(b: *Build, options: *const SharedBuildOptions, lib_color: ?*Step.Compile, srcs: []const []const u8, cflags: []const []const u8) *Module {
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

    if (lib_color) |lib| {
        module.addObject(lib);
    }

    return module;
}

fn buildZigModule(b: *Build, options: *const SharedBuildOptions, main: LazyPath, imports: []const Module.Import) *Module {
    const module = b.createModule(.{
        .root_source_file = main,
        .target = options.target,
        .optimize = options.optimize,
        .link_libc = true,
        .imports = imports,
    });

    return module;
}

pub fn build(b: *Build) !void {
    // General options
    var cflags: std.ArrayList([]const u8) = try .initCapacity(b.allocator, 16);
    cflags.appendSliceAssumeCapacity(&.{ "-std=c23", "-Wall", "-D_GNU_SOURCE", "-DC23" });

    if (b.release_mode == .off) {
        cflags.appendSliceAssumeCapacity(&.{"-g"});
    } else {
        cflags.appendSliceAssumeCapacity(&.{ "-Wextra", "-DNDEBUG" });
        switch (b.release_mode) {
            .fast => {
                cflags.appendSliceAssumeCapacity(&.{ "-O3", "-ffast-math" });
            },
            .safe => {
                cflags.appendSliceAssumeCapacity(&.{ "-fstack-protector-all", "-O" });
            },
            .small => {
                cflags.appendSliceAssumeCapacity(&.{"-Os"});
            },
            else => {},
        }
    }

    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{
        .preferred_optimize_mode = .ReleaseFast,
    });

    const no_bin = b.option(bool, "no-bin", "Do not emit binaries") orelse false;
    const emit_man = b.option(bool, "emit-man-pages", "Generate man pages using GNU `help2man`") orelse false;

    // TODO: grab `.version` in build.zig.zon (if it can be done 0.15.0, ziglang/zig#22775).
    const version = "0.0.0";

    const injectedOptions = b.addOptions();
    injectedOptions.addOption([]const u8, "version", version);

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

    // LibColor (all programs use this) //
    const lib_color = b.addObject(.{
        .name = "libcolor",
        .root_module = buildCModule(b, &options, null, &[_][]const u8{"src/libcolor/colors.c"}, cflags.items),
    });
    const lib_color_zig = b.createModule(.{
        .root_source_file = b.path("src/libcolor/colors.zig"),
        .target = options.target,
        .optimize = options.optimize,
        .link_libc = false,
    });

    // Utilities
    const file_io_module = b.createModule(.{
        .root_source_file = b.path("src/file-io/file-io.zig"),
        .target = target,
        .optimize = optimize,
    });

    const imports: []const Module.Import = &.{
        .{ .name = "options", .module = injectedOptions.createModule() },
        .{ .name = "colors", .module = lib_color_zig },
        .{ .name = "file_io", .module = file_io_module },
    };

    // Update versioning on the C side first. //
    const info_dir = b.addWriteFiles();
    const info_file = info_dir.add("app_info.h", b.fmt(
        \\// This is a [Semantic Version](https://semver.org/)
        \\#define __CU_FIED_VERSION__ "{s}"
        \\
    , .{version}));

    const write_info = b.addUpdateSourceFiles();
    write_info.addCopyFileToSource(info_file, "src/app_info.h");

    b.step("write-info", "Generate app_info.h").dependOn(&write_info.step);

    // LSF
    const lsf_src_files = [_][]const u8{
        "src/ls/main.c",
        "src/ls/print.c",
        "src/stat/do_stat.c",
        "src/ctypes/strbuild.c",
        "src/ctypes/table.c",
    };
    const lsf = buildCModule(b, &options, lib_color, &lsf_src_files, cflags.items);
    buildCli(b, "lsf", lsf, &options);

    // STATF
    const statf_src_files = [_][]const u8{
        "src/stat/main.c",
        "src/stat/do_stat.c",
        "src/ctypes/strbuild.c",
        "src/ctypes/table.c",
    };
    const statf = buildCModule(b, &options, lib_color, &statf_src_files, cflags.items);
    buildCli(b, "statf", statf, &options);

    // TOUCHF
    const touchf_src_files = [_][]const u8{
        "src/touch/main.c",
        "src/ctypes/table.c",
    };
    const touchf = buildCModule(b, &options, lib_color, &touchf_src_files, cflags.items);
    buildCli(b, "touchf", touchf, &options);

    // MVF
    const mvf_main = b.path("src/file-io/mv/main.zig");
    const mvf = buildZigModule(b, &options, mvf_main, imports);
    buildCli(b, "mvf", mvf, &options);

    // CPF
    const cpf_main = b.path("src/file-io/cp/main.zig");
    const cpf = buildZigModule(b, &options, cpf_main, imports);
    buildCli(b, "cpf", cpf, &options);
}

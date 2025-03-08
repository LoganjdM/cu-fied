const std = @import("std");
const builtin = @import("builtin");
const print = std.debug.print;
const Build = std.Build;
const proc = std.process;

var global_check: *Build.Step = undefined;
fn add_build_steps(b: *Build, name: []const u8, check_exe: *Build.Step, run_exe: *Build.Step) !void {
    var gpa: std.heap.DebugAllocator(.{}) = .init;
    defer std.debug.assert(gpa.deinit() == .ok);
    const alloc = gpa.allocator();

    const check_str = try std.fmt.allocPrint(alloc, "check-{s}", .{name});
    const check_desc_str = try std.fmt.allocPrint(alloc, "See if {s} compiles", .{name});
    defer alloc.free(check_str);
    defer alloc.free(check_desc_str);

    const check = b.step(check_str, check_desc_str);
    check.dependOn(check_exe);
    global_check.dependOn(check_exe);

    const run_str = try std.fmt.allocPrint(alloc, "run-{s}", .{name});
    const run_desc_str = try std.fmt.allocPrint(alloc, "Run {s}", .{name});
    defer alloc.free(run_str);
    defer alloc.free(run_desc_str);

    const run = b.step(run_str, run_desc_str);
    run.dependOn(run_exe);
}

var cflags: [3][]const u8 = undefined;
fn build_c(b: *Build, srcs: anytype, target: Build.ResolvedTarget, optimize: std.builtin.OptimizeMode, name: []const u8) !void {
    const exe = b.addExecutable(.{ .name = name, .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }) });

    exe.root_module.addCSourceFiles(.{ .files = srcs, .flags = &cflags });
    b.installArtifact(exe);

    const exe_check = b.addExecutable(.{ .name = name, .root_module = b.createModule(.{ .target = target, .optimize = optimize, .link_libc = true }) });

    exe_check.root_module.addCSourceFiles(.{ .files = srcs, .flags = &cflags });

    const run_exe = b.addRunArtifact(exe);
    if (b.args) |args| run_exe.addArgs(args);
    try add_build_steps(b, name, &exe_check.step, &run_exe.step);
}

var colors_module: *Build.Module = undefined;
fn build_zig(b: *Build, main: Build.LazyPath, target: Build.ResolvedTarget, optimize: std.builtin.OptimizeMode, name: []const u8) !void {
    const exe = b.addExecutable(.{
        .name = name,
        .root_module = b.createModule(.{
            .root_source_file = main,
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    exe.root_module.addImport("colors", colors_module);
    b.installArtifact(exe);

    const exe_check = b.addExecutable(.{
        .name = name,
        .root_module = b.createModule(.{
            .root_source_file = main,
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });
    exe_check.root_module.addIncludePath(b.path("src"));

    const run_exe = b.addRunArtifact(exe);
    if (b.args) |args| run_exe.addArgs(args);
    try add_build_steps(b, name, &exe_check.step, &run_exe.step);
}

fn run_help2man(self: *Build.Step, opt: Build.Step.MakeOptions) !void {
    _ = self;
    _ = opt;

    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    const alloc = arena.allocator();
    defer {
        _ = arena.reset(.free_all);
        _ = arena.deinit();
    }

    var help2man_result: proc.Child.RunResult = try proc.Child.run(.{ .allocator = alloc, .argv = &[_][]const u8{ "help2man", "zig-out/bin/lsf", "-o", "docs/lsf.1" } });

    help2man_result = try proc.Child.run(.{ .allocator = alloc, .argv = &[_][]const u8{ "help2man", "zig-out/bin/mvf", "-o", "docs/mvf.1" } });

    help2man_result = try proc.Child.run(.{ .allocator = alloc, .argv = &[_][]const u8{ "help2man", "zig-out/bin/touchf", "-o", "docs/touchf.1" } });
}

pub fn build(b: *Build) !void {
    cflags = if (b.release_mode == .off)
        .{ "-std=c23", "-g", "-D_DEFAULT_SOURCE" }
    else
        .{ "-std=c23", "-fstack-protector-all", "-D_DEFAULT_SOURCE" };

    // General options
    const target = b.standardTargetOptions(.{ .default_target = .{
        .abi = if (builtin.target.os.tag == .linux) .gnu else null,
    } });
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseFast });

    // Global
    global_check = b.step("check", "Check if all apps compile");
    colors_module = b.addModule("colors", .{
    	.root_source_file = b.path("src/colors.zig"),
    	.target = target,
    	.optimize = optimize,
    	.link_libc = true
    });
    colors_module.addIncludePath(b.path("src"));
    const fmt_step = b.step("fmt", "Format all zig code");
    const check_fmt_step = b.step("check-fmt", "Check formatting of all zig code");

    const fmt_paths = .{ "src", "build.zig", "build.zig.zon" };
    const fmt = b.addFmt(.{ .paths = &fmt_paths });
    fmt_step.dependOn(&fmt.step);

    const check_fmt = b.addFmt(.{ .paths = &fmt_paths, .check = true });
    check_fmt_step.dependOn(&check_fmt.step);

    // Dependencies
    const clap = b.dependency("clap", .{});

    // First update versioning's on the C side.. //
    var fp: std.fs.File = try std.fs.cwd().openFile("src/app_info.h", .{ .mode = .write_only });
    // TODO: get this to grab `.version` in build.zig.zon //
    try fp.writer().print("// This is a [Semantic Version](https://semver.org/).\nconst char vers[] = \"{s}\";", .{"0.0.0"}); // catch {
    fp.close();

    // LSF
    const lsf_src_files = [_][]const u8{ "src/ls/main.c", "src/ctypes/strbuild.c", "src/ctypes/table.c" };
    try build_c(b, &lsf_src_files, target, optimize, "lsf");

    // TOUCHF
    const touchf_src_files = [_][]const u8{ "src/touch/main.c", "src/ctypes/table.c" };
    try build_c(b, &touchf_src_files, target, optimize, "touchf");

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
    try build_zig(b, cpf, target, optimize, "cpf");

    // generate man pages //
    const help2man = b.step("help2man", "Use GNU `help2man` to generate man pages.");
    help2man.makeFn = run_help2man;
}

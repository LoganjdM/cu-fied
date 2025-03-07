const std = @import("std");
const builtin = @import("builtin");
const Build = std.Build;
const proc = std.process;

pub fn build(b: *Build) !void {
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

    // First update versioning's on the C side.. //
    var fp: std.fs.File = try std.fs.cwd().openFile("src/app_info.h", .{ .mode = .write_only });
    // TODO: get this to grab `.version` in build.zig.zon //
    try fp.writer().print("// This is a [Semantic Version](https://semver.org/).\nconst char vers[] = \"{s}\";", .{"0.0.0"});
    fp.close();

    // LSF
    const lsf_src_files = [_][]const u8{ "src/ls/main.c", "src/type/strbuild.c", "src/type/table.c" };
    const lsf_exe = b.addExecutable(.{
        .name = "lsf",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });

    lsf_exe.root_module.addCSourceFiles(.{
        .files = &lsf_src_files,
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
    lsf_exe_check.root_module.addCSourceFiles(.{
        .files = &lsf_src_files,
        .flags = c_flags,
    });

    const lsf_check = b.step("check-lsf", "Check if LSF compiles");
    lsf_check.dependOn(&lsf_exe_check.step);

    const run_lsf_exe = b.addRunArtifact(lsf_exe);
    if (b.args) |args| run_lsf_exe.addArgs(args);
    const run_lsf = b.step("run-lsf", "Run LSF");
    run_lsf.dependOn(&run_lsf_exe.step);

    // TOUCHF
    const touchf_src_files = [_][]const u8{ "src/touch/main.c", "src/type/table.c" };
    const touchf_exe = b.addExecutable(.{
    	.name = "touchf",
    	.root_module = b.createModule(.{
    		.target = target,
    		.optimize = optimize,
    		.link_libc = true,
    	}),
    });
    touchf_exe.root_module.addCSourceFiles(.{
    	.files = &touchf_src_files,
    	.flags = c_flags
    });
    b.installArtifact(touchf_exe);

    const touchf_exe_check = b.addExecutable(.{
			.name = "touchf",
			.root_module = b.createModule(.{
			.target = target,
			.optimize = optimize,
			.link_libc = true,
		}),
	});
	touchf_exe_check.root_module.addCSourceFiles(.{
		.files = &touchf_src_files,
		.flags = c_flags
	});

    const touchf_check = b.step("check-touchf", "Check if TouchF compiles");
    touchf_check.dependOn(&touchf_exe_check.step);

    const run_touchf_exe = b.addRunArtifact(touchf_exe);
    if (b.args) |args| run_touchf_exe.addArgs(args);
    const run_touchf = b.step("run-touchf", "Run TouchF");
    run_touchf.dependOn(&run_touchf_exe.step);

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

    // generate man pages //
    const help2man = b.step("help2man", "Use GNU `help2man` to generate man pages.");
    help2man.makeFn = run_help2man;
}

fn run_help2man(self: *Build.Step, opt: Build.Step.MakeOptions) !void {
    _ = self;
    _ = opt;

    var gpa: std.heap.DebugAllocator(.{}) = .init;
    defer std.debug.assert(gpa.deinit() == .ok);
    const alloc = gpa.allocator();

    var help2man_result: proc.Child.RunResult = try proc.Child.run(.{ .allocator = alloc, .argv = &[_][]const u8{ "help2man", "zig-out/bin/lsf", "-o", "docs/lsf.1" } });
    alloc.free(help2man_result.stdout);
    alloc.free(help2man_result.stderr);

    help2man_result = try proc.Child.run(.{ .allocator = alloc, .argv = &[_][]const u8{ "help2man", "zig-out/bin/mvf", "-o", "docs/mvf.1" } });
    alloc.free(help2man_result.stdout);
    alloc.free(help2man_result.stderr);
    
    help2man_result = try proc.Child.run(.{ .allocator = alloc, .argv = &[_][]const u8{ "help2man", "zig-out/bin/touchf", "-o", "docs/touchf.1" } });
    alloc.free(help2man_result.stdout);
    alloc.free(help2man_result.stderr);
}

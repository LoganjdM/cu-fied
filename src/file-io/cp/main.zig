const std = @import("std");
const log = std.log;
const stdout = std.io.getStdOut();
const stderr = std.io.getStdErr();
const color = @import("colors");
const is_debug = (@import("builtin").mode == std.builtin.OptimizeMode.Debug);
const fs = std.fs;
// const zon = @import("../../../build.zig.zon"); // how get .version tag on `build.zig.zon`?

const params = enum(u16) {
    recursive = 0b1,
    force = 0b10,
    link = 0b100,
    interactive = 0b1000,
    verbose = 0b10000,
};
fn argGiven(args: u16, arg: params) bool {
	return (args & @intFromEnum(arg)) != 0;
}

fn isArg(arg: [*:0]const u8, comptime short: []const u8, comptime long: []const u8) bool {
    return std.mem.eql(u8, arg[0..short.len], short) or std.mem.eql(u8, arg[0..long.len], long);
}

fn parseArgs(argv: [][*:0]u8, files: *std.ArrayListAligned([*:0]u8, null)) error{ OutOfMemory, InvalidArgument }!u16 {
    var args: u16 = 0b0; // lmao eli

    for (argv, 0..) |arg, i| {
        if (i == 0) continue; // skip exec

        if (arg[0] != '-') {
            try files.append(arg);
            continue;
        }

        if (isArg(arg, "-r", "--recursive")) {
            args |= @intFromEnum(params.recursive);
        } else if (isArg(arg, "-f", "--force")) {
            args |= @intFromEnum(params.force);
        } else if (isArg(arg, "-l", "--link")) {
            args |= @intFromEnum(params.link);
        } else if (isArg(arg, "-i", "--interactive")) {
            args |= @intFromEnum(params.interactive);
        } else if (isArg(arg, "-v", "--verbose")) {
            args |= @intFromEnum(params.verbose);
        } else if (isArg(arg, "-h", "--help")) {
            const help_message = @embedFile("help.txt");
            nosuspend stdout.writer().print(help_message, .{}) catch continue; // eh dont give a shi, we are closing anyways //
            std.process.exit(0);
        } else if (isArg(arg, "-v", "--version")) {
            // stdout.print("{i}", .{.version}); // how get .version tag on `build.zig.zon`?
            std.process.exit(0);
        }
    }
    if (files.items.len < 2) return error.InvalidArgument;

    return args;
}

fn zigStrToC(str: []u8) [*c]u8 {
	return @ptrCast(str);
}

pub fn main() u8 {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    const gpa_alloc = gpa.allocator();
    defer _ = gpa.deinit();

    var files = std.ArrayList([*:0]u8).init(gpa_alloc);
    defer files.deinit();

    const args = parseArgs(std.os.argv, &files) catch |err| {
        if (err == error.InvalidArgument) {
            const msg = if (files.items.len == 0) "Missing file arguments!" else "Missing destination file argument!";
            color.print(stderr, color.red, "{s}\n", .{msg});
            return 2;
        } else {
            color.print(stderr, color.red, "Failed to reallocate memory for extra file arguments!\n", .{});
            return 1;
        }
    };

    // lets not iterate over files.items if we aren't printing them //
    if (is_debug) {
        log.debug("operands:", .{});
        for (files.items) |file| {
            log.debug("\t{s}", .{file});
        }
        log.debug("args: {b}", .{args});
    }

    const dest: []u8 = gpa_alloc.alloc(u8, files.items.len + 1) catch {
        color.print(stderr, color.red, "Failed to allocate memory for destination file argument!\n", .{});
        return 1;
    };
    @memcpy(dest, files.pop().?);
    defer gpa_alloc.free(dest);
    

	var dot_count: u8 = 0;
    for (files.items) |file_slice| {
    	// these *:0 are really fucking annoying so fuck it do it the c way of looking for \0 //
    	const file: []u8 = gpa_alloc.alloc(u8, std.mem.len(file_slice)) catch { 
	        color.print(stderr, color.red, "Failed to allocate memory for source file argument!\n", .{});
	        continue;
    	};
    	@memcpy(file, file_slice);
    	defer gpa_alloc.free(file);

		if(argGiven(args, params.verbose)) {
			if (dot_count < 3) dot_count += 1 else dot_count -= 2;
			// printf may as well be its own programming language kek //
			// also if we have this cool ... thing, we should also probably use %*s fuckery to vertically allign this instead of \t //
			_ = std.c.printf("copying %s\t to %s%.*s\n", zigStrToC(file), zigStrToC(dest), dot_count, "...");
		}

		// does this file exist? //
	    fs.cwd().access(file, .{ .mode = .read_only }) catch |err| {
	    	if (!argGiven(args, params.verbose)) continue;
	    	
	        color.print(stderr, color.red, "Failed to open", .{});
	        color.print(stderr, color.blue, " {s}", .{file});
	        const reason = blk: {
	            if (err == error.PermissionDenied) break :blk " (Do you have permission?)";
	            if (err == error.FileNotFound) break :blk " (does it exist?)";
	            if (err == error.BadPathName) break :blk " (is it a valid path?)";
	            break :blk "!";
	        };
	        color.print(stderr, color.red, "{s}\n", .{reason});
	        continue;
	    };

	    
    }
    return 0;
}

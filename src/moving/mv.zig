const c = @cImport({
    @cInclude("stdio.h");
    @cInclude("stdlib.h");
    @cInclude("move.c");
    @cInclude("string.h");
    @cInclude("../colors.h");
});
const std = @import("std");
const mem = std.mem;

var gp_alloc = std.heap.GeneralPurposeAllocator(.{}){};
const gpa = gp_alloc.allocator();

const move_args = struct {
    dest: [*c]u8,
    src: [][*c]u8,
    pos: u32,
    fn unwind(self: *move_args) [*c]u8 {
        if (self.src[self.pos] == 0) {
            return 0; // '\0' //
        }
        c.free(self.src[self.pos]);
        self.pos -= 1;
        return self.src[self.pos];
    }
};

fn parse_args() !move_args {
    var params: [][*c]u8 = undefined;
    var param_count: u32 = 0;
    var exe_call: bool = true;
    for (std.os.argv) |arg| {
        if (exe_call) {
            exe_call = false;
            continue;
        }
        if (arg[0] != '-') {
            // FIXME: mem leak if fail //
            params[param_count] = try gpa.create(u8);
            params[param_count] = arg;
            _ = c.printf("hi\n");
            param_count += 1;
            continue;
        } //if(mem.eql(u8, @as([]const u8, arg), "-r")) { // <<< FIXME
        // 	c.move_args |= c.MOVE_ARGS_RECURSIVE;
        // }
    }
    if (param_count < 2) {
        return error.InvalidArgs;
    }
    // FIXME: mem leak if fail //
    var dest: [*c]u8 = try gpa.create(u8);
    dest = c.strcpy(dest, params[param_count]);
    var result: move_args = move_args{ .dest = dest, .src = params, .pos = param_count };
    _ = c.printf("%s %s", result.unwind(), result.dest);
    return result;
}

pub fn main() u8 {
    var files: move_args = parse_args() catch |err| {
        if (err == error.OutOfMemory) {
            _ = c.escape_code(c.stderr, c.RED);
            _ = c.printf("Could not allocate suffiecient memory for paths!\n");
        } else {
            _ = c.escape_code(c.stderr, c.YELLOW);
            _ = c.printf("You need to specify a source path and destination path!\n");
        }
        _ = c.escape_code(c.stderr, c.RESET);
        // gp_alloc.deinit();
        return 1;
    };

    var src: [*c]u8 = files.unwind();
    while (files.pos > 0) {
        _ = c.printf("mv %s %s\n", src, files.dest);
        src = files.unwind();
    }
    // gp_alloc.deinit();
    return 0;
}

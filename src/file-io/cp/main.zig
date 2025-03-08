const std = @import("std");

var args: u16 = 0b0;
const arg = enum(u16) {
	recursive 		= 0b1,
	force 			= 0b01,
	link 			= 0b001,
	interactive 	= 0b0001,
	verbose 		= 0b00001
};

fn parse_args(argv: [][*:0]u8) void {
	_ = argv;
}

pub fn main() !void {
    std.debug.print("Hello, World!\n", .{});
}

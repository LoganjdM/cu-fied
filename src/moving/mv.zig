const c = @cImport({
	@cInclude("stdio.h");
	@cInclude("stdlib.h");
	@cInclude("../colors.h");
});
const std = @import("std");

// TODO: auctually make auctually make lmfao //
pub fn main() u8 {
	_=c.escape_code(c.stdout, c.BLUE);
	_=c.printf("Hello %s\n", "world!"); _=c.escape_code(c.stdout, c.RESET);
	return 0;
}
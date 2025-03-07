#include "../type/table.h"

#ifndef __has_embed	
char c_hello[] = "\n";
char cpp_hello[] = "\n";
char lua_hello[] = "\n";
char teal_hello[] = "\n";
char python_hello[] = "\n";
char csharp_hello[] = "\n";
char zig_hello[] = "\n";
#else


char c_hello[] = {
	#embed "fileconts/hello.c"
	, '\0'
};
char cpp_hello[] = {
	#embed "fileconts/hello.cpp"
	, '\0'
};
char lua_hello[] = {
	#embed "fileconts/hello.lua"
	, '\0'
};
char teal_hello[] = {
	#embed "fileconts/hello.tl"
	, '\0'
};
char python_hello[] = {
	#embed "fileconts/hello.py"
	, '\0'
};
char csharp_hello[] = {
	#embed "fileconts/hello.cs"
	, '\0'
};
char zig_hello[] = {
	#embed "fileconts/hello.zig"
	, '\0'
};

#endif
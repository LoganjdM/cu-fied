#include "../ctypes/table.h"

#ifndef __has_embed	
char c_example[] = "\n";
char cpp_example[] = "\n";
char lua_example[] = "\n";
char teal_example[] = "\n";
char python_example[] = "\n";
char csharp_example[] = "\n";
char zig_example[] = "\n";
char txt_example[] = "\n";
char markdown_example[] = "\n";
#else

char c_example[] = {
	#embed "fileconts/example.c"
	, '\0' };
char cpp_example[] = {
	#embed "fileconts/example.cpp"
	, '\0' };
char lua_example[] = {
	#embed "fileconts/example.lua"
	, '\0' };
char teal_example[] = {
	#embed "fileconts/example.tl"
	, '\0' };
char python_example[] = {
	#embed "fileconts/example.py"
	, '\0' };
char csharp_example[] = {
	#embed "fileconts/example.cs"
	, '\0' };
char zig_example[] = {
	#embed "fileconts/example.zig"
	, '\0' };
char txt_example[] = {
	#embed "fileconts/example.txt"
	, '\0' };
char markdown_example[] = {
	#embed "fileconts/example.md"
	, '\0' };

#endif
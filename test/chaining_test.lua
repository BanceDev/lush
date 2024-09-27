--[[
Copyright (c) 2024, Lance Borden
All rights reserved.

This software is licensed under the BSD 3-Clause License.
You may obtain a copy of the license at:
https://opensource.org/licenses/BSD-3-Clause

Redistribution and use in source and binary forms, with or without
modification, are permitted under the conditions stated in the BSD 3-Clause
License.

THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTIES,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
]]

local cwd = lush.getcwd()
lush.cd("~/.lush/scripts")
if lush.exec('cat "example.lua" | grep "hello" | sort | uniq') then
	print("piping test passed ✅\n")
else
	print("piping test failed ❌\n")
end

if lush.exec("echo hi && echo bye") then
	print("and test passed ✅\n")
else
	print("and test failed ❌\n")
end

if lush.exec("cd lol || echo lol does not exist") then
	print("or test passed ✅\n")
else
	print("or test failed ❌\n")
end

if lush.exec("sleep 2 &") then
	print("background test passed ✅\n")
else
	print("background test failed ❌\n")
end

if lush.exec("echo hi; echo bye") then
	print("semicolon test passed ✅\n")
else
	print("semicolon test failed ❌\n")
end

if lush.exec("echo hi > test.txt") then
	print("redirect test passed ✅\n")
else
	print("redirect test failed ❌\n")
end

if lush.exec("echo hi >> test.txt") then
	print("append test passed ✅\n")
else
	print("append test failed ❌\n")
end

lush.cd(cwd)

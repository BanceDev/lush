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
	lush.exit()
end

if lush.exec("echo hi && echo bye") then
	print("and test passed ✅\n")
else
	print("and test failed ❌\n")
	lush.exit()
end

if lush.exec("cd lol || echo lol does not exist") then
	print("or test passed ✅\n")
else
	print("or test failed ❌\n")
	lush.exit()
end

if lush.exec("sleep 2 &") then
	print("background test passed ✅\n")
else
	print("background test failed ❌\n")
	lush.exit()
end

if lush.exec("echo hi; echo bye") then
	print("semicolon test passed ✅\n")
else
	print("semicolon test failed ❌\n")
	lush.exit()
end

if
	lush.exec(
		"echo hi > test.txt; echo hi 1> test.txt; echo this wont redirect 2> test.txt; echo but this will &> test.txt"
	)
then
	print("redirect test passed ✅\n")
else
	print("redirect test failed ❌\n")
	lush.exit()
end

if
	lush.exec(
		"echo hi >> test.txt; echo hi 1>> test.txt; echo this wont append 2>> test.txt; echo but this will &>> test.txt"
	)
then
	print("append test passed ✅\n")
else
	print("append test failed ❌\n")
	lush.exit()
end

if
	lush.exec(
		'cat "example.lua" | grep "hello" | sort | uniq && echo hi >> test.txt; echo this should print && cd lol || echo lol doesnt exist &> test.txt'
	)
then
	print("complex chain test passed ✅\n")
else
	print("complex chain test failed ❌\n")
	lush.exit()
end

lush.cd(cwd)

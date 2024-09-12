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
lush.cd(cwd)

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

if lush.exists("~/.lush/scripts/example.lua") then
	print("exists test passed ✅\n")
else
	print("exists test failed ❌\n")
	lush.exit()
end

if lush.isFile("~/.lush/scripts/example.lua") then
	print("isFile test passed ✅\n")
else
	print("isFile test failed ❌\n")
	lush.exit()
end

if not lush.isDirectory("~/.lush/scripts/example.lua") then
	print("isDirectory test passed ✅\n")
else
	print("isDirectory test failed ❌\n")
	lush.exit()
end

if lush.isReadable("~/.lush/scripts/example.lua") then
	print("isReadable test passed ✅\n")
else
	print("isReadable test failed ❌\n")
	lush.exit()
end

if lush.isWriteable("~/.lush/scripts/example.lua") then
	print("isWriteable test passed ✅\n")
else
	print("isWriteable test failed ❌\n")
	lush.exit()
end

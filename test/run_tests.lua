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

-- TODO: Add API method for asserting command output equals some string
print("Starting Lunar Shell End-to-End Testing...\n")
print("Entering Debug Mode...")
lush.debug(true)
local rc = true

print("Testing Args...")
rc = lush.exec("args_test.lua testarg1 testarg2 testarg3")
if rc == false then
	lush.exit()
end

print("\nTesting Chaining...")
rc = lush.exec("chaining_test.lua")
if rc == false then
	lush.exit()
end

print("\nTesting File Checks...")
rc = lush.exec("filecheck_test.lua")
if rc == false then
	lush.exit()
end

print("\nTesting History...")
rc = lush.exec("history_test.lua")
if rc == false then
	lush.exit()
end

print("\nTesting Environment Variables...")
rc = lush.exec("env_test.lua")
if rc == false then
	lush.exit()
end

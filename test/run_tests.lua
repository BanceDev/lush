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

print("Starting Lunar Shell End-to-End Testing...\n")
print("Entering Debug Mode...")
lush.debug(true)

print("Testing Args...")
lush.exec("args_test.lua testarg1 testarg2 testarg3")

print("\nTesting Piping...")
lush.exec("pipes_test.lua")

print("\nTesting File Checks...")
lush.exec("filecheck_test.lua")

print("\nTesting History...")
lush.exec("history_test.lua")

print("\nTesting Environment Variables...")
lush.exec("env_test.lua")

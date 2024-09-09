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

-- This file is your init.lua. It will be automatically executed each time you open
-- a new session of Lunar Shell.

-- Setting environment variables
local path = lush.getenv("HOME") .. "/bin:" .. lush.getenv("PATH")
lush.setenv("PATH", path)

-- the prompt can be customized here too
-- %u is username, %h is hostname, %w is current working directory
lush.setPrompt("[%u@%h: %w]")
-- any global functions that accept either no parameters or just an array of args
-- will be read in and stored as built in commands for the shell
function my_command(args)
	print("my custom command")
end

-- all functions from the Lunar Shell Lua API are available to you to
-- customize your startup however you want

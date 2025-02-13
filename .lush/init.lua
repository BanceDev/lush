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

-- you can choose to enable/disable inline suggestions
lush.suggestions(true)

-- the prompt can be customized here too
-- %u is username, %h is hostname, %w is current working directory
-- %t is current time in hr:min:sec, %d is date in MM/DD/YYYY
lush.setPrompt("[%u@%h: %w]")

-- aliases can be defined using the alias method by passing the alias name
-- and the command to execute with the alias
lush.alias("h", "help")

-- you can set a backup shell for functionality not supported by Lunar Shell
-- lush.altShell("bash")

-- all functions from the Lunar Shell Lua API are available to you to
-- customize your startup however you want

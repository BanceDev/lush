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

lush.setenv("ENVTEST", "envtest")
if lush.getenv("ENVTEST") == "envtest" then
	print("setenv test passed ✅\n")
else
	print("setenv test failed ❌\n")
	lush.exit()
end

lush.unsetenv("ENVTEST")
if lush.getenv("ENVTEST") == nil then
	print("unsetenv test passed ✅\n")
else
	print("unsetenv test failed ❌\n")
	lush.exit()
end

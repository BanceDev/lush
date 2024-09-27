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

if lush.lastHistory() == "history_test.lua" then
	print("lastHistory test passed ✅\n")
else
	print("lastHistory test failed ❌\n")
end

if lush.getHistory(1) == lush.lastHistory() then
	-- ensure that piping history is stored correctly
	if lush.getHistory(9) == 'cat "example.lua" | grep "hello" | sort | uniq' then
		-- ensure args history is stored correctly
		if lush.getHistory(11) == "args_test.lua testarg1 testarg2 testarg3" then
			print("getHistory test passed ✅\n")
		else
			print("getHistory test failed at args history ❌\n")
		end
	else
		print("getHistory test failed at piping history ❌\n")
	end
else
	print("getHistory test failed at lastHistory ❌\n")
end

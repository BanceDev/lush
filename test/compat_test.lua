-- test/compat_test.lua
-- A small test suite for the lua-compat-5.3 layer.
-- Can be run in two ways:
-- 1. ./lush test/compat_test.lua (runs all tests)
-- 2. ./lush test/compat_test.lua test_table_unpack (runs a single test)

local tests = {}

function tests.test_table_unpack()
  print("--- Running test: table.unpack ---")
  local my_table = { "a", "b", "c" }
  -- The compat layer provides table.unpack for Lua 5.1.
  local x, y, z = table.unpack(my_table)
  assert(x == "a", "unpack failed for first element")
  assert(y == "b", "unpack failed for second element")
  assert(z == "c", "unpack failed for third element")
  print("...SUCCESS!")
end

function tests.test_math_log_base()
  print("--- Running test: math.log with base ---")
  -- Lua 5.1's math.log only takes one argument. The compat layer adds the base.
  local result = math.log(100, 10)
  -- Use a small epsilon for floating point comparison
  assert(math.abs(result - 2) < 1e-9, "math.log(100, 10) should be 2")
  print("...SUCCESS!")
end

function tests.test_string_rep_separator()
    print("--- Running test: string.rep with separator ---")
    -- Lua 5.1's string.rep doesn't have the separator argument.
    local result = string.rep("a", 3, "-")
    assert(result == "a-a-a", "string.rep with separator failed, got: " .. tostring(result))
    print("...SUCCESS!")
end


-- NOTE: The lush C host provides arguments in a global table named 'args', not 'arg'.
local test_to_run = args and args[1]

if test_to_run and tests[test_to_run] then
  -- If a valid test name is provided, run only that test.
  local success, err = pcall(tests[test_to_run])
  if not success then
    print("...FAILURE: " .. err)
  end
elseif test_to_run then
  -- If an invalid name is provided, show an error.
  print("ERROR: Test function '" .. test_to_run .. "' not found.")
else
  -- If no specific test is requested, run all of them.
  print("--- Running all compatibility tests ---")
  for name, func in pairs(tests) do
    local success, err = pcall(func)
    if not success then
      print("...FAILURE on test '" .. name .. "': " .. err)
    end
  end
  print("--- All tests complete ---")
end

<p align="center">
  <img width="256" height=auto src="https://github.com/BanceDev/lush/blob/main/logo.png">
  <br/>
  <img src="https://img.shields.io/github/contributors/bancedev/lush" alt="contributors">
  <img src="https://img.shields.io/github/license/bancedev/lush" alt="license">
  <img src="https://img.shields.io/github/forks/bancedev/lush" alt="forks">
</p>

---

# Lunar Shell

Lunar Shell (lush) is an open source unix shell with a single goal in mind. That goal is to offer the ability to write shell scripts for your operating system entirely in Lua. The Lua scripting language has many powerful features that allow for more control in the hands of the user to automate tasks on their machine.

## Compiling/Installation

Clone the repo and run the install script to get the development version. If you want the most recent stable version download the source code zip from the most recent [release](https://github.com/BanceDev/lush/releases). Extract it and run the install script. This will also automatically set your system shell to Lunar Shell, you will need to log out and back in for the changes to take effect.

```
git clone https://github.com/BanceDev/lush.git
cd lush
sh install.sh
```

To update Lunar Shell pull the repo/download the newest release and then run the install script again.

## Lua Shell Scripting

<p align="center">
  <img width="512" height=auto src="https://github.com/BanceDev/lush/blob/main/lua_scripting.gif">
</p>

```lua
-- example lua script to compile a C file
if args ~= nil and args[1] ~= nil then
	if args[1]:match("%.c$") then
		lush.exec("gcc -o " .. args[1]:sub(1, -3) .. " " .. args[1])
	end
else
	print("must pass a C file to compile")git
end
```

With the robust and ever growing Lua API that Lunar Shell has builtin, not only can you create powerful shell scripts to automate your workflow but also reap the benefits of having an easy to understand scripting language embedded into your command line.

To run a Lua script with Lunar Shell just type the name of the lua file you want to run followed by any arguments you want to pass to the script. Lunar Shell will automatically search the current working directory as well as the ```~/.lush/scripts``` directory and then execute the file if it locates a match.

Lunar Shell also entirely supports the Lua interpreter, running on version 5.4. This means you can also just run Lua programs you write like native apps in your shell, no need to make any calls to the API.

## Using the Lunar Shell Lua API

Using the Lunar Shell API to make your own scripts is super simple. Upon installing Lunar Shell you can find an example script located at ```~/.lush/scripts/example.lua``` this script acts as a self documenting guide on how to use the API. Every function that exists in the API can be found in the example script, along with helpful comments to explain what the functions do.

## Contributing

- Before opening an issue or a PR please check out the [contributing guide](https://github.com/BanceDev/lush/blob/main/CONTRIBUTING.md).
- For bug reports and feature suggestions please use [issues](https://github.com/BanceDev/lush/issues).
- If you wish to contribute code of your own please submit a [pull request](https://github.com/BanceDev/lush/pulls).
- Note: It is likely best to submit an issue before a PR to see if the feature is wanted before spending time making a commit.
- All help is welcome!

## License

This project is licensed under the BSD 3-Clause License.

The BSD 3-Clause License allows for redistribution and use in source and binary forms, with or without modification, provided the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions, and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions, and the following disclaimer in the documentation and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

For more details, see the [LICENSE](./LICENSE) file.


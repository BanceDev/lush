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

Clone the repo and run the install script. This will also automatically set your system shell to Lunar Shell, you will need to log out and back in for the changes to take effect.

```
git clone https://github.com/BanceDev/lush.git
cd lush
sh install.sh
```

To update Lunar Shell pull the repo and run the install script again.

## Lua Shell Scripting

<p align="center">
  <img width="256" height=auto src="https://github.com/BanceDev/lush/blob/main/demo.png">
</p>

With the robust and ever growing Lua API that Lunar Shell has builtin, not only can you create powerful shell scripts to automate your workflow but also reap the benefits of having an easy to understand scripting language embedded into your command line.

To run a Lua script with Lunar Shell just type the name of the lua file you want to run followed by any arguments you want to pass to the script. Lunar Shell will automatically search the current working directory as well as the ~/.lush/scripts directory and then execute the file if it locates a match.

Lunar Shell also entirely supports the Lua interpreter, running on version 5.4. This means you can also just run Lua programs you write like native apps in your shell, no need to make any calls to the API.

## Contributing

- Before opening an issue or a PR please check out the [contributing guide](https://github.com/BanceDev/lush/blob/main/CONTRIBUTING.md).
- For bug reports and feature suggestions please use [issues](https://github.com/BanceDev/lush/issues).
- If you wish to contribute code of your own please submit a [pull request](https://github.com/BanceDev/lush/pulls).
- Note: It is likely best to submit an issue before a PR to see if the feature is wanted before spending time making a commit.
- All help is welcome!

#!/bin/sh

premake5 gmake
make
if [ ! -d ~/.lush ]; then
    cp -rf ./.lush ~/
fi

# always update example
cp -f ./.lush/scripts/example.lua ~/.lush/scripts/example.lua

# Install the new shell binary to a temporary location
sudo cp ./bin/Debug/lush/lush /usr/bin/lush.new

# Atomically replace the old binary
sudo mv /usr/bin/lush.new /usr/bin/lush

# Ensure the shell is registered in /etc/shells
if ! grep -Fxq "/usr/bin/lush" /etc/shells; then
    echo "/usr/bin/lush" | sudo tee -a /etc/shells >/dev/null
fi

echo "====================="
echo "   UPDATE FINISHED   "
echo "====================="

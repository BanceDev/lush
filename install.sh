#!/bin/sh

PREMAKE_VERSION="5.0.0-beta2"
OS="linux"

echo "downloading premake $PREMAKE_VERSION"
wget -q https://github.com/premake/premake-core/releases/download/v${PREMAKE_VERSION}/premake-${PREMAKE_VERSION}-${OS}.tar.gz -O premake.tar.gz
echo "extracting premake"
tar -xzf premake.tar.gz
echo "installing premake"
sudo mv premake5 example.so libluasocket.so /usr/bin
sudo chmod +x /usr/bin/premake5
rm premake.tar.gz

premake5 gmake
make

# Install the new shell binary to a temporary location
sudo cp ./bin/Debug/lush/lush /usr/bin/lush.new

# Atomically replace the old binary
sudo mv /usr/bin/lush.new /usr/bin/lush

# Ensure the shell is registered in /etc/shells
if ! grep -Fxq "/usr/bin/lush" /etc/shells; then
    echo "/usr/bin/lush" | sudo tee -a /etc/shells >/dev/null
fi

# Optionally change the shell
chsh -s /usr/bin/lush

echo "====================="
echo "INSTALLATION FINISHED"
echo "====================="

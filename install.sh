#!/bin/sh

echo "Initializing and updating submodules..."
git submodule update --init --recursive

# Function to install packages using apt (Debian/Ubuntu)
install_with_apt() {
    sudo apt-get update
    sudo apt-get install -y lua5.4 liblua5.4-dev
}

# Function to install packages using yum (Red Hat/CentOS)
install_with_yum() {
    sudo yum install -y epel-release
    sudo yum install -y lua lua-devel
}

# New package manager used in Fedora
install_with_dnf() {
    sudo dnf install -y lua lua-devel
}

# Function to install packages using pacman (Arch Linux)
install_with_pacman() {
    sudo pacman -Sy --noconfirm lua
}

if [ -f /etc/arch-release ]; then
    install_with_pacman
elif [ -f /etc/debian_version ]; then
    install_with_apt
elif [ -f /etc/redhat-release ] || [ -f /etc/centos-release ]; then
    if command -v dnf >/dev/null 2>&1; then
        install_with_dnf
    else
        install_with_yum
    fi
else
    echo "Your linux distro is not supported currently."
    echo "You need to manually install these packages: lua and your distro's lua dev package"
fi

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
if [ ! -d ~/.config/lush ]; then
    cp -rf ./lush ~/.config/
fi

# always update example
cp -f ./lush/scripts/example.lua ~/config/lush/scripts/example.lua

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

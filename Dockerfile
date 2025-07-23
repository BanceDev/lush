# Use a base image with build tools
FROM ubuntu:22.04

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install necessary build tools for your C project
RUN apt-get update && apt-get install -y \
    build-essential \
    wget \
    unzip \
    git \
    lua5.4 \
    liblua5.4-dev \
    && rm -rf /var/lib/apt/lists/*

# Download and install Premake5
RUN wget https://github.com/premake/premake-core/releases/download/v5.0.0-beta2/premake-5.0.0-beta2-linux.tar.gz -O premake.tar.gz && \
    tar -xvf premake.tar.gz && \
    mv premake5 /usr/local/bin/

# Set the working directory inside the container
WORKDIR /app

COPY . .

# Initialize git submodules inside the container
RUN git submodule update --init --recursive

# Creates the .lush config directory in the root user home directory as install.sh expects.
RUN mkdir -p /root/.lush && cp -r ./.lush/* /root/.lush/

CMD ["premake5", "gmake2"]


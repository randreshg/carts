FROM ubuntu:22.04

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies (SSH server/client, build tools, and compilers)
RUN apt-get update && apt-get install -y \
    build-essential \
    ninja-build \
    git \
    binutils \
    lld \
    python3 \
    python3-pip \
    openssh-server \
    openssh-client \
    gdb \
    ca-certificates \
    curl \
    vim \
    wget \
    gnupg \
    lsb-release \
    software-properties-common \
    autotools-dev \
    autoconf \
    automake \
    libtool \
    && rm -rf /var/lib/apt/lists/*

# Install newer CMake (3.20+ required by CARTS)
RUN wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null && \
    echo 'deb https://apt.kitware.com/ubuntu/ jammy main' | tee /etc/apt/sources.list.d/kitware.list && \
    apt-get update && apt-get install -y cmake && \
    rm -rf /var/lib/apt/lists/*

# Install LLVM 18 (required by CARTS)
RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor -o /etc/apt/trusted.gpg.d/llvm-snapshot.gpg && \
    echo "deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-18 main" >> /etc/apt/sources.list && \
    apt-get update && apt-get install -y \
    llvm-18 \
    llvm-18-dev \
    clang-18 \
    clang++-18 \
    libclang-rt-18-dev \
    libasan6 \
    libtsan2 \
    libubsan1 \
    && rm -rf /var/lib/apt/lists/*

# Create symlinks for easier access
RUN ln -s /usr/bin/clang-18 /usr/bin/clang && \
    ln -s /usr/bin/clang++-18 /usr/bin/clang++ && \
    ln -s /usr/bin/llvm-config-18 /usr/bin/llvm-config

# Install Node.js and npm (latest LTS version)
RUN curl -fsSL https://deb.nodesource.com/setup_lts.x | bash - && \
    apt-get install -y nodejs && \
    rm -rf /var/lib/apt/lists/*

# Verify Node.js and npm installation
RUN node --version && npm --version

# Configure SSH for passwordless access
RUN mkdir /var/run/sshd && \
    echo 'root:arts' | chpasswd && \
    sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config && \
    sed -i 's/#PasswordAuthentication yes/PasswordAuthentication yes/' /etc/ssh/sshd_config

# Generate SSH host keys (server) and root client keys
RUN ssh-keygen -A && \
    mkdir -p /root/.ssh && chmod 700 /root/.ssh && \
    ssh-keygen -t ed25519 -N "" -f /root/.ssh/id_ed25519 && \
    cat /root/.ssh/id_ed25519.pub >> /root/.ssh/authorized_keys && \
    chmod 600 /root/.ssh/authorized_keys

# Pre-seed known_hosts with cluster hostnames using the baked host key
RUN set -e; \
    mkdir -p /root/.ssh; chmod 700 /root/.ssh; \
    HOST_PUB=$(cat /etc/ssh/ssh_host_ed25519_key.pub); \
    printf "arts-node-1 %s\n" "$HOST_PUB" > /root/.ssh/known_hosts; \
    printf "arts-node-2 %s\n" "$HOST_PUB" >> /root/.ssh/known_hosts; \
    printf "arts-node-3 %s\n" "$HOST_PUB" >> /root/.ssh/known_hosts; \
    printf "arts-node-4 %s\n" "$HOST_PUB" >> /root/.ssh/known_hosts; \
    printf "arts-node-5 %s\n" "$HOST_PUB" >> /root/.ssh/known_hosts; \
    printf "arts-node-6 %s\n" "$HOST_PUB" >> /root/.ssh/known_hosts; \
    chmod 644 /root/.ssh/known_hosts

# Configure git for faster operations
RUN git config --global init.defaultBranch main \
    && git config --global pull.rebase false \
    && git config --global core.preloadindex true \
    && git config --global core.fscache true \
    && git config --global gc.auto 256 \
    && git config --global http.postBuffer 1048576000 \
    && git config --global http.maxRequestBuffer 100M \
    && git config --global core.compression 0

# Default working directory
WORKDIR /root

# Environment variables
ENV PATH="/opt/carts/tools:$PATH"


# Expose SSH port
EXPOSE 22

# Set working directory
WORKDIR /workspace

# Command to keep container running and start SSH daemon
CMD ["/usr/sbin/sshd", "-D"]

FROM ubuntu:22.04

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies (build tools, compilers, and utilities)
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
    clangd-18 \
    libclang-rt-18-dev \
    libasan6 \
    libtsan2 \
    libubsan1 \
    && rm -rf /var/lib/apt/lists/*

# Create symlinks for easier access
RUN ln -s /usr/bin/clang-18 /usr/bin/clang && \
    ln -s /usr/bin/clang++-18 /usr/bin/clang++ && \
    ln -s /usr/bin/clangd-18 /usr/bin/clangd && \
    ln -s /usr/bin/llvm-config-18 /usr/bin/llvm-config

# Install Poetry (Python dependency manager)
RUN curl -sSL https://install.python-poetry.org | python3 - && \
    ln -s /root/.local/bin/poetry /usr/local/bin/poetry

# Install Node.js and npm (latest LTS version)
RUN curl -fsSL https://deb.nodesource.com/setup_lts.x | bash - && \
    apt-get install -y nodejs && \
    rm -rf /var/lib/apt/lists/*

# Verify Node.js and npm installation
RUN node --version && npm --version

# Configure git for faster operations
RUN git config --global init.defaultBranch main \
    && git config --global pull.rebase false \
    && git config --global core.preloadindex true \
    && git config --global core.fscache true \
    && git config --global gc.auto 256 \
    && git config --global http.postBuffer 1048576000 \
    && git config --global http.maxRequestBuffer 100M \
    && git config --global core.compression 0

# Install Slurm and Munge for simulated cluster scheduling
RUN apt-get update && apt-get install -y \
    slurm-wlm \
    munge \
    && rm -rf /var/lib/apt/lists/*

# Generate munge key (baked into image = same key on all containers)
RUN dd if=/dev/urandom bs=1 count=1024 > /etc/munge/munge.key 2>/dev/null && \
    chown munge:munge /etc/munge/munge.key && \
    chmod 400 /etc/munge/munge.key

# Create Slurm spool and log directories
RUN mkdir -p /var/spool/slurmctld /var/spool/slurmd /var/log/slurm && \
    chmod 755 /var/spool/slurmctld /var/spool/slurmd /var/log/slurm

# Copy Slurm configuration
COPY slurm.conf /etc/slurm/slurm.conf

# Default working directory
WORKDIR /root

# Environment variables
ENV PATH="/opt/carts/tools:$PATH"


# Set working directory
WORKDIR /workspace

# Keep container running
CMD ["sleep", "infinity"]

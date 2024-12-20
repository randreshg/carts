# DESCRIPTION
# - Makefile for CARTS
# - Dependencies: ARTS, LLVM, Polygeist
# - Required: CMake, Ninja, Clang, Clang++


# Source Directories
CARTS_DIR = ${shell pwd}
ARTS_DIR ?= ${CARTS_DIR}/external/arts
POLYGEIST_DIR ?= ${CARTS_DIR}/external/Polygeist
LLVM_DIR ?= ${POLYGEIST_DIR}/llvm-project

# Install Directories
CARTS_INSTALL_DIR ?= ${CARTS_DIR}/.install
ARTS_INSTALL_DIR ?=$(CARTS_INSTALL_DIR)/arts
LLVM_INSTALL_DIR ?=$(CARTS_INSTALL_DIR)/llvm
POLYGEIST_INSTALL_DIR ?=$(CARTS_INSTALL_DIR)/polygeist

# Build Directories
CARTS_BUILD_DIR=.build
ARTS_BUILD_DIR =$(ARTS_DIR)/build
POLYGEIST_BUILD_DIR =$(POLYGEIST_DIR)/build
LLVM_BUILD_DIR =$(LLVM_DIR)/build

# Targets
# all: hooks postinstall

installdeps: arts
	@echo "Installing dependencies"
	@echo "Installing LLVM"
	@make llvm
	@echo "Installing Polygeist"
	@make polygeist
enable:
	echo "export PATH=$(LLVM_INSTALL_DIR)/bin:\$$PATH" > enable
	echo "export LD_LIBRARY_PATH=/home/randres/projects/CARTS/.install/polygeist/lib:LD_LIBRARY_PATH" >> enable

activate:
	echo "export PATH=$(POLYGEIST_INSTALL_DIR)/bin:$(LLVM_INSTALL_DIR)/bin:\$$PATH" > enable
	echo "export LD_LIBRARY_PATH=$(POLYGEIST_INSTALL_DIR)/lib:$(LLVM_INSTALL_DIR)/lib:\$$LD_LIBRARY_PATH" >> enable

# LLVM
llvm: .llvm
.llvm:
	mkdir -p $(LLVM_BUILD_DIR)
	mkdir -p $(LLVM_INSTALL_DIR)
	cmake -B $(LLVM_BUILD_DIR) \
		-S $(LLVM_DIR)/llvm -G Ninja \
		-DCMAKE_INSTALL_PREFIX=$(LLVM_INSTALL_DIR) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_C_COMPILER=clang \
		-DCMAKE_CXX_COMPILER=clang++ \
		-DLLVM_ENABLE_PROJECTS='mlir;clang' \
		-DLLVM_OPTIMIZED_TABLEGEN=ON \
		-DLLVM_TARGETS_TO_BUILD="host" \
		-DLLVM_USE_LINKER=lld \
		-DLLVM_ENABLE_ASSERTIONS=ON \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON 
	ninja -C $(LLVM_BUILD_DIR) install
llvm-clean:
	# [[ -d $(LLVM_DIR) ]] && make -C $(LLVM_DIR) uninstall
	[[ -d $(LLVM_DIR) ]] && rm -rf $(LLVM_DIR)
	rm -f -r $(LLVM_INSTALL_DIR)

# Polygeist
polygeist-download:$(POLYGEIST_DIR)
	mkdir -p $@
	git clone --recursive https://github.com/llvm/Polygeist $@
polygeist: .polygeist
.polygeist: 
	mkdir -p $(POLYGEIST_BUILD_DIR)
	mkdir -p $(POLYGEIST_INSTALL_DIR)
	cmake -B $(POLYGEIST_BUILD_DIR) \
		-S $(POLYGEIST_DIR) -G Ninja \
		-DCMAKE_INSTALL_PREFIX=$(POLYGEIST_INSTALL_DIR) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_C_COMPILER=clang \
		-DCMAKE_CXX_COMPILER=clang++ \
		-DMLIR_DIR=$(LLVM_BUILD_DIR)/lib/cmake/mlir \
		-DClang_DIR=$(LLVM_BUILD_DIR)/lib/cmake/clang/ \
		-DLLVM_EXTERNAL_LIT="$(LLVM_BUILD_DIR)/bin/llvm-lit" \
		-DLLVM_USE_LINKER=lld \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON 
	ninja -C $(POLYGEIST_BUILD_DIR) install

# ARTS
$(ARTS_DIR):
	mkdir -p $@
	git clone --depth 1 https://github.com/randreshg/ARTS.git $@
arts: .arts
.arts: $(ARTS_DIR)
	mkdir -p $</build
	mkdir -p $(ARTS_INSTALL_DIR)
	cmake -B $</build -S $< \
		-DCMAKE_C_COMPILER=clang \
		-DCMAKE_CXX_COMPILER=clang++ \
		-DCMAKE_INSTALL_PREFIX=$(ARTS_INSTALL_DIR)
	make -C $</build all -j
	make -C $</build install -j
arts-clean:
	rm -f -r $(ARTS_DIR)
	rm -f -r $(ARTS_INSTALL_DIR)

# CARTS
build:
	mkdir -p $(BUILD_DIR) -S . \
	mkdir -p $(CARTS_INSTALL_DIR)
	cmake -B $(BUILD_DIR)
		-DCMAKE_C_COMPILER=clang \
		-DCMAKE_CXX_COMPILER=clang++ \
		-DCMAKE_INSTALL_PREFIX=$(CARTS_INSTALL_DIR)/carts
	make -C $(BUILD_DIR) all -j

install: build
	make -C $(BUILD_DIR) install -j

uninstall:
	-cat $(BUILD_DIR)/install_manifest.txt | xargs rm -f -r
	rm -f enable
	rm -rf $(BUILD_DIR)

fulluninstall: uninstall arts-clean
	rm -f .arts

clean:
	make -C $(BUILD_DIR) clean -j
	rm -rf $(BUILD_DIR)

.PHONY: all build install postinstall uninstall fulluninstall  clean test

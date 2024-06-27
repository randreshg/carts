ARTS_DIR ?= external/arts
LLVM_DIR ?= external/llvm

BUILD_DIR=.build
HOOKS_DIR=.githooks
CARTS_INSTALL_DIR ?= ${shell pwd}/.install
ARTS_INSTALL_DIR ?=$(CARTS_INSTALL_DIR)/arts
LLVM_INSTALL_DIR ?=$(CARTS_INSTALL_DIR)/llvm

NORM_RUNTIME=$(CARTS_INSTALL_DIR)/bin/CARTS-norm-runtime
RUNTIME_BC=$(CARTS_INSTALL_DIR)/lib/CARTS.impl.bc
DECL_BC=$(CARTS_INSTALL_DIR)/lib/CARTS.decl.bc


all: hooks postinstall

# EXTERNAL DEPENDENCIES
installdeps: arts

# ENABLE
enable:
	echo "export PATH=$(LLVM_INSTALL_DIR)/bin:\$$PATH" > enable
	echo "export LD_LIBRARY_PATH=$(LLVM_INSTALL_DIR)/lib:\$$LD_LIBRARY_PATH" >> enable

# LLVM
$(LLVM_DIR):
	mkdir -p $@
	[[ -z "$(shell ls -A $@)" ]] &&  git clone --depth 1 --branch release/18.x https://github.com/llvm/llvm-project.git $@
llvm: .llvm
.llvm: $(LLVM_DIR)
	mkdir -p $</build
	mkdir -p $(LLVM_INSTALL_DIR)
	cmake -B $</build -S $</llvm -G Ninja \
		-DCMAKE_INSTALL_PREFIX=$(LLVM_INSTALL_DIR) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_C_COMPILER=clang \
		-DCMAKE_CXX_COMPILER=clang++ \
		-DLLVM_ENABLE_RUNTIMES='openmp' \
		-DLLVM_ENABLE_PROJECTS='clang;compiler-rt' \
		-DLLVM_ENABLE_BACKTRACES=ON \
		-DLLVM_APPEND_VC_REV=OFF \
		-DLLVM_ENABLE_ASSERTIONS=ON \
		-DBUILD_SHARED_LIBS=ON \
		-DLLVM_OPTIMIZED_TABLEGEN=ON \
		-DLLVM_CCACHE_BUILD=OFF \
		-DCLANG_ENABLE_STATIC_ANALYZER=ON  \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON 
	ninja -C $</build -j32 install
llvm-clean:
	# [[ -d $(LLVM_DIR) ]] && make -C $(LLVM_DIR) uninstall
	[[ -d $(LLVM_DIR) ]] && rm -rf $(LLVM_DIR)
	rm -f -r $(LLVM_INSTALL_DIR)


# ARTS
$(ARTS_DIR):
	mkdir -p $@
	git clone --depth 1 https://github.com/randreshg/ARTS.git $@
arts: .arts
.arts: $(ARTS_DIR)
	mkdir -p $</build
	mkdir -p $(ARTS_INSTALL_DIR)
	cmake -B $</build -S $< -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_INSTALL_PREFIX=$(ARTS_INSTALL_DIR)
arts-clean:
	rm -f -r $(ARTS_DIR)
	rm -f -r $(ARTS_INSTALL_DIR)

# CARTS
build:
	mkdir -p $(BUILD_DIR)
	mkdir -p $(CARTS_INSTALL_DIR)
	cmake -DCMAKE_INSTALL_PREFIX=$(CARTS_INSTALL_DIR)/carts \
				-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
				-S . -B $(BUILD_DIR)
	make -C $(BUILD_DIR) all -j32

install: build
	make -C $(BUILD_DIR) install -j32

uninstall:
	-cat $(BUILD_DIR)/install_manifest.txt | xargs rm -f
	rm -f enable
	rm -rf $(BUILD_DIR)

fulluninstall: uninstall arts-clean
	rm -f .arts

clean:
	make -C $(BUILD_DIR) clean -j32
	rm -rf $(BUILD_DIR)

.PHONY: all build install postinstall uninstall fulluninstall  clean test

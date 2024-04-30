NOELLE_DIR ?= external/noelle
ARTS_DIR ?= external/arts

BUILD_DIR=.build
HOOKS_DIR=.githooks
CARTS_INSTALL_DIR ?= ${shell pwd}/.install
NOELLE_INSTALL_DIR ?=$(CARTS_INSTALL_DIR)/noelle
ARTS_INSTALL_DIR ?=$(CARTS_INSTALL_DIR)/arts

NORM_RUNTIME=$(CARTS_INSTALL_DIR)/bin/CARTS-norm-runtime
RUNTIME_BC=$(CARTS_INSTALL_DIR)/lib/CARTS.impl.bc
DECL_BC=$(CARTS_INSTALL_DIR)/lib/CARTS.decl.bc

all: noelle hooks postinstall

# EXTERNAL DEPENDENCIES
installdeps: arts noelle

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

# NOELLE
$(NOELLE_DIR):
	mkdir -p $@
	git clone --depth 1 --branch v14 https://github.com/randreshg/noelle.git $@
noelle: .noelle
.noelle: $(NOELLE_DIR)
	mkdir -p $(NOELLE_INSTALL_DIR)
	NOELLE_INSTALL_DIR=$(NOELLE_INSTALL_DIR) NOELLE_SCAF=OFF NOELLE_SVF=OFF NOELLE_AUTOTUNER=OFF make -C $<
noelle-clean:
	[[ -d $(NOELLE_DIR) ]] && make -C $(NOELLE_DIR) uninstall
	[[ -d $(NOELLE_DIR) ]] && rm -rf $(NOELLE_DIR)
	rm -f -r .noelle

# CARTS
build:
	mkdir -p $(BUILD_DIR)
	mkdir -p $(CARTS_INSTALL_DIR)
	cmake -DCMAKE_INSTALL_PREFIX=$(CARTS_INSTALL_DIR) -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DNOELLE_INSTALL_DIR=$(NOELLE_INSTALL_DIR) -S . -B $(BUILD_DIR)
	make -C $(BUILD_DIR) all -j32

install: build
	make -C $(BUILD_DIR) install -j32

postinstall: install
	$(NORM_RUNTIME) $(RUNTIME_BC) $(DECL_BC)
	@cp $(BUILD_DIR)/compile_commands.json .

benchmark: all
	make -C $(BUILD_DIR) bitcodes -j32

test:
	make -C $(BUILD_DIR) tests -j32
	cd $(BUILD_DIR) && ctest

documentation: all
	make -C $(BUILD_DIR)/docs documentation -j32 

hooks:
	make -C $(HOOKS_DIR) all



uninstall:
	-cat $(BUILD_DIR)/install_manifest.txt | xargs rm -f
	rm -f enable
	rm -rf $(BUILD_DIR)

# NOELLE
fulluninstall: uninstall noelle-clean arts-clean
	rm -f .noelle
	rm -f .arts

clean:
	make -C $(BUILD_DIR) clean -j32
	rm -rf $(BUILD_DIR)

.PHONY: all noelle build install postinstall uninstall fulluninstall  clean test

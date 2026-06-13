// RUN: %carts-compile %s --O3 --arts-config %arts_config --start-from=sde-planning --pipeline=sde-planning | %FileCheck %s --check-prefix=SDE
// RUN: %carts-compile %s --O3 --arts-config %arts_config --start-from=sde-planning --pipeline=sde-to-arts | %FileCheck %s --check-prefix=ARTS --implicit-check-not=sde.mu_access_window

// SDE production creates a committed MU access-window carrier, and the direct
// SDE-to-ARTS boundary consumes it rather than letting it cross the dialect
// frontier.

// SDE-LABEL: func.func @sde_to_arts_production_carrier
// SDE: sde.su_iterate
// SDE: sde.cu_region <parallel>
// SDE: sde.mu_access_window write %{{.*}} : memref<4x256xf32>

// ARTS-LABEL: func.func @sde_to_arts_production_carrier
// ARTS: arts.db_alloc
// ARTS-SAME: <block>
// ARTS-SAME: elementSizes
// ARTS: arts.db_acquire
// ARTS-SAME: partitioning(<block>)
// ARTS: arts.edt

func.func @sde_to_arts_production_carrier() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c1024 = arith.constant 1024 : index
  %cst = arith.constant 1.0 : f32
  %A = sde.mu_alloc : memref<1024xf32>
	    sde.su_iterate (%c0) to (%c1024) step (%c1) classification(<elementwise>) {
	    ^bb0(%i: index):
	      sde.cu_region <parallel> {
	        memref.store %cst, %A[%i] : memref<1024xf32>
	      }
	    } {physicalOwnerDims = [0], physicalBlockShape = [256]}
  return
}

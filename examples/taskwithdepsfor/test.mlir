module {
  // Function to sum elements of a memref
  func.func @sum_elements(%array: memref<?xf32>) -> f32 {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %sum = arith.constant 0.0 : f32

    // Query the size of the first dimension
    %size = memref.dim %array, %c0 : memref<?xf32>

    // Loop over the array
    scf.for %i = %c0 to %size step %c1 iter_args(%current_sum = %sum) -> (f32) {
      %value = memref.load %array[%i] : memref<?xf32>
      %new_sum = arith.addf %current_sum, %value : f32
      scf.yield %new_sum : f32
    }

    return %sum : f32
  }

  // Main function
  func.func @main() -> i32 {
    // Define the size of the array
    %size = arith.constant 5 : index

    // Allocate a memref with the given size
    %array = memref.alloc(%size) : memref<?xf32>

    // Initialize the array with values
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %value1 = arith.constant 1.0 : f32
    %value2 = arith.constant 2.0 : f32
    %value3 = arith.constant 3.0 : f32
    %value4 = arith.constant 4.0 : f32
    %value5 = arith.constant 5.0 : f32
    memref.store %value1, %array[%c0] : memref<?xf32>
    memref.store %value2, %array[%c1] : memref<?xf32>
    %c2 = arith.addi %c1, %c1 : index
    memref.store %value3, %array[%c2] : memref<?xf32>
    %c3 = arith.addi %c2, %c1 : index
    memref.store %value4, %array[%c3] : memref<?xf32>
    %c4 = arith.addi %c3, %c1 : index
    memref.store %value5, %array[%c4] : memref<?xf32>

    // Call the sum_elements function
    %sum = call @sum_elements(%array) : (memref<?xf32>) -> f32

    // Free the array
    memref.dealloc %array : memref<?xf32>

    // Return 0 as an integer
    %return_value = arith.constant 0 : i32
    return %return_value : i32
  }
}
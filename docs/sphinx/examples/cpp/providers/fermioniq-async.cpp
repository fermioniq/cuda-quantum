// Compile and run with:
// ```
// nvq++ --target fermioniq fermioniq.cpp -o out.x && ./out.x
// ```
// This will submit the job to the fermioniq emulator.
// ```
// nvq++ --target fermioniq
// fermioniq.cpp -o out.x && ./out.x
// ```

#include <cudaq.h>
#include <fstream>

// Define a simple quantum kernel to execute on fermioniq.
struct ghz {
  // Maximally entangled state between 5 qubits.
  auto operator()() __qpu__ {
    cudaq::qvector q(3);
    h(q[0]);
    for (int i = 0; i < 2; i++) {
      x<cudaq::ctrl>(q[i], q[i + 1]);
    }
    auto result = mz(q);
  }
};

int main() {
  // Submit to IonQ asynchronously (e.g., continue executing
  // code in the file until the job has been returned).
  auto future = cudaq::sample_async(ghz{});
  // ... classical code to execute in the meantime ...

  // Can write the future to file:
  {
    std::ofstream out("saveMe.json");
    out << future;
  }

  // Then come back and read it in later.
  cudaq::async_result<cudaq::sample_result> readIn;
  std::ifstream in("saveMe.json");
  in >> readIn;

  // Get the results of the read in future.
  auto async_counts = readIn.get();
  async_counts.dump();

}

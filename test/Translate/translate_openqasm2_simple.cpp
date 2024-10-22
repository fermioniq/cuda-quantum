/*******************************************************************************
 * Copyright (c) 2022 - 2024 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

// RUN: cudaq-quake  %s | cudaq-translate --convert-to=openqasm2 | FileCheck %s

#include <cudaq.h>
#include <fstream>

struct kernel {
  void operator()() __qpu__ {
    cudaq::qvector q(2);
    h(q[0]);
    x<cudaq::ctrl>(q[0], q[1]);
    mz(q);
  }
};

int main() {
  auto counts = cudaq::sample(kernel{});
  counts.dump();
}

// CHECK:  // Code generated by NVIDIA's nvq++ compiler
// CHECK:  OPENQASM 2.0;

// CHECK:  include "qelib1.inc";

// CHECK:  gate ZN6kernelclEv(param0)  {
// CHECK:  }

// CHECK:  qreg var0[2];
// CHECK:  h var0[0];
// CHECK:  cx var0[0], var0[1];
// CHECK:  creg var3[2];
// CHECK:  measure var0 -> var3;

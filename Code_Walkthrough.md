# Complete Code Walkthrough

This document explains the entire execution flow of the **Hardware Trojan Insertion Framework**, starting from `main.cpp` and diving into every function call across the codebase.

---

## 1. Entry Point: `src/main.cpp`
The execution begins in the `main()` function.

### A. Batch Directory Setup (Lines 118-120)
The program defines two directories to scan: `inputs/combinational` and `inputs/sequential`. It iterates through them using `std::filesystem`.

### B. `processFile(...)` (Line 13)
For each `.bench` file found, `main` calls `processFile`. This is the core orchestrator.

#### Step 1: Parsing the Netlist (Line 18)
It creates a `Netlist` object and calls `netlist.parse(inputPath)`.
> **Jump to `src/Netlist.cpp` -> `Netlist::parse()`**:
> *   **Line 66**: Opens the file.
> *   **Line 91**: Regex matches `INPUT(x)`. Creates a node `x` of type `INPUT`.
> *   **Line 96**: Regex matches `OUTPUT(y)`. Creates a node `y`.
> *   **Line 102**: Regex matches gates like `g = NAND(a, b)`.
> *   **Sequential Logic (Line 117)**: If the gate is `DFF` (Flip-Flop):
>     *   The output `Q` is treated as an **Input** (PPI).
>     *   The input `D` is treated as an **Output** (PPO).
>     *   *Why?* This "Full Scan Assumption" breaks loops, allowing us to test sequential circuits as if they were combinational.

#### Step 2: Simulation (Line 26)
`main` creates a `Simulator` and calls `sim.findRareNodes(10000, 0.2)`.
> **Jump to `src/Simulator.cpp` -> `Simulator::findRareNodes()`**:
> *   **Line 71**: Runs a loop 10,000 times (Monte Carlo).
> *   **Line 73**: Randomly assigns 0 or 1 to every Input.
> *   **Line 81**: Calls `evaluate(g)` for every gate.
>     *   **`evaluate()` (Line 55)**: A recursive function. It asks for input values, then calls `computeGate()` (Line 16) to calculate the logic (AND, OR, XOR, etc.).
> *   **Line 89**: It counts how often each node is `1`.
> *   **Line 104**: If a node is `1` very rarely (e.g., < 20% of the time), it marks `rare_value = 1`. If it's `0` rarely, `rare_value = 0`.
> *   **Result**: We now have a list of "Rare Nodes" (Candidates for triggers).

#### Step 3: Compatibility Analysis (Line 40)
`main` creates a `CompatibilityGraph` and calls two critical functions.
1.  **`cg.generateTestVectors(rareNodes)`**:
    > **Jump to `src/CompatibilityGraph.cpp`**:
    > *   **Line 22**: For every Rare Node, it asks: "Can I force this node to its rare value?"
    > *   It calls `podem.generateTest(node, value)`.
    >     > **Jump to `src/PODEM.cpp` -> `PODEM::generateTest()`**:
    >     > *   **Line 350**: Logic Translation. If we want a node to be `1`, we behave as if it's "Stuck-At-0" and try to detect it (setting the value to `D`).
    >     > *   **Line 354**: Calls `podemRecursion()`.
    >     >     *   **Line 276**: `getObjective()` finds a gate with an Unknown (`X`) input that needs setting to propagate the fault.
    >     >     *   **Line 280**: `backtrace()` traces backward from that gate to a Primary Input (PI) to decide what input to set.
    >     >     *   **Line 324**: It tries setting the PI to 1, then implies logic. If that fails, it backtracks and sets it to 0.
    >     >     *   **5-Valued Logic**: It uses `0`, `1`, `X`, `D`, `D_BAR` to correctly handle `XOR` gates in sequential circuits (Line 38).
    > *   **Result**: A "Test Vector" (Input Pattern) that activates the rare node.

2.  **`cg.buildGraph()`**:
    > **Jump to `src/CompatibilityGraph.cpp`**:
    > *   **Line 45**: It compares every pair of Rare Nodes.
    > *   **Line 54**: `areVectorsCompatible()` checks if Vector A and Vector B conflict (e.g., A needs Input1=0, B needs Input1=1).
    > *   If no conflict, it draws an **Edge** between them in the graph.

#### Step 4: Clique Finding (Line 55)
`main` asks the user for a "Trigger Size" (e.g., 4) and calls `cg.findCliques(4)`.
> **Jump to `src/CompatibilityGraph.cpp` -> `findCliques()`**:
> *   It uses the **Bron-Kerbosch Algorithm** (Line 65) to find a "Clique" (a group of nodes that are ALL connected to each other).
> *   **Line 67**: It has a safety limit (50,000 steps) to prevent hanging on dense graphs like `s499`.
> *   **Result**: A list of nodes that can *all* be rare simultaneously (The Trigger).

#### Step 5: Trojan Generation (Line 96)
`main` creates a `TrojanGenerator` and calls two functions.

1.  **`tg.generateTrigger(clique)`**:
    > **Jump to `src/TrojanGenerator.cpp`**:
    > *   **Line 32**: If the clique is huge (>8), it builds a **Tree** of gates (ANDs feeding ANDs) to avoid fan-in limits.
    > *   **Line 110**: Otherwise, it builds a **Flat Trigger**. It uses AND gates for Rare-1 nodes and NOR gates for Rare-0 nodes, combining them into a final single wire that goes HIGH when the Trojan triggers.

2.  **`tg.insertPayload(trigger, config)`**:
    > **Jump to `src/TrojanGenerator.cpp`**:
    > *   **Line 138**: **Victim Selection**. It traces downstream from the trigger to find a safe "Victim" wire to attack.
    > *   **Line 232**: **ID Shifting**. It calls `netlist->shiftIDs()` to rename all nodes *above* the victim. This opens up a numeric "Gap" (e.g., usually IDs are 1, 2, 3... it makes them 1, 2, ... gap ... 10, 11).
    > *   **Line 254**: **Payload Insertion**. It inserts new gates into that gap based on the user choice:
    >     *   **XOR**: Inverts the victim wire.
    >     *   **Performance**: Adds a chain of buffers (Delay).
    >     *   **Info Leak**: MUXes a secret internal node to the output.

#### Step 6: Saving (Line 106)
`main` calls `netlist.write(outputPath)`.
> **Jump to `src/Netlist.cpp` -> `Netlist::write()`**:
> *   **Line 163**: Writes `INPUT()` list.
> *   **Line 179**: Writes `OUTPUT()` list.
> *   **Line 218**: **Topological Sort**. It re-orders the gates so that `TrojanGate` acts as an input to `VictimGate`.
> *   **Line 203**: **Breaking Cycles**. Critical for sequential circuits (`s499`). It ignores `DFF` feedback loops during sorting so it can write the file as a flat list without getting stuck in an infinite loop.

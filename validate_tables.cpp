// Validation Tool for Tables 2 & 3 (Paper Replication)
// Table 2: Area Overhead (Gate Count Increase)
// Table 3: Detection Probability (Stealth against Random patterns)

#include "Netlist.h"
#include "Simulator.h"
#include "CompatibilityGraph.h"
#include "TrojanGenerator.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <iomanip>
#include <random>

namespace fs = std::filesystem;

struct TableMetrics {
    std::string circuit;
    int originalGates;
    int trojanGates;
    double areaOverhead; // %
    int triggerSize;
    int detectedCount;
    int totalVectors;
    double detectionProb;
};

TableMetrics validateBenchmark(std::string benchPath, int cliqueSize) {
    TableMetrics metrics;
    metrics.circuit = fs::path(benchPath).stem().string();
    metrics.totalVectors = 100000; // 100k vectors for Validation (Paper used 1M)
    metrics.triggerSize = cliqueSize;

    // 1. Parse Original
    Netlist netlist;
    if (!netlist.parse(benchPath)) {
        std::cerr << "Failed to parse " << benchPath << "\n";
        return metrics;
    }
    metrics.originalGates = netlist.getGates().size();

    // 2. Identify Rare Nodes & Clique (Standard Setup)
    Simulator sim(&netlist);
    sim.findRareNodes(10000, 0.20); // 10k setup
    
    std::vector<Node*> rareNodes;
    for(Node* n : netlist.getAllNodes()) {
        if(n->rare_value != -1) rareNodes.push_back(n);
    }
    
    CompatibilityGraph cg(&netlist);
    cg.generateTestVectors(rareNodes);
    cg.buildGraph();
    auto cliques = cg.findCliques(cliqueSize);
    
    if(cliques.empty()) {
        std::cout << "[WARN] No cliques of size " << cliqueSize << " for " << metrics.circuit << ". Skipping.\n";
        metrics.trojanGates = metrics.originalGates; // No change
        metrics.areaOverhead = 0.0;
        metrics.detectedCount = -1;
        return metrics;
    }
    
    // 3. Insert Trojan
    TrojanGenerator generator(&netlist);
    Node* trigger = generator.generateTrigger(cliques[0]); // Use first clique
    TrojanConfig config;
    config.type = TrojanType::FUNCTIONAL_CHANGE_XOR;
    config.triggerSize = cliqueSize;
    generator.insertPayload(trigger, config); // Insert
    
    // 4. Measure Area Overhead (Table 2)
    // Note: Netlist::getGates() might need refetching or size check
    // TrojanGenerator uses createGate which pushes to 'gates' or 'outputs'
    // 'gates' in Netlist is std::vector<Node*>.
    metrics.trojanGates = netlist.getGates().size(); 
    int diff = metrics.trojanGates - metrics.originalGates;
    metrics.areaOverhead = (double)diff * 100.0 / metrics.originalGates;
    
    std::cout << "Original: " << metrics.originalGates << ", Trojan: " << metrics.trojanGates 
              << ", Overhead: " << std::fixed << std::setprecision(2) << metrics.areaOverhead << "%\n";

    // 5. Measure Stealth (Table 3)
    // Run 1M random vectors. If trigger output == 1, it's detected.
    std::cout << "Running Stealth Simulation (" << metrics.totalVectors << " vectors)...\n";
    
    // Trigger is the node returned by generateTrigger.
    // However, for valid detection, we usually check the PAYLOAD effect.
    // But evaluating the Trigger node itself is sufficient to know if it *would* trigger.
    // If Trigger == 1, then the Trojan is Active.
    
    int activations = 0;
    
    // Optimized simulation loop
    // clearValues overhead might be high for 1M.
    // Simulator::findRareNodes does this efficiently. 
    // We can just use a tight loop here.
    
    for(int i=0; i<metrics.totalVectors; ++i) {
        sim.clearValues();
        for(Node* in : netlist.getInputs()) {
            in->value = std::rand() % 2;
        }
        // Only evaluate cone of Trigger? No, need full circuit state potentially.
        // Full evaluate
        for(Node* g : netlist.getGates()) sim.evaluate(g);
        
        // Check Trigger Status
        // Note: Trojan gates are now in netlist.getGates()
        // We can just check trigger->value directly? 
        // Need to ensure 'trigger' node was evaluated.
        // Since it's in the gates list (added by createGate), it should be evaluated.
        
        // Sim.evaluate(trigger) ensures it's computed.
        if (sim.evaluate(trigger) == 1) {
            activations++;
        }
        
        if (i % 50000 == 0) std::cout << "  Sim " << i << "\r";
    }
    std::cout << "  Sim " << metrics.totalVectors << " [Done]\n";
    
    metrics.detectedCount = activations;
    metrics.detectionProb = (double)activations / metrics.totalVectors;
    
    return metrics;
}

int main() {
    std::srand(std::time(nullptr));
    std::ofstream csv("validation_tables.csv");
    csv << "Circuit,OriginalGates,TrojanGates,OverheadPct,TriggerSize,TotalVectors,Activations,DetectionProb\n";
    
    std::cout << "Generating Tables 2 & 3...\n";
    
    std::vector<std::pair<std::string, int>> targets = {
        {"inputs/combinational/c2670.bench", 4},
        {"inputs/combinational/c3540.bench", 2},  // Size 2 (Low density)
        {"inputs/combinational/c5315.bench", 4},
        {"inputs/combinational/c6288.bench", 2},  // Size 2 (Very sparse)
        {"inputs/sequential/s1423.bench", 4},
        {"inputs/sequential/s13207.bench", 2},
        {"inputs/sequential/s15850.bench", 2}     // Size 2 (Complex)
    };

    for(auto& t : targets) {
        TableMetrics m = validateBenchmark(t.first, t.second);
        if(m.detectedCount != -1) {
            csv << m.circuit << "," 
                << m.originalGates << "," 
                << m.trojanGates << "," 
                << std::fixed << std::setprecision(4) << m.areaOverhead << ","
                << m.triggerSize << ","
                << m.totalVectors << ","
                << m.detectedCount << ","
                << std::scientific << m.detectionProb << "\n";
        }
    }
    
    csv.close();
    std::cout << "\nResults saved to validation_tables.csv\n";
    return 0;
}

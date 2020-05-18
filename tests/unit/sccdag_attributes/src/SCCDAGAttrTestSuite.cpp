/*
 * Copyright 2016 - 2019  Angelo Matni, Simone Campanoni
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "SCCDAGAttrTestSuite.hpp"

using namespace llvm;

// Register pass to "opt"
char SCCDAGAttrTestSuite::ID = 0;
static RegisterPass<SCCDAGAttrTestSuite> X("UnitTester", "SCCDAG Attribute Unit Tester");

// Register pass to "clang"
static SCCDAGAttrTestSuite * _PassMaker = NULL;
static RegisterStandardPasses _RegPass1(PassManagerBuilder::EP_OptimizerLast,
    [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new SCCDAGAttrTestSuite());}}); // ** for -Ox
static RegisterStandardPasses _RegPass2(PassManagerBuilder::EP_EnabledOnOptLevel0,
    [](const PassManagerBuilder&, legacy::PassManagerBase& PM) {
        if(!_PassMaker){ PM.add(_PassMaker = new SCCDAGAttrTestSuite());}});// ** for -O0

const char *SCCDAGAttrTestSuite::tests[] = {
  "sccdag nodes",
  "scc with IV",
  "reducible SCC",
  "clonable SCC",
  "inter iteration top loop dependencies",
  "intra iteration top loop dependencies",
  "normalized top loop sccdag"
};
TestFunction SCCDAGAttrTestSuite::testFns[] = {
  SCCDAGAttrTestSuite::sccdagHasCorrectSCCs,
  SCCDAGAttrTestSuite::sccsWithIVAreFound,
  SCCDAGAttrTestSuite::reducibleSCCsAreFound,
  SCCDAGAttrTestSuite::clonableSCCsAreFound,
  SCCDAGAttrTestSuite::interIterationDependencies,
  SCCDAGAttrTestSuite::intraIterationDependencies,
  SCCDAGAttrTestSuite::normalizedTopLoopSCCDAG
};

bool SCCDAGAttrTestSuite::doInitialization (Module &M) {
  errs() << "SCCDAGAttrTestSuite: Initialize\n";
  const int numTests = sizeof(tests) / sizeof(tests[0]);
  this->suite = new TestSuite("SCCDAGAttrTestSuite", tests, testFns, numTests, "test.txt");
  this->M = &M;
  return false;
}

void SCCDAGAttrTestSuite::getAnalysisUsage (AnalysisUsage &AU) const {
  AU.addRequired<PDGAnalysis>();
  AU.addRequired<DominatorTreeWrapperPass>();
  AU.addRequired<PostDominatorTreeWrapperPass>();
  AU.addRequired<ScalarEvolutionWrapperPass>();
  AU.addRequired<LoopInfoWrapperPass>();
  AU.addRequired<CallGraphWrapperPass>();
}

bool SCCDAGAttrTestSuite::runOnModule (Module &M) {
  errs() << "SCCDAGAttrTestSuite: Start\n";
  auto mainFunction = M.getFunction("main");

  this->LI = &getAnalysis<LoopInfoWrapperPass>(*mainFunction).getLoopInfo();
  // TODO: Grab first loop and produce attributes on it
  LoopsSummary LIS;
  Loop *topLoop = LI->getLoopsInPreorder()[0];
  LIS.populate(*LI, topLoop);

  auto *DT = &getAnalysis<DominatorTreeWrapperPass>(*mainFunction).getDomTree();
  auto *PDT = &getAnalysis<PostDominatorTreeWrapperPass>(*mainFunction).getPostDomTree();
  DominatorSummary DS(*DT, *PDT);

  this->fdg = getAnalysis<PDGAnalysis>().getFunctionPDG(*mainFunction);
  this->sccdag = new SCCDAG(fdg);

  this->SE = &getAnalysis<ScalarEvolutionWrapperPass>(*mainFunction).getSE();

  this->attrs = new SCCDAGAttrs();
  this->attrs->populate(sccdag, LIS, *SE, DS);

  auto loopDG = fdg->createLoopsSubgraph(topLoop);
  this->sccdagTopLoopNorm = new SCCDAG(loopDG);
  // PDGPrinter printer;
  // printer.writeGraph<SCCDAG>("graph-top-loop.dot", sccdagTopLoopNorm);
  SCCDAGNormalizer normalizer(*sccdagTopLoopNorm, LIS, *SE, DS);
  normalizer.normalizeInPlace();

  suite->runTests((ModulePass &)*this);

  delete this->attrs;
  delete this->sccdag;
  delete this->sccdagTopLoopNorm;
  delete fdg;

  return false;
}

std::string combineValues (std::vector<std::string> values, std::string delimiter) {
  std::string allValues = values[0];
  for (int i = 1; i < values.size(); ++i) {
    allValues += delimiter + values[i];
  }

  return allValues;
}

Values SCCDAGAttrTestSuite::sccdagHasCorrectSCCs (ModulePass &pass) {
  SCCDAGAttrTestSuite &attrPass = static_cast<SCCDAGAttrTestSuite &>(pass);
  return attrPass.getValuesOfSCCDAG(*attrPass.sccdag);
}

Values SCCDAGAttrTestSuite::normalizedTopLoopSCCDAG (ModulePass &pass) {
  SCCDAGAttrTestSuite &attrPass = static_cast<SCCDAGAttrTestSuite &>(pass);
  return attrPass.getValuesOfSCCDAG(*attrPass.sccdagTopLoopNorm);
}

Values SCCDAGAttrTestSuite::getValuesOfSCCDAG (SCCDAG &dag) {
  Values valueNames;
  for (auto node : dag.getNodes()) {
    std::vector<std::string> sccValues;
    for (auto nodePair : node->getT()->internalNodePairs()) {
      sccValues.push_back(suite->valueToString(nodePair.first));
    }
    valueNames.insert(combineValues(sccValues, suite->unorderedValueDelimiter));
  }
  return valueNames;
}

Values SCCDAGAttrTestSuite::sccsWithIVAreFound (ModulePass &pass) {
  SCCDAGAttrTestSuite &attrPass = static_cast<SCCDAGAttrTestSuite &>(pass);
  std::set<SCC *> sccs;
  for (auto node : attrPass.sccdag->getNodes()) {
    SCCAttrs *sccAttrs = attrPass.attrs->getSCCAttrs(node->getT());
    if (sccAttrs->isInductionVariableSCC()) sccs.insert(node->getT());
  }

  return SCCDAGAttrTestSuite::printSCCs(pass, sccs);
}

Values SCCDAGAttrTestSuite::reducibleSCCsAreFound (ModulePass &pass) {
  SCCDAGAttrTestSuite &attrPass = static_cast<SCCDAGAttrTestSuite &>(pass);
  std::set<SCC *> sccs;
  for (auto node : attrPass.sccdag->getNodes()) {
    SCCAttrs *sccAttrs = attrPass.attrs->getSCCAttrs(node->getT());
    if (sccAttrs->canExecuteReducibly()) sccs.insert(node->getT());
  }

  return SCCDAGAttrTestSuite::printSCCs(pass, sccs);
}

Values SCCDAGAttrTestSuite::clonableSCCsAreFound (ModulePass &pass) {
  SCCDAGAttrTestSuite &attrPass = static_cast<SCCDAGAttrTestSuite &>(pass);
  std::set<SCC *> sccs;
  for (auto node : attrPass.sccdag->getNodes()) {
    SCCAttrs *sccAttrs = attrPass.attrs->getSCCAttrs(node->getT());
    if (sccAttrs->canBeCloned()) sccs.insert(node->getT());
  }

  return SCCDAGAttrTestSuite::printSCCs(pass, sccs);
}

Values SCCDAGAttrTestSuite::printSCCs (ModulePass &pass, std::set<SCC *> sccs) {
  SCCDAGAttrTestSuite &attrPass = static_cast<SCCDAGAttrTestSuite &>(pass);
  Values valueNames{};
  for (auto scc : sccs) {
    std::vector<std::string> sccValues;
    for (auto nodePair : scc->internalNodePairs()) {
      sccValues.push_back(attrPass.suite->valueToString(nodePair.first));
    }
    valueNames.insert(combineValues(sccValues, attrPass.suite->unorderedValueDelimiter));
  }

  return valueNames;
}

Values SCCDAGAttrTestSuite::interIterationDependencies (ModulePass &pass) {
  SCCDAGAttrTestSuite &attrPass = static_cast<SCCDAGAttrTestSuite &>(pass);
  Values valueNames{};
  for (auto sccAndDeps : attrPass.attrs->interIterDeps) {
    for (auto dep : sccAndDeps.second) {
      std::string outValue = attrPass.suite->valueToString(dep->getOutgoingT());
      std::string inValue = attrPass.suite->valueToString(dep->getIncomingT());
      valueNames.insert(outValue + attrPass.suite->orderedValueDelimiter + inValue);
    }
  }

  return valueNames;
}

Values SCCDAGAttrTestSuite::intraIterationDependencies (ModulePass &pass) {
  SCCDAGAttrTestSuite &attrPass = static_cast<SCCDAGAttrTestSuite &>(pass);
  Values valueNames{};
  for (auto sccAndDeps : attrPass.attrs->intraIterDeps) {
    for (auto dep : sccAndDeps.second) {
      std::string outValue = attrPass.suite->valueToString(dep->getOutgoingT());
      std::string inValue = attrPass.suite->valueToString(dep->getIncomingT());
      valueNames.insert(outValue + attrPass.suite->orderedValueDelimiter + inValue);
    }
  }

  return valueNames;
}
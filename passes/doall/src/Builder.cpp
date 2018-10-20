#include "DOALL.hpp"

std::unique_ptr<ChunkerInfo> DOALL::createChunkingFuncAndArgs (
  LoopDependenceInfoForParallelizer *LDI,
  Parallelization &par
) {

  auto chunker = std::make_unique<ChunkerInfo>();
  auto M = LDI->function->getParent();
  auto &cxt = M->getContext();

  auto voidTy = Type::getVoidTy(cxt);
  auto ptrTy_int8 = PointerType::getUnqual(par.int8);
  auto funcArgTypes = ArrayRef<Type*>({
    ptrTy_int8,
    par.int64,
    par.int64,
    par.int64
  });
  auto funcTy = FunctionType::get(voidTy, funcArgTypes, false);
  chunker->f = cast<Function>(M->getOrInsertFunction("", funcTy));

  /*
   * Create entry and exit blocks
   * Create outer loop header and latch
   */
  chunker->entryBlock = chunker->createChunkerBB();
  chunker->exitBlock = chunker->createChunkerBB();
  chunker->chHeader = chunker->createChunkerBB();
  chunker->chLatch = chunker->createChunkerBB();

  /*
   * Collect arguments of chunker function
   */
  auto argIter = chunker->f->arg_begin();
  chunker->envArgVal = (Value *) &*(argIter++);
  chunker->coreArgVal = (Value *) &*(argIter++); 
  chunker->numCoresArgVal = (Value *) &*(argIter++);
  chunker->chunkSizeArgVal = (Value *) &*(argIter++);

  /*
   * Create environment context within chunking function
   */
  envBuilder->createEnvUsers(1);
  auto envUser = envBuilder->getUser(0);
  IRBuilder<> entryB(chunker->entryBlock);
  envUser->setEnvArray(entryB.CreateBitCast(
    chunker->envArgVal,
    PointerType::getUnqual(envBuilder->getEnvArrayTy())
  ));

  return chunker;
}

void DOALL::cloneSequentialLoop (
  LoopDependenceInfoForParallelizer *LDI,
  Parallelization &par,
  std::unique_ptr<ChunkerInfo> &chunker
) {

  /*
   * Create inner loop
   */
  for (auto originBB : LDI->liSummary.topLoop->bbs) {
    auto cloneBB = chunker->createChunkerBB();
    IRBuilder<> builder(cloneBB);
    chunker->innerBBMap[originBB] = cloneBB;
    for (auto &I : *originBB) {
      auto cloneI = builder.Insert(I.clone());
      chunker->innerValMap[&I] = cloneI;
    }
  }

  /*
   * Map inner loop preheader to outer loop header
   * TODO(SIMONE): explain why the mapping isn't from inner loop preheader to outer loop preheader
   * TODO(ANGELO): fix the fact that this is incorrect. It highlights a bigger issue:
   *   The inner and outer loops of our DOALL execution need to be flipped. The outer loop should be
   *   the original loop, the inner loop should be constructed. This fixes the below incorrect mapping
   *   so that it is between loop preheaders.
   */
  chunker->innerBBMap[LDI->preHeader] = chunker->chHeader;

  /*
   * Map single exit block of inner loop to outer loop latch
   */
  auto singleExitBB = LDI->loopExitBlocks[0];
  assert(singleExitBB != nullptr);
  chunker->innerBBMap[singleExitBB] = chunker->chLatch;

  return ;
}

void DOALL::mapOriginLoopValueUses (
  LoopDependenceInfoForParallelizer *LDI,
  Parallelization &par,
  std::unique_ptr<ChunkerInfo> &chunker
) {
  for (auto iPair : chunker->innerValMap) {
    auto cloneI = iPair.second;
    if (auto terminator = dyn_cast<TerminatorInst>(cloneI)) {
      for (int i = 0; i < terminator->getNumSuccessors(); ++i) {
        auto succBB = terminator->getSuccessor(i);
        assert(chunker->innerBBMap.find(succBB) != chunker->innerBBMap.end());
        terminator->setSuccessor(i, chunker->innerBBMap[succBB]);
      }
    }

    if (auto phi = dyn_cast<PHINode>(cloneI)) {
      for (int i = 0; i < phi->getNumIncomingValues(); ++i) {
        auto cloneBB = chunker->innerBBMap[phi->getIncomingBlock(i)];
        phi->setIncomingBlock(i, cloneBB);
      }
    }

    // TODO(angelo): Add exhaustive search of types to parallelization
    // utilities for use in DSWP and here in DOALL
    for (auto &op : cloneI->operands()) {

      /*
       * Fetch the value used by the instruction.
       */
      auto opV = op.get();

      /*
       * Check if the value is a loop live-in one.
       */
      if (chunker->preEnvMap.find(opV) != chunker->preEnvMap.end()) {

        /*
         * The value is a loop live-in one.
         * Set it to the value loaded outside the parallelized loop.
         */
        op.set(chunker->preEnvMap[opV]);
        continue ;
      }

      /*
       * The value is not a loop live-in one.
       * 
       * Check if the value is generated by another instruction inside the loop.
       */
      if (auto opI = dyn_cast<Instruction>(opV)) {
        if (chunker->innerValMap.find(opI) != chunker->innerValMap.end()) {

         /*
          * The value is generated by another instruction inside the loop.
          * Set it to be the value generated by the equivalent instruction included in the parallelized loop.
          */
          op.set(chunker->innerValMap[opI]);
        }
      }
    }
  }
}

void DOALL::createOuterLoop (
  LoopDependenceInfoForParallelizer *LDI,
  Parallelization &par,
  std::unique_ptr<ChunkerInfo> &chunker
) {
  IRBuilder<> entryB(chunker->entryBlock);
  IRBuilder<> chHeaderB(chunker->chHeader);
  IRBuilder<> chLatchB(chunker->chLatch);

  /*
   * Determine start value and step size for outer chunking loop
   */
  auto startVal = chunker->cloneIVInfo.start;
  auto coreOffset = entryB.CreateZExtOrTrunc(
    entryB.CreateMul(chunker->coreArgVal, chunker->chunkSizeArgVal),
    startVal->getType()
  );
  auto outerIVStart = entryB.CreateAdd(startVal, coreOffset);

  auto outerIV = chHeaderB.CreatePHI(startVal->getType(), /*numReservedValues=*/2);
  chunker->outerIV = outerIV;

  auto outerIVStepSize = entryB.CreateZExtOrTrunc(
    entryB.CreateMul(chunker->numCoresArgVal, chunker->chunkSizeArgVal),
    startVal->getType()
  );
  outerIVStepSize = entryB.CreateMul(outerIVStepSize, chunker->cloneIVInfo.step);
  auto outerIVInc = chLatchB.CreateAdd(outerIV, outerIVStepSize);

  outerIV->addIncoming(outerIVStart, chunker->entryBlock);
  outerIV->addIncoming(outerIVInc, chunker->chLatch);

  entryB.CreateBr(chunker->chHeader);
  chLatchB.CreateBr(chunker->chHeader);

  auto outerIVCmp = chHeaderB.CreateICmpULT(outerIV, chunker->cloneIVInfo.cmpIVTo);
  auto innerHeader = chunker->innerBBMap[LDI->header];
  chHeaderB.CreateCondBr(outerIVCmp, innerHeader, chunker->exitBlock);
}

void DOALL::alterInnerLoopToIterateChunks (
  LoopDependenceInfoForParallelizer *LDI,
  Parallelization &par,
  std::unique_ptr<ChunkerInfo> &chunker
) {

  /*
   * Reset start to 0, revise latch to only inc
   */
  auto PHIType = chunker->cloneIV->getType();
  auto startPHIIndex = 0;
  if (chunker->cloneIV->getIncomingBlock(1) == chunker->chHeader) {
    startPHIIndex = 1;
  }
  chunker->cloneIV->setIncomingValue(
    startPHIIndex,
    ConstantInt::get(PHIType, 0)
  );

  auto originStepper = *(chunker->originIVAttrs->PHIAccumulators.begin());
  auto innerStepper = chunker->innerValMap[originStepper];
  auto stepValueIndex = 0;
  if (isa<ConstantInt>(innerStepper->getOperand(1))) {
    stepValueIndex = 1;
  }
  innerStepper->setOperand(
    stepValueIndex,
    ConstantInt::get(PHIType, 1)
  );

  /*
   * Create new, composite induction variable for inner loop
   */
  auto innerHeader = chunker->innerBBMap[LDI->header];
  IRBuilder<> headerBuilder(innerHeader);
  auto sumIV = headerBuilder.CreateAdd(chunker->cloneIV, chunker->outerIV);
  for (auto &use : chunker->originIVAttrs->singlePHI->uses()) {
    auto cloneI = chunker->innerValMap[(Instruction *)use.getUser()];
    auto cloneU = (User *)cloneI;
    if (cloneU == innerStepper || cloneI->getParent() == innerHeader) continue;
    cloneU->replaceUsesOfWith(chunker->cloneIV, sumIV);
  }

  /*
   * Replace inner loop original condition with less than total loop size condition
   * Add a cond to check for less than chunk size
   */
  auto innerCmp = chunker->cloneIVInfo.cmp;
  innerCmp->setPredicate(CmpInst::Predicate::ICMP_ULT);
  innerCmp->setOperand(0, sumIV);
  innerCmp->setOperand(1, chunker->cloneIVInfo.cmpIVTo);

  auto sumIVInst = cast<Instruction>(sumIV);
  sumIVInst->removeFromParent();
  sumIVInst->insertBefore(innerCmp);

  auto innerBr = chunker->cloneIVInfo.br;
  auto innerBodySuccIndex = 0;
  if (innerBr->getSuccessor(0) == chunker->chLatch) {
    innerBodySuccIndex = 1;
  }
  auto innerBodyBB = innerBr->getSuccessor(innerBodySuccIndex);

  auto chunkCmpBB = chunker->createChunkerBB();
  IRBuilder<> chunkCmpBuilder(chunkCmpBB);

  innerBr->setSuccessor(0, chunkCmpBB);
  innerBr->setSuccessor(1, chunker->chLatch);

  IRBuilder<> entryB(chunker->entryBlock->getTerminator());
  auto castChunkSize = entryB.CreateZExtOrTrunc(chunker->chunkSizeArgVal, chunker->cloneIV->getType());
  Value *chunkCmp = chunkCmpBuilder.CreateICmpULT(chunker->cloneIV, castChunkSize);
  chunkCmpBuilder.CreateCondBr(chunkCmp, innerBodyBB, chunker->chLatch);

  return ;
}

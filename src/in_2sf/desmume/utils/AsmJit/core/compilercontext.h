// [AsmJit]
// Complete JIT Assembler for C++ Language.
//
// [License]
// Zlib - See COPYING file in this package.

#pragma once

// [Dependencies - AsmJit]
#include "../core/compiler.h"
#include "../core/compilerfunc.h"
#include "../core/compileritem.h"
#include "../core/zonememory.h"

// [Api-Begin]
#include "../core/apibegin.h"

namespace AsmJit
{

// ============================================================================
// [AsmJit::CompilerContext]
// ============================================================================

struct CompilerContext
{
	ASMJIT_NO_COPY(CompilerContext)

	// --------------------------------------------------------------------------
	// [Construction / Destruction]
	// --------------------------------------------------------------------------

	ASMJIT_API CompilerContext(Compiler *compiler);
	ASMJIT_API virtual ~CompilerContext();

	// --------------------------------------------------------------------------
	// [Accessor]
	// --------------------------------------------------------------------------

	Compiler *getCompiler() const { return this->_compiler; }
  
	CompilerFuncDecl *getFunc() const { return this->_func; }

	CompilerItem *getExtraBlock() const { return this->_extraBlock; }
  
	void setExtraBlock(CompilerItem *item) { this->_extraBlock = item; }

	// --------------------------------------------------------------------------
	// [Members]
	// --------------------------------------------------------------------------

	//! @brief ZoneMemory manager.
	ZoneMemory _zoneMemory;

	//! @brief Compiler.
	Compiler *_compiler;
	//! @brief Function.
	CompilerFuncDecl *_func;

	//! @brief Start of the current active scope.
	CompilerItem *_start;
	//! @brief End of the current active scope.
	CompilerItem *_stop;
	//! @brief Item that is used to insert some code after the function body.
	CompilerItem *_extraBlock;

	//! @brief Current state (used by register allocator).
	CompilerState *_state;
	//! @brief Link to circular double-linked list containing all active variables
	//! of the current state.
	CompilerVar *_active;

	//! @brief Current offset, used in prepare() stage. Each item should increment it.
	uint32_t _currentOffset;
	//! @brief Whether current code is unreachable.
	uint32_t _isUnreachable;
};

} // AsmJit namespace

// [Api-End]
#include "../core/apiend.h"

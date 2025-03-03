//===--- Scope.h - Scope interface ------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the Scope interface.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_SEMA_SCOPE_H
#define LLVM_CLANG_SEMA_SCOPE_H

#include "clang/Basic/Diagnostic.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"

namespace llvm {

class raw_ostream;

}

namespace clang {

class Decl;
class UsingDirectiveDecl;
class VarDecl;

/// Scope - A scope is a transient data structure that is used while parsing the
/// program.  It assists with resolving identifiers to the appropriate
/// declaration.
///
class Scope {
public:
  /// ScopeFlags - These are bitfields that are or'd together when creating a
  /// scope, which defines the sorts of things the scope contains.
  enum ScopeFlags {
    /// \brief This indicates that the scope corresponds to a function, which
    /// means that labels are set here.
    FnScope       = 0x01,

    /// \brief This is a while, do, switch, for, etc that can have break
    /// statements embedded into it.
    BreakScope    = 0x02,

    /// \brief This is a while, do, for, which can have continue statements
    /// embedded into it.
    ContinueScope = 0x04,

    /// \brief This is a scope that can contain a declaration.  Some scopes
    /// just contain loop constructs but don't contain decls.
    DeclScope = 0x08,

    /// \brief The controlling scope in a if/switch/while/for statement.
    ControlScope = 0x10,

    /// \brief The scope of a struct/union/class definition.
    ClassScope = 0x20,

    /// \brief This is a scope that corresponds to a block/closure object.
    /// Blocks serve as top-level scopes for some objects like labels, they
    /// also prevent things like break and continue.  BlockScopes always have
    /// the FnScope and DeclScope flags set as well.
    BlockScope = 0x40,

    /// \brief This is a scope that corresponds to the
    /// template parameters of a C++ template. Template parameter
    /// scope starts at the 'template' keyword and ends when the
    /// template declaration ends.
    TemplateParamScope = 0x80,

    /// \brief This is a scope that corresponds to the
    /// parameters within a function prototype.
    FunctionPrototypeScope = 0x100,

    /// \brief This is a scope that corresponds to the parameters within
    /// a function prototype for a function declaration (as opposed to any
    /// other kind of function declarator). Always has FunctionPrototypeScope
    /// set as well.
    FunctionDeclarationScope = 0x200,

    /// \brief This is a scope that corresponds to the Objective-C
    /// \@catch statement.
    AtCatchScope = 0x400,
    
    /// \brief This scope corresponds to an Objective-C method body.
    /// It always has FnScope and DeclScope set as well.
    ObjCMethodScope = 0x800,

    /// \brief This is a scope that corresponds to a switch statement.
    SwitchScope = 0x1000,

    /// \brief This is the scope of a C++ try statement.
    TryScope = 0x2000,

    /// \brief This is the scope for a function-level C++ try or catch scope.
    FnTryCatchScope = 0x4000,

    /// \brief This is the scope of OpenMP executable directive.
    OpenMPDirectiveScope = 0x8000,

    /// \brief This is the scope of some OpenMP loop directive.
    OpenMPLoopDirectiveScope = 0x10000,

    /// \brief This is the scope of some OpenMP simd directive.
    /// For example, it is used for 'omp simd', 'omp for simd'.
    /// This flag is propagated to children scopes.
    OpenMPSimdDirectiveScope = 0x20000,

    /// This scope corresponds to an enum.
    EnumScope = 0x40000,

    /// This scope corresponds to an SEH try.
    SEHTryScope = 0x80000,

    /// This scope corresponds to an SEH except.
    SEHExceptScope = 0x100000,

    /// We are currently in the filter expression of an SEH except block.
    SEHFilterScope = 0x200000,

//@@
    /// \brief This is a QuasiQuotes scope.
    QuasiQuotesScope = 0x400000,
    /// \brief This is an Escape scope.
    EscapeScope = 0x800000,
//@@
  };
private:
  /// The parent scope for this scope.  This is null for the translation-unit
  /// scope.
  Scope *AnyParent;

  /// Flags - This contains a set of ScopeFlags, which indicates how the scope
  /// interrelates with other control flow statements.
  unsigned Flags;

  /// Depth - This is the depth of this scope.  The translation-unit scope has
  /// depth 0.
  unsigned short Depth;

  /// \brief Declarations with static linkage are mangled with the number of
  /// scopes seen as a component.
  unsigned short MSLastManglingNumber;

  unsigned short MSCurManglingNumber;

  /// PrototypeDepth - This is the number of function prototype scopes
  /// enclosing this scope, including this scope.
  unsigned short PrototypeDepth;

  /// PrototypeIndex - This is the number of parameters currently
  /// declared in this scope.
  unsigned short PrototypeIndex;

  /// FnParent - If this scope has a parent scope that is a function body, this
  /// pointer is non-null and points to it.  This is used for label processing.
  Scope *FnParent;
  Scope *MSLastManglingParent;

  /// BreakParent/ContinueParent - This is a direct link to the innermost
  /// BreakScope/ContinueScope which contains the contents of this scope
  /// for control flow purposes (and might be this scope itself), or null
  /// if there is no such scope.
  Scope *BreakParent, *ContinueParent;

  /// BlockParent - This is a direct link to the immediately containing
  /// BlockScope if this scope is not one, or null if there is none.
  Scope *BlockParent;

  /// TemplateParamParent - This is a direct link to the
  /// immediately containing template parameter scope. In the
  /// case of nested templates, template parameter scopes can have
  /// other template parameter scopes as parents.
  Scope *TemplateParamParent;

//@@
  /// QuasiQuotesParent - If this scope has a parent QuasiQuotes scope,
  /// this pointer is non-null and points to it.
  Scope *QuasiQuotesParent;

  /// EscapeParent - If this scope has a parent Escape scope,
  /// this pointer is non-null and points to it.
  Scope *EscapeParent;
//@@

  /// DeclsInScope - This keeps track of all declarations in this scope.  When
  /// the declaration is added to the scope, it is set as the current
  /// declaration for the identifier in the IdentifierTable.  When the scope is
  /// popped, these declarations are removed from the IdentifierTable's notion
  /// of current declaration.  It is up to the current Action implementation to
  /// implement these semantics.
  typedef llvm::SmallPtrSet<Decl *, 32> DeclSetTy;
  DeclSetTy DeclsInScope;

  /// The DeclContext with which this scope is associated. For
  /// example, the entity of a class scope is the class itself, the
  /// entity of a function scope is a function, etc.
  DeclContext *Entity;

  typedef SmallVector<UsingDirectiveDecl *, 2> UsingDirectivesTy;
  UsingDirectivesTy UsingDirectives;

  /// \brief Used to determine if errors occurred in this scope.
  DiagnosticErrorTrap ErrorTrap;

  /// A lattice consisting of undefined, a single NRVO candidate variable in
  /// this scope, or over-defined. The bit is true when over-defined.
  llvm::PointerIntPair<VarDecl *, 1, bool> NRVO;

public:
  Scope(Scope *Parent, unsigned ScopeFlags, DiagnosticsEngine &Diag)
    : ErrorTrap(Diag) {
    Init(Parent, ScopeFlags);
  }

  /// getFlags - Return the flags for this scope.
  ///
  unsigned getFlags() const { return Flags; }
  void setFlags(unsigned F) { Flags = F; }

  /// isBlockScope - Return true if this scope correspond to a closure.
  bool isBlockScope() const { return Flags & BlockScope; }

  /// getParent - Return the scope that this is nested in.
  ///
  const Scope *getParent() const { return AnyParent; }
  Scope *getParent() { return AnyParent; }

  /// getFnParent - Return the closest scope that is a function body.
  ///
  const Scope *getFnParent() const { return FnParent; }
  Scope *getFnParent() { return FnParent; }

  const Scope *getMSLastManglingParent() const {
    return MSLastManglingParent;
  }
  Scope *getMSLastManglingParent() { return MSLastManglingParent; }

  /// getContinueParent - Return the closest scope that a continue statement
  /// would be affected by.
  Scope *getContinueParent() {
    return ContinueParent;
  }

  const Scope *getContinueParent() const {
    return const_cast<Scope*>(this)->getContinueParent();
  }

  /// getBreakParent - Return the closest scope that a break statement
  /// would be affected by.
  Scope *getBreakParent() {
    return BreakParent;
  }
  const Scope *getBreakParent() const {
    return const_cast<Scope*>(this)->getBreakParent();
  }

  Scope *getBlockParent() { return BlockParent; }
  const Scope *getBlockParent() const { return BlockParent; }

  Scope *getTemplateParamParent() { return TemplateParamParent; }
  const Scope *getTemplateParamParent() const { return TemplateParamParent; }

//@@
  /// getQuasiQuotesParent - Return the closest QuasiQuotes scope.
  ///
  const Scope *getQuasiQuotesParent() const { return QuasiQuotesParent; }
  Scope *getQuasiQuotesParent() { return QuasiQuotesParent; }

  /// getEscapeParent - Return the closest Escape scope.
  ///
  const Scope *getEscapeParent() const { return EscapeParent; }
  Scope *getEscapeParent() { return EscapeParent; }
//@@

  /// Returns the number of function prototype scopes in this scope
  /// chain.
  unsigned getFunctionPrototypeDepth() const {
    return PrototypeDepth;
  }

  /// Return the number of parameters declared in this function
  /// prototype, increasing it by one for the next call.
  unsigned getNextFunctionPrototypeIndex() {
    assert(isFunctionPrototypeScope());
    return PrototypeIndex++;
  }

  typedef llvm::iterator_range<DeclSetTy::iterator> decl_range;
  decl_range decls() const {
    return decl_range(DeclsInScope.begin(), DeclsInScope.end());
  }
  bool decl_empty() const { return DeclsInScope.empty(); }

  void AddDecl(Decl *D) {
    DeclsInScope.insert(D);
  }

  void RemoveDecl(Decl *D) {
    DeclsInScope.erase(D);
  }

  void incrementMSManglingNumber() {
    if (Scope *MSLMP = getMSLastManglingParent()) {
      MSLMP->MSLastManglingNumber += 1;
      MSCurManglingNumber += 1;
    }
  }

  void decrementMSManglingNumber() {
    if (Scope *MSLMP = getMSLastManglingParent()) {
      MSLMP->MSLastManglingNumber -= 1;
      MSCurManglingNumber -= 1;
    }
  }

  unsigned getMSLastManglingNumber() const {
    if (const Scope *MSLMP = getMSLastManglingParent())
      return MSLMP->MSLastManglingNumber;
    return 1;
  }

  unsigned getMSCurManglingNumber() const {
    return MSCurManglingNumber;
  }

  /// isDeclScope - Return true if this is the scope that the specified decl is
  /// declared in.
  bool isDeclScope(Decl *D) {
    return DeclsInScope.count(D) != 0;
  }

  DeclContext *getEntity() const { return Entity; }
  void setEntity(DeclContext *E) { Entity = E; }

  bool hasErrorOccurred() const { return ErrorTrap.hasErrorOccurred(); }

  bool hasUnrecoverableErrorOccurred() const {
    return ErrorTrap.hasUnrecoverableErrorOccurred();
  }

  /// isFunctionScope() - Return true if this scope is a function scope.
  bool isFunctionScope() const { return (getFlags() & Scope::FnScope); }

  /// isClassScope - Return true if this scope is a class/struct/union scope.
  bool isClassScope() const {
    return (getFlags() & Scope::ClassScope);
  }

  /// isInCXXInlineMethodScope - Return true if this scope is a C++ inline
  /// method scope or is inside one.
  bool isInCXXInlineMethodScope() const {
    if (const Scope *FnS = getFnParent()) {
      assert(FnS->getParent() && "TUScope not created?");
      return FnS->getParent()->isClassScope();
    }
    return false;
  }
  
  /// isInObjcMethodScope - Return true if this scope is, or is contained in, an
  /// Objective-C method body.  Note that this method is not constant time.
  bool isInObjcMethodScope() const {
    for (const Scope *S = this; S; S = S->getParent()) {
      // If this scope is an objc method scope, then we succeed.
      if (S->getFlags() & ObjCMethodScope)
        return true;
    }
    return false;
  }

  /// isInObjcMethodOuterScope - Return true if this scope is an
  /// Objective-C method outer most body.
  bool isInObjcMethodOuterScope() const {
    if (const Scope *S = this) {
      // If this scope is an objc method scope, then we succeed.
      if (S->getFlags() & ObjCMethodScope)
        return true;
    }
    return false;
  }

  
  /// isTemplateParamScope - Return true if this scope is a C++
  /// template parameter scope.
  bool isTemplateParamScope() const {
    return getFlags() & Scope::TemplateParamScope;
  }

  /// isFunctionPrototypeScope - Return true if this scope is a
  /// function prototype scope.
  bool isFunctionPrototypeScope() const {
    return getFlags() & Scope::FunctionPrototypeScope;
  }

  /// isAtCatchScope - Return true if this scope is \@catch.
  bool isAtCatchScope() const {
    return getFlags() & Scope::AtCatchScope;
  }

  /// isSwitchScope - Return true if this scope is a switch scope.
  bool isSwitchScope() const {
    for (const Scope *S = this; S; S = S->getParent()) {
      if (S->getFlags() & Scope::SwitchScope)
        return true;
      else if (S->getFlags() & (Scope::FnScope | Scope::ClassScope |
                                Scope::BlockScope | Scope::TemplateParamScope |
                                Scope::FunctionPrototypeScope |
                                Scope::AtCatchScope | Scope::ObjCMethodScope))
        return false;
    }
    return false;
  }

  /// \brief Determines whether this scope is the OpenMP directive scope
  bool isOpenMPDirectiveScope() const {
    return (getFlags() & Scope::OpenMPDirectiveScope);
  }

  /// \brief Determine whether this scope is some OpenMP loop directive scope
  /// (for example, 'omp for', 'omp simd').
  bool isOpenMPLoopDirectiveScope() const {
    if (getFlags() & Scope::OpenMPLoopDirectiveScope) {
      assert(isOpenMPDirectiveScope() &&
             "OpenMP loop directive scope is not a directive scope");
      return true;
    }
    return false;
  }

  /// \brief Determine whether this scope is (or is nested into) some OpenMP
  /// loop simd directive scope (for example, 'omp simd', 'omp for simd').
  bool isOpenMPSimdDirectiveScope() const {
    return getFlags() & Scope::OpenMPSimdDirectiveScope;
  }

  /// \brief Determine whether this scope is a loop having OpenMP loop
  /// directive attached.
  bool isOpenMPLoopScope() const {
    const Scope *P = getParent();
    return P && P->isOpenMPLoopDirectiveScope();
  }

  /// \brief Determine whether this scope is a C++ 'try' block.
  bool isTryScope() const { return getFlags() & Scope::TryScope; }

  /// \brief Determine whether this scope is a SEH '__try' block.
  bool isSEHTryScope() const { return getFlags() & Scope::SEHTryScope; }

  /// \brief Determine whether this scope is a SEH '__except' block.
  bool isSEHExceptScope() const { return getFlags() & Scope::SEHExceptScope; }

  /// \brief Returns if rhs has a higher scope depth than this.
  ///
  /// The caller is responsible for calling this only if one of the two scopes
  /// is an ancestor of the other.
  bool Contains(const Scope& rhs) const { return Depth < rhs.Depth; }

  /// containedInPrototypeScope - Return true if this or a parent scope
  /// is a FunctionPrototypeScope.
  bool containedInPrototypeScope() const;

  void PushUsingDirective(UsingDirectiveDecl *UDir) {
    UsingDirectives.push_back(UDir);
  }

  typedef llvm::iterator_range<UsingDirectivesTy::iterator>
    using_directives_range;

  using_directives_range using_directives() {
    return using_directives_range(UsingDirectives.begin(),
                                  UsingDirectives.end());
  }

  void addNRVOCandidate(VarDecl *VD) {
    if (NRVO.getInt())
      return;
    if (NRVO.getPointer() == nullptr) {
      NRVO.setPointer(VD);
      return;
    }
    if (NRVO.getPointer() != VD)
      setNoNRVO();
  }

  void setNoNRVO() {
    NRVO.setInt(1);
    NRVO.setPointer(nullptr);
  }

  void mergeNRVOIntoParent();

  /// Init - This is used by the parser to implement scope caching.
  ///
  void Init(Scope *parent, unsigned flags);

  /// \brief Sets up the specified scope flags and adjusts the scope state
  /// variables accordingly.
  ///
  void AddFlags(unsigned Flags);

  void dumpImpl(raw_ostream &OS) const;
  void dump() const;
};

}  // end namespace clang

#endif

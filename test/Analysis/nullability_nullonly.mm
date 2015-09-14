// RUN: %clang_cc1 -analyze -analyzer-checker=core,alpha.nullability.NullPassedToNonnull,alpha.nullability.NullReturnedFromNonnull -verify %s

int getRandom();

typedef struct Dummy { int val; } Dummy;

void takesNullable(Dummy *_Nullable);
void takesNonnull(Dummy *_Nonnull);
Dummy *_Nullable returnsNullable();

void testBasicRules() {
  // The tracking of nullable values is turned off.
  Dummy *p = returnsNullable();
  takesNonnull(p); // no warning
  Dummy *q = 0;
  if (getRandom()) {
    takesNullable(q);
    takesNonnull(q); // expected-warning {{}}
  }
}

Dummy *_Nonnull testNullReturn() {
  Dummy *p = 0;
  return p; // expected-warning {{}}
}

void onlyReportFirstPreconditionViolationOnPath() {
  Dummy *p = 0;
  takesNonnull(p); // expected-warning {{}}
  takesNonnull(p); // No warning.
  // Passing null to nonnull is a sink. Stop the analysis.
  int i = 0;
  i = 5 / i; // no warning
  (void)i;
}

Dummy *_Nonnull doNotWarnWhenPreconditionIsViolatedInTopFunc(
    Dummy *_Nonnull p) {
  if (!p) {
    Dummy *ret =
        0; // avoid compiler warning (which is not generated by the analyzer)
    if (getRandom())
      return ret; // no warning
    else
      return p; // no warning
  } else {
    return p;
  }
}

Dummy *_Nonnull doNotWarnWhenPreconditionIsViolated(Dummy *_Nonnull p) {
  if (!p) {
    Dummy *ret =
        0; // avoid compiler warning (which is not generated by the analyzer)
    if (getRandom())
      return ret; // no warning
    else
      return p; // no warning
  } else {
    return p;
  }
}

void testPreconditionViolationInInlinedFunction(Dummy *p) {
  doNotWarnWhenPreconditionIsViolated(p);
}

void inlinedNullable(Dummy *_Nullable p) {
  if (p) return;
}
void inlinedNonnull(Dummy *_Nonnull p) {
  if (p) return;
}
void inlinedUnspecified(Dummy *p) {
  if (p) return;
}

Dummy *_Nonnull testDefensiveInlineChecks(Dummy * p) {
  switch (getRandom()) {
  case 1: inlinedNullable(p); break;
  case 2: inlinedNonnull(p); break;
  case 3: inlinedUnspecified(p); break;
  }
  if (getRandom())
    takesNonnull(p);
  return p;
}
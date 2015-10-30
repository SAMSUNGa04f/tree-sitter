#include "compiler/compiler_spec_helper.h"
#include "compiler/build_tables/lex_item.h"
#include "compiler/rules/metadata.h"

using namespace rules;
using namespace build_tables;

START_TEST

describe("LexItem", []() {
  describe("is_token_start()", [&]() {
    Symbol sym(1);
    rule_ptr token_start = make_shared<Metadata>(str("a"), map<MetadataKey, int>({
      { START_TOKEN, 1 }
    }));

    it("returns true for rules designated as token starts", [&]() {
      LexItem item(sym, token_start);
      AssertThat(item.is_token_start(), IsTrue());
    });

    it("returns false for rules not designated as token starts", [&]() {
      AssertThat(LexItem(sym, make_shared<Metadata>(str("a"), map<MetadataKey, int>({
        { START_TOKEN, 0 }
      }))).is_token_start(), IsFalse());
      AssertThat(LexItem(sym, str("a")).is_token_start(), IsFalse());
    });

    describe("when given a sequence containing a token start", [&]() {
      it("returns true when the rule before the token start may be blank", [&]() {
        LexItem item(sym, seq({ repeat(str("a")), token_start }));
        AssertThat(item.is_token_start(), IsTrue());
      });

      it("returns false when the rule before the token start cannot be blank", [&]() {
        LexItem item(sym, seq({ str("a"), token_start }));
        AssertThat(item.is_token_start(), IsFalse());
      });
    });
  });
});

describe("LexItemSet::transitions()", [&]() {
  it("handles single characters", [&]() {
    LexItemSet item_set({
      LexItem(Symbol(1), character({ 'x' })),
    });

    AssertThat(
      item_set.transitions(),
      Equals(LexItemSet::TransitionMap({
        {
          CharacterSet().include('x'),
          {
            LexItemSet({
              LexItem(Symbol(1), blank()),
            }),
            PrecedenceRange()
          }
        }
      })));
  });

  it("handles sequences", [&]() {
    LexItemSet item_set({
      LexItem(Symbol(1), seq({
        character({ 'w' }),
        character({ 'x' }),
        character({ 'y' }),
        character({ 'z' }),
      })),
    });

    AssertThat(
      item_set.transitions(),
      Equals(LexItemSet::TransitionMap({
        {
          CharacterSet().include('w'),
          {
            LexItemSet({
              LexItem(Symbol(1), seq({
                character({ 'x' }),
                character({ 'y' }),
                character({ 'z' }),
              })),
            }),
            PrecedenceRange()
          }
        }
      })));
  });

  it("handles sequences with nested precedence", [&]() {
    LexItemSet item_set({
      LexItem(Symbol(1), seq({
        prec(3, seq({
          prec(4, seq({
            character({ 'w' }),
            character({ 'x' }) })),
          character({ 'y' }) })),
        character({ 'z' }),
      })),
    });

    AssertThat(
      item_set.transitions(),
      Equals(LexItemSet::TransitionMap({
        {
          CharacterSet().include('w'),
          {
            LexItemSet({
              LexItem(Symbol(1), seq({
                prec(3, seq({
                  prec(4, character({ 'x' })),
                  character({ 'y' }) })),
                character({ 'z' }),
              })),
            }),
            PrecedenceRange(4)
          }
        }
      })));

    LexItemSet item_set2({
      LexItem(Symbol(1), seq({
        prec(3, seq({
          prec(4, character({ 'x' })),
          character({ 'y' }) })),
        character({ 'z' }),
      })),
    });

    AssertThat(
      item_set2.transitions(),
      Equals(LexItemSet::TransitionMap({
        {
          CharacterSet().include('x'),
          {
            LexItemSet({
              LexItem(Symbol(1), seq({
                prec(3, character({ 'y' })),
                character({ 'z' }),
              })),
            }),
            PrecedenceRange(3)
          }
        }
      })));
  });

  it("handles sequences where the left hand side can be blank", [&]() {
    LexItemSet item_set({
      LexItem(Symbol(1), seq({
        choice({
          character({ 'x' }),
          blank(),
        }),
        character({ 'y' }),
        character({ 'z' }),
      })),
    });

    AssertThat(
      item_set.transitions(),
      Equals(LexItemSet::TransitionMap({
        {
          CharacterSet().include('x'),
          {
            LexItemSet({
              LexItem(Symbol(1), seq({
                character({ 'y' }),
                character({ 'z' }),
              })),
            }),
            PrecedenceRange()
          }
        },
        {
          CharacterSet().include('y'),
          {
            LexItemSet({
              LexItem(Symbol(1), character({ 'z' })),
            }),
            PrecedenceRange()
          }
        }
      })));
  });

  it("handles blanks", [&]() {
    LexItemSet item_set({
      LexItem(Symbol(1), blank()),
    });

    AssertThat(item_set.transitions(), IsEmpty());
  });

  it("handles repeats", [&]() {
    LexItemSet item_set({
      LexItem(Symbol(1), repeat1(seq({
        character({ 'a' }),
        character({ 'b' }),
      }))),
      LexItem(Symbol(2), repeat1(character({ 'c' }))),
    });

    AssertThat(
      item_set.transitions(),
      Equals(LexItemSet::TransitionMap({
        {
          CharacterSet().include('a'),
          {
            LexItemSet({
              LexItem(Symbol(1), seq({
                character({ 'b' }),
                repeat1(seq({
                  character({ 'a' }),
                  character({ 'b' }),
                }))
              })),
              LexItem(Symbol(1), character({ 'b' })),
            }),
            PrecedenceRange()
          }
        },
        {
          CharacterSet().include('c'),
          {
            LexItemSet({
              LexItem(Symbol(2), repeat1(character({ 'c' }))),
              LexItem(Symbol(2), blank()),
            }),
            PrecedenceRange()
          }
        }
      })));
  });

  it("handles choices between overlapping character sets", [&]() {
    LexItemSet item_set({
      LexItem(Symbol(1), choice({
        prec(2, seq({
          character({ 'a', 'b', 'c', 'd'  }),
          character({ 'x' }),
        })),
        prec(3, seq({
          character({ 'c', 'd', 'e', 'f' }),
          character({ 'y' }),
        })),
      }))
    });

    AssertThat(
      item_set.transitions(),
      Equals(LexItemSet::TransitionMap({
        {
          CharacterSet().include('a', 'b'),
          {
            LexItemSet({
              LexItem(Symbol(1), prec(2, character({ 'x' }))),
            }),
            PrecedenceRange(2)
          }
        },
        {
          CharacterSet().include('c', 'd'),
          {
            LexItemSet({
              LexItem(Symbol(1), prec(2, character({ 'x' }))),
              LexItem(Symbol(1), prec(3, character({ 'y' }))),
            }),
            PrecedenceRange(2, 3)
          }
        },
        {
          CharacterSet().include('e', 'f'),
          {
            LexItemSet({
              LexItem(Symbol(1), prec(3, character({ 'y' }))),
            }),
            PrecedenceRange(3)
          }
        },
      })));
  });

  it("handles choices between a subset and a superset of characters", [&]() {
    LexItemSet item_set({
      LexItem(Symbol(1), choice({
        seq({
          character({ 'b', 'c', 'd' }),
          character({ 'x' }),
        }),
        seq({
          character({ 'a', 'b', 'c', 'd', 'e', 'f' }),
          character({ 'y' }),
        }),
      })),
    });

    AssertThat(
      item_set.transitions(),
      Equals(LexItemSet::TransitionMap({
        {
          CharacterSet().include('a').include('e', 'f'),
          {
            LexItemSet({
              LexItem(Symbol(1), character({ 'y' })),
            }),
            PrecedenceRange()
          }
        },
        {
          CharacterSet().include('b', 'd'),
          {
            LexItemSet({
              LexItem(Symbol(1), character({ 'x' })),
              LexItem(Symbol(1), character({ 'y' })),
            }),
            PrecedenceRange()
          }
        },
      })));
  });

  it("handles choices between whitelisted and blacklisted character sets", [&]() {
    LexItemSet item_set({
      LexItem(Symbol(1), seq({
        choice({
          character({ '/' }, false),
          seq({
            character({ '\\' }),
            character({ '/' }),
          }),
        }),
        character({ '/' }),
      }))
    });

    AssertThat(
      item_set.transitions(),
      Equals(LexItemSet::TransitionMap({
        {
          CharacterSet().include_all().exclude('/').exclude('\\'),
          {
            LexItemSet({
              LexItem(Symbol(1), character({ '/' })),
            }),
            PrecedenceRange()
          }
        },
        {
          CharacterSet().include('\\'),
          {
            LexItemSet({
              LexItem(Symbol(1), character({ '/' })),
              LexItem(Symbol(1), seq({ character({ '/' }), character({ '/' }) })),
            }),
            PrecedenceRange()
          }
        },
      })));
  });

  it("handles different items with overlapping character sets", [&]() {
    LexItemSet set1({
      LexItem(Symbol(1), character({ 'a', 'b', 'c', 'd', 'e', 'f' })),
      LexItem(Symbol(2), character({ 'e', 'f', 'g', 'h', 'i' }))
    });

    AssertThat(set1.transitions(), Equals(LexItemSet::TransitionMap({
      {
        CharacterSet().include('a', 'd'),
        {
          LexItemSet({
            LexItem(Symbol(1), blank()),
          }),
          PrecedenceRange()
        }
      },
      {
        CharacterSet().include('e', 'f'),
        {
          LexItemSet({
            LexItem(Symbol(1), blank()),
            LexItem(Symbol(2), blank()),
          }),
          PrecedenceRange()
        }
      },
      {
        CharacterSet().include('g', 'i'),
        {
          LexItemSet({
            LexItem(Symbol(2), blank()),
          }),
          PrecedenceRange()
        }
      },
    })));
  });
});

END_TEST

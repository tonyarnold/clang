//===--- Format.cpp - Format C++ code -------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This is EXPERIMENTAL code under heavy development. It is not in a state yet,
//  where it can be used to format real code.
//
//  Implements Format.h.
//
//===----------------------------------------------------------------------===//

#include "clang/Format/Format.h"

#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"

namespace clang {
namespace format {

struct FormatToken {
  Token Tok;

  unsigned NewlinesBefore;
  unsigned WhiteSpaceLength;
  SourceLocation WhiteSpaceStart;
};

class Formatter {
public:
  Formatter(Lexer &Lex, SourceManager &Sources,
            const std::vector<CodeRange> &Ranges)
      : Lex(Lex), Sources(Sources), EndOfFile(false) {}

  tooling::Replacements format() {
    Lex.SetKeepWhitespaceMode(true);

    FormatToken NextToken;
    NextToken.WhiteSpaceLength = 0;

    // Read token stream and turn tokens into FormatTokens.
    while (!EndOfFile) {
      NextToken.Tok = getNextToken();
      StringRef Data(Sources.getCharacterData(NextToken.Tok.getLocation()),
          NextToken.Tok.getLength());
      if (NextToken.WhiteSpaceLength == 0) {
        NextToken.WhiteSpaceStart = NextToken.Tok.getLocation();
        NextToken.NewlinesBefore = 0;
      }
      if (NextToken.Tok.getKind() == tok::unknown) {
        StringRef Data(Sources.getCharacterData(NextToken.Tok.getLocation()),
                       NextToken.Tok.getLength());
        if (std::find(Data.begin(), Data.end(), '\n') != Data.end())
          ++NextToken.NewlinesBefore;
        NextToken.WhiteSpaceLength += NextToken.Tok.getLength();
        continue;
      }
      Tokens.push_back(NextToken);
      NextToken.WhiteSpaceLength = 0;
    }

    splitAndFormatContinuations();

    return Replaces;
  }

private:
  unsigned handlePPDirective(unsigned start) {
    for (unsigned i = start; i < Tokens.size() - 1; i++) {
      if (Tokens[i + 1].NewlinesBefore > 0) {
        llvm::outs() << "skipped pp directive until " << i << "\n";
        return i;
      }
    }
    return Tokens.size();
  }

  /// \brief Split token stream into continuations, i.e. something that we'd
  /// on a single line if we didn't have a column limit.
  void splitAndFormatContinuations() {
    unsigned Level = 0;
    unsigned ParenLevel = 0;
    unsigned ContinuationStart = 0;
    std::vector<bool> IsCompound;
    IsCompound.push_back(true);
    for (unsigned i = 0; i < Tokens.size(); i++) {
      if (Tokens[i].Tok.getKind() == tok::hash) {
        llvm::outs() << "hash found\n";
        i = handlePPDirective(i);
        ContinuationStart = i + 1;
        continue;
      }
      if (Tokens[i].Tok.getKind() == tok::comment) {
        if (i == ContinuationStart) {
          addNewline(ContinuationStart, Level);
          while (Tokens[i].NewlinesBefore == 0)
            ++i;
          ContinuationStart = i + 1;
        }
        continue;
      }
      if (Tokens[i].Tok.getKind() == tok::l_paren) {
        ++ParenLevel;
      } else if (Tokens[i].Tok.getKind() == tok::r_paren) {
        --ParenLevel;
      } else if (ParenLevel == 0) {
        if (Tokens[i].Tok.getKind() == tok::l_brace ||
            Tokens[i].Tok.getKind() == tok::r_brace ||
            Tokens[i].Tok.getKind() == tok::semi) {
          if (Tokens[i].Tok.getKind() == tok::r_brace) {
            if (i < Tokens.size() - 1) {
              if (Tokens[i + 1].Tok.getKind() == tok::semi)
                ++i;
            }
            --Level;
            IsCompound.pop_back();
            addNewline(ContinuationStart, Level);
            formatContinuation(ContinuationStart, i, Level);

            while (!IsCompound.back()) {
              --Level;
              IsCompound.pop_back();
            }
          } else {
            addNewline(ContinuationStart, Level);
            formatContinuation(ContinuationStart, i, Level);
          }

          while (Tokens[i].Tok.getKind() == tok::semi && !IsCompound.back()) {
            --Level;
            IsCompound.pop_back();
          }

          if (Tokens[i].Tok.getKind() == tok::l_brace) {
            ++Level;
            IsCompound.push_back(true);
          }

          ContinuationStart = i + 1;
        }

        else if (i != ContinuationStart) {
          if (isIfForOrWhile(Tokens[ContinuationStart].Tok)) {
            addNewline(ContinuationStart, Level);
            formatContinuation(ContinuationStart, i - 1, Level);
            ++Level;
            IsCompound.push_back(false);
            ContinuationStart = i;
          }
        }
      }
    }
  }

  // The current state when indenting a continuation.
  struct IndentState {
    unsigned ParenLevel;
    unsigned Column;
    std::vector<unsigned> Indent;
    std::vector<unsigned> UsedIndent;
  };

  // Append the token at 'Index' to the IndentState 'State'.
  void addToken(unsigned Index, bool Newline, bool DryRun, IndentState &State) {
    if (Tokens[Index].Tok.getKind() == tok::l_paren) {
      State.UsedIndent.push_back(State.UsedIndent.back());
      State.Indent.push_back(State.UsedIndent.back() + 4);
      ++State.ParenLevel;
    }
    if (Newline) {
      if (!DryRun)
        setWhitespace(Tokens[Index], 1, State.Indent[State.ParenLevel]);
      State.Column = State.Indent[State.ParenLevel] +
          Tokens[Index].Tok.getLength();
      State.UsedIndent[State.ParenLevel] = State.Indent[State.ParenLevel];
    } else {
      bool Space = spaceRequiredBetween(Tokens[Index - 1].Tok,
                                        Tokens[Index].Tok);
      if (Tokens[Index].NewlinesBefore == 0)
        Space = Tokens[Index].WhiteSpaceLength > 0;
      if (!DryRun)
        setWhitespace(Tokens[Index], 0, Space ? 1 : 0);
      if (Tokens[Index - 1].Tok.getKind() == tok::l_paren)
        State.Indent[State.ParenLevel] = State.Column;
      State.Column += Tokens[Index].Tok.getLength() + (Space ? 1 : 0);
    }

    if (Tokens[Index].Tok.getKind() == tok::r_paren) {
      --State.ParenLevel;
      State.Indent.pop_back();
    }
  }

  bool canBreakAfter(Token tok) {
    return tok.getKind() == tok::comma || tok.getKind() == tok::semi ||
        tok.getKind() == tok::l_paren;
  }

  // Calculate the number of lines needed to format the remaining part of the
  // continuation starting in the state 'State'. If 'NewLine' is set, a new line
  // will be added after the previous token.
  // 'EndIndex' is the last token belonging to the continuation.
  // 'StopAt' is used for optimization. If we can determine that we'll
  // definitely need more than 'StopAt' additional lines, we already know of a
  // better solution.
  int numLines(IndentState State, bool NewLine, unsigned Index,
               unsigned EndIndex, int StopAt) {
    count++;

    // We are at the end of the continuation, so we don't need any more lines.
    if (Index > EndIndex)
      return 0;

    addToken(Index - 1, NewLine, true, State);
    if (NewLine)
      --StopAt;

    // Exceeding 80 columns is bad.
    if (State.Column > 80)
      return 10000;

    if (StopAt < 1)
      return 10000;

    int NoBreak = numLines(State, false, Index + 1, EndIndex, StopAt);
    if (!canBreakAfter(Tokens[Index - 1].Tok))
      return NoBreak + (NewLine ? 1 : 0);
    int Break = numLines(State, true, Index + 1, EndIndex,
                         std::min(StopAt, NoBreak));
    return std::min(NoBreak, Break) + (NewLine ? 1 : 0);
  }

  void formatContinuation(unsigned StartIndex, unsigned EndIndex,
                          unsigned Level) {
    count = 0;
    IndentState State;
    State.ParenLevel = 0;
    State.Column = Level * 2 + Tokens[StartIndex].Tok.getLength();
    State.UsedIndent.push_back(Level * 2);
    State.Indent.push_back(Level * 2 + 4);
    for (unsigned i = StartIndex + 1; i <= EndIndex; ++i) {
      bool InsertNewLine = Tokens[i].NewlinesBefore > 0;
      if (!InsertNewLine) {
        int NoBreak = numLines(State, false, i + 1, EndIndex, 100000);
        int Break = numLines(State, true, i + 1, EndIndex, 100000);
        InsertNewLine = Break < NoBreak;
      }
      addToken(i, InsertNewLine, false, State);
    }
    llvm::outs() << "Tried combinations: " << count << "\n";
  }

  void setWhitespace(const FormatToken& Tok, unsigned NewLines,
                     unsigned Spaces) {
    Replaces.insert(tooling::Replacement(Sources, Tok.WhiteSpaceStart,
                                         Tok.WhiteSpaceLength,
                                         std::string(NewLines, '\n') +
                                         std::string(Spaces, ' ')));
  }

  bool isIfForOrWhile(Token Tok) {
    if (Tok.getKind() != tok::raw_identifier)
      return false;
    StringRef Data(Sources.getCharacterData(Tok.getLocation()),
        Tok.getLength());
    return Data == "for" || Data == "while" || Data == "if";
  }

  bool spaceRequiredBetween(Token Left, Token Right) {

    if (Left.is(tok::period) || Right.is(tok::period))
      return false;
    if (Left.is(tok::colon) || Right.is(tok::colon))
      return false;
    if (Left.is(tok::plusplus) && Right.is(tok::raw_identifier))
      return false;
    if (Left.is(tok::l_paren))
      return false;
    if (Right.is(tok::r_paren) || Right.is(tok::semi) || Right.is(tok::comma))
      return false;
    if (Right.is(tok::l_paren)) {
      return isIfForOrWhile(Left);
    }
    return true;
  }

  Token getNextToken() {
    Token tok;
    EndOfFile = Lex.LexFromRawLexer(tok);
    return tok;
  }

  /// \brief Add a new line before token \c Index.
  void addNewline(unsigned Index, unsigned Level) {
    if (Tokens[Index].WhiteSpaceStart.isValid()) {
      unsigned Newlines = Tokens[Index].NewlinesBefore;
      if (Newlines == 0 && Index != 0)
        Newlines = 1;
      setWhitespace(Tokens[Index], Newlines, Level * 2);
    }
  }

  Lexer &Lex;
  SourceManager &Sources;
  bool EndOfFile;
  tooling::Replacements Replaces;
  std::vector<FormatToken> Tokens;

  // Count number of tried states visited when formatting a continuation.
  unsigned int count;
};

tooling::Replacements reformat(Lexer &Lex, SourceManager &Sources,
                               std::vector<CodeRange> Ranges) {
  Formatter formatter(Lex, Sources, Ranges);
  return formatter.format();
}

}  // namespace format
}  // namespace clang
